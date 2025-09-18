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

// Forward declaration for the cleanup function
void cleanupWeatherConnection();

// Static TLS connection handle
static esp_tls_t *tls = NULL;

// Root CA certificate for api.open-meteo.com (Let's Encrypt R11)
static const char *open_meteo_com_root_ca = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIFBjCCAu6gAwIBAgIRAIp9PhPWLzDvI4a9KQdrNPgwDQYJKoZIhvcNAQELBQAw\n" \
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n" \
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMjQwMzEzMDAwMDAw\n" \
"WhcNMjcwMzEyMjM1OTU5WjAzMQswCQYDVQQGEwJVUzEWMBQGA1UEChMNTGV0J3Mg\n" \
"RW5jcnlwdDEMMAoGA1UEAxMDUjExMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIB\n" \
"CgKCAQEAuoe8XBsAOcvKCs3UZxD5ATylTqVhyybKUvsVAbe5KPUoHu0nsyQYOWcJ\n" \
"DAjs4DqwO3cOvfPlOVRBDE6uQdaZdN5R2+97/1i9qLcT9t4x1fJyyXJqC4N0lZxG\n" \
"AGQUmfOx2SLZzaiSqhwmej/+71gFewiVgdtxD4774zEJuwm+UE1fj5F2PVqdnoPy\n" \
"6cRms+EGZkNIGIBloDcYmpuEMpexsr3E+BUAnSeI++JjF5ZsmydnS8TbKF5pwnnw\n" \
"SVzgJFDhxLyhBax7QG0AtMJBP6dYuC/FXJuluwme8f7rsIU5/agK70XEeOtlKsLP\n" \
"Xzze41xNG/cLJyuqC0J3U095ah2H2QIDAQABo4H4MIH1MA4GA1UdDwEB/wQEAwIB\n" \
"hjAdBgNVHSUEFjAUBggrBgEFBQcDAgYIKwYBBQUHAwEwEgYDVR0TAQH/BAgwBgEB\n" \
"/wIBADAdBgNVHQ4EFgQUxc9GpOr0w8B6bJXELbBeki8m47kwHwYDVR0jBBgwFoAU\n" \
"ebRZ5nu25eQBc4AIiMgaWPbpm24wMgYIKwYBBQUHAQEEJjAkMCIGCCsGAQUFBzAC\n" \
"hhZodHRwOi8veDEuaS5sZW5jci5vcmcvMBMGA1UdIAQMMAowCAYGZ4EMAQIBMCcG\n" \
"A1UdHwQgMB4wHKAaoBiGFmh0dHA6Ly94MS5jLmxlbmNyLm9yZy8wDQYJKoZIhvcN\n" \
"AQELBQADggIBAE7iiV0KAxyQOND1H/lxXPjDj7I3iHpvsCUf7b632IYGjukJhM1y\n" \
"v4Hz/MrPU0jtvfZpQtSlET41yBOykh0FX+ou1Nj4ScOt9ZmWnO8m2OG0JAtIIE38\n" \
"01S0qcYhyOE2G/93ZCkXufBL713qzXnQv5C/viOykNpKqUgxdKlEC+Hi9i2DcaR1\n" \
"e9KUwQUZRhy5j/PEdEglKg3l9dtD4tuTm7kZtB8v32oOjzHTYw+7KdzdZiw/sBtn\n" \
"UfhBPORNuay4pJxmY/WrhSMdzFO2q3Gu3MUBcdo27goYKjL9CTF8j/Zz55yctUoV\n" \
"aneCWs/ajUX+HypkBTA+c8LGDLnWO2NKq0YD/pnARkAnYGPfUDoHR9gVSp/qRx+Z\n" \
"WghiDLZsMwhN1zjtSC0uBWiugF3vTNzYIEFfaPG7Ws3jDrAMMYebQ95JQ+HIBD/R\n" \
"PBuHRTBpqKlyDnkSHDHYPiNX3adPoPAcgdF3H2/W0rmoswMWgTlLn1Wu0mrks7/q\n" \
"pdWfS6PJ1jty80r2VKsM/Dj3YIDfbjXKdaFU5C+8bhfJGqU3taKauuz0wHVGT3eo\n" \
"6FlWkWYtbt4pgdamlwVeZEW+LM7qZEJEsMNPrfC03APKmZsJgpWCDWOKZvkZcvjV\n" \
"uYkQ4omYCTX5ohy+knMjdOmdH9c7SpqEWBDC86fiNex+O0XOMEZSa8DA\n" \
"-----END CERTIFICATE-----\n";

// This new function is responsible for fetching all weather data (current, daily, and hourly) in a single API call.
// It performs one atomic attempt. Retries are handled by the calling function.
static bool fetchWeatherDataFromApi() {
    // The previous check using esp_tls_get_bytes_avail() was not reliable.
    // A disconnected socket is more robustly detected by a failed write or read.
    // The esp_tls_conn_write() call below will fail if the connection has been
    // closed by the peer, triggering the cleanup and retry logic.

    if (!tls) {
        esp_tls_cfg_t cfg = {};
        cfg.cacert_buf = (const unsigned char *)open_meteo_com_root_ca;
        cfg.cacert_bytes = strlen(open_meteo_com_root_ca) + 1;
        cfg.timeout_ms = 5000; // 5 second timeout for TLS read/write operations

        tls = esp_tls_init();
        if (tls == NULL) {
            Log_printf(LOG_LEVEL_ERROR, "Failed to allocate TLS handle.");
            return false;
        }

        const char *hostname = "api.open-meteo.com";
        int ret = esp_tls_conn_new_sync(hostname, strlen(hostname), 443, &cfg, tls);
        if (ret < 0) {
            Log_printf(LOG_LEVEL_ERROR, "Failed to create TLS connection. esp_tls_conn_new_sync returned: %d", ret);
            int esp_tls_code, esp_tls_flags;
            esp_tls_error_handle_t esp_tls_error_handle;
            esp_tls_get_error_handle(tls, &esp_tls_error_handle);
            esp_err_t err = esp_tls_get_and_clear_last_error(esp_tls_error_handle, &esp_tls_code, &esp_tls_flags);
            if (err == ESP_OK) {
                Log_printf(LOG_LEVEL_ERROR, "Last ESP-TLS error: 0x%x, Last mbedTLS error: 0x%x", esp_tls_code, esp_tls_flags);
            }
            esp_tls_conn_destroy(tls);
            tls = NULL;
            return false;
        }
        Log_printf(LOG_LEVEL_DEBUG, "TLS connection established.");
    }

    String tempUnit = currentSettings.useMetricUnits ? "celsius" : "fahrenheit";
    String speedUnit = currentSettings.useMetricUnits ? "kmh" : "mph";
    char request[512];
    snprintf(request, sizeof(request),
             "GET /v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m"
             "&hourly=temperature_2m,weather_code"
             "&daily=temperature_2m_max,temperature_2m_min,weather_code,sunrise,sunset,precipitation_probability_max,wind_speed_10m_max"
             "&forecast_days=2&forecast_hours=12&temperature_unit=%s&wind_speed_unit=%s&timezone=auto&timeformat=unixtime"
             " HTTP/1.1\r\n"
             "Host: api.open-meteo.com\r\n"
             "Connection: keep-alive\r\n"
             "\r\n",
             currentSettings.latitude, currentSettings.longitude, tempUnit.c_str(), speedUnit.c_str());

    if (esp_tls_conn_write(tls, request, strlen(request)) < 0) {
        Log_printf(LOG_LEVEL_ERROR, "esp_tls_conn_write failed. Cleaning up connection.");
        int esp_tls_code, esp_tls_flags;
        esp_tls_error_handle_t esp_tls_error_handle;
        esp_tls_get_error_handle(tls, &esp_tls_error_handle);
        esp_err_t err = esp_tls_get_and_clear_last_error(esp_tls_error_handle, &esp_tls_code, &esp_tls_flags);
        if (err == ESP_OK) {
            Log_printf(LOG_LEVEL_ERROR, "Last ESP-TLS error: 0x%x, Last mbedTLS error: 0x%x", esp_tls_code, esp_tls_flags);
        }
        cleanupWeatherConnection();
        return false;
    }

    // Use a stack-allocated buffer for headers to avoid heap fragmentation.
    char header_buf[2048];
    int http_status = 0;
    size_t header_len = 0;
    unsigned long header_read_start_time = millis();
    const unsigned long HEADER_TIMEOUT_MS = 10000; // 10-second timeout

    char* body_start_ptr = NULL;
    bool operation_failed = false;

    while (millis() - header_read_start_time < HEADER_TIMEOUT_MS) {
        int ret = esp_tls_conn_read(tls,
                                    (unsigned char *)header_buf + header_len,
                                    sizeof(header_buf) - header_len - 1);

        if (ret < 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                Log_printf(LOG_LEVEL_ERROR, "esp_tls_conn_read failed: -0x%x", -ret);
                cleanupWeatherConnection();
                operation_failed = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
            continue; // Continue while loop for non-fatal SSL want_read/write
        }

        if (ret == 0) {
            // According to esp-tls docs, a return value of 0 means the connection was closed.
            Log_printf(LOG_LEVEL_WARN, "Connection closed by peer during header read.");
            cleanupWeatherConnection();
            operation_failed = true;
            break;
        }

        header_len += ret;
        header_buf[header_len] = '\0'; // Null-terminate the buffer

        // Check if we've found the end of the headers
        body_start_ptr = strstr(header_buf, "\r\n\r\n");
        if (body_start_ptr) {
            break; // Found headers, exit while loop
        }

        // Check if the buffer is full
        if (header_len >= sizeof(header_buf) - 1) {
            Log_printf(LOG_LEVEL_ERROR, "Headers too large, aborting.");
            cleanupWeatherConnection();
            operation_failed = true;
            break;
        }
    }

    if (operation_failed) {
        return false;
    }

    if (!body_start_ptr) {
        Log_printf(LOG_LEVEL_ERROR, "Timed out waiting for HTTP headers or headers incomplete.");
        cleanupWeatherConnection();
        return false;
    }

    // sscanf is fragile. A robust parsing of the status code is needed.
    const char* status_line_start = strstr(header_buf, "HTTP/");
    if (status_line_start) {
        const char* code_start = strchr(status_line_start, ' ');
        if (code_start) {
            http_status = strtol(code_start + 1, NULL, 10);
        } else {
            Log_printf(LOG_LEVEL_ERROR, "Malformed HTTP status line: no space found.");
            http_status = 0; // Indicate failure
        }
    } else {
        Log_printf(LOG_LEVEL_ERROR, "Could not find HTTP status line in response.");
        http_status = 0; // Indicate failure
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

        // Do not clean up the connection for HTTP errors; the connection itself is likely fine.
        // The calling function will handle the retry logic.
        return false;
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
    DeserializationError error = deserializeJson(doc, combined_stream, DeserializationOption::Filter(filter));

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
                    xSemaphoreGive(xDisplayDataMutex);
                    return false;
                }

                xSemaphoreGive(xDisplayDataMutex);
                return true; // Success
            }
            // This path should ideally not be reached with portMAX_DELAY, but as a safeguard:
            Log_printf(LOG_LEVEL_ERROR, "Could not obtain display data mutex, weather data processing aborted.");
            return false;
        } else {
            // This block handles cases where the API returns a valid JSON with an error message.
            const char* reason = doc["reason"].as<const char*>();
            Log_printf(LOG_LEVEL_WARN, "Unified Weather API returned an error: %s", reason);
            // Do not clean up the connection for API-level errors. The connection is healthy.
            if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                currentWeatherData.errorReason = reason;
                xSemaphoreGive(xDisplayDataMutex);
            }
            return false;
        }
    } else {
        // This block handles JSON parsing errors.
        Log_printf(LOG_LEVEL_WARN, "Failed to parse Unified Weather JSON. E: %s", error.c_str());
        // Do not clean up the connection for parsing errors. The data is bad, not the connection.
        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
            currentWeatherData.errorReason = "WEATHER PARSING FAILED";
            xSemaphoreGive(xDisplayDataMutex);
        }
        return false;
    }
}

void cleanupWeatherConnection() {
    if (tls) {
        esp_tls_conn_destroy(tls);
        tls = NULL;
        Log_printf(LOG_LEVEL_DEBUG, "TLS connection cleaned up.");
    }
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
                    reason_lower.find("out of range") != std::string::npos) {
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

void fetchApiDataTask(void* p) {
	FetchDataParams* params = (FetchDataParams*)p;
	int index = params->pointIndex;
	delete params;

    Log_printf(LOG_LEVEL_INFO, "Fetching API data for data point %d", index);

	if (index < 0 || index >= currentSettings.numDataPoints) {
		vTaskDelete(NULL);
		return;
	}

	DataPoint point = currentSettings.dataPoints[index];
	HTTPClient http;
	WiFiClientSecure client;
	client.setInsecure();

	if (http.begin(client, point.url.c_str())) {
		if (!point.authHeaderKey.empty() && !point.authHeaderValue.empty()) {
			http.addHeader(point.authHeaderKey.c_str(), point.authHeaderValue.c_str());
		}

		Log_printf(LOG_LEVEL_DEBUG, "API URL for data point %d: %s", index, point.url.c_str());
		int httpCode = http.GET();
		Log_printf(LOG_LEVEL_DEBUG, "API HTTP Code for data point %d: %d", index, httpCode);
		if (httpCode == HTTP_CODE_OK) {
			JsonDocument doc;
            if (http.getStreamPtr()) {
			    DeserializationError error = deserializeJson(doc, http.getStream());
			    if (error == DeserializationError::Ok) {
				    if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
					    Log_printf(LOG_LEVEL_DEBUG, "Successfully parsed API JSON for data point %d", index);
					    JsonVariant monthVar = getJsonVariant(doc.as<JsonVariant>(), point.monthPath.c_str());
					    JsonVariant dayVar = getJsonVariant(doc.as<JsonVariant>(), point.dayPath.c_str());
					    JsonVariant yearVar = getJsonVariant(doc.as<JsonVariant>(), point.yearPath.c_str());
					    JsonVariant timeVar = getJsonVariant(doc.as<JsonVariant>(), point.timePath.c_str());

					    if (!monthVar.isNull()) displayPages[index].month = monthVar.as<String>().c_str();
					    if (!dayVar.isNull()) displayPages[index].day = dayVar.as<String>().c_str();
					    if (!yearVar.isNull()) displayPages[index].year = yearVar.as<String>().c_str();
					    if (!timeVar.isNull()) displayPages[index].time = timeVar.as<String>().c_str();
					    lastGoodDisplayPages[index] = displayPages[index];
					    dataPointFetchFailures[index] = 0;
                        isMarqueeBufferDirty = true; // Mark the buffer as dirty
					    xSemaphoreGive(xDisplayDataMutex);
                    }
			    } else {
                    Log_printf(LOG_LEVEL_WARN, "Failed to parse API JSON for data point %d. Error: %s", index, error.c_str());
				    if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                        dataPointFetchFailures[index]++;
                        xSemaphoreGive(xDisplayDataMutex);
                    }
			    }
            }
		} else {
            Log_printf(LOG_LEVEL_WARN, "API request for data point %d failed with HTTP code %d", index, httpCode);
			if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                dataPointFetchFailures[index]++;
                xSemaphoreGive(xDisplayDataMutex);
            }
		}
		http.end();
	} else {
        Log_printf(LOG_LEVEL_ERROR, "Failed to begin HTTP client for data point %d", index);
		if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
            dataPointFetchFailures[index]++;
            xSemaphoreGive(xDisplayDataMutex);
        }
	}

	if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
        if (dataPointFetchFailures[index] > MAX_FETCH_FAILURES) {
            displayPages[index] = lastGoodDisplayPages[index];
        }
        xSemaphoreGive(xDisplayDataMutex);
    }
	
	__atomic_add_fetch(&requestsCompleted, 1, __ATOMIC_SEQ_CST);
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

	if (!currentSettings.dataLinkEnabled || isFetchingData) {
		xSemaphoreGive(xDisplayDataMutex);
		return;
	}

	unsigned long now = millis();
	if (now - lastDataLinkFetch > (unsigned long)currentSettings.dataLinkRefreshInterval * 60000) {
		lastDataLinkFetch = now;
		isFetchingData = true;
        Log_printf(LOG_LEVEL_INFO, "Starting data link fetch for %d data points.", currentSettings.numDataPoints);
		xSemaphoreGive(xDisplayDataMutex);
        
		requestsCompleted = 0;
        int tasksCreated = 0;

		for (int i = 0; i < currentSettings.numDataPoints; i++) {
			if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_API && !currentSettings.dataPoints[i].url.empty()) {
				FetchDataParams* params = new FetchDataParams{ i, 0 };
				if (xTaskCreatePinnedToCore(fetchApiDataTask, "fetchApiDataTask", 8192, params, 1, NULL, 0) == pdPASS) {
					tasksCreated++;
				}
				else {
                    Log_printf(LOG_LEVEL_ERROR, "Failed to create fetchApiDataTask for data point %d", i);
					delete params;
				}
			}
		}
        if (tasksCreated > 0) {
            Log_printf(LOG_LEVEL_DEBUG, "Created %d data fetch tasks. Starting status checker.", tasksCreated);
            xTaskCreatePinnedToCore(checkDataFetchStatusTask, "checkDataFetchStatusTask", 2048, (void*)tasksCreated, 1, NULL, 0);
        } else {
            Log_printf(LOG_LEVEL_INFO, "No API data points to fetch.");
            if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                isFetchingData = false;
                xSemaphoreGive(xDisplayDataMutex);
            }
        }
	} else {
		xSemaphoreGive(xDisplayDataMutex);
	}
}