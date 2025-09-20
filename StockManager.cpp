#include "StockManager.h"
#include "DebugLog.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "timezone.h"
#include <esp_tls.h>

extern bool timeSynchronized;

void broadcastStockUpdate(); // Forward declaration

// Forward declaration for the task function
void fetchSingleStockTask(void* p);

// A simple Stream implementation for esp_tls
class TlsStream : public Stream {
private:
    esp_tls_t *tls;

public:
    TlsStream(esp_tls_t *tls_handle) : tls(tls_handle) {}

    virtual int available() {
        // esp_tls_get_bytes_avail can be unreliable, so we just indicate that data *might* be available.
        // The read() function will handle blocking/timeouts.
        return esp_tls_get_bytes_avail(tls);
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

// Root CA certificate for financialmodelingprep.com (Amazon RSA 2048 M04)
static const char *fmp_root_ca = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIEXjCCA0agAwIBAgITB3MSTyqVLj7Rili9uF0bwM5fJzANBgkqhkiG9w0BAQsF\n" \
"ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n" \
"b24gUm9vdCBDQSAxMB4XDTIyMDgyMzIyMjYzNVoXDTMwMDgyMzIyMjYzNVowPDEL\n" \
"MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEcMBoGA1UEAxMTQW1hem9uIFJT\n" \
"QSAyMDQ4IE0wNDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAM3pVR6A\n" \
"lQOp4xe776FdePXyejgA38mYx1ou9/jrpV6Sfn+/oqBKgwhY6ePsQHHQayWBJdBn\n" \
"v4Wz363qRI4XUh9swBFJ11TnZ3LqOMvHmWq2+loA0QPtOfXdJ2fHBLrBrngtJ/GB\n" \
"0p5olAVYrSZgvQGP16Rf8ddtNyxEEhYm3HuhmNi+vSeAq1tLYJPAvRCXonTpWdSD\n" \
"xY6hvdmxlqTYi82AtBXSfpGQ58HHM0hw0C6aQakghrwWi5fGslLOqzpimNMIsT7c\n" \
"qa0GJx6JfKqJqmQQNplO2h8n9ZsFJgBowof01ppdoLAWg6caMOM0om/VILKaa30F\n" \
"9W/r8Qjah7ltGVkCAwEAAaOCAVowggFWMBIGA1UdEwEB/wQIMAYBAf8CAQAwDgYD\n" \
"VR0PAQH/BAQDAgGGMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjAdBgNV\n" \
"HQ4EFgQUH1KSYVaCVH+BZtgdPQqqMlyH3QgwHwYDVR0jBBgwFoAUhBjMhTTsvAyU\n" \
"lC4IWZzHshBOCggwewYIKwYBBQUHAQEEbzBtMC8GCCsGAQUFBzABhiNodHRwOi8v\n" \
"b2NzcC5yb290Y2ExLmFtYXpvbnRydXN0LmNvbTA6BggrBgEFBQcwAoYuaHR0cDov\n" \
"L2NydC5yb290Y2ExLmFtYXpvbnRydXN0LmNvbS9yb290Y2ExLmNlcjA/BgNVHR8E\n" \
"ODA2MDSgMqAwhi5odHRwOi8vY3JsLnJvb3RjYTEuYW1hem9udHJ1c3QuY29tL3Jv\n" \
"b3RjYTEuY3JsMBMGA1UdIAQMMAowCAYGZ4EMAQIBMA0GCSqGSIb3DQEBCwUAA4IB\n" \
"AQA+1O5UsAaNuW3lHzJtpNGwBnZd9QEYFtxpiAnIaV4qApnGS9OCw5ZPwie7YSlD\n" \
"ZF5yyFPsFhUC2Q9uJHY/CRV1b5hIiGH0+6+w5PgKiY1MWuWT8VAaJjFxvuhM7a/e\n" \
"fN2TIw1Wd6WCl6YRisunjQOrSP+unqC8A540JNyZ1JOE3jVqat3OZBGgMvihdj2w\n" \
"Y23EpwesrKiQzkHzmvSH67PVW4ycbPy08HVZnBxZ5NrlGG9bwXR3fNTaz+c+Ej6c\n" \
"5AnwI3qkOFgSkg3Y75cdFz6pO/olK+e3AqygAcv0WjzmkDPuBjssuZjCHMC56oH3\n" \
"GJkV29Di2j5prHJbwZjG1inU\n" \
"-----END CERTIFICATE-----\n";

#include <Preferences.h>

StockManager::StockManager() :
    _api_key(""),
    _refresh_interval_ms(20 * 60 * 1000), // Default 20 minutes
    _last_fetch_time(0),
    _enabled(false),
    _is_fetching(false),
    _current_asset_index(0),
    _current_page_index(0),
    _api_usage_count(0),
    _running_tasks(0),
    _data_updated(false) {
    _task_mutex = xSemaphoreCreateMutex();
    _assets_mutex = xSemaphoreCreateMutex();
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

bool StockManager::addAsset(const String& symbol) {
    xSemaphoreTake(_assets_mutex, portMAX_DELAY);
    for (const auto& asset : _assets) {
        if (asset.symbol.equalsIgnoreCase(symbol)) {
            Log_printf(LOG_LEVEL_WARN, "Asset %s already exists.", symbol.c_str());
            xSemaphoreGive(_assets_mutex);
            return false;
        }
    }

    Asset newAsset;
    newAsset.symbol = symbol;

    // Release the mutex before making a network call
    xSemaphoreGive(_assets_mutex);

    newAsset.exchange = fetchExchangeForSymbol(symbol);
    if (newAsset.exchange.isEmpty()) {
        Log_printf(LOG_LEVEL_WARN, "Could not determine exchange for %s. Adding asset with empty exchange.", symbol.c_str());
    }

    // Re-acquire the mutex to add the new asset to the vector
    xSemaphoreTake(_assets_mutex, portMAX_DELAY);
    _assets.push_back(newAsset);
    xSemaphoreGive(_assets_mutex);

    Log_printf(LOG_LEVEL_INFO, "Added asset: %s on exchange %s", symbol.c_str(), newAsset.exchange.c_str());

    // After adding, trigger an immediate fetch for this new asset
    // so the UI can be populated with fresh data right away.
    Log_printf(LOG_LEVEL_INFO, "Triggering immediate fetch for new asset: %s", symbol.c_str());
    std::vector<String> symbols_to_fetch;
    symbols_to_fetch.push_back(symbol);

    // This runs the fetch in a separate task so it doesn't block the UI response.
    StockFetchParams* params = new StockFetchParams{symbols_to_fetch, this};
    // Note: A new task function `fetchSingleStockTask` is used to avoid interfering with batch updates.
    if (xTaskCreate(fetchSingleStockTask, "singleStockFetch", 8192, params, 1, NULL) != pdPASS) {
        Log_printf(LOG_LEVEL_ERROR, "Failed to create single stock fetch task for %s.", symbol.c_str());
        delete params; // Clean up if task creation fails
    }

    return true;
}

bool StockManager::removeAsset(const String& symbol) {
    xSemaphoreTake(_assets_mutex, portMAX_DELAY);
    auto it = std::remove_if(_assets.begin(), _assets.end(), [&](const Asset& asset) {
        return asset.symbol.equalsIgnoreCase(symbol);
    });

    bool removed = false;
    if (it != _assets.end()) {
        _assets.erase(it, _assets.end());
        Log_printf(LOG_LEVEL_INFO, "Removed asset: %s", symbol.c_str());
        if (_current_asset_index >= _assets.size() && !_assets.empty()) {
            _current_asset_index = _assets.size() - 1;
        }
        removed = true;
    }
    xSemaphoreGive(_assets_mutex);
    return removed;
}

void StockManager::reorderAssets(const std::vector<String>& symbols) {
    xSemaphoreTake(_assets_mutex, portMAX_DELAY);
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
    xSemaphoreGive(_assets_mutex);
    Log_printf(LOG_LEVEL_INFO, "Assets reordered.");
}

void StockManager::clearAssets() {
    xSemaphoreTake(_assets_mutex, portMAX_DELAY);
    _assets.clear();
    xSemaphoreGive(_assets_mutex);
    Log_printf(LOG_LEVEL_INFO, "All assets cleared.");
}

const std::vector<Asset>& StockManager::getAssets() const {
    // Note: This method returns a const reference.
    // The caller must not hold onto this reference for a long time
    // and must be aware that it's only safe to use as long as
    // the StockManager instance is valid and not being modified
    // by another thread. For simple, short-lived operations,
    // this is much more efficient than copying the vector.
    // The caller should not attempt to modify the returned vector.
    return _assets;
}

// Forward declaration for the FreeRTOS task
void fetchStockDataBatchTask(void* p);

void StockManager::fetchData() {
    if (!timeSynchronized) {
        // Log_printf(LOG_LEVEL_WARN, "fetchData: Waiting for NTP time sync.");
        return;
    }

    if (_assets.empty() || _api_key.isEmpty()) {
        _is_fetching = false;
        return;
    }

    Log_printf(LOG_LEVEL_INFO, "Fetching stock data...");
    _is_fetching = true;
    _last_fetch_time = millis();

    std::vector<String> stocks;
    std::vector<String> cryptos;

    xSemaphoreTake(_assets_mutex, portMAX_DELAY);
    for (const auto& asset : _assets) {
        // Simple heuristic to distinguish crypto from stocks
        if (asset.symbol.indexOf("-USD") != -1 || asset.symbol.indexOf("-EUR") != -1) {
            cryptos.push_back(asset.symbol);
        } else {
            stocks.push_back(asset.symbol);
        }
    }
    xSemaphoreGive(_assets_mutex);

    xSemaphoreTake(_task_mutex, portMAX_DELAY);
    _running_tasks = 0;

    // Create a task for all cryptos (they trade 24/7)
    if (!cryptos.empty()) {
        StockFetchParams* params = new StockFetchParams{cryptos, this};
        if (xTaskCreate(fetchStockDataBatchTask, "cryptoFetch", 8192, params, 1, NULL) == pdPASS) {
            _running_tasks = _running_tasks + 1;
            Log_printf(LOG_LEVEL_INFO, "Created task to fetch %d crypto assets.", cryptos.size());
        } else {
            Log_printf(LOG_LEVEL_ERROR, "Failed to create crypto fetch task.");
            delete params;
        }
    }

    // Create a single task for all stocks
    if (!stocks.empty()) {
        StockFetchParams* params = new StockFetchParams{stocks, this};
        if (xTaskCreate(fetchStockDataBatchTask, "stockFetch", 8192, params, 1, NULL) == pdPASS) {
            _running_tasks = _running_tasks + 1;
            Log_printf(LOG_LEVEL_INFO, "Created task to fetch %d stock assets.", stocks.size());
        } else {
            Log_printf(LOG_LEVEL_ERROR, "Failed to create stock fetch task.");
            delete params;
        }
    }

    xSemaphoreGive(_task_mutex);

    if (_running_tasks == 0) {
        _is_fetching = false;
        Log_printf(LOG_LEVEL_INFO, "No assets to fetch at this time.");
    }
}


String StockManager::fetchExchangeForSymbol(const String& symbol) const {
    Log_printf(LOG_LEVEL_INFO, "Fetching exchange for symbol: %s", symbol.c_str());

    esp_tls_t *tls_search = esp_tls_init();
    if (!tls_search) {
        Log_printf(LOG_LEVEL_ERROR, "Failed to allocate search TLS handle.");
        return "";
    }

    String exchange = "";
    char header_buf[2048];
    char* body_start_ptr = NULL;
    size_t header_len = 0;

    {
        esp_tls_cfg_t cfg = {};
        cfg.cacert_buf = (const unsigned char *)fmp_root_ca;
        cfg.cacert_bytes = strlen(fmp_root_ca) + 1;
        cfg.timeout_ms = 10000;

        const char *hostname = "financialmodelingprep.com";
        if (esp_tls_conn_new_sync(hostname, strlen(hostname), 443, &cfg, tls_search) < 0) {
            Log_printf(LOG_LEVEL_ERROR, "Failed to create search TLS connection.");
            goto cleanup;
        }

        char request[512];
        snprintf(request, sizeof(request),
                 "GET /stable/quote?symbol=%s&apikey=%s HTTP/1.1\r\n"
                 "Host: financialmodelingprep.com\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 symbol.c_str(), _api_key.c_str());

        if (esp_tls_conn_write(tls_search, request, strlen(request)) < 0) {
            Log_printf(LOG_LEVEL_ERROR, "Search esp_tls_conn_write failed.");
            goto cleanup;
        }

        unsigned long header_read_start_time = millis();
        const unsigned long HEADER_TIMEOUT_MS = 10000;

        while (millis() - header_read_start_time < HEADER_TIMEOUT_MS) {
            int ret = esp_tls_conn_read(tls_search, (unsigned char *)header_buf + header_len, sizeof(header_buf) - header_len - 1);
            if (ret < 0) {
                if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                    Log_printf(LOG_LEVEL_ERROR, "Search esp_tls_conn_read failed: -0x%x", -ret);
                    goto cleanup;
                }
                vTaskDelay(pdMS_TO_TICKS(20));
                continue;
            }
            if (ret == 0) {
                Log_printf(LOG_LEVEL_WARN, "Search connection closed by peer during header read.");
                goto cleanup;
            }
            header_len += ret;
            header_buf[header_len] = '\0';
            body_start_ptr = strstr(header_buf, "\r\n\r\n");
            if (body_start_ptr) break;
        }

        if (!body_start_ptr) {
            Log_printf(LOG_LEVEL_ERROR, "Timed out waiting for search HTTP headers.");
            goto cleanup;
        }

        body_start_ptr += 4;
        size_t body_part_len = header_len - (body_start_ptr - header_buf);

        TlsStream tls_stream(tls_search);
        CombinedStream combined_stream(body_start_ptr, body_part_len, tls_stream);

        bool is_chunked = (strcasestr(header_buf, "Transfer-Encoding: chunked") != NULL);

        JsonDocument doc;
        DeserializationError error;
        if (is_chunked) {
            DechunkingStream dechunking_stream(combined_stream);
            error = deserializeJson(doc, dechunking_stream);
        } else {
            error = deserializeJson(doc, combined_stream);
        }

        if (error == DeserializationError::Ok) {
            // The API can return a single quote object, an array with a single quote object, or an empty array.
            JsonObject quote_obj;
            if (doc.is<JsonObject>()) {
                quote_obj = doc.as<JsonObject>();
            } else if (doc.is<JsonArray>()) {
                JsonArray array = doc.as<JsonArray>();
                if (array.size() > 0) {
                    quote_obj = array[0].as<JsonObject>();
                } else {
                    Log_printf(LOG_LEVEL_WARN, "Search response for '%s' was an empty array.", symbol.c_str());
                }
            }

            if (!quote_obj.isNull()) {
                exchange = quote_obj["exchange"].as<String>();
                if (!exchange.isEmpty()) {
                    Log_printf(LOG_LEVEL_INFO, "Found exchange '%s' for symbol '%s'", exchange.c_str(), symbol.c_str());
                } else {
                    // This can happen for invalid symbols, which might return a valid object but with no exchange.
                    Log_printf(LOG_LEVEL_WARN, "Exchange field is empty for symbol '%s'. It may be an invalid symbol.", symbol.c_str());
                }
            } else {
                // This handles cases where the response was not an object or a non-empty array.
                Log_printf(LOG_LEVEL_WARN, "Response for symbol '%s' did not contain a valid quote object.", symbol.c_str());
            }
        } else {
            Log_printf(LOG_LEVEL_ERROR, "Failed to parse search JSON for '%s': %s", symbol.c_str(), error.c_str());
        }
    }

cleanup:
    esp_tls_conn_destroy(tls_search);
    return exchange;
}

FetchStatus StockManager::fetchDataForSingleSymbol(const std::vector<String>& symbol_vec) {
    if (symbol_vec.empty()) {
        return FETCH_SUCCESS; // Nothing to fetch
    }

    esp_tls_t *tls_stock = esp_tls_init();
    if (!tls_stock) {
        Log_printf(LOG_LEVEL_ERROR, "Failed to allocate stock TLS handle.");
        return FETCH_CONNECTION_FAILED;
    }

    FetchStatus status = FETCH_FAILED;
    char header_buf[2048];
    char* body_start_ptr = NULL;
    size_t header_len = 0;

    {
        esp_tls_cfg_t cfg = {};
        cfg.cacert_buf = (const unsigned char *)fmp_root_ca;
        cfg.cacert_bytes = strlen(fmp_root_ca) + 1;
        cfg.timeout_ms = 10000;

        const char *hostname = "financialmodelingprep.com";
        if (esp_tls_conn_new_sync(hostname, strlen(hostname), 443, &cfg, tls_stock) < 0) {
            Log_printf(LOG_LEVEL_ERROR, "Failed to create stock TLS connection.");
            status = FETCH_CONNECTION_FAILED;
            goto cleanup;
        }

        Log_printf(LOG_LEVEL_DEBUG, "Stock TLS connection established.");

        char request[2048];
        char url_log[512];
        char* symbols_str_buf = NULL; // This is no longer used but kept to avoid breaking cleanup logic.

        // The vector should contain exactly one symbol. It must be capitalized for the API.
        std::vector<String> upper_symbols;
        upper_symbols.reserve(symbol_vec.size());
        for (const auto& s : symbol_vec) {
            String upper_s = s;
            upper_s.toUpperCase();
            upper_symbols.push_back(upper_s);
        }

        // All fetches are now for a single symbol, so we removed the batch logic.
        const char* symbol_cstr = upper_symbols[0].c_str();
        snprintf(request, sizeof(request),
                    "GET /stable/quote?symbol=%s&apikey=%s HTTP/1.1\r\n"
                    "Host: financialmodelingprep.com\r\n"
                    "Connection: close\r\n"
                    "\r\n",
                    symbol_cstr, _api_key.c_str());

        snprintf(url_log, sizeof(url_log), "https://financialmodelingprep.com/stable/quote?symbol=%s&apikey=REDACTED", symbol_cstr);

        Log_printf(LOG_LEVEL_INFO, "Fetching stock data from URL: %s", url_log);

        if (esp_tls_conn_write(tls_stock, request, strlen(request)) < 0) {
            Log_printf(LOG_LEVEL_ERROR, "Stock esp_tls_conn_write failed.");
            status = FETCH_CONNECTION_FAILED;
            goto cleanup;
        }
        _api_usage_count++;

        unsigned long header_read_start_time = millis();
        const unsigned long HEADER_TIMEOUT_MS = 10000;

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
                xSemaphoreTake(_assets_mutex, portMAX_DELAY);
                for (const auto& symbol : symbol_vec) {
                    auto it = std::find_if(_assets.begin(), _assets.end(), [&](const Asset& asset) {
                        return asset.symbol.equalsIgnoreCase(symbol);
                    });
                    if (it != _assets.end()) {
                        it->data_valid = false;
                        it->error_reason = "INVALID API KEY";
                    }
                }
                xSemaphoreGive(_assets_mutex);
            }
            if (http_status == 429) {
                status = FETCH_RATE_LIMITED;
            } else {
                status = FETCH_FAILED;
            }
            goto cleanup;
        }

        body_start_ptr += 4;
        size_t body_part_len = header_len - (body_start_ptr - header_buf);

        TlsStream tls_stream(tls_stock);
        CombinedStream combined_stream(body_start_ptr, body_part_len, tls_stream);

        bool is_chunked = (strcasestr(header_buf, "Transfer-Encoding: chunked") != NULL);

        JsonDocument doc;
        DeserializationError error;

        // For single symbol lookups, the API returns a single JSON object.
        // The parseJsonResponse function is designed to handle this.

        // Stream the response directly to the JSON parser to conserve memory.
        if (is_chunked) {
            DechunkingStream dechunking_stream(combined_stream);
            error = deserializeJson(doc, dechunking_stream);
        } else {
            error = deserializeJson(doc, combined_stream);
        }

        if (error == DeserializationError::Ok) {
            parseJsonResponse(doc, symbol_vec);
            status = FETCH_SUCCESS;
        } else {
            Log_printf(LOG_LEVEL_ERROR, "Failed to parse stock JSON: %s", error.c_str());
            status = FETCH_FAILED;
        }
    }

cleanup:
    Log_printf(LOG_LEVEL_DEBUG, "Stock TLS connection closing.");
    esp_tls_conn_destroy(tls_stock);
    return status;
}

void StockManager::fetchDataForMultipleSymbols(const std::vector<String>& symbols) {
    for (const auto& symbol : symbols) {
        FetchStatus status = FETCH_FAILED;
        int attempt = 0;
        const int maxAttempts = 3;

        // Create a vector with a single symbol to pass to the API function
        std::vector<String> single_symbol_vec;
        single_symbol_vec.push_back(symbol);

        while (attempt < maxAttempts) {
            attempt++;
            Log_printf(LOG_LEVEL_INFO, "Stock fetch for %s, attempt %d of %d...", symbol.c_str(), attempt, maxAttempts);
            status = fetchDataForSingleSymbol(single_symbol_vec);

            if (status == FETCH_SUCCESS) {
                // Log_printf(LOG_LEVEL_INFO, "Successfully fetched stock data for %s on attempt %d.", symbol.c_str(), attempt);
                break; // Success, exit the retry loop for this symbol
            }

            if (status == FETCH_FAILED) {
                Log_printf(LOG_LEVEL_ERROR, "Stock fetch for %s failed with an unrecoverable error.", symbol.c_str());
                break; // Don't retry on hard errors
            }

            if (attempt < maxAttempts) {
                if (status == FETCH_RATE_LIMITED) {
                    Log_printf(LOG_LEVEL_WARN, "Rate limited on %s. Retrying in 60 seconds...", symbol.c_str());
                    vTaskDelay(pdMS_TO_TICKS(60000));
                } else { // FETCH_CONNECTION_FAILED
                    Log_printf(LOG_LEVEL_WARN, "Stock fetch for %s failed on attempt %d. Retrying in 5 seconds...", symbol.c_str(), attempt);
                    vTaskDelay(pdMS_TO_TICKS(5000));
                }
            }
        }

        if (status != FETCH_SUCCESS) {
            Log_printf(LOG_LEVEL_ERROR, "Stock fetch for %s failed after all attempts.", symbol.c_str());
        }

        // Small delay between API calls to be nice to the server.
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void StockManager::parseJsonResponse(JsonDocument& doc, const std::vector<String>& requested_symbols) {
    xSemaphoreTake(_assets_mutex, portMAX_DELAY);

    // First, check for a global API error before processing symbols.
    if (doc.is<JsonObject>()) {
        JsonObject obj = doc.as<JsonObject>();
        if (obj.containsKey("Error Message")) {
            String error_msg = obj["Error Message"];
            Log_printf(LOG_LEVEL_ERROR, "API Error: %s", error_msg.c_str());

            // Check if the error is about the API key.
            if (error_msg.indexOf("Invalid API KEY") != -1) {
                // Mark all requested symbols with this error.
                for (const auto& req_sym : requested_symbols) {
                    auto it = std::find_if(_assets.begin(), _assets.end(), [&](const Asset& asset) {
                        return asset.symbol.equalsIgnoreCase(req_sym);
                    });
                    if (it != _assets.end()) {
                        it->data_valid = false;
                        it->error_reason = "INVALID API KEY";
                    }
                }
                xSemaphoreGive(_assets_mutex);
                return; // Stop further processing
            }
        }
    }

    // Create a temporary list of received symbols for efficient lookup.
    std::vector<String> received_symbols;

    // Lambda to process a single JSON object from the API response.
    auto process_quote = [&](JsonObject quote) {
        if (quote.isNull()) {
            return;
        }

        String symbol = quote["symbol"];
        if (symbol.isEmpty()) {
            // This case is now handled by the global error check above, but we keep this as a safeguard.
            return;
        }
        received_symbols.push_back(symbol);

        auto it = std::find_if(_assets.begin(), _assets.end(), [&](const Asset& asset) {
            return asset.symbol.equalsIgnoreCase(symbol);
        });

        if (it != _assets.end()) {
            it->price = quote["price"].as<float>();
            it->change_percent = quote["changePercentage"].as<float>();
            it->day_high = quote["dayHigh"].as<float>();
            it->day_low = quote["dayLow"].as<float>();
            it->volume = quote["volume"].as<unsigned long>();
            it->name = quote["name"].as<String>();
            it->currency = quote["currency"].as<String>();
            it->last_update = millis();
            it->data_valid = true;
            it->error_reason = ""; // Clear previous error
            _data_updated = true; // Flag that data has changed
            Log_printf(LOG_LEVEL_INFO, "Updated data for %s", symbol.c_str());
        } else {
            Log_printf(LOG_LEVEL_WARN, "Received data for untracked symbol: %s", symbol.c_str());
        }
    };

    // The API can return a single quote object, an array with a single quote object,
    // or an error object. An invalid symbol might return an empty array `[]`.
    if (doc.is<JsonObject>()) {
        // Handle single object response
        process_quote(doc.as<JsonObject>());
    } else if (doc.is<JsonArray>()) {
        // Handle array response
        JsonArray array = doc.as<JsonArray>();
        if (array.size() > 0) {
            // Process the first element of the array, as that's the expected format for single symbol lookups.
            process_quote(array[0].as<JsonObject>());
        } else {
            // Handle empty array `[]` for an invalid symbol
            Log_printf(LOG_LEVEL_WARN, "JSON response was an empty array. Assuming requested symbol is invalid.");
        }
    } else {
        // This handles other unexpected response types.
        Log_printf(LOG_LEVEL_WARN, "JSON response was not a valid object or array. Assuming all requested symbols are invalid.");
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
                // Only mark as invalid symbol if no other error has been set. This prevents
                // overwriting a more specific error (like "INVALID API KEY" from the HTTP status check).
                if (it->error_reason.isEmpty()) {
                    it->data_valid = false;
                    it->error_reason = "INVALID SYMBOL";
                    Log_printf(LOG_LEVEL_WARN, "Symbol %s requested but not found in API response. Marked as invalid.", req_sym.c_str());
                }
            }
        }
    }
    xSemaphoreGive(_assets_mutex);
}

void fetchStockDataBatchTask(void* p) {
    StockFetchParams* params = (StockFetchParams*)p;
    params->manager->fetchDataForMultipleSymbols(params->symbols);

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

// This is the new task function for fetching a single, newly added asset.
// It does not interfere with the main batch fetching state (_is_fetching, _running_tasks).
void fetchSingleStockTask(void* p) {
    StockFetchParams* params = (StockFetchParams*)p;
    Log_printf(LOG_LEVEL_INFO, "Single stock fetch task started for %s.", params->symbols[0].c_str());
    params->manager->fetchDataForMultipleSymbols(params->symbols);
    Log_printf(LOG_LEVEL_INFO, "Single stock fetch task finished for %s.", params->symbols[0].c_str());
    broadcastStockUpdate();
    delete params;
    vTaskDelete(NULL);
}

String getCurrencySymbol(const String& currency_code) {
    if (currency_code.isEmpty() || currency_code == "USD" || currency_code.equalsIgnoreCase("null")) return "$";
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

    xSemaphoreTake(_assets_mutex, portMAX_DELAY);

    if (_assets.empty()) {
        xSemaphoreGive(_assets_mutex);
        return "ADD SYMBOLS IN UI";
    }

    if (_current_asset_index >= _assets.size() || _current_asset_index < 0) {
        _current_asset_index = 0;
    }

    const Asset& current_asset = _assets[_current_asset_index];

    if (!current_asset.data_valid) {
        String error_msg;
        if (!current_asset.error_reason.isEmpty()) {
            error_msg = current_asset.symbol + " " + current_asset.error_reason;
        } else {
            error_msg = current_asset.symbol + " NO DATA";
        }
        xSemaphoreGive(_assets_mutex);
        return error_msg;
    }

    char buffer[128]; // Buffer for formatting the string
    String currency_symbol_str = getCurrencySymbol(current_asset.currency);
    const char* currency_symbol = currency_symbol_str.c_str();

    // Always display a simplified line with symbol, price, and change.
    char change_str[10];
    snprintf(change_str, sizeof(change_str), "%+.2f%%", current_asset.change_percent);

    char price_buf[32];
    snprintf(price_buf, sizeof(price_buf), "%s%.2f", currency_symbol, current_asset.price);

    snprintf(buffer, sizeof(buffer), "%s %s %s",
            current_asset.symbol.c_str(),
            price_buf,
            change_str);

    xSemaphoreGive(_assets_mutex);
    return String(buffer);
}

bool StockManager::hasDataBeenUpdated() {
    return _data_updated;
}

void StockManager::clearDataUpdatedFlag() {
    _data_updated = false;
}

void StockManager::nextPage() {
    xSemaphoreTake(_assets_mutex, portMAX_DELAY);
    if (_assets.empty()) {
        xSemaphoreGive(_assets_mutex);
        return;
    }

    const int NUM_PAGES_PER_ASSET = 2; // Page 0: Price, Page 1: High/Low

    _current_page_index++;
    if (_current_page_index >= NUM_PAGES_PER_ASSET) {
        _current_page_index = 0;
        _current_asset_index++;
        if (_current_asset_index >= _assets.size()) {
            _current_asset_index = 0;
        }
        Log_printf(LOG_LEVEL_INFO, "Stock Ticker: Next Asset (%d)", _current_asset_index);
    } else {
        Log_printf(LOG_LEVEL_INFO, "Stock Ticker: Next Page (%d) for Asset %d", _current_page_index, _current_asset_index);
    }
    xSemaphoreGive(_assets_mutex);
}

void StockManager::previousPage() {
    xSemaphoreTake(_assets_mutex, portMAX_DELAY);
    if (_assets.empty()) {
        xSemaphoreGive(_assets_mutex);
        return;
    }

    const int NUM_PAGES_PER_ASSET = 2;

    _current_page_index--;
    if (_current_page_index < 0) {
        _current_page_index = NUM_PAGES_PER_ASSET - 1;
        _current_asset_index--;
        if (_current_asset_index < 0) {
            // If we were at the first asset, wrap around to the last page of the last asset
            _current_asset_index = _assets.size() - 1;
        }
        Log_printf(LOG_LEVEL_INFO, "Stock Ticker: Previous Asset (%d)", _current_asset_index);
    } else {
        Log_printf(LOG_LEVEL_INFO, "Stock Ticker: Previous Page (%d) for Asset %d", _current_page_index, _current_asset_index);
    }
    xSemaphoreGive(_assets_mutex);
}

void StockManager::setApiKey(const String& key) {
    _api_key = key;
}

String StockManager::getApiKey() const {
    return _api_key;
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
    // Market status check is disabled to stay within free API limits.
    return true;
}

void StockManager::updateAndSaveAssets(const std::vector<String>& symbols) {
    Log_printf(LOG_LEVEL_INFO, "Updating and saving assets.");

    // Create a new vector to hold the updated and ordered assets.
    std::vector<Asset> new_assets;

    // To preserve existing asset data, create a temporary map of the current assets.
    std::map<String, Asset> current_assets_map;
    xSemaphoreTake(_assets_mutex, portMAX_DELAY);
    for (const auto& asset : _assets) {
        current_assets_map[asset.symbol] = asset;
    }
    xSemaphoreGive(_assets_mutex);

    // Iterate through the desired list of symbols.
    for (const auto& symbol : symbols) {
        if (symbol.isEmpty()) continue;

        // Check if the asset already exists in our map.
        auto it = current_assets_map.find(symbol);
        if (it != current_assets_map.end()) {
            // Asset exists, so we move it to the new list.
            new_assets.push_back(it->second);
        } else {
            // This is a new asset. We need to create it and fetch its exchange.
            Log_printf(LOG_LEVEL_INFO, "Found new asset to add: %s", symbol.c_str());
            Asset newAsset;
            newAsset.symbol = symbol;
            newAsset.exchange = fetchExchangeForSymbol(symbol); // This is a blocking network call.

            if (newAsset.exchange.isEmpty()) {
                Log_printf(LOG_LEVEL_WARN, "Could not determine exchange for %s. Adding asset with empty exchange.", symbol.c_str());
            }
            new_assets.push_back(newAsset);

            // Trigger an immediate data fetch for this new asset in a background task.
            Log_printf(LOG_LEVEL_INFO, "Triggering immediate fetch for new asset: %s", symbol.c_str());
            std::vector<String> symbols_to_fetch;
            symbols_to_fetch.push_back(symbol);
            StockFetchParams* params = new StockFetchParams{symbols_to_fetch, this};
            if (xTaskCreate(fetchSingleStockTask, "singleStockFetch", 8192, params, 1, NULL) != pdPASS) {
                Log_printf(LOG_LEVEL_ERROR, "Failed to create single stock fetch task for %s.", symbol.c_str());
                delete params;
            }
        }
    }

    // Now, replace the old assets vector with the newly constructed one.
    xSemaphoreTake(_assets_mutex, portMAX_DELAY);
    _assets = new_assets;
    // Reset the ticker if the current index is now out of bounds.
    if (_current_asset_index >= _assets.size() && !_assets.empty()) {
        _current_asset_index = 0;
    } else if (_assets.empty()) {
        _current_asset_index = 0;
    }
    xSemaphoreGive(_assets_mutex);

    // Finally, save the new state to flash.
    saveAssets();
    Log_printf(LOG_LEVEL_INFO, "Finished updating and saving assets.");
}

void StockManager::updateAssetsFromJson(const String& jsonString) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);

    if (error) {
        Log_printf(LOG_LEVEL_ERROR, "Failed to parse stock assets JSON: %s", error.c_str());
        return;
    }

    xSemaphoreTake(_assets_mutex, portMAX_DELAY);
    _assets.clear();
    JsonArray assetsArray = doc.as<JsonArray>();
    for (JsonObject assetObj : assetsArray) {
        Asset newAsset;
        newAsset.symbol = assetObj["symbol"].as<String>();
        newAsset.exchange = assetObj["exchange"].as<String>();
        _assets.push_back(newAsset);
    }
    int numLoaded = _assets.size();
    xSemaphoreGive(_assets_mutex);
    Log_printf(LOG_LEVEL_INFO, "Loaded %d stock assets from JSON.", numLoaded);
}

void StockManager::saveAssets() {
    std::vector<Asset> assets_copy;
    xSemaphoreTake(_assets_mutex, portMAX_DELAY);
    assets_copy = _assets;
    xSemaphoreGive(_assets_mutex);

    if (assets_copy.empty()) {
        Preferences preferences;
        preferences.begin("stocks", false);
        preferences.putString("assets", "[]");
        preferences.end();
        Log_printf(LOG_LEVEL_INFO, "No assets to save. Ensured assets in flash are cleared.");
        return;
    }

    // Calculate the required size for the JSON document.
    // See ArduinoJson Assistant: https://arduinojson.org/v6/assistant/
    size_t required_size = JSON_ARRAY_SIZE(assets_copy.size());
    for (const auto& asset : assets_copy) {
        required_size += JSON_OBJECT_SIZE(2);          // Two members: "symbol", "exchange"
        required_size += asset.symbol.length() + 1;     // +1 for null-terminator
        required_size += asset.exchange.length() + 1;   // +1 for null-terminator
    }

    // Use DynamicJsonDocument because the size is determined at runtime.
    DynamicJsonDocument doc(required_size);

    JsonArray assetsArray = doc.to<JsonArray>();
    for (const auto& asset : assets_copy) {
        JsonObject assetObj = assetsArray.add<JsonObject>();
        assetObj["symbol"] = asset.symbol;
        assetObj["exchange"] = asset.exchange;
    }

    if (doc.overflowed()) {
        Log_printf(LOG_LEVEL_ERROR, "Failed to serialize assets JSON: document overflowed. Capacity: %u, Usage: %u", doc.capacity(), doc.memoryUsage());
        // We should not save a partial file.
        return;
    }

    String jsonString;
    size_t actual_size = serializeJson(doc, jsonString);

    if (actual_size == 0 && !assets_copy.empty()) {
        Log_printf(LOG_LEVEL_ERROR, "Failed to serialize assets JSON: serializeJson returned 0.");
        return;
    }

    Log_printf(LOG_LEVEL_INFO, "Saving assets. Calculated size: %u, Actual size: %u, Num assets: %d", required_size, actual_size, assetsArray.size());

    Preferences preferences;
    preferences.begin("stocks", false);
    size_t bytes_written = preferences.putString("assets", jsonString);
    preferences.end();

    if (bytes_written == 0) {
        Log_printf(LOG_LEVEL_ERROR, "Failed to save assets to flash: putString returned 0.");
    } else {
        Log_printf(LOG_LEVEL_INFO, "Saved %d stock assets to flash. (%d bytes written)", assetsArray.size(), bytes_written);
    }
}

void StockManager::loadAssets() {
    Preferences preferences;
    preferences.begin("stocks", true); // read-only
    String assetsJson = preferences.getString("assets", "[]");
    preferences.end();
    updateAssetsFromJson(assetsJson);
}

bool StockManager::isTimeSynchronized() const {
    return timeSynchronized;
}

void StockManager::resetTicker() {
    xSemaphoreTake(_assets_mutex, portMAX_DELAY);
    _current_asset_index = 0;
    _current_page_index = 0;
    xSemaphoreGive(_assets_mutex);
    Log_printf(LOG_LEVEL_INFO, "Stock ticker reset to initial state.");
}

Asset StockManager::getCurrentStockInfo() const {
    xSemaphoreTake(_assets_mutex, portMAX_DELAY);
    if (_assets.empty()) {
        xSemaphoreGive(_assets_mutex);
        return Asset(); // Return an empty/default asset
    }
    // Ensure index is within bounds
    size_t index = _current_asset_index;
    if (index >= _assets.size()) {
        index = 0;
    }
    Asset current_asset = _assets[index];
    xSemaphoreGive(_assets_mutex);
    return current_asset;
}
