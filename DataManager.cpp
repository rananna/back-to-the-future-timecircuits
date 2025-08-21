#include "DataManager.h"
#include "EventManager.h"
#include "DisplayManager.h" // For showTemporaryMessage
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

/**
 * @brief URL-encodes a string.
 * @param msg The string to encode.
 * @return The URL-encoded string.
 */
String urlEncode(const char* msg) {
    const char *hex = "0123456789abcdef";
    String encodedMsg = "";
    while (*msg!='\0'){
        if( ('a' <= *msg && *msg <= 'z')
                || ('A' <= *msg && *msg <= 'Z')
                || ('0' <= *msg && *msg <= '9') || *msg == '-' || *msg == '_' || *msg == '.') {
            encodedMsg += *msg;
        } else {
            // Encode non-alphanumeric characters as %XX
            encodedMsg += '%';
            encodedMsg += hex[*msg >> 4];
            encodedMsg += hex[*msg & 15];
        }
        msg++;
    }
    return encodedMsg;
}

/**
 * @brief Safely retrieves a nested value from a JSON object using a dot-and-bracket path.
 * @param root The root JsonVariant to search within.
 * @param path A string representing the path (e.g., "results[0].name").
 * @return The found JsonVariant, or a null JsonVariant if not found.
 */
JsonVariant getJsonVariant(JsonVariant root, const char* path) {
    char path_copy[128];
    // Make a mutable copy of the path because strtok_r modifies the string.
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    JsonVariant current = root;
    char* context = NULL;
    // Tokenize the path by dots and brackets.
    char* token = strtok_r(path_copy, ".[]", &context);
    while (token != NULL) {
        if (current.isNull()) return JsonVariant(); // Path is invalid
        if (current.is<JsonObject>()) {
            current = current[token];
        } else if (current.is<JsonArray>()) {
            current = current[atoi(token)];
        } else {
            return JsonVariant(); // Cannot traverse further
        }
        token = strtok_r(NULL, ".[]", &context);
    }
    return current;
}

/**
 * @brief Fetches weather data. This function handles geocoding and the actual weather API call. Runs in a dedicated FreeRTOS task.
 * @param params A pointer to a WeatherTaskParams struct containing the city name and whether to force geocoding.
 */
void fetchWeatherData(WeatherTaskParams* params) {
    std::string taskCityName = params->cityName;
    bool forceGeocode = params->forceGeocode;
    delete params; // Clean up the dynamically allocated memory for the parameters.

    if (taskCityName.empty()) {
        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
            currentWeatherData.dataValid = false;
            xSemaphoreGive(xDisplayDataMutex);
        }
        return;
    }

    bool needsGeocoding = forceGeocode;
    // Check if we need to geocode: either forced or the city name has changed.
    if (!forceGeocode) {
        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
            if (taskCityName != lastCityName) {
                needsGeocoding = true;
            }
            xSemaphoreGive(xDisplayDataMutex);
        }
    }
    
    // --- Geocoding Step ---
    if (needsGeocoding) {
        bool geocodeSuccess = false;
        for (int i = 0; i < 3; i++) { // Retry up to 3 times for network reliability.
            showTemporaryMessage("GEO", "", "SRCH", "", 1000);
            HTTPClient http;
            WiFiClientSecure client;
            client.setInsecure(); // Skip certificate validation to save memory.
            String geocodeUrl = "https://geocoding-api.open-meteo.com/v1/search?name=" + urlEncode(taskCityName.c_str());
            if (http.begin(client, geocodeUrl)) {
                int httpCode = http.GET();
                if (httpCode == HTTP_CODE_OK) {
                    DynamicJsonDocument doc(1024);
                    deserializeJson(doc, http.getStream());
                    JsonArray results = doc["results"];
                    if (!results.isNull() && results.size() > 0) {
                        // Safely update the shared settings structure.
                        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                            currentSettings.latitude = doc["results"][0]["latitude"];
                            currentSettings.longitude = doc["results"][0]["longitude"];
                            lastCityName = taskCityName; // Update the cache
                            xSemaphoreGive(xDisplayDataMutex);
                        }
                        geocodeSuccess = true;
                        http.end();
                        break; // Exit retry loop on success.
                    }
                }
                http.end();
            }
            delay(1000); // Wait 1 second before retrying.
        }

        if (!geocodeSuccess) {
            showTemporaryMessage("GEO", "", "FAIL", "", 2000);
            if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                currentWeatherData.dataValid = false;
                xSemaphoreGive(xDisplayDataMutex);
            }
            return; // Stop if geocoding fails.
        }
    }

    // --- Weather Fetch Step ---
    bool weatherSuccess = false;
    for (int i = 0; i < 3; i++) { // Retry up to 3 times.
        HTTPClient http;
        WiFiClientSecure client;
        client.setInsecure();
        String tempUnit = currentSettings.useMetricUnits ? "celsius" : "fahrenheit";
        String speedUnit = currentSettings.useMetricUnits ? "kmh" : "mph";
        // Construct the long API URL for Open-Meteo.
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

                // Use mutex to safely write to the shared currentWeatherData struct.
                if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                    if (error == DeserializationError::Ok && !doc.containsKey("error")) {
                        // Parse all the required data points from the JSON response.
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

                        // Parse the hourly forecast for the next 3 hours.
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
                    xSemaphoreGive(xDisplayDataMutex); // Release the mutex.
                }
                http.end();
                if (weatherSuccess) break; // Exit retry loop on success.
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

/**
 * @brief Task wrapper to fetch weather data.
 */
void fetchWeatherDataTask(void* p) {
    WeatherTaskParams* params = new WeatherTaskParams{currentSettings.cityName, false};
    fetchWeatherData(params);
    vTaskDelete(NULL); // End the task.
}

/**
 * @brief Task wrapper to force a weather data fetch and geocoding.
 */
void forceFetchWeatherDataTask(void* p) {
    WeatherTaskParams* params = (WeatherTaskParams*)p;
    fetchWeatherData(params);
    vTaskDelete(NULL); // End the task.
}

/**
 * @brief Task responsible for fetching and parsing data from a single API endpoint.
 * This runs as a separate, non-blocking FreeRTOS task.
 * @param p A void pointer to a FetchDataParams struct.
 */
void fetchApiDataTask(void* p) {
	// Safely cast and immediately delete the parameters struct to prevent memory leaks.
	FetchDataParams* params = (FetchDataParams*)p;
	int index = params->pointIndex;
	delete params;

	if (index < 0 || index >= currentSettings.numDataPoints) {
		vTaskDelete(NULL);
		// Invalid index, end the task.
		return;
	}

	DataPoint point = currentSettings.dataPoints[index];
	HTTPClient http;
	WiFiClientSecure client;
	client.setInsecure();
	// Skip certificate validation for memory efficiency.

	if (http.begin(client, point.url.c_str())) {
		if (!point.authHeaderKey.empty() && !point.authHeaderValue.empty()) {
			http.addHeader(point.authHeaderKey.c_str(), point.authHeaderValue.c_str());
		}

		int httpCode = http.GET();
		if (httpCode == HTTP_CODE_OK) {
			DynamicJsonDocument doc(4096);
			DeserializationError error = deserializeJson(doc, http.getStream());
			if (error == DeserializationError::Ok) {
				// Use the mutex to safely access the shared display data.
				if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
					JsonVariant monthVar = getJsonVariant(doc.as<JsonVariant>(), point.monthPath.c_str());
					JsonVariant dayVar = getJsonVariant(doc.as<JsonVariant>(), point.dayPath.c_str());
					JsonVariant yearVar = getJsonVariant(doc.as<JsonVariant>(), point.yearPath.c_str());
					JsonVariant timeVar = getJsonVariant(doc.as<JsonVariant>(), point.timePath.c_str());

					// Update display data only if the JSON path was valid.
					if (!monthVar.isNull()) displayPages[index].month = monthVar.as<String>().c_str();
					if (!dayVar.isNull()) displayPages[index].day = dayVar.as<String>().c_str();
					if (!yearVar.isNull()) displayPages[index].year = yearVar.as<String>().c_str();
					if (!timeVar.isNull()) displayPages[index].time = timeVar.as<String>().c_str();
					// On success, cache the good data and reset the failure counter.
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

	// If fetching fails multiple times, revert to the last known good data.
	if (dataPointFetchFailures[index] > MAX_FETCH_FAILURES) {
		 if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
			displayPages[index] = lastGoodDisplayPages[index];
			 xSemaphoreGive(xDisplayDataMutex);
		}
	}
	
	// Atomically increment the completed requests counter.
	__atomic_add_fetch(&requestsCompleted, 1, __ATOMIC_SEQ_CST);
	vTaskDelete(NULL); // End the task.
}

/**
 * @brief Manages the periodic fetching of data for the Data Link marquee.
 * This function is non-blocking and creates separate tasks for each API call.
 */
void fetchDataLink() {
	// Use a mutex to prevent race conditions with the isFetchingData flag.
	if (xSemaphoreTake(xDisplayDataMutex, 0) != pdTRUE) {
		return;
		// Could not get the lock, try again later.
	}

	// Don't fetch if the feature is disabled or if a fetch is already in progress.
	if (!currentSettings.dataLinkEnabled || isFetchingData) {
		xSemaphoreGive(xDisplayDataMutex);
		return;
	}

	unsigned long now = millis();
	// Check if it's time to refresh based on the user-configured interval.
	if (now - lastDataLinkFetch > (unsigned long)currentSettings.dataLinkRefreshInterval * 60000) {
		lastDataLinkFetch = now;
		isFetchingData = true;
		requestsCompleted = 0;
		int tasksCreated = 0;

		// Iterate through all configured data points.
		for (int i = 0; i < currentSettings.numDataPoints; i++) {
			// Only fetch data for API-based sources with a valid URL.
			if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_API && !currentSettings.dataPoints[i].url.empty()) {
				// Dynamically allocate memory for task parameters. This is crucial!
				FetchDataParams* params = new FetchDataParams{ i, 0 }; // Initialize with index
				// Create a new task to handle this specific API request.
				if (xTaskCreate(fetchApiDataTask, "fetchApiDataTask", 8192, params, 1, NULL) == pdPASS) {
					tasksCreated++;
				}
				else {
					// If task creation fails, delete the params to prevent a memory leak.
					delete params;
				}
			}
		}

		// If all tasks have completed, reset the fetching flag.
		if (requestsCompleted >= tasksCreated) {
			isFetchingData = false;
		}
	}
	xSemaphoreGive(xDisplayDataMutex);
}