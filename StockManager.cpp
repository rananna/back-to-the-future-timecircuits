#include "StockManager.h"
#include "DebugLog.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "timezone.h"
#include <esp_tls.h>

extern bool timeSynchronized;

// A simple Stream implementation for esp_tls
class TlsStream : public Stream {
private:
    esp_tls_t *tls;

public:
    TlsStream(esp_tls_t *tls_handle) : tls(tls_handle) {}

    virtual int available() {
        // esp_tls_get_bytes_avail can be unreliable, so we just indicate that data *might* be available.
        // The read() function will handle blocking/timeouts.
        return 1;
    }

    virtual int read() {
        unsigned char c;
        int ret;
        unsigned long start_time = millis();
        // This timeout should be longer than the TLS-level timeout, but not infinite.
        // It's a safety net for the stream consumer (ArduinoJson). 15 seconds.
        const unsigned long READ_TIMEOUT_MS = 15000;

        while (millis() - start_time < READ_TIMEOUT_MS) {
            ret = esp_tls_conn_read(tls, &c, 1);

            if (ret > 0) {
                return c; // Success, return the character
            }

            if (ret == 0) {
                // Connection closed by peer
                return -1;
            }

            // ret < 0, it's an error
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                // A fatal error occurred
                return -1;
            }

            // It's a WANT_READ or WANT_WRITE. We need to wait and retry.
            // A small delay to prevent busy-waiting and yield to other tasks.
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        // If we get here, we've timed out.
        Log_printf(LOG_LEVEL_WARN, "TlsStream::read() timed out after %lu ms", READ_TIMEOUT_MS);
        return -1;
    }

    virtual int peek() {
        // Not implemented for this use case.
        return -1;
    }

    virtual void flush() {
        // Not implemented
    }

    virtual size_t write(uint8_t data) {
        if (esp_tls_conn_write(tls, &data, 1) > 0) {
            return 1;
        }
        return 0;
    }
};

// A stream that combines a pre-read buffer with another stream.
// This is necessary because we read a chunk of the response to find the
// end of the HTTP headers, and that chunk may contain the start of the body.
class DechunkingStream : public Stream {
private:
    Stream& _stream;
    size_t _chunk_len;
    bool _stream_ended;

    // Helper to read a line from the stream
    String readLine() {
        String line = "";
        unsigned long start_time = millis();
        // A short timeout to avoid getting stuck forever
        const unsigned long READLINE_TIMEOUT_MS = 2000;
        while (millis() - start_time < READLINE_TIMEOUT_MS) {
            if (!_stream.available()) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            char c = _stream.read();
            if (c < 0) { // Should not happen if available() is reliable, but as a safeguard
                break;
            }
            if (c == '\r') {
                continue; // Ignore CR
            }
            if (c == '\n') {
                break; // End of line
            }
            line += c;
        }
        return line;
    }

    // Read the next chunk size. Returns false if parsing fails or end of stream.
    bool readChunkSize() {
        if (_stream_ended) {
            return false;
        }

        String line = readLine();
        if (line.length() == 0) {
            Log_printf(LOG_LEVEL_WARN, "DechunkingStream: Timed out or failed to read chunk size line.");
            _stream_ended = true;
            return false;
        }

        // strtol with base 16 to parse hex
        _chunk_len = strtol(line.c_str(), NULL, 16);
        Log_printf(LOG_LEVEL_DEBUG, "DechunkingStream: Read chunk size %d (0x%X)", _chunk_len, _chunk_len);

        if (_chunk_len == 0) {
            _stream_ended = true;
            // Consume final CRLF if present
            readLine();
        }
        return _chunk_len > 0;
    }

public:
    DechunkingStream(Stream& s)
        : _stream(s), _chunk_len(0), _stream_ended(false) {}

    virtual int available() {
        if (_stream_ended) return 0;
        // This is just an estimate. The stream consumer should rely on read() returning -1.
        return _chunk_len + _stream.available();
    }

    virtual int read() {
        if (_stream_ended) {
            return -1;
        }

        if (_chunk_len == 0) {
            if (!readChunkSize()) {
                return -1; // End of stream or error
            }
        }

        if (_chunk_len > 0) {
            int val = _stream.read();
            if (val != -1) {
                _chunk_len--;
                if (_chunk_len == 0) {
                    // End of chunk, consume the trailing CRLF
                    readLine();
                }
                return val;
            } else {
                // Underlying stream ended unexpectedly
                _stream_ended = true;
                return -1;
            }
        }
        // Should not be reached if logic is correct
        return -1;
    }

    virtual int peek() {
        // Peek is complicated to implement correctly with de-chunking,
        // and ArduinoJson's parser doesn't require it.
        return -1;
    }

    virtual void flush() {
        _stream.flush();
    }

    virtual size_t write(uint8_t data) {
        // Not needed for this implementation
        return 0;
    }
};

class CombinedStream : public Stream {
private:
    const char* _buffer;
    size_t _buffer_len;
    size_t _buffer_pos;
    Stream& _stream;

public:
    CombinedStream(const char* buf, size_t len, Stream& s)
        : _buffer(buf), _buffer_len(len), _buffer_pos(0), _stream(s) {}

    virtual int available() {
        return (_buffer_len - _buffer_pos) + _stream.available();
    }

    virtual int read() {
        if (_buffer_pos < _buffer_len) {
            return _buffer[_buffer_pos++];
        }
        return _stream.read();
    }

    virtual int peek() {
        if (_buffer_pos < _buffer_len) {
            return _buffer[_buffer_pos];
        }
        return _stream.peek();
    }

    virtual void flush() {
        _stream.flush();
    }

    virtual size_t write(uint8_t data) {
        // Not needed for this implementation
        return 0;
    }
};

// Root CA certificate for financialmodelingprep.com (Amazon Root CA 1)
static const char *fmp_root_ca = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIEkjCCA3qgAwIBAgITBn+USionzfP6wq4rAfkI7rnExjANBgkqhkiG9w0BAQsF\n" \
"ADCBmDELMAkGA1UEBhMCVVMxEDAOBgNVBAgTB0FyaXpvbmExEzARBgNVBAcTClNj\n" \
"b3R0c2RhbGUxJTAjBgNVBAoTHFN0YXJmaWVsZCBUZWNobm9sb2dpZXMsIEluYy4x\n" \
"OzA5BgNVBAMTMlN0YXJmaWVsZCBTZXJ2aWNlcyBSb290IENlcnRpZmljYXRlIEF1\n" \
"dGhvcml0eSAtIEcyMB4XDTE1MDUyNTEyMDAwMFoXDTM3MTIzMTAxMDAwMFowOTEL\n" \
"MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n" \
"b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n" \
"ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n" \
"9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n" \
"IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n" \
"VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n" \
"93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n" \
"jgSubJrIqg0CAwEAAaOCATEwggEtMA8GA1UdEwEB/wQFMAMBAf8wDgYDVR0PAQH/\n" \
"BAQDAgGGMB0GA1UdDgQWBBSEGMyFNOy8DJSULghZnMeyEE4KCDAfBgNVHSMEGDAW\n" \
"gBScXwDfqgHXMCs4iKK4bUqc8hGRgzB4BggrBgEFBQcBAQRsMGowLgYIKwYBBQUH\n" \
"MAGGImh0dHA6Ly9vY3NwLnJvb3RnMi5hbWF6b250cnVzdC5jb20wOAYIKwYBBQUH\n" \
"MAKGLGh0dHA6Ly9jcnQucm9vdGcyLmFtYXpvbnRydXN0LmNvbS9yb290ZzIuY2Vy\n" \
"MD0GA1UdHwQ2MDQwMqAwoC6GLGh0dHA6Ly9jcmwucm9vdGcyLmFtYXpvbnRydXN0\n" \
"LmNvbS9yb290ZzIuY3JsMBEGA1UdIAQKMAgwBgYEVR0gADANBgkqhkiG9w0BAQsF\n" \
"AAOCAQEAYjdCXLwQtT6LLOkMm2xF4gcAevnFWAu5CIw+7bMlPLVvUOTNNWqnkzSW\n" \
"MiGpSESrnO09tKpzbeR/FoCJbM8oAxiDR3mjEH4wW6w7sGDgd9QIpuEdfF7Au/ma\n" \
"eyKdpwAJfqxGF4PcnCZXmTA5YpaP7dreqsXMGz7KQ2hsVxa81Q4gLv7/wmpdLqBK\n" \
"bRRYh5TmOTFffHPLkIhqhBGWJ6bt2YFGpn6jcgAKUj6DiAdjd4lpFw85hdKrCEVN\n" \
"0FE6/V1dN2RMfjCyVSRCnTawXZwXgWHxyvkQAiSr6w10kY17RSlQOYiypok1JR4U\n" \
"akcjMS9cmvqtmg5iUaQqqcT5NJ0hGA==\n" \
"-----END CERTIFICATE-----\n";

StockManager::StockManager() :
    _api_key(""),
    _refresh_interval_ms(2 * 60 * 1000), // Default 2 minutes
    _last_fetch_time(0),
    _enabled(false),
    _is_fetching(false),
    _current_asset_index(0),
    _current_page_index(0),
    _api_usage_count(0),
    _running_tasks(0) {
    _task_mutex = xSemaphoreCreateMutex();
}

void StockManager::begin() {
    Log_printf(LOG_LEVEL_INFO, "StockManager initialized.");
}

void StockManager::loop() {
    if (!_enabled || _is_fetching || _api_key.isEmpty()) {
        return;
    }

    unsigned long now = millis();
    if (now - _last_fetch_time > _refresh_interval_ms) {
        fetchData();
    }
}

bool StockManager::addAsset(const String& symbol, AssetType type) {
    for (const auto& asset : _assets) {
        if (asset.symbol.equalsIgnoreCase(symbol)) {
            Log_printf(LOG_LEVEL_WARN, "Asset %s already exists.", symbol.c_str());
            return false;
        }
    }

    Asset newAsset;
    newAsset.symbol = symbol;
    newAsset.type = type;
    _assets.push_back(newAsset);
    Log_printf(LOG_LEVEL_INFO, "Added asset: %s", symbol.c_str());
    return true;
}

bool StockManager::removeAsset(const String& symbol) {
    auto it = std::remove_if(_assets.begin(), _assets.end(), [&](const Asset& asset) {
        return asset.symbol.equalsIgnoreCase(symbol);
    });

    if (it != _assets.end()) {
        _assets.erase(it, _assets.end());
        Log_printf(LOG_LEVEL_INFO, "Removed asset: %s", symbol.c_str());
        if (_current_asset_index >= _assets.size() && !_assets.empty()) {
            _current_asset_index = _assets.size() - 1;
        }
        return true;
    }
    return false;
}

void StockManager::reorderAssets(const std::vector<String>& symbols) {
    std::vector<Asset> reordered_assets;
    for (const auto& symbol : symbols) {
        for (const auto& asset : _assets) {
            if (asset.symbol.equalsIgnoreCase(symbol)) {
                reordered_assets.push_back(asset);
                break;
            }
        }
    }
    _assets = reordered_assets;
    Log_printf(LOG_LEVEL_INFO, "Assets reordered.");
}

void StockManager::clearAssets() {
    _assets.clear();
    Log_printf(LOG_LEVEL_INFO, "All assets cleared.");
}

const std::vector<Asset>& StockManager::getAssets() const {
    return _assets;
}

// Forward declaration for the FreeRTOS task
void fetchStockDataBatchTask(void* p);

struct StockFetchParams {
    std::vector<String> symbols;
    AssetType type;
    StockManager* manager;
};

void StockManager::fetchData() {
    if (!isMarketOpen()) {
        Log_printf(LOG_LEVEL_INFO, "Market is closed. Skipping stock data fetch.");
        _last_fetch_time = millis();
        return;
    }

    Log_printf(LOG_LEVEL_INFO, "Fetching stock data...");
    _is_fetching = true;
    _last_fetch_time = millis();

    std::map<String, std::vector<String>> stocks_by_tz;
    std::vector<String> cryptos;
    std::map<String, std::vector<String>> indices_by_tz;

    for (const auto& asset : _assets) {
        switch (asset.type) {
            case STOCK: {
                const char* tz = asset.timezone.isEmpty() ? "EST5EDT,M3.2.0,M11.1.0" : asset.timezone.c_str();
                if (isStockMarketOpen(tz)) {
                    stocks_by_tz[tz].push_back(asset.symbol);
                }
                break;
            }
            case CRYPTO:
                // Crypto market is always open, so no check needed here.
                cryptos.push_back(asset.symbol);
                break;
            case INDEX: {
                const char* tz = asset.timezone.isEmpty() ? "EST5EDT,M3.2.0,M11.1.0" : asset.timezone.c_str();
                if (isStockMarketOpen(tz)) {
                    indices_by_tz[tz].push_back(asset.symbol);
                }
                break;
            }
        }
    }

    xSemaphoreTake(_task_mutex, portMAX_DELAY);
    _running_tasks = 0;

    // Create tasks for stocks, grouped by timezone
    for (auto const& pair : stocks_by_tz) {
        const std::vector<String>& symbols = pair.second;
        if (!symbols.empty()) {
            StockFetchParams* params = new StockFetchParams{symbols, STOCK, this};
            if (xTaskCreate(fetchStockDataBatchTask, "stockFetch", 8192, params, 1, NULL) == pdPASS) {
                _running_tasks += 1;
            } else {
                delete params;
            }
        }
    }

    // Create task for cryptos
    if (!cryptos.empty()) {
        StockFetchParams* params = new StockFetchParams{cryptos, CRYPTO, this};
        if (xTaskCreate(fetchStockDataBatchTask, "cryptoFetch", 8192, params, 1, NULL) == pdPASS) {
            _running_tasks += 1;
        } else {
            delete params;
        }
    }

    // Create tasks for indices, grouped by timezone
    for (auto const& pair : indices_by_tz) {
        const std::vector<String>& symbols = pair.second;
        if (!symbols.empty()) {
            StockFetchParams* params = new StockFetchParams{symbols, INDEX, this};
            if (xTaskCreate(fetchStockDataBatchTask, "indexFetch", 8192, params, 1, NULL) == pdPASS) {
                _running_tasks += 1;
            } else {
                delete params;
            }
        }
    }

    xSemaphoreGive(_task_mutex);

    if (_running_tasks == 0) {
        _is_fetching = false;
    }
}

FetchStatus StockManager::fetchBatchDataFromApi(const std::vector<String>& symbols, AssetType type) {
    esp_tls_t *tls_stock = esp_tls_init();
    if (!tls_stock) {
        Log_printf(LOG_LEVEL_ERROR, "Failed to allocate stock TLS handle.");
        return FETCH_CONNECTION_FAILED;
    }

    FetchStatus status = FETCH_FAILED; // Default status
    char header_buf[2048];
    char* body_start_ptr = NULL;
    size_t header_len = 0;

    // This block scopes the variables that were causing issues with goto.
    // The goto will now jump to the 'cleanup' label outside this block.
    {
        esp_tls_cfg_t cfg = {};
        cfg.cacert_buf = (const unsigned char *)fmp_root_ca;
        cfg.cacert_bytes = strlen(fmp_root_ca) + 1;
        cfg.timeout_ms = 10000; // Increased timeout for connection

        const char *hostname = "financialmodelingprep.com";
        if (esp_tls_conn_new_sync(hostname, strlen(hostname), 443, &cfg, tls_stock) < 0) {
            Log_printf(LOG_LEVEL_ERROR, "Failed to create stock TLS connection.");
            status = FETCH_CONNECTION_FAILED;
            goto cleanup;
        }

        Log_printf(LOG_LEVEL_DEBUG, "Stock TLS connection established.");

        String symbols_str = "";
        for (const auto& s : symbols) {
            symbols_str += s + ",";
        }
        if (symbols_str.length() > 0) {
            symbols_str.remove(symbols_str.length() - 1);
        }

        char request[512];
        snprintf(request, sizeof(request),
                "GET /api/v3/quote/%s?apikey=%s HTTP/1.1\r\n"
                "Host: financialmodelingprep.com\r\n"
                "Connection: close\r\n" // Use close instead of keep-alive
                "\r\n",
                symbols_str.c_str(), _api_key.c_str());

        // --- START: MODIFICATION - Add logging for stock API calls ---
        char url_log[256];
        snprintf(url_log, sizeof(url_log), "https://financialmodelingprep.com/api/v3/quote/%s?apikey=REDACTED", symbols_str.c_str());
        Log_printf(LOG_LEVEL_INFO, "Fetching stock data from URL: %s", url_log);
        // --- END: MODIFICATION ---

        if (esp_tls_conn_write(tls_stock, request, strlen(request)) < 0) {
            Log_printf(LOG_LEVEL_ERROR, "Stock esp_tls_conn_write failed.");
            status = FETCH_CONNECTION_FAILED;
            goto cleanup;
        }
        _api_usage_count++;

        unsigned long header_read_start_time = millis();
        const unsigned long HEADER_TIMEOUT_MS = 10000;

        // Read headers and find the body
        while (millis() - header_read_start_time < HEADER_TIMEOUT_MS) {
            int ret = esp_tls_conn_read(tls_stock,
                                        (unsigned char *)header_buf + header_len,
                                        sizeof(header_buf) - header_len - 1);

            if (ret < 0) {
                if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                    Log_printf(LOG_LEVEL_ERROR, "Stock esp_tls_conn_read failed: -0x%x", -ret);
                    status = FETCH_CONNECTION_FAILED;
                    goto cleanup;
                }
                vTaskDelay(pdMS_TO_TICKS(20));
                continue;
            }

            if (ret == 0) {
                Log_printf(LOG_LEVEL_WARN, "Stock connection closed by peer during header read.");
                status = FETCH_CONNECTION_FAILED;
                goto cleanup;
            }

            header_len += ret;
            header_buf[header_len] = '\0';

            body_start_ptr = strstr(header_buf, "\r\n\r\n");
            if (body_start_ptr) {
                break;
            }

            if (header_len >= sizeof(header_buf) - 1) {
                Log_printf(LOG_LEVEL_ERROR, "Stock headers too large, aborting.");
                status = FETCH_FAILED;
                goto cleanup;
            }
        }

        if (!body_start_ptr) {
            Log_printf(LOG_LEVEL_ERROR, "Timed out waiting for stock HTTP headers.");
            status = FETCH_CONNECTION_FAILED;
            goto cleanup;
        }

        // Parse HTTP status code
        int http_status = 0;
        const char* status_line_start = strstr(header_buf, "HTTP/");
        if (status_line_start) {
            const char* code_start = strchr(status_line_start, ' ');
            if (code_start) {
                http_status = strtol(code_start + 1, NULL, 10);
            }
        }

        if (http_status != 200) {
            Log_printf(LOG_LEVEL_WARN, "Stock HTTP request failed with code %d.", http_status);
            if (http_status == 401 || http_status == 403) {
                // Mark all assets as invalid due to API key error
                for (const auto& symbol : symbols) {
                    auto it = std::find_if(_assets.begin(), _assets.end(), [&](const Asset& asset) {
                        return asset.symbol.equalsIgnoreCase(symbol);
                    });
                    if (it != _assets.end()) {
                        it->data_valid = false;
                        it->error_reason = "INVALID API KEY";
                    }
                }
            }
            if (http_status == 429) {
                status = FETCH_RATE_LIMITED;
            } else {
                status = FETCH_FAILED;
            }
            goto cleanup;
        }

        // Prepare for JSON parsing
        body_start_ptr += 4; // Move past the double CRLF
        size_t body_part_len = header_len - (body_start_ptr - header_buf);

        TlsStream tls_stream(tls_stock);
        CombinedStream combined_stream(body_start_ptr, body_part_len, tls_stream);

        bool is_chunked = (strcasestr(header_buf, "Transfer-Encoding: chunked") != NULL);

        JsonDocument filter;
        filter[0]["symbol"] = true;
        filter[0]["name"] = true;
        filter[0]["price"] = true;
        filter[0]["changesPercentage"] = true;
        filter[0]["dayLow"] = true;
        filter[0]["dayHigh"] = true;
        filter[0]["volume"] = true;

        JsonDocument doc;
        DeserializationError error;

        if (is_chunked) {
            DechunkingStream dechunking_stream(combined_stream);
            error = deserializeJson(doc, dechunking_stream, DeserializationOption::Filter(filter));
        } else {
            error = deserializeJson(doc, combined_stream, DeserializationOption::Filter(filter));
        }

        if (error == DeserializationError::Ok) {
            parseJsonResponse(doc, type, symbols);
            status = FETCH_SUCCESS;
        } else {
            Log_printf(LOG_LEVEL_ERROR, "Failed to parse stock JSON for %s: %s", symbols_str.c_str(), error.c_str());
            status = FETCH_FAILED;
        }
    }

cleanup:
    Log_printf(LOG_LEVEL_DEBUG, "Stock TLS connection closing.");
    esp_tls_conn_destroy(tls_stock);
    return status;
}

void StockManager::fetchBatchData(const std::vector<String>& symbols, AssetType type) {
    FetchStatus status = FETCH_FAILED;
    int attempt = 0;
    const int maxAttempts = 3;

    while (attempt < maxAttempts) {
        attempt++;
        Log_printf(LOG_LEVEL_INFO, "Stock fetch attempt %d of %d...", attempt, maxAttempts);
        status = fetchBatchDataFromApi(symbols, type);

        if (status == FETCH_SUCCESS) {
            Log_printf(LOG_LEVEL_INFO, "Successfully fetched stock data on attempt %d.", attempt);
            return; // Exit successfully
        }

        if (status == FETCH_FAILED) {
            Log_printf(LOG_LEVEL_ERROR, "Stock fetch failed with an unrecoverable error.");
            return; // Don't retry on hard errors
        }

        if (attempt < maxAttempts) {
            if (status == FETCH_RATE_LIMITED) {
                Log_printf(LOG_LEVEL_WARN, "Rate limited. Retrying in 60 seconds...");
                vTaskDelay(pdMS_TO_TICKS(60000));
            } else { // FETCH_CONNECTION_FAILED
                Log_printf(LOG_LEVEL_WARN, "Stock fetch failed on attempt %d. Retrying in 5 seconds...", attempt);
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        }
    }

    Log_printf(LOG_LEVEL_ERROR, "Stock fetch failed after all attempts.");
}

void StockManager::parseJsonResponse(JsonDocument& doc, AssetType type, const std::vector<String>& requested_symbols) {
    JsonArray array = doc.as<JsonArray>();

    // Create a temporary list of received symbols for efficient lookup.
    std::vector<String> received_symbols;
    if (!array.isNull()) {
        for (JsonObject quote : array) {
            String symbol = quote["symbol"];
            if (symbol.isEmpty()) {
                continue;
            }
            received_symbols.push_back(symbol);

            auto it = std::find_if(_assets.begin(), _assets.end(), [&](const Asset& asset) {
                return asset.symbol.equalsIgnoreCase(symbol);
            });

            if (it != _assets.end()) {
                it->price = quote["price"].as<float>();
                it->change_percent = quote["changesPercentage"].as<float>();
                it->day_high = quote["dayHigh"].as<float>();
                it->day_low = quote["dayLow"].as<float>();
                it->volume = quote["volume"].as<unsigned long>();
                it->name = quote["name"].as<String>();
                it->currency = quote["currency"].as<String>();
                it->last_update = millis();
                it->data_valid = true;
                it->error_reason = ""; // Clear previous error
                Log_printf(LOG_LEVEL_INFO, "Updated data for %s", symbol.c_str());
            } else {
                Log_printf(LOG_LEVEL_WARN, "Received data for untracked symbol: %s", symbol.c_str());
            }
        }
    } else {
        Log_printf(LOG_LEVEL_WARN, "JSON response was null or not an array. Assuming all requested symbols are invalid.");
    }

    // Now, iterate through the originally requested symbols and mark any that were not in the response as invalid.
    // This ensures that symbols that result in an empty or malformed response are correctly handled.
    for (const auto& req_sym : requested_symbols) {
        bool found = false;
        for (const auto& rec_sym : received_symbols) {
            if (req_sym.equalsIgnoreCase(rec_sym)) {
                found = true;
                break;
            }
        }
        if (!found) {
            auto it = std::find_if(_assets.begin(), _assets.end(), [&](const Asset& asset) {
                return asset.symbol.equalsIgnoreCase(req_sym);
            });
            if (it != _assets.end()) {
                it->data_valid = false;
                it->error_reason = "INVALID";
                Log_printf(LOG_LEVEL_WARN, "Symbol %s requested but not found in API response. Marked as invalid.", req_sym.c_str());
            }
        }
    }
}

void fetchStockDataBatchTask(void* p) {
    StockFetchParams* params = (StockFetchParams*)p;
    params->manager->fetchBatchData(params->symbols, params->type);

    xSemaphoreTake(params->manager->_task_mutex, portMAX_DELAY);
    params->manager->_running_tasks -= 1;
    if (params->manager->_running_tasks == 0) {
        params->manager->_is_fetching = false;
        Log_printf(LOG_LEVEL_INFO, "All stock fetch tasks complete.");
    }
    xSemaphoreGive(params->manager->_task_mutex);

    delete params;
    vTaskDelete(NULL);
}

String getCurrencySymbol(const String& currency_code) {
    if (currency_code == "USD") return "$";
    if (currency_code == "EUR") return "€";
    if (currency_code == "GBP") return "£";
    if (currency_code == "JPY") return "¥";
    if (currency_code == "CAD") return "$";
    if (currency_code == "AUD") return "$";
    if (currency_code == "CHF") return "Fr";
    return currency_code; // Fallback to the code itself
}

String StockManager::getMarqueeLine() {
    if (!_enabled) {
        return "";
    }

    if (!isMarketOpen()) {
        return "MARKET CLOSED";
    }

    if (_assets.empty()) {
        return "ADD ASSETS IN UI";
    }

    if (_current_asset_index >= _assets.size()) {
        _current_asset_index = 0;
    }

    const Asset& current_asset = _assets[_current_asset_index];

    if (!current_asset.data_valid) {
        if (!current_asset.error_reason.isEmpty()) {
            return current_asset.symbol + " " + current_asset.error_reason;
        }
        return current_asset.symbol + " NO DATA";
    }

    char buffer[50];
    String currency_symbol = getCurrencySymbol(current_asset.currency);
    switch (_current_page_index) {
        case 0: // Price and Percentage Change
            snprintf(buffer, sizeof(buffer), "%s %s%.2f %.2f%%",
                     current_asset.symbol.c_str(),
                     currency_symbol.c_str(),
                     current_asset.price,
                     current_asset.change_percent);
            break;
        case 1: // Day's High and Low
            snprintf(buffer, sizeof(buffer), "%s H:%s%.2f L:%s%.2f",
                     current_asset.symbol.c_str(),
                     currency_symbol.c_str(),
                     current_asset.day_high,
                     currency_symbol.c_str(),
                     current_asset.day_low);
            break;
        case 2: // Trading Volume
            snprintf(buffer, sizeof(buffer), "%s VOL: %lu",
                     current_asset.symbol.c_str(),
                     current_asset.volume);
            break;
        default:
            _current_page_index = 0;
            return getMarqueeLine();
    }

    return String(buffer);
}

void StockManager::nextPage() {
    if (_assets.empty()) return;

    _current_page_index++;
    if (_current_page_index > 2) { // 3 pages: Price, High/Low, Volume
        _current_page_index = 0;
        _current_asset_index++;
        if (_current_asset_index >= _assets.size()) {
            _current_asset_index = 0;
        }
    }
    Log_printf(LOG_LEVEL_INFO, "Stock Ticker: Next Page (Asset: %d, Page: %d)", _current_asset_index, _current_page_index);
}

void StockManager::previousPage() {
    if (_assets.empty()) return;

    _current_page_index--;
    if (_current_page_index < 0) {
        _current_page_index = 2;
        _current_asset_index--;
        if (_current_asset_index < 0) {
            _current_asset_index = _assets.size() - 1;
        }
    }
    Log_printf(LOG_LEVEL_INFO, "Stock Ticker: Previous Page (Asset: %d, Page: %d)", _current_asset_index, _current_page_index);
}

void StockManager::setApiKey(const String& key) {
    _api_key = key;
}

void StockManager::setRefreshInterval(unsigned long interval) {
    _refresh_interval_ms = interval * 60 * 1000;
}

void StockManager::setEnabled(bool enabled) {
    _enabled = enabled;
    if (_enabled) {
        _last_fetch_time = 0; // Force fetch on enable
    }
}

int StockManager::getApiUsage() const {
    return _api_usage_count;
}

bool StockManager::isMarketOpen() const {
    bool crypto_market_open = isCryptoMarketOpen();

    for (const auto& asset : _assets) {
        if (asset.type == CRYPTO && crypto_market_open) {
            return true;
        }
        // For stocks and indices, check their specific market hours.
        // If an asset has no timezone set, we can default to US market hours or assume it's closed.
        // For now, we'll default to US hours if the timezone string is empty.
        if (asset.type == STOCK || asset.type == INDEX) {
            const char* tz = asset.timezone.isEmpty() ? "EST5EDT,M3.2.0,M11.1.0" : asset.timezone.c_str();
            if (isStockMarketOpen(tz)) {
                return true;
            }
        }
    }
    return false;
}

bool isStockMarketOpen(const char* tz_string) {
    if (!timeSynchronized) return false;

    // Set the timezone for this specific check
    setenv("TZ", tz_string, 1);
    tzset();

    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);

    // Market is open Monday (1) to Friday (5)
    if (timeinfo->tm_wday < 1 || timeinfo->tm_wday > 5) {
        return false;
    }

    // Market hours are 9:30 AM to 4:00 PM
    int current_minutes = timeinfo->tm_hour * 60 + timeinfo->tm_min;
    int market_open_minutes = 9 * 60 + 30;
    int market_close_minutes = 16 * 60;

    if (current_minutes >= market_open_minutes && current_minutes < market_close_minutes) {
        return true;
    }

    return false;
}

bool StockManager::isCryptoMarketOpen() const {
    return true; // Crypto market is 24/7
}
