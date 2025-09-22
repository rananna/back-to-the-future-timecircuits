/**
 * @file DataManager.cpp
 * @brief Handles fetching and parsing data from external web APIs.
 * @details This module is responsible for all outbound network requests to services
 * like Open-Meteo (for weather), Financial Modeling Prep (for stocks), and custom
 * user-defined APIs for the Data Link feature. It uses FreeRTOS tasks to perform
 * these network operations asynchronously, preventing the main application loop
 * from blocking and ensuring the display remains responsive.
 */

#include <esp_tls.h>
#include "DebugLog.h"
#include "DataManager.h"
#include "EventManager.h"
#include "DisplayManager.h"
#include "web_server.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
extern bool weatherDataUpdated;
#include <WiFiClientSecure.h>
#include <time.h>

// Define the number of hourly forecasts to retrieve and process.
#define NUM_HOURLY_FORECASTS 3

extern StockData stockData[3];

String urlEncode(const char* msg) {
    const char *hex = "0123456789abcdef";
    String encodedMsg = "";
    while (*msg!='\0'){
        if( ('a' <= *msg && *msg <= 'z')
                || ('A' <= *msg && *msg <= 'Z')
                || ('0' <= *msg && *msg <= '9') || *msg == '-' || *msg == '_' || *msg == '.') {
            encodedMsg += *msg;
        } else {
            encodedMsg += '%';
            encodedMsg += hex[*msg >> 4];
            encodedMsg += hex[*msg & 15];
        }
        msg++;
    }
    return encodedMsg;
}

// In DataManager.cpp

/**
 * @brief Safely retrieves a nested value from a JSON object using a dot-and-bracket path.
 * @details This robust helper function allows for traversing a complex JSON structure
 * to extract a specific value. It supports nested objects (e.g., "data.current.temp")
 * and array indices (e.g., "results[0].value"). It makes a copy of the path string
 * to safely use `strtok_r`, which modifies the string it processes.
 * @param root The root JsonVariant to start the search from.
 * @param path A C-string representing the path to the desired value.
 * @return A JsonVariant containing the found value, or a null JsonVariant if not found.
 */
JsonVariant getJsonVariant(JsonVariant root, const char* path) {
    if (!path || path[0] == '\0') {
        return JsonVariant(); // Return null variant if path is invalid.
    }

    // Use a stack-allocated buffer to avoid heap fragmentation.
    // This is safer for long-running embedded systems.
    const size_t max_path_len = 256; // A reasonable limit for JSON paths.
    if (strlen(path) >= max_path_len) {
        Log_printf(LOG_LEVEL_ERROR, "JSON path is too long!");
        return JsonVariant();
    }
    char path_copy[max_path_len];
    strncpy(path_copy, path, max_path_len);
    path_copy[max_path_len - 1] = '\0'; // Ensure null termination.

    JsonVariant current = root;
    char* context = NULL;
    char* token = strtok_r(path_copy, ".[]", &context);
    while (token != NULL) {
        if (current.isNull()) {
            return JsonVariant();
        }
        if (current.is<JsonObject>()) {
            current = current[token];
        } else if (current.is<JsonArray>()) {
            char* endptr;
            long index = strtol(token, &endptr, 10);
            if (*endptr == '\0') { // It's a valid number
                 current = current[index];
            } else { // It's a key in an object that happens to be a number-like string
                 current = current[token];
            }
        } else {
            return JsonVariant();
        }
        token = strtok_r(NULL, ".[]", &context);
    }
    return current;
}



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

// Root CA certificate for api.open-meteo.com (Let's Encrypt R13)
static const char *open_meteo_com_root_ca = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIFBTCCAu2gAwIBAgIQWgDyEtjUtIDzkkFX6imDBTANBgkqhkiG9w0BAQsFADBP\n" \
"MQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFy\n" \
"Y2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBYMTAeFw0yNDAzMTMwMDAwMDBa\n" \
"Fw0yNzAzMTIyMzU5NTlaMDMxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBF\n" \
"bmNyeXB0MQwwCgYDVQQDEwNSMTMwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEK\n" \
"AoIBAQClZ3CN0FaBZBUXYc25BtStGZCMJlA3mBZjklTb2cyEBZPs0+wIG6BgUUNI\n" \
"fSvHSJaetC3ancgnO1ehn6vw1g7UDjDKb5ux0daknTI+WE41b0VYaHEX/D7YXYKg\n" \
"L7JRbLAaXbhZzjVlyIuhrxA3/+OcXcJJFzT/jCuLjfC8cSyTDB0FxLrHzarJXnzR\n" \
"yQH3nAP2/Apd9Np75tt2QnDr9E0i2gB3b9bJXxf92nUupVcM9upctuBzpWjPoXTi\n" \
"dYJ+EJ/B9aLrAek4sQpEzNPCifVJNYIKNLMc6YjCR06CDgo28EdPivEpBHXazeGa\n" \
"XP9enZiVuppD0EqiFwUBBDDTMrOPAgMBAAGjgfgwgfUwDgYDVR0PAQH/BAQDAgGG\n" \
"MB0GA1UdJQQWMBQGCCsGAQUFBwMCBggrBgEFBQcDATASBgNVHRMBAf8ECDAGAQH/\n" \
"AgEAMB0GA1UdDgQWBBTnq58PLDOgU9NeT3jIsoQOO9aSMzAfBgNVHSMEGDAWgBR5\n" \
"tFnme7bl5AFzgAiIyBpY9umbbjAyBggrBgEFBQcBAQQmMCQwIgYIKwYBBQUHMAKG\n" \
"Fmh0dHA6Ly94MS5pLmxlbmNyLm9yZy8wEwYDVR0gBAwwCjAIBgZngQwBAgEwJwYD\n" \
"VR0fBCAwHjAcoBqgGIYWaHR0cDovL3gxLmMubGVuY3Iub3JnLzANBgkqhkiG9w0B\n" \
"AQsFAAOCAgEAUTdYUqEimzW7TbrOypLqCfL7VOwYf/Q79OH5cHLCZeggfQhDconl\n" \
"k7Kgh8b0vi+/XuWu7CN8n/UPeg1vo3G+taXirrytthQinAHGwc/UdbOygJa9zuBc\n" \
"VyqoH3CXTXDInT+8a+c3aEVMJ2St+pSn4ed+WkDp8ijsijvEyFwE47hulW0Ltzjg\n" \
"9fOV5Pmrg/zxWbRuL+k0DBDHEJennCsAen7c35Pmx7jpmJ/HtgRhcnz0yjSBvyIw\n" \
"6L1QIupkCv2SBODT/xDD3gfQQyKv6roV4G2EhfEyAsWpmojxjCUCGiyg97FvDtm/\n" \
"NK2LSc9lybKxB73I2+P2G3CaWpvvpAiHCVu30jW8GCxKdfhsXtnIy2imskQqVZ2m\n" \
"0Pmxobb28Tucr7xBK7CtwvPrb79os7u2XP3O5f9b/H66GNyRrglRXlrYjI1oGYL/\n" \
"f4I1n/Sgusda6WvA6C190kxjU15Y12mHU4+BxyR9cx2hhGS9fAjMZKJss28qxvz6\n" \
"Axu4CaDmRNZpK/pQrXF17yXCXkmEWgvSOEZy6Z9pcbLIVEGckV/iVeq0AOo2pkg9\n" \
"p4QRIy0tK2diRENLSF2KysFwbY6B26BFeFs3v1sYVRhFW9nLkOrQVporCS0KyZmf\n" \
"wVD89qSTlnctLcZnIavjKsKUu1nA1iU0yYMdYepKR7lWbnwhdx3ewok=\n" \
"-----END CERTIFICATE-----\n";

// This new function is responsible for fetching all weather data (current, daily, and hourly) in a single API call.
// It performs one atomic attempt. Retries are handled by the calling function.
static bool fetchWeatherDataFromApi() {
    esp_tls_t *tls = esp_tls_init();
    if (tls == NULL) {
        Log_printf(LOG_LEVEL_ERROR, "Failed to allocate TLS handle.");
        return false;
    }

    // This boolean will track the outcome of the operation.
    bool success = false;

    // Use a do-while(0) loop and `break` statements instead of `goto` to avoid
    // "crosses initialization" errors. This is a safer pattern for resource management.
    do {
        esp_tls_cfg_t cfg = {};
        cfg.cacert_buf = (const unsigned char *)open_meteo_com_root_ca;
        cfg.cacert_bytes = strlen(open_meteo_com_root_ca) + 1;
        cfg.timeout_ms = 10000; // 10 second timeout for TLS handshake and read/write operations

        const char *hostname = "api.open-meteo.com";
        int ret = esp_tls_conn_new_sync(hostname, strlen(hostname), 443, &cfg, tls);
        if (ret < 0) {
            Log_printf(LOG_LEVEL_ERROR, "Failed to create TLS connection. esp_tls_conn_new_sync returned: -0x%x", -ret);
            int esp_tls_code, esp_tls_flags;
            esp_tls_error_handle_t esp_tls_error_handle;
            esp_tls_get_error_handle(tls, &esp_tls_error_handle);
            esp_err_t err = esp_tls_get_and_clear_last_error(esp_tls_error_handle, &esp_tls_code, &esp_tls_flags);
            if (err == ESP_OK) {
                Log_printf(LOG_LEVEL_ERROR, "Last ESP-TLS error: 0x%x, Last mbedTLS error: 0x%x", esp_tls_code, esp_tls_flags);
                // Check if the mbedTLS error flags indicate a certificate validation problem.
                // Certificate errors in mbedTLS are represented by flags with the BADCERT_ prefix.
                // These typically have higher-order bits set. A simple check for non-zero is a good start,
                // but a more specific check for certificate issues is better.
                // MBEDTLS_X509_BADCERT_EXPIRED = 0x2000, MBEDTLS_X509_BADCERT_NOT_TRUSTED = 0x2800
                if (esp_tls_flags & 0b0010000000000000) { // Check for the BADCERT bit range
                    if (xSemaphoreTake(xDisplayDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        currentWeatherData.errorReason = "CERT_ERROR";
                        xSemaphoreGive(xDisplayDataMutex);
                    }
                }
            }
            break;
        }
        Log_printf(LOG_LEVEL_DEBUG, "TLS connection established.");

        char tempUnit[11]; // "fahrenheit" is 10 chars + null
        char speedUnit[4]; // "kmh" or "mph" is 3 chars + null
        strncpy(tempUnit, currentSettings.useMetricUnits ? "celsius" : "fahrenheit", sizeof(tempUnit));
        strncpy(speedUnit, currentSettings.useMetricUnits ? "kmh" : "mph", sizeof(speedUnit));

        char request[512];
        snprintf(request, sizeof(request),
                "GET /v1/forecast?latitude=%.4f&longitude=%.4f"
                "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m"
                "&hourly=temperature_2m,weather_code"
                "&daily=temperature_2m_max,temperature_2m_min,weather_code,sunrise,sunset,precipitation_probability_max,wind_speed_10m_max"
                "&forecast_days=2&temperature_unit=%s&wind_speed_unit=%s&timezone=auto&timeformat=unixtime"
                " HTTP/1.1\r\n"
                "Host: api.open-meteo.com\r\n"
                "Connection: close\r\n"
                "\r\n",
                currentSettings.latitude, currentSettings.longitude, tempUnit, speedUnit);

        // --- Start of new logging ---
        // Log the full request URL for debugging purposes.
        char full_url[512];
        snprintf(full_url, sizeof(full_url),
                "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
                "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m"
                "&hourly=temperature_2m,weather_code"
                "&daily=temperature_2m_max,temperature_2m_min,weather_code,sunrise,sunset,precipitation_probability_max,wind_speed_10m_max"
                "&forecast_days=2&temperature_unit=%s&wind_speed_unit=%s&timezone=auto&timeformat=unixtime",
                currentSettings.latitude, currentSettings.longitude, tempUnit, speedUnit);
        Log_printf(LOG_LEVEL_INFO, "Weather API URL: %s", full_url);
        // --- End of new logging ---

        if (esp_tls_conn_write(tls, request, strlen(request)) < 0) {
            Log_printf(LOG_LEVEL_ERROR, "esp_tls_conn_write failed.");
            int esp_tls_code, esp_tls_flags;
            esp_tls_error_handle_t esp_tls_error_handle;
            esp_tls_get_error_handle(tls, &esp_tls_error_handle);
            esp_err_t err = esp_tls_get_and_clear_last_error(esp_tls_error_handle, &esp_tls_code, &esp_tls_flags);
            if (err == ESP_OK) {
                Log_printf(LOG_LEVEL_ERROR, "Last ESP-TLS error: 0x%x, Last mbedTLS error: 0x%x", esp_tls_code, esp_tls_flags);
            }
            break;
        }

        // Use a stack-allocated buffer for headers to avoid heap fragmentation.
        char header_buf[2048];
        int http_status = 0;
        size_t header_len = 0;
        unsigned long header_read_start_time = millis();
        const unsigned long HEADER_TIMEOUT_MS = 10000; // 10-second timeout

        char* body_start_ptr = NULL;

        while (millis() - header_read_start_time < HEADER_TIMEOUT_MS) {
            int read_ret = esp_tls_conn_read(tls,
                                        (unsigned char *)header_buf + header_len,
                                        sizeof(header_buf) - header_len - 1);

            if (read_ret < 0) {
                if (read_ret != MBEDTLS_ERR_SSL_WANT_READ && read_ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                    Log_printf(LOG_LEVEL_ERROR, "esp_tls_conn_read failed: -0x%x", -read_ret);
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(20));
                continue; // Continue while loop for non-fatal SSL want_read/write
            }

            if (read_ret == 0) {
                // The connection was closed gracefully by the peer after sending all data.
                // This is expected with "Connection: close". Break the loop and proceed.
                Log_printf(LOG_LEVEL_DEBUG, "Connection closed by peer, which is expected.");
                break;
            }

            header_len += read_ret;
            header_buf[header_len] = '\0'; // Null-terminate the buffer

            // Check if we've found the end of the headers
            body_start_ptr = strstr(header_buf, "\r\n\r\n");
            if (body_start_ptr) {
                // Found headers, but we should continue reading until the connection is closed
                // by the peer, as more body data might be waiting. We just stop looking for the
                // header separator.
                // For simplicity in this model, we'll break and assume the rest of the body follows.
                // A more robust implementation might continue reading until read_ret is 0.
            }

            // Check if the buffer is full
            if (header_len >= sizeof(header_buf) - 1) {
                Log_printf(LOG_LEVEL_ERROR, "Headers and initial body part are too large for buffer, aborting.");
                break;
            }
        }

        // After the read loop, we must have found the header separator
        body_start_ptr = strstr(header_buf, "\r\n\r\n");
        if (!body_start_ptr) {
            Log_printf(LOG_LEVEL_ERROR, "Could not find end of HTTP headers in response.");
            break;
        }

        // sscanf is fragile. A robust parsing of the status code is needed.
        const char* status_line_start = strstr(header_buf, "HTTP/");
        if (status_line_start) {
            const char* code_start = strchr(status_line_start, ' ');
            if (code_start) {
                http_status = strtol(code_start + 1, NULL, 10);
            } else {
                Log_printf(LOG_LEVEL_ERROR, "Malformed HTTP status line: no space found.");
                break;
            }
        } else {
            Log_printf(LOG_LEVEL_ERROR, "Could not find HTTP status line in response.");
            break;
        }

        if (http_status != 200) {
            Log_printf(LOG_LEVEL_WARN, "HTTP request failed with code %d.", http_status);

            // Log a preview of the server's error message.
            char* body_preview_start = strstr(header_buf, "\r\n\r\n");
            if (body_preview_start) {
                body_preview_start += 4; // Move past the separator
                char preview_buf[129]; // 128 chars + null
                size_t preview_len = strlen(body_preview_start);
                if (preview_len > 128) {
                    preview_len = 128;
                }
                strncpy(preview_buf, body_preview_start, preview_len);
                preview_buf[preview_len] = '\0';
                Log_printf(LOG_LEVEL_WARN, "Server response preview: %s", preview_buf);
            }
            break;
        }

        // The headers are now processed, proceed to the body
        body_start_ptr += 4;
        size_t body_part_len = header_len - (body_start_ptr - header_buf);

        // --- Add logging to see the raw response body ---
        char response_preview[513]; // 512 chars + null terminator
        size_t preview_len = (body_part_len < 512) ? body_part_len : 512;
        strncpy(response_preview, body_start_ptr, preview_len);
        response_preview[preview_len] = '\0';
        Log_printf(LOG_LEVEL_DEBUG, "Raw Weather Response Body Preview (first %d bytes): %s", preview_len, response_preview);
        // --- End of logging ---

        TlsStream tls_stream(tls);
        CombinedStream combined_stream(body_start_ptr, body_part_len, tls_stream);

        // Check for chunked encoding. Use strcasestr for case-insensitivity.
        bool is_chunked = false;
        // Note: strcasestr is a GNU extension, but it is available in ESP-IDF.
        if (strcasestr(header_buf, "Transfer-Encoding: chunked")) {
            is_chunked = true;
            Log_printf(LOG_LEVEL_INFO, "Chunked transfer encoding detected.");
        }

        JsonDocument filter;
        filter["timezone"] = true;
        JsonObject current_filter = filter["current"].to<JsonObject>();
        current_filter["time"] = true;
        current_filter["temperature_2m"] = true;
        current_filter["relative_humidity_2m"] = true;
        current_filter["apparent_temperature"] = true;
        current_filter["weather_code"] = true;
        current_filter["wind_speed_10m"] = true;
        JsonObject hourly_filter = filter["hourly"].to<JsonObject>();
        hourly_filter["time"] = true;
        hourly_filter["temperature_2m"] = true;
        hourly_filter["weather_code"] = true;
        JsonObject daily_filter = filter["daily"].to<JsonObject>();
        daily_filter["time"] = true;
        daily_filter["temperature_2m_max"] = true;
        daily_filter["temperature_2m_min"] = true;
        daily_filter["weather_code"] = true;
        daily_filter["sunrise"] = true;
        daily_filter["sunset"] = true;
        daily_filter["precipitation_probability_max"] = true;
        daily_filter["wind_speed_10m_max"] = true;

        JsonDocument doc;
        DeserializationError error;

        if (is_chunked) {
            DechunkingStream dechunking_stream(combined_stream);
            error = deserializeJson(doc, dechunking_stream, DeserializationOption::Filter(filter));
        } else {
            error = deserializeJson(doc, combined_stream, DeserializationOption::Filter(filter));
        }

        if (error == DeserializationError::Ok && !doc.isNull()) {
            if (doc["error"].isNull()) {
                if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                    Log_printf(LOG_LEVEL_DEBUG, "Successfully parsed Unified Weather JSON");

                    bool allDataPresent = true;
                    auto getJsonValue = [&](const JsonVariant& parent, const char* key) -> JsonVariant {
                        if (parent.isNull() || parent[key].isNull()) {
                            allDataPresent = false;
                            Log_printf(LOG_LEVEL_WARN, "Weather JSON missing key: %s", key);
                            return JsonVariant();
                        }
                        return parent[key];
                    };
                    auto getJsonValueFromArray = [&](const JsonVariant& parent, int index) -> JsonVariant {
                        if (parent.isNull() || !parent.is<JsonArray>() || parent.size() <= index) {
                            allDataPresent = false;
                            Log_printf(LOG_LEVEL_WARN, "Weather JSON array access out of bounds at index %d", index);
                            return JsonVariant();
                        }
                        return parent[index];
                    };

                    JsonObject current = doc["current"];
                    JsonObject daily = doc["daily"];
                    JsonObject hourly = doc["hourly"];

                    if (!doc["timezone"].isNull()) { currentWeatherData.timezone = doc["timezone"].as<const char*>(); }
                    currentWeatherData.temperature = getJsonValue(current, "temperature_2m");
                    currentWeatherData.apparentTemperature = getJsonValue(current, "apparent_temperature");
                    currentWeatherData.windSpeed = getJsonValue(current, "wind_speed_10m");
                    currentWeatherData.humidity = getJsonValue(current, "relative_humidity_2m");
                    currentWeatherData.weatherCode = getJsonValue(current, "weather_code");
                    currentWeatherData.dailyHigh = getJsonValueFromArray(getJsonValue(daily, "temperature_2m_max"), 0);
                    currentWeatherData.dailyLow = getJsonValueFromArray(getJsonValue(daily, "temperature_2m_min"), 0);
                    currentWeatherData.sunrise = getJsonValueFromArray(getJsonValue(daily, "sunrise"), 0);
                    currentWeatherData.sunset = getJsonValueFromArray(getJsonValue(daily, "sunset"), 0);
                    currentWeatherData.precipitationProbability = getJsonValueFromArray(getJsonValue(daily, "precipitation_probability_max"), 0);
                    currentWeatherData.maxWindSpeed = getJsonValueFromArray(getJsonValue(daily, "wind_speed_10m_max"), 0);
                    currentWeatherData.tomorrowHigh = getJsonValueFromArray(getJsonValue(daily, "temperature_2m_max"), 1);
                    currentWeatherData.tomorrowLow = getJsonValueFromArray(getJsonValue(daily, "temperature_2m_min"), 1);
                    currentWeatherData.tomorrowWeatherCode = getJsonValueFromArray(getJsonValue(daily, "weather_code"), 1);

                    JsonArray hourly_temp = getJsonValue(hourly, "temperature_2m").as<JsonArray>();
                    JsonArray hourly_code = getJsonValue(hourly, "weather_code").as<JsonArray>();

                    time_t now = getJsonValue(current, "time").as<time_t>();
                    if (now == 0) {
                        allDataPresent = false;
                        Log_printf(LOG_LEVEL_WARN, "Weather JSON missing current.time");
                    }

                    int startIndex = -1;
                    JsonArray timeArray = hourly["time"];
                    if (!timeArray.isNull()) {
                        for (int i = 0; i < timeArray.size(); i++) {
                            if (timeArray[i].as<time_t>() >= now) {
                                startIndex = i;
                                Log_printf(LOG_LEVEL_DEBUG, "Found start index %d for hourly forecast (current: %lu, forecast: %lu).", startIndex, (unsigned long)now, (unsigned long)timeArray[i].as<time_t>());
                                break;
                            }
                        }
                    } else { allDataPresent = false; Log_printf(LOG_LEVEL_WARN, "Weather JSON missing hourly.time array"); }

                    if (startIndex != -1) {
                        for (int j = 0; j < NUM_HOURLY_FORECASTS; j++) {
                            int forecastIndex = startIndex + j;
                            if (forecastIndex < hourly_temp.size() && forecastIndex < hourly_code.size()) {
                                currentWeatherData.hourlyTemp[j] = hourly_temp[forecastIndex];
                                currentWeatherData.hourlyCode[j] = hourly_code[forecastIndex];
                            } else { currentWeatherData.hourlyTemp[j] = -999; currentWeatherData.hourlyCode[j] = -1; }
                        }
                    } else {
                        Log_printf(LOG_LEVEL_WARN, "Could not find current hour in hourly forecast data. Hourly forecast may be inaccurate.");
                        bool fallbackSuccess = true;
                        for (int j = 0; j < NUM_HOURLY_FORECASTS; j++) {
                            int forecastIndex = j;
                            if (forecastIndex < hourly_temp.size() && forecastIndex < hourly_code.size()) {
                                currentWeatherData.hourlyTemp[j] = hourly_temp[forecastIndex];
                                currentWeatherData.hourlyCode[j] = hourly_code[forecastIndex];
                            } else { currentWeatherData.hourlyTemp[j] = -999; currentWeatherData.hourlyCode[j] = -1; fallbackSuccess = false; }
                        }
                        if (!fallbackSuccess) { allDataPresent = false; Log_printf(LOG_LEVEL_WARN, "Hourly forecast fallback also failed to retrieve complete data."); }
                    }

                    if (!allDataPresent) {
                        currentWeatherData.errorReason = "INCOMPLETE WEATHER DATA";
                        success = false;
                    } else {
                        success = true; // Success!
                    }
                    xSemaphoreGive(xDisplayDataMutex);
                } else {
                    Log_printf(LOG_LEVEL_ERROR, "Could not obtain display data mutex, weather data processing aborted.");
                    success = false;
                }
            } else {
                // This block handles cases where the API returns a valid JSON with an error message.
                const char* reason = doc["reason"].as<const char*>();
                Log_printf(LOG_LEVEL_WARN, "Unified Weather API returned an error: %s", reason);
                if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                    currentWeatherData.errorReason = reason;
                    xSemaphoreGive(xDisplayDataMutex);
                }
                success = false;
            }
        } else {
            // This block handles JSON parsing errors.
            Log_printf(LOG_LEVEL_WARN, "Failed to parse Unified Weather JSON. E: %s", error.c_str());
            if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                currentWeatherData.errorReason = "WEATHER PARSING FAILED";
                xSemaphoreGive(xDisplayDataMutex);
            }
            success = false;
        }
    } while(0);

    if (tls) {
        esp_tls_conn_destroy(tls);
        tls = NULL;
    }
    return success;
}

#include <algorithm>
#include <cctype>

// Function to trim leading and trailing whitespace from a std::string
void trim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

void fetchWeatherData(WeatherTaskParams* params) {
    // This function is now simplified. It only fetches weather for the coordinates
    // stored in currentSettings. The UI is responsible for all geocoding.
    float latitude = params->latitude;
    float longitude = params->longitude;
    delete params; // Clean up the params object.

    Log_printf(LOG_LEVEL_INFO, "Fetching weather data by coordinates. Lat: %f, Lon: %f", latitude, longitude);

    // Update the global settings with the coordinates for this fetch.
    if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
        currentSettings.latitude = latitude;
        currentSettings.longitude = longitude;
        xSemaphoreGive(xDisplayDataMutex);
    }

    // Clear any previous error state before starting the fetch process.
    if (xSemaphoreTake(xDisplayDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        currentWeatherData.errorReason.clear();
        xSemaphoreGive(xDisplayDataMutex);
    }

    // Call the new unified function to fetch all weather data, with retry logic.
    bool success = false;
    int attempt = 0;
    const int maxAttempts = 3; // Use a maximum of 3 attempts

    while (attempt < maxAttempts && !success) {
        attempt++;
        Log_printf(LOG_LEVEL_INFO, "Weather fetch attempt %d of %d...", attempt, maxAttempts);
        success = fetchWeatherDataFromApi();

        if (success) {
            Log_printf(LOG_LEVEL_INFO, "Successfully fetched all weather data on attempt %d.", attempt);
            break;
        }

        // After a failed attempt, check for unrecoverable errors to avoid pointless retries.
        if (xSemaphoreTake(xDisplayDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (!currentWeatherData.errorReason.empty()) {
                // The Open-Meteo API returns reasons like "Invalid timezone" or "Latitude is out of bounds".
                // These are configuration errors that won't be fixed by a simple retry.
                // We check for keywords in a case-insensitive way.
                std::string reason_lower = currentWeatherData.errorReason;
                std::transform(reason_lower.begin(), reason_lower.end(), reason_lower.begin(),
                    [](unsigned char c){ return std::tolower(c); });

                if (reason_lower.find("invalid") != std::string::npos ||
                    reason_lower.find("out of range") != std::string::npos ||
                    reason_lower.find("cert_error") != std::string::npos) {
                    Log_printf(LOG_LEVEL_ERROR, "Unrecoverable API error: %s. Aborting retries.", currentWeatherData.errorReason.c_str());
                    xSemaphoreGive(xDisplayDataMutex);
                    break; // Exit the retry loop immediately.
                }
            }
            xSemaphoreGive(xDisplayDataMutex);
        }

        if (attempt < maxAttempts) {
            Log_printf(LOG_LEVEL_WARN, "Weather fetch failed on attempt %d. Retrying in 5 seconds...", attempt);
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }

    if (success) {
        Log_printf(LOG_LEVEL_INFO, "Successfully fetched all weather data.");
        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
            currentWeatherData.dataValid = true;
            weatherDataUpdated = true; // Signal to the display manager
            isWeatherBufferDirty = true;
            xSemaphoreGive(xDisplayDataMutex);
            broadcastWeatherUpdate(); // Push the new data to the UI
        }
    } else {
        Log_printf(LOG_LEVEL_ERROR, "Weather fetch failed after all attempts.");
        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
            currentWeatherData.dataValid = false;
            // The specific error reason should already be set by fetchWeatherDataFromApi
            if (currentWeatherData.errorReason.empty()) {
                 currentWeatherData.errorReason = "WEATHER API FAILED - CHECK WI-FI";
            }
            xSemaphoreGive(xDisplayDataMutex);
        }
    }
}


void fetchWeatherDataTask(void* p) {
    // This task is for routine, non-forced updates.
    // It uses the latitude and longitude stored in the current settings.
    WeatherTaskParams* params = new WeatherTaskParams{currentSettings.cityName, false, currentSettings.latitude, currentSettings.longitude};
    fetchWeatherData(params);
    isFetchingWeather = false;
    vTaskDelete(NULL);
}

void forceFetchWeatherDataTask(void* p) {
    WeatherTaskParams* params = (WeatherTaskParams*)p;
    fetchWeatherData(params);
    isFetchingWeather = false;
    vTaskDelete(NULL);
}

void checkDataFetchStatusTask(void* p) {
    int tasksCreated = (int)p;
    Log_printf(LOG_LEVEL_DEBUG, "Data fetch status checker task started, waiting for %d tasks.", tasksCreated);
    while(true) {
        int completed = __atomic_load_n(&requestsCompleted, __ATOMIC_SEQ_CST);
        if (completed >= tasksCreated) {
            Log_printf(LOG_LEVEL_INFO, "All %d data fetch tasks completed.", completed);
            if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                isFetchingData = false;
                xSemaphoreGive(xDisplayDataMutex);
            }
            vTaskDelete(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void fetchDataLink() {
    if (xSemaphoreTake(xDisplayDataMutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return;
    }

    if (!currentSettings.dataLinkEnabled) {
        xSemaphoreGive(xDisplayDataMutex);
        return;
    }

    // This function now primarily handles static text updates.
    // MQTT and HA data are updated reactively via the mqttCallback.
    // We will still iterate through all points to ensure static text is displayed.

    bool needsUpdate = false;
    for (int i = 0; i < currentSettings.numDataPoints; i++) {
        if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_STATIC) {
            needsUpdate = true;
            break;
        }
    }

    if (!needsUpdate) {
        xSemaphoreGive(xDisplayDataMutex);
        return;
    }

    // Since this function is now lighter, we can run it more frequently
    // without the need for the isFetchingData flag, but we'll keep a simple timer.
	unsigned long now = millis();
	if (now - lastDataLinkFetch > 5000) { // Check every 5 seconds for static text
        lastDataLinkFetch = now;
        Log_printf(LOG_LEVEL_INFO, "Updating static text for Data Link marquee.");
        
        for (int i = 0; i < currentSettings.numDataPoints; i++) {
            if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_STATIC) {
                DataPoint point = currentSettings.dataPoints[i];

                // Always treat as scrolling text as per new simplified logic
                displayPages[i].year = point.scrollingText;
                displayPages[i].month = "";
                displayPages[i].day = "";
                displayPages[i].time = "";

                lastGoodDisplayPages[i] = displayPages[i];
                isMarqueeBufferDirty = true;
            }
        }
    }
    xSemaphoreGive(xDisplayDataMutex);
}