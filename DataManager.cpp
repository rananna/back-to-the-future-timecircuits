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

JsonVariant getJsonVariant(JsonVariant root, const char* path) {
    // --- START: MODIFICATION ---
    // Use dynamic allocation to prevent a stack buffer overflow from long JSON paths.
    size_t path_len = strlen(path) + 1;
    char* path_copy = new char[path_len];
    if (!path_copy) {
        return JsonVariant(); // Allocation failed
    }
    strncpy(path_copy, path, path_len);
    // --- END: MODIFICATION ---

    JsonVariant current = root;
    char* context = NULL;
    char* token = strtok_r(path_copy, ".[]", &context);
    while (token != NULL) {
        if (current.isNull()) {
            delete[] path_copy; // Clean up memory
            return JsonVariant();
        }
        if (current.is<JsonObject>()) {
            current = current[token];
        } else if (current.is<JsonArray>()) {
            current = current[atoi(token)];
        } else {
            delete[] path_copy; // Clean up memory
            return JsonVariant();
        }
        token = strtok_r(NULL, ".[]", &context);
    }

    delete[] path_copy; // Clean up memory before returning
    return current;
}

void fetchStockDataTask(void* p) {
    FetchDataParams* params = (FetchDataParams*)p;
    int rowIndex = params->pointIndex;
    delete params;

    std::string symbol_str;
    if (rowIndex == 0) symbol_str = currentSettings.stockRow1_symbol;
    else if (rowIndex == 1) symbol_str = currentSettings.stockRow2_symbol;
    else symbol_str = currentSettings.stockRow3_symbol;

    if (symbol_str.empty() || currentSettings.alphaVantageApiKey.empty()) {
        vTaskDelete(NULL);
        return;
    }

    String symbol = String(symbol_str.c_str());
    String apiKey = currentSettings.alphaVantageApiKey.c_str();
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
                    xSemaphoreGive(xDisplayDataMutex);
                }
            } else {
                if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                    stockData[rowIndex].dataValid = false;
                    stockData[rowIndex].price = "NO";
                    stockData[rowIndex].change_percent = "DATA";
                    xSemaphoreGive(xDisplayDataMutex);
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
        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
            currentWeatherData.dataValid = false;
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
                        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                            currentSettings.latitude = doc["results"][0]["latitude"];
                            currentSettings.longitude = doc["results"][0]["longitude"];
                            lastCityName = taskCityName;
                            xSemaphoreGive(xDisplayDataMutex);
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
            if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                currentWeatherData.dataValid = false;
                xSemaphoreGive(xDisplayDataMutex);
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

                if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
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
                }
                http.end();
                if (weatherSuccess) break;
            }
            http.end();
        }
        delay(1000);
    }
    
    if (!weatherSuccess) {
      if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
        currentWeatherData.dataValid = false;
        xSemaphoreGive(xDisplayDataMutex);
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
				if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
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
				 dataPointFetchFailures[index]++;
			}
		} else {
			dataPointFetchFailures[index]++;
		}
		http.end();
	} else {
		dataPointFetchFailures[index]++;
	}

	if (dataPointFetchFailures[index] > MAX_FETCH_FAILURES) {
		 if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
			displayPages[index] = lastGoodDisplayPages[index];
			 xSemaphoreGive(xDisplayDataMutex);
		}
	}
	
	__atomic_add_fetch(&requestsCompleted, 1, __ATOMIC_SEQ_CST);
	vTaskDelete(NULL);
}

void fetchDataLink() {
	if (xSemaphoreTake(xDisplayDataMutex, 0) != pdTRUE) {
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
		requestsCompleted = 0;
		int tasksCreated = 0;

		for (int i = 0; i < currentSettings.numDataPoints; i++) {
			if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_API && !currentSettings.dataPoints[i].url.empty()) {
				FetchDataParams* params = new FetchDataParams{ i, 0 };
				if (xTaskCreate(fetchApiDataTask, "fetchApiDataTask", 8192, params, 1, NULL) == pdPASS) {
					tasksCreated++;
				}
				else {
					delete params;
				}
			}
		}

		if (requestsCompleted >= tasksCreated) {
			isFetchingData = false;
		}
	}
	xSemaphoreGive(xDisplayDataMutex);
}