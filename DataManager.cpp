/**
 * @file DataManager.cpp
 * @brief Handles fetching and parsing data from external web APIs.
 * @details This module is responsible for all outbound network requests to services
 * like Open-Meteo (for weather), Financial Modeling Prep (for stocks), and custom
 * user-defined APIs for the Data Link feature. It uses FreeRTOS tasks to perform
 * these network operations asynchronously, preventing the main application loop
 * from blocking and ensuring the display remains responsive.
 */

#include "DataManager.h"
#include "EventManager.h"
#include "DisplayManager.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

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
        Serial.println("ERROR: JSON path is too long!");
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
                Serial.println("DATA_LOG: Attempting to take display data mutex in fetchStockDataTask (quote).");
                if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                    Serial.println("DATA_LOG: Mutex taken in fetchStockDataTask (quote).");
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
                    xSemaphoreGive(xDisplayDataMutex);
                    Serial.println("DATA_LOG: Mutex released in fetchStockDataTask (quote).");
                } else {
                    Serial.println("DATA_LOG: FAILED to take display data mutex in fetchStockDataTask (quote).");
                }
            } else {
                Serial.println("DATA_LOG: Attempting to take display data mutex in fetchStockDataTask (no quote).");
                if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                    Serial.println("DATA_LOG: Mutex taken in fetchStockDataTask (no quote).");
                    stockData[rowIndex].dataValid = false;
                    stockData[rowIndex].price = "NO";
                    stockData[rowIndex].change_percent = "DATA";
                    xSemaphoreGive(xDisplayDataMutex);
                    Serial.println("DATA_LOG: Mutex released in fetchStockDataTask (no quote).");
                } else {
                    Serial.println("DATA_LOG: FAILED to take display data mutex in fetchStockDataTask (no quote).");
                }
            }
        }
        http.end();
    }
    vTaskDelete(NULL);
}


void fetchWeatherData(WeatherTaskParams* params) {
    std::string taskCityName = params->cityName;
    bool forceGeocode = params->forceGeocode;
    delete params;

    if (taskCityName.empty()) {
        Serial.println("DATA_LOG: Attempting to take display data mutex in fetchWeatherData (taskCityName empty).");
        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
            Serial.println("DATA_LOG: Mutex taken in fetchWeatherData (taskCityName empty).");
            currentWeatherData.dataValid = false;
            xSemaphoreGive(xDisplayDataMutex);
            Serial.println("DATA_LOG: Mutex released in fetchWeatherData (taskCityName empty).");
        } else {
            Serial.println("DATA_LOG: FAILED to take display data mutex in fetchWeatherData (taskCityName empty).");
        }
        return;
    }

    bool needsGeocoding = forceGeocode;
    if (!forceGeocode) {
        Serial.println("DATA_LOG: Attempting to take display data mutex in fetchWeatherData (forceGeocode).");
        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
            Serial.println("DATA_LOG: Mutex taken in fetchWeatherData (forceGeocode).");
            if (taskCityName != lastCityName) {
                needsGeocoding = true;
            }
            xSemaphoreGive(xDisplayDataMutex);
            Serial.println("DATA_LOG: Mutex released in fetchWeatherData (forceGeocode).");
        } else {
            Serial.println("DATA_LOG: FAILED to take display data mutex in fetchWeatherData (forceGeocode).");
        }
    }
    
    if (needsGeocoding) {
        bool geocodeSuccess = false;
        for (int i = 0; i < 3; i++) {
            showTemporaryMessage("GEO", "", "SRCH", "", 1000);
            HTTPClient http;
            WiFiClientSecure client;
            client.setInsecure();
            String geocodeUrl = "https://geocoding-api.open-meteo.com/v1/search?name=" + urlEncode(taskCityName.c_str());
            if (http.begin(client, geocodeUrl)) {
                int httpCode = http.GET();
                if (httpCode == HTTP_CODE_OK) {
                    DynamicJsonDocument doc(1024);
                    deserializeJson(doc, http.getStream());
                    JsonArray results = doc["results"];
                    if (!results.isNull() && results.size() > 0) {
                        Serial.println("DATA_LOG: Attempting to take display data mutex in fetchWeatherData (geocode success).");
                        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                            Serial.println("DATA_LOG: Mutex taken in fetchWeatherData (geocode success).");
                            currentSettings.latitude = doc["results"][0]["latitude"];
                            currentSettings.longitude = doc["results"][0]["longitude"];
                            lastCityName = taskCityName;
                            xSemaphoreGive(xDisplayDataMutex);
                            Serial.println("DATA_LOG: Mutex released in fetchWeatherData (geocode success).");
                        } else {
                            Serial.println("DATA_LOG: FAILED to take display data mutex in fetchWeatherData (geocode success).");
                        }
                        geocodeSuccess = true;
                        http.end();
                        break;
                    }
                }
                http.end();
            }
            delay(1000);
        }

        if (!geocodeSuccess) {
            showTemporaryMessage("GEO", "", "FAIL", "", 2000);
            Serial.println("DATA_LOG: Attempting to take display data mutex in fetchWeatherData (geocode fail).");
            if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                Serial.println("DATA_LOG: Mutex taken in fetchWeatherData (geocode fail).");
                currentWeatherData.dataValid = false;
                xSemaphoreGive(xDisplayDataMutex);
                Serial.println("DATA_LOG: Mutex released in fetchWeatherData (geocode fail).");
            } else {
                Serial.println("DATA_LOG: FAILED to take display data mutex in fetchWeatherData (geocode fail).");
            }
            return;
        }
    }

    bool weatherSuccess = false;
    for (int i = 0; i < 3; i++) {
        HTTPClient http;
        WiFiClientSecure client;
        client.setInsecure();
        String tempUnit = currentSettings.useMetricUnits ? "celsius" : "fahrenheit";
        String speedUnit = currentSettings.useMetricUnits ? "kmh" : "mph";
        String weatherUrl = "https://api.open-meteo.com/v1/forecast?latitude=" + String(currentSettings.latitude, 4) + 
                     "&longitude=" + String(currentSettings.longitude, 4) + 
                     "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m" +
                     "&daily=temperature_2m_max,temperature_2m_min,weather_code,sunrise,sunset,precipitation_probability_max,wind_speed_10m_max" + 
                     "&hourly=temperature_2m,weather_code" +
                     "&forecast_days=2" +
                     "&temperature_unit=" + tempUnit + "&wind_speed_unit=" + speedUnit;
        if (http.begin(client, weatherUrl)) {
            int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                DynamicJsonDocument doc(4096);
                DeserializationError error = deserializeJson(doc, payload);

                Serial.println("DATA_LOG: Attempting to take display data mutex in fetchWeatherData (weather data).");
                if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                    Serial.println("DATA_LOG: Mutex taken in fetchWeatherData (weather data).");
                    if (error == DeserializationError::Ok && !doc.containsKey("error")) {
                        currentWeatherData.temperature = doc["current"]["temperature_2m"];
                        currentWeatherData.apparentTemperature = doc["current"]["apparent_temperature"];
                        currentWeatherData.windSpeed = doc["current"]["wind_speed_10m"];
                        currentWeatherData.humidity = doc["current"]["relative_humidity_2m"];
                        currentWeatherData.weatherCode = doc["current"]["weather_code"];
                        currentWeatherData.dailyHigh = doc["daily"]["temperature_2m_max"][0];
                        currentWeatherData.dailyLow = doc["daily"]["temperature_2m_min"][0];
                        currentWeatherData.sunrise = doc["daily"]["sunrise"][0];
                        currentWeatherData.sunset = doc["daily"]["sunset"][0];
                        currentWeatherData.precipitationProbability = doc["daily"]["precipitation_probability_max"][0];
                        currentWeatherData.maxWindSpeed = doc["daily"]["wind_speed_10m_max"][0];
                        currentWeatherData.tomorrowHigh = doc["daily"]["temperature_2m_max"][1];
                        currentWeatherData.tomorrowLow = doc["daily"]["temperature_2m_min"][1];
                        currentWeatherData.tomorrowWeatherCode = doc["daily"]["weather_code"][1];

                        time_t now;
                        time(&now);
                        struct tm timeinfo;
                        localtime_r(&now, &timeinfo);
                        int currentHour = timeinfo.tm_hour;
                        JsonArray hourly_temp = doc["hourly"]["temperature_2m"];
                        JsonArray hourly_code = doc["hourly"]["weather_code"];
                        for (int j = 0; j < 3; j++) {
                            int forecastHour = currentHour + j + 1;
                            if (forecastHour < 24) {
                                currentWeatherData.hourlyTemp[j] = hourly_temp[forecastHour];
                                currentWeatherData.hourlyCode[j] = hourly_code[forecastHour];
                            }
                        }
                        
                        currentWeatherData.dataValid = true;
                        weatherSuccess = true;
                    } else {
                        currentWeatherData.dataValid = false;
                    }
                    xSemaphoreGive(xDisplayDataMutex);
                    Serial.println("DATA_LOG: Mutex released in fetchWeatherData (weather data).");
                } else {
                    Serial.println("DATA_LOG: FAILED to take display data mutex in fetchWeatherData (weather data).");
                }
                http.end();
                if (weatherSuccess) break;
            }
            http.end();
        }
        delay(1000);
    }
    
    if (!weatherSuccess) {
      Serial.println("DATA_LOG: Attempting to take display data mutex in fetchWeatherData (weather fail).");
      if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
        Serial.println("DATA_LOG: Mutex taken in fetchWeatherData (weather fail).");
        currentWeatherData.dataValid = false;
        xSemaphoreGive(xDisplayDataMutex);
        Serial.println("DATA_LOG: Mutex released in fetchWeatherData (weather fail).");
      } else {
        Serial.println("DATA_LOG: FAILED to take display data mutex in fetchWeatherData (weather fail).");
      }
      showTemporaryMessage("API", "", "FAIL", "", 2000);
    }
}

void fetchWeatherDataTask(void* p) {
    WeatherTaskParams* params = new WeatherTaskParams{currentSettings.cityName, false};
    fetchWeatherData(params);
    vTaskDelete(NULL);
}

void forceFetchWeatherDataTask(void* p) {
    WeatherTaskParams* params = (WeatherTaskParams*)p;
    fetchWeatherData(params);
    vTaskDelete(NULL);
}

void fetchApiDataTask(void* p) {
	FetchDataParams* params = (FetchDataParams*)p;
	int index = params->pointIndex;
	delete params;

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

		int httpCode = http.GET();
		if (httpCode == HTTP_CODE_OK) {
			DynamicJsonDocument doc(4096);
			DeserializationError error = deserializeJson(doc, http.getStream());
			if (error == DeserializationError::Ok) {
                Serial.println("DATA_LOG: Attempting to take display data mutex in fetchApiDataTask (success).");
				if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                    Serial.println("DATA_LOG: Mutex taken in fetchApiDataTask (success).");
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
                    Serial.println("DATA_LOG: Mutex released in fetchApiDataTask (success).");
				} else {
                    Serial.println("DATA_LOG: FAILED to take display data mutex in fetchApiDataTask (success).");
                }
			} else {
                Serial.println("DATA_LOG: Attempting to take display data mutex in fetchApiDataTask (deserialization error).");
				if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                    Serial.println("DATA_LOG: Mutex taken in fetchApiDataTask (deserialization error).");
                    dataPointFetchFailures[index]++;
                    xSemaphoreGive(xDisplayDataMutex);
                    Serial.println("DATA_LOG: Mutex released in fetchApiDataTask (deserialization error).");
                } else {
                    Serial.println("DATA_LOG: FAILED to take display data mutex in fetchApiDataTask (deserialization error).");
                }
			}
		} else {
            Serial.println("DATA_LOG: Attempting to take display data mutex in fetchApiDataTask (http error).");
			if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                Serial.println("DATA_LOG: Mutex taken in fetchApiDataTask (http error).");
                dataPointFetchFailures[index]++;
                xSemaphoreGive(xDisplayDataMutex);
                Serial.println("DATA_LOG: Mutex released in fetchApiDataTask (http error).");
            } else {
                Serial.println("DATA_LOG: FAILED to take display data mutex in fetchApiDataTask (http error).");
            }
		}
		http.end();
	} else {
        Serial.println("DATA_LOG: Attempting to take display data mutex in fetchApiDataTask (http begin fail).");
		if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
            Serial.println("DATA_LOG: Mutex taken in fetchApiDataTask (http begin fail).");
            dataPointFetchFailures[index]++;
            xSemaphoreGive(xDisplayDataMutex);
            Serial.println("DATA_LOG: Mutex released in fetchApiDataTask (http begin fail).");
        } else {
            Serial.println("DATA_LOG: FAILED to take display data mutex in fetchApiDataTask (http begin fail).");
        }
	}

    Serial.println("DATA_LOG: Attempting to take display data mutex in fetchApiDataTask (final).");
	if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
        Serial.println("DATA_LOG: Mutex taken in fetchApiDataTask (final).");
        if (dataPointFetchFailures[index] > MAX_FETCH_FAILURES) {
            displayPages[index] = lastGoodDisplayPages[index];
        }
        xSemaphoreGive(xDisplayDataMutex);
        Serial.println("DATA_LOG: Mutex released in fetchApiDataTask (final).");
    } else {
        Serial.println("DATA_LOG: FAILED to take display data mutex in fetchApiDataTask (final).");
    }
	
	__atomic_add_fetch(&requestsCompleted, 1, __ATOMIC_SEQ_CST);
	vTaskDelete(NULL);
}

void checkDataFetchStatusTask(void* p) {
    int tasksCreated = (int)p;
    while(true) {
        if (__atomic_load_n(&requestsCompleted, __ATOMIC_SEQ_CST) >= tasksCreated) {
            Serial.println("DATA_LOG: Attempting to take display data mutex in checkDataFetchStatusTask.");
            if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                Serial.println("DATA_LOG: Mutex taken in checkDataFetchStatusTask.");
                isFetchingData = false;
                xSemaphoreGive(xDisplayDataMutex);
                Serial.println("DATA_LOG: Mutex released in checkDataFetchStatusTask.");
            } else {
                Serial.println("DATA_LOG: FAILED to take display data mutex in checkDataFetchStatusTask.");
            }
            vTaskDelete(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void fetchDataLink() {
    Serial.println("DATA_LOG: Attempting to take display data mutex in fetchDataLink.");
	if (xSemaphoreTake(xDisplayDataMutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        Serial.println("DATA_LOG: FAILED to take display data mutex in fetchDataLink.");
		return;
	}
    Serial.println("DATA_LOG: Mutex taken in fetchDataLink.");

	if (!currentSettings.dataLinkEnabled || isFetchingData) {
		xSemaphoreGive(xDisplayDataMutex);
        Serial.println("DATA_LOG: Mutex released in fetchDataLink (dataLink disabled or already fetching).");
		return;
	}

	unsigned long now = millis();
	if (now - lastDataLinkFetch > (unsigned long)currentSettings.dataLinkRefreshInterval * 60000) {
		lastDataLinkFetch = now;
		isFetchingData = true;
		xSemaphoreGive(xDisplayDataMutex);
        Serial.println("DATA_LOG: Mutex released in fetchDataLink (starting fetch).");
        
		requestsCompleted = 0;
        int tasksCreated = 0;

		for (int i = 0; i < currentSettings.numDataPoints; i++) {
			if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_API && !currentSettings.dataPoints[i].url.empty()) {
				FetchDataParams* params = new FetchDataParams{ i, 0 };
				if (xTaskCreatePinnedToCore(fetchApiDataTask, "fetchApiDataTask", 8192, params, 1, NULL, 0) == pdPASS) {
					tasksCreated++;
				}
				else {
					delete params;
				}
			}
		}
        if (tasksCreated > 0) {
            xTaskCreatePinnedToCore(checkDataFetchStatusTask, "checkDataFetchStatusTask", 2048, (void*)tasksCreated, 1, NULL, 0);
        } else {
            Serial.println("DATA_LOG: Attempting to take display data mutex in fetchDataLink (no tasks created).");
            if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                Serial.println("DATA_LOG: Mutex taken in fetchDataLink (no tasks created).");
                isFetchingData = false;
                xSemaphoreGive(xDisplayDataMutex);
                Serial.println("DATA_LOG: Mutex released in fetchDataLink (no tasks created).");
            } else {
                Serial.println("DATA_LOG: FAILED to take display data mutex in fetchDataLink (no tasks created).");
            }
        }
	} else {
		xSemaphoreGive(xDisplayDataMutex);
        Serial.println("DATA_LOG: Mutex released in fetchDataLink (not time to fetch).");
	}
}