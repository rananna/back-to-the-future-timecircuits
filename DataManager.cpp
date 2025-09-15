/**
 * @file DataManager.cpp
 * @brief Handles fetching and parsing data from external web APIs.
 * @details This module is responsible for all outbound network requests to services
 * like Open-Meteo (for weather), Financial Modeling Prep (for stocks), and custom
 * user-defined APIs for the Data Link feature. It uses FreeRTOS tasks to perform
 * these network operations asynchronously, preventing the main application loop
 * from blocking and ensuring the display remains responsive.
 */

#include "DebugLog.h"
#include "DataManager.h"
#include "EventManager.h"
#include "DisplayManager.h"
#include <WiFi.h>
#include <HTTPClient.h>
extern bool weatherDataUpdated;
#include <WiFiClientSecure.h>
#include <time.h>

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

/**
 * @brief A FreeRTOS task to fetch stock data for a single symbol.
 * @details This function runs on a separate task to prevent blocking the main loop.
 * It constructs the appropriate URL for the Financial Modeling Prep API, makes an
 * HTTPS GET request, parses the resulting JSON, formats the data, and updates the
 * global `stockData` array. It uses a mutex to ensure thread-safe access to the
 * shared data structure.
 * @param p A void pointer to a FetchDataParams struct, which contains the index
 * of the stock row to be updated. The task is responsible for deleting this struct.
 */
void fetchStockDataTask(void* p) {
    FetchDataParams* params = (FetchDataParams*)p;
    int rowIndex = params->pointIndex;
    delete params;

    Log_printf(LOG_LEVEL_INFO, "Fetching stock data for row %d", rowIndex);

    std::string symbol_str;
    if (rowIndex == 0) symbol_str = currentSettings.stockRow1_symbol;
    else if (rowIndex == 1) symbol_str = currentSettings.stockRow2_symbol;
    else symbol_str = currentSettings.stockRow3_symbol;

    if (symbol_str.empty() || currentSettings.financialModelingPrepApiKey.empty()) {
        vTaskDelete(NULL);
        return;
    }

    String symbol = String(symbol_str.c_str());
    String apiKey = currentSettings.financialModelingPrepApiKey.c_str();
    String url;

    if (symbol.startsWith("^")) {
        url = "https://financialmodelingprep.com/api/v3/quote-short/" + symbol + "?apikey=" + apiKey;
    } else {
        url = "https://financialmodelingprep.com/api/v3/quote/" + symbol + "?apikey=" + apiKey;
    }

    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    if (http.begin(client, url)) {
        int httpCode = http.GET();
        Log_printf(LOG_LEVEL_DEBUG, "Stock API URL: %s", url.c_str());
        Log_printf(LOG_LEVEL_DEBUG, "Stock API HTTP Code: %d", httpCode);
        if (httpCode == HTTP_CODE_OK) {
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, http.getStream());
            
            JsonObject quote;
            if (!doc.isNull() && doc.is<JsonArray>() && doc.size() > 0) {
                quote = doc[0];
            } else if (!doc.isNull() && doc.is<JsonObject>()) {
                quote = doc.as<JsonObject>();
            }

            if (quote) {
                if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                    stockData[rowIndex].symbol = quote["symbol"].as<String>().c_str();
                    
                    float price = quote["price"].as<float>();
                    char priceBuffer[6];
                    if (price >= 1000) {
                      dtostrf(price, 4, 0, priceBuffer);
                    } else if (price >= 100) {
                      dtostrf(price, 4, 1, priceBuffer);
                    } else {
                      dtostrf(price, 4, 2, priceBuffer);
                    }
                    stockData[rowIndex].price = priceBuffer;

                    if (quote.containsKey("changesPercentage")) {
                        float changeFloat = quote["changesPercentage"].as<float>();
                        char changeBuffer[10];
                        dtostrf(changeFloat, 1, 2, changeBuffer);
                        stockData[rowIndex].change_percent = changeBuffer;
                    } else {
                        stockData[rowIndex].change_percent = "----";
                    }

                    stockData[rowIndex].dataValid = true;
                    Log_printf(LOG_LEVEL_DEBUG, "Successfully parsed stock data for %s", symbol_str.c_str());
                    xSemaphoreGive(xDisplayDataMutex);
                }
            } else {
                Log_printf(LOG_LEVEL_WARN, "Could not parse stock JSON for %s", symbol_str.c_str());
                if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                    stockData[rowIndex].dataValid = false;
                    stockData[rowIndex].price = "NO";
                    stockData[rowIndex].change_percent = "DATA";
                    xSemaphoreGive(xDisplayDataMutex);
                }
            }
        } else {
            Log_printf(LOG_LEVEL_WARN, "Stock API request failed with HTTP code %d", httpCode);
        }
        http.end();
    } else {
        Log_printf(LOG_LEVEL_ERROR, "Failed to begin HTTP client for stock API.");
    }
    vTaskDelete(NULL);
}


// Forward declarations for our new separated fetch functions
static bool fetchCurrentAndDailyData();
static bool fetchHourlyData();

// This function will be responsible for fetching the current and daily weather data.
static bool fetchCurrentAndDailyData() {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    String tempUnit = currentSettings.useMetricUnits ? "celsius" : "fahrenheit";
    String speedUnit = currentSettings.useMetricUnits ? "kmh" : "mph";
    char weatherUrl[512];

    snprintf(weatherUrl, sizeof(weatherUrl),
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,weather_code,wind_speed_10m&daily=temperature_2m_max,temperature_2m_min,weather_code,sunrise,sunset,precipitation_probability_max,wind_speed_10m_max&forecast_days=2&temperature_unit=%s&wind_speed_unit=%s&timezone=auto",
             currentSettings.latitude, currentSettings.longitude, tempUnit.c_str(), speedUnit.c_str());

    if (http.begin(client, weatherUrl)) {
        Log_printf(LOG_LEVEL_DEBUG, "Current/Daily Weather URL: %s", weatherUrl);
        int httpCode = http.GET();
        Log_printf(LOG_LEVEL_DEBUG, "Current/Daily Weather API HTTP Code: %d", httpCode);

        if (httpCode == HTTP_CODE_OK) {

            String payload = http.getString();
            http.end(); // End connection immediately
            DynamicJsonDocument doc(8192);
            DeserializationError error = deserializeJson(doc, payload);


            if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                if (error == DeserializationError::Ok && !doc.containsKey("error")) {
                    Log_printf(LOG_LEVEL_DEBUG, "Successfully parsed Current/Daily weather JSON");

                    bool allDataPresent = true;
                    auto getJsonValue = [&](const JsonVariant& parent, const char* key) -> JsonVariant {
                        if (parent.isNull() || !parent.containsKey(key)) {
                            allDataPresent = false;
                            Log_printf(LOG_LEVEL_WARN, "Weather JSON missing key: %s", key);
                            return JsonVariant();
                        }
                        return parent[key];
                    };

                    auto getJsonValueFromArray = [&](const JsonVariant& parent, int index) -> JsonVariant {
                        if (parent.isNull() || parent.size() <= index) {
                            allDataPresent = false;
                            Log_printf(LOG_LEVEL_WARN, "Weather JSON array access out of bounds at index %d", index);
                            return JsonVariant();
                        }
                        return parent[index];
                    };

                    JsonObject current = doc["current"];
                    JsonObject daily = doc["daily"];

                    currentWeatherData.temperature = getJsonValue(current, "temperature_2m");
                    currentWeatherData.apparentTemperature = getJsonValue(current, "apparent_temperature");
                    currentWeatherData.windSpeed = getJsonValue(current, "wind_speed_10m");
                    currentWeatherData.humidity = getJsonValue(current, "relative_humidity_2m");
                    currentWeatherData.weatherCode = getJsonValue(current, "weather_code");
                    // is_day is not stored in currentWeatherData struct, but we requested it.
                    // This is fine, we can add it later if needed.

                    currentWeatherData.dailyHigh = getJsonValueFromArray(getJsonValue(daily, "temperature_2m_max"), 0);
                    currentWeatherData.dailyLow = getJsonValueFromArray(getJsonValue(daily, "temperature_2m_min"), 0);
                    currentWeatherData.sunrise = getJsonValueFromArray(getJsonValue(daily, "sunrise"), 0);
                    currentWeatherData.sunset = getJsonValueFromArray(getJsonValue(daily, "sunset"), 0);
                    currentWeatherData.precipitationProbability = getJsonValueFromArray(getJsonValue(daily, "precipitation_probability_max"), 0);
                    currentWeatherData.maxWindSpeed = getJsonValueFromArray(getJsonValue(daily, "wind_speed_10m_max"), 0);

                    currentWeatherData.tomorrowHigh = getJsonValueFromArray(getJsonValue(daily, "temperature_2m_max"), 1);
                    currentWeatherData.tomorrowLow = getJsonValueFromArray(getJsonValue(daily, "temperature_2m_min"), 1);
                    currentWeatherData.tomorrowWeatherCode = getJsonValueFromArray(getJsonValue(daily, "weather_code"), 1);

                    if (!allDataPresent) {
                        currentWeatherData.errorReason = "INCOMPLETE WEATHER DATA";
                        xSemaphoreGive(xDisplayDataMutex);
                        return false;
                    }

                    xSemaphoreGive(xDisplayDataMutex);
                    return true;

                } else {
                    Log_printf(LOG_LEVEL_WARN, "Failed to parse Current/Daily weather JSON. E: %s", error.c_str());
                    currentWeatherData.errorReason = "WEATHER API PARSING FAILED";
                    xSemaphoreGive(xDisplayDataMutex);
                }
            }
        } else {
            Log_printf(LOG_LEVEL_WARN, "Current/Daily weather API request failed with HTTP code %d", httpCode);
            http.end();
        }
    } else {
        Log_printf(LOG_LEVEL_ERROR, "Failed to begin HTTP client for Current/Daily weather API.");
    }

    return false;
}

// This function will be responsible for fetching the hourly forecast data.
static bool fetchHourlyData() {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    String tempUnit = currentSettings.useMetricUnits ? "celsius" : "fahrenheit";
    char weatherUrl[384];
    snprintf(weatherUrl, sizeof(weatherUrl),
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&hourly=temperature_2m,weather_code&forecast_days=2&timezone=auto&temperature_unit=%s",
             currentSettings.latitude, currentSettings.longitude, tempUnit.c_str());

    if (http.begin(client, weatherUrl)) {
        Log_printf(LOG_LEVEL_DEBUG, "Hourly Weather URL: %s", weatherUrl);
        int httpCode = http.GET();
        Log_printf(LOG_LEVEL_DEBUG, "Hourly Weather API HTTP Code: %d", httpCode);

        if (httpCode == HTTP_CODE_OK) {

            String payload = http.getString();
            http.end(); // End connection immediately
            DynamicJsonDocument doc(8192);
            DeserializationError error = deserializeJson(doc, payload);

            if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                if (error == DeserializationError::Ok && !doc.containsKey("error")) {
                    Log_printf(LOG_LEVEL_DEBUG, "Successfully parsed Hourly weather JSON");

                    JsonObject hourly = doc["hourly"];
                    if (hourly.isNull()) {
                        currentWeatherData.errorReason = "HOURLY DATA OBJECT MISSING";
                        xSemaphoreGive(xDisplayDataMutex);
                        return false;
                    }

                    JsonArray timeArray = hourly["time"];
                    JsonArray tempArray = hourly["temperature_2m"];
                    JsonArray codeArray = hourly["weather_code"];

                    if (timeArray.isNull() || tempArray.isNull() || codeArray.isNull()) {
                        currentWeatherData.errorReason = "INCOMPLETE HOURLY DATA";
                        xSemaphoreGive(xDisplayDataMutex);
                        return false;
                    }

                    // Get current time to find the starting index for the forecast
                    struct tm timeinfo;
                    if (!getLocalTime(&timeinfo, 5000)) { // 5-second timeout
                        Log_printf(LOG_LEVEL_ERROR, "Failed to get local time for hourly forecast.");
                        currentWeatherData.errorReason = "ERROR GETTING LOCAL TIME";
                        xSemaphoreGive(xDisplayDataMutex);
                        return false;
                    }


                    char currentTimeStr[17]; // "YYYY-MM-DDTHH:00" + null
                    strftime(currentTimeStr, sizeof(currentTimeStr), "%Y-%m-%dT%H:00", &timeinfo);


                    int startIndex = -1;
                    for (int i = 0; i < timeArray.size(); i++) {
                        if (strcmp(timeArray[i], currentTimeStr) == 0) {
                            startIndex = i;
                            break;
                        }
                    }

                    if (startIndex != -1) {
                        for (int j = 0; j < 3; j++) {
                            int forecastIndex = startIndex + j + 1;
                            if (forecastIndex < tempArray.size() && forecastIndex < codeArray.size()) {
                                currentWeatherData.hourlyTemp[j] = tempArray[forecastIndex];
                                currentWeatherData.hourlyCode[j] = codeArray[forecastIndex];
                            } else {
                                // Not enough forecast data, fill with a placeholder if needed

                                currentWeatherData.hourlyTemp[j] = -999;

                                currentWeatherData.hourlyCode[j] = -1;
                            }
                        }
                    } else {
                        Log_printf(LOG_LEVEL_WARN, "Could not find current hour in hourly forecast data.");
                        // We can still try to fill the data from the start of the array as a fallback
                        for (int j = 0; j < 3; j++) {
                             if (j < tempArray.size() && j < codeArray.size()) {
                                currentWeatherData.hourlyTemp[j] = tempArray[j];
                                currentWeatherData.hourlyCode[j] = codeArray[j];
                            }
                        }
                    }

                    xSemaphoreGive(xDisplayDataMutex);
                    return true;

                } else {
                    Log_printf(LOG_LEVEL_WARN, "Failed to parse Hourly weather JSON. E: %s", error.c_str());
                    currentWeatherData.errorReason = "HOURLY API PARSING FAILED";
                    xSemaphoreGive(xDisplayDataMutex);
                }
            }
        } else {
            Log_printf(LOG_LEVEL_WARN, "Hourly weather API request failed with HTTP code %d", httpCode);
            http.end();
        }
    } else {
        Log_printf(LOG_LEVEL_ERROR, "Failed to begin HTTP client for Hourly weather API.");
    }

    return false;
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
    std::string taskCityName = params->cityName;
    bool forceGeocode = params->forceGeocode;
    delete params;

    trim(taskCityName);

    Log_printf(LOG_LEVEL_INFO, "Fetching weather data for city: %s (force geocode: %s)", taskCityName.c_str(), forceGeocode ? "true" : "false");

    if (taskCityName.empty()) {
        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
            currentWeatherData.dataValid = false;
            currentWeatherData.errorReason = "CITY NAME IS EMPTY";
            xSemaphoreGive(xDisplayDataMutex);
        }
        return;
    }

    bool needsGeocoding = forceGeocode;
    if (!forceGeocode) {
        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
            if (taskCityName != lastCityName) {
                needsGeocoding = true;
            }
            xSemaphoreGive(xDisplayDataMutex);
        }
    }
    
    if (needsGeocoding) {
        Log_printf(LOG_LEVEL_INFO, "Geocoding required for city: %s", taskCityName.c_str());
        bool geocodeSuccess = false;
        for (int i = 0; i < 3; i++) {
            showTemporaryMessage("GEO", "", "SRCH", "", 1000);
            HTTPClient http;
            WiFiClientSecure client;
            client.setInsecure();
            String geocodeUrl = "https://geocoding-api.open-meteo.com/v1/search?name=" + urlEncode(taskCityName.c_str()) + "&count=1&language=en&format=json";
            if (http.begin(client, geocodeUrl)) {
                Log_printf(LOG_LEVEL_DEBUG, "Geocode URL: %s", geocodeUrl.c_str());
                int httpCode = http.GET();
                Log_printf(LOG_LEVEL_DEBUG, "Geocode HTTP Code: %d", httpCode);
                if (httpCode == HTTP_CODE_OK) {
                    DynamicJsonDocument doc(1024);
                    deserializeJson(doc, http.getStream());
                    JsonArray results = doc["results"];
                    if (!results.isNull() && results.size() > 0) {
                        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                            currentSettings.latitude = doc["results"][0]["latitude"];
                            currentSettings.longitude = doc["results"][0]["longitude"];
                            currentWeatherData.latitude = currentSettings.latitude;
                            currentWeatherData.longitude = currentSettings.longitude;
                            lastCityName = taskCityName;
                            xSemaphoreGive(xDisplayDataMutex);
                        }
                        geocodeSuccess = true;
                        Log_printf(LOG_LEVEL_INFO, "Geocode success for %s. Lat: %f, Lon: %f", taskCityName.c_str(), currentSettings.latitude, currentSettings.longitude);
                        http.end();
                        break;
                    }
                } else {
                    Log_printf(LOG_LEVEL_WARN, "Geocode request failed with HTTP code %d", httpCode);
                }
                http.end();
            } else {
                Log_printf(LOG_LEVEL_ERROR, "Failed to begin HTTP client for geocoding.");
            }
            delay(1000);
        }

        if (!geocodeSuccess) {
            Log_printf(LOG_LEVEL_ERROR, "Geocoding failed for %s after multiple retries.", taskCityName.c_str());
            if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                currentWeatherData.dataValid = false;
                currentWeatherData.errorReason = "GEOCODING FAILED - CHECK CITY NAME";
                xSemaphoreGive(xDisplayDataMutex);
            }
            return;
        }
    }

    // Now, call the two new functions to fetch the weather data, with retry logic.
    bool currentDailySuccess = false;
    bool hourlySuccess = false;
    int attempt = 0;
    const int maxAttempts = 4;

    while (attempt < maxAttempts && (!currentDailySuccess || !hourlySuccess)) {
        attempt++;
        Log_printf(LOG_LEVEL_INFO, "Weather fetch attempt %d of %d...", attempt, maxAttempts);

        // Only try to fetch data that hasn't been successfully fetched yet.
        if (!currentDailySuccess) {
            currentDailySuccess = fetchCurrentAndDailyData();
        }
        if (!hourlySuccess) {
            hourlySuccess = fetchHourlyData();
        }

        if (currentDailySuccess && hourlySuccess) {
            Log_printf(LOG_LEVEL_INFO, "Successfully fetched all weather data on attempt %d.", attempt);
            break;
        } else if (attempt < maxAttempts) {
            Log_printf(LOG_LEVEL_WARN, "Partial or full weather fetch failed on attempt %d. Retrying in 2 seconds...", attempt);
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }

    if (currentDailySuccess && hourlySuccess) {
        Log_printf(LOG_LEVEL_INFO, "Successfully fetched all weather data for %s.", taskCityName.c_str());
        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
            currentWeatherData.dataValid = true;
            weatherDataUpdated = true; // Signal to the display manager
            xSemaphoreGive(xDisplayDataMutex);
        }
    } else {
        Log_printf(LOG_LEVEL_ERROR, "Weather fetch failed for %s.", taskCityName.c_str());
        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
            currentWeatherData.dataValid = false;
            // Combine the reasons from the failed fetches if available
            if (!currentDailySuccess && !currentWeatherData.errorReason.empty()) {
                // Keep the reason from fetchCurrentAndDailyData
            } else if (!hourlySuccess && !currentWeatherData.errorReason.empty()) {
                // Keep the reason from fetchHourlyData
            } else {
                currentWeatherData.errorReason = "WEATHER API FAILED - CHECK WI-FI";
            }
            xSemaphoreGive(xDisplayDataMutex);
        }
    }
}

void fetchWeatherDataTask(void* p) {
    WeatherTaskParams* params = new WeatherTaskParams{currentSettings.cityName, false};
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
			DynamicJsonDocument doc(4096);
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
					xSemaphoreGive(xDisplayDataMutex);
                }
			} else {
                Log_printf(LOG_LEVEL_WARN, "Failed to parse API JSON for data point %d. Error: %s", index, error.c_str());
				if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                    dataPointFetchFailures[index]++;
                    xSemaphoreGive(xDisplayDataMutex);
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