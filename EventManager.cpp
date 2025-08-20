/**
 * @file EventManager.cpp
 * @brief Implements the core logic for state management, animations, and data fetching.
 */

#include "EventManager.h"
#include "web_server.h" // For access to extern variables and tasks
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// --- EXTERN DECLARATIONS ---
// These variables are defined in the main .ino file and are used here.
extern ClockSettings currentSettings;
extern MarqueeData displayPages[5];
extern MarqueeData lastGoodDisplayPages[5];
extern WeatherData currentWeatherData;
extern std::string lastCityName;
extern bool isAnimating;
extern unsigned long animationStartTime;
extern unsigned long lastAnimationFrameTime;
extern AnimationPhase currentPhase;
extern bool isDisplayAsleep;
extern BootSequenceState bootState;
extern unsigned long bootStateStartTime;
extern unsigned long lastGlitchTime;
extern bool isGlitching;
extern unsigned long glitchStartTime;
extern unsigned long lastPresetCycleTime;
extern bool isEchoEffectActive;
extern unsigned long echoEffectStartTime;
extern unsigned long lastEchoCheckTime;
extern bool isFlickeringNow;
extern unsigned long flickerStartTime;
extern int flickerDisplayIndex;
extern MarqueeState marqueeState;
extern unsigned long lastDataLinkFetch;
extern unsigned long lastMarqueeStateChange;
extern int marqueeScrollPosition;
extern int marqueeScrollPositionYear;
extern volatile bool isFetchingData;
extern int dataPointFetchFailures[5];
extern bool isMalfunctioning;
extern unsigned long malfunctionStartTime;
extern MalfunctionPhase currentMalfunctionPhase;
extern volatile int requestsCompleted;
extern SemaphoreHandle_t xDisplayDataMutex;
extern PubSubClient mqttClient;
extern bool timeSynchronized;

// Structs to hold time info specifically for the hardcoded time travel animation sequence.
struct tm realDepartureTimeInfo; // The actual time the animation starts
struct tm animDestTimeInfo;      // Hardcoded movie destination time (Nov 05, 1955)
struct tm animPresTimeInfo;      // Hardcoded movie present time (Oct 26, 1985)
struct tm animLastTimeInfo;      // Hardcoded movie last departed time (Oct 26, 1985)


// --- FUNCTION IMPLEMENTATIONS ---

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
 * @brief Displays a temporary message on the bottom display row.
 * @param month Text for the MONTH display (3 chars).
 * @param day Text for the DAY display (2 chars).
 * @param year Text for the YEAR display (4 chars).
 * @param time Text for the TIME display (4 chars).
 * @param duration The duration in milliseconds to show the message.
 */
void showTemporaryMessage(const char* month, const char* day, const char* year, const char* time, int duration) {
    #if ENABLE_HARDWARE
    printToDisplay(lastRow.month, month, 1);
    printToDisplay(lastRow.day, day, 2);
    printToDisplay(lastRow.year, year);
    printToDisplay(lastRow.time, time);
    lastRow.month.writeDisplay();
    lastRow.day.writeDisplay();
    lastRow.year.writeDisplay();
    lastRow.time.writeDisplay();
    delay(duration); // Note: This is a blocking delay. Use sparingly.
    #endif
}

/**
 * @brief Maps a WMO weather code to a 2-character display icon.
 * @param code The integer weather code from the Open-Meteo API.
 * @return A 2-character string representing the weather icon.
 */
const char* getIconForWeatherCode(int code) {
    switch (code) {
        case 0: case 1: return "SU"; // Clear, Mainly clear
        case 2: return "CL"; // Partly cloudy
        case 3: return "CL"; // Overcast
        case 45: case 48: return "CL"; // Fog
        case 51: case 53: case 55: return "RN"; // Drizzle
        case 61: case 63: case 65: return "RN"; // Rain
        case 66: case 67: return "RN"; // Freezing Rain
        case 71: case 73: case 75: return "SN"; // Snow
        case 77: return "SN"; // Snow grains
        case 80: case 81: case 82: return "RN"; // Rain showers
        case 85: case 86: return "SN"; // Snow showers
        case 95: case 96: case 99: return "ST"; // Thunderstorm
        default: return "--";
    }
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
        ESP_LOGE("Weather", "City name is empty, cannot fetch weather.");
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
            ESP_LOGI("Weather", "Geocoding attempt %d for %s", i + 1, taskCityName.c_str());
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
                        ESP_LOGI("Weather", "Geocoded %s to Lat: %f, Lon: %f", taskCityName.c_str(), currentSettings.latitude, currentSettings.longitude);
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
            ESP_LOGE("Weather", "Geocoding failed for city: %s after all retries.", taskCityName.c_str());
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
        ESP_LOGI("Weather", "Weather fetch attempt %d", i + 1);
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
                        ESP_LOGI("Weather", "Successfully fetched weather data.");
                    } else {
                        currentWeatherData.dataValid = false;
                        ESP_LOGE("Weather", "Weather JSON parsing failed: %s", error.c_str());
                    }
                    xSemaphoreGive(xDisplayDataMutex); // Release the mutex.
                }
                http.end();
                if (weatherSuccess) break; // Exit retry loop on success.
            } else {
                ESP_LOGE("Weather", "Weather HTTP request failed on attempt %d, error: %s", i + 1, http.errorToString(httpCode).c_str());
            }
            http.end();
        } else {
            ESP_LOGE("Weather", "Unable to connect to weather API on attempt %d.", i + 1);
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
 * @brief Configures the MQTT client with broker details from settings.
 */
void setupMqtt() {
  if (currentSettings.mqttBroker.empty()) {
    ESP_LOGI("MQTT", "Broker not configured. Skipping MQTT setup.");
    return;
  }
  mqttClient.setServer(currentSettings.mqttBroker.c_str(), currentSettings.mqttPort);
  mqttClient.setCallback(mqttCallback);
  ESP_LOGI("MQTT", "Client configured for broker %s:%d", currentSettings.mqttBroker.c_str(), currentSettings.mqttPort);
}

/**
 * @brief Attempts to reconnect to the MQTT broker if the connection is lost.
 */
void reconnectMqtt() {
  if (currentSettings.mqttBroker.empty()) return;
  if (!mqttClient.connected()) {
    ESP_LOGI("MQTT", "Attempting MQTT connection...");
    // Create a unique client ID to avoid conflicts.
    String clientId = "BTTF-Clock-";
    clientId += String(random(0xffff), HEX);
    bool connected = false;
    // Connect with or without authentication based on settings.
    // Use Last Will and Testament (LWT) to publish "offline" message on disconnect.
    if (!currentSettings.mqttUser.empty()) {
        connected = mqttClient.connect(clientId.c_str(), currentSettings.mqttUser.c_str(), currentSettings.mqttPassword.c_str(), "bttf-clock/status", 1, true, "offline");
    } else {
        connected = mqttClient.connect(clientId.c_str(), "bttf-clock/status", 1, true, "offline");
    }

    if (connected) {
      ESP_LOGI("MQTT", "Connected to broker!");
      // Publish "online" status upon successful connection.
      mqttClient.publish("bttf-clock/status", "online", true);
      // Subscribe to all topics configured in the data points.
      for (int i = 0; i < currentSettings.numDataPoints; i++) {
        if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_MQTT && !currentSettings.dataPoints[i].mqttTopic.empty()) {
          mqttClient.subscribe(currentSettings.dataPoints[i].mqttTopic.c_str());
          ESP_LOGI("MQTT", "Subscribed to topic: %s", currentSettings.dataPoints[i].mqttTopic.c_str());
        }
      }
    } else {
      ESP_LOGE("MQTT", "Failed to connect, rc=%d. Will try again in 5 seconds.", mqttClient.state());
    }
  }
}

/**
 * @brief Callback function that is executed when an MQTT message is received.
 * @param topic The topic of the received message.
 * @param payload The message payload.
 * @param length The length of the payload.
 */
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  ESP_LOGI("MQTT", "Message arrived [%s] %s", topic, message.c_str());

  // Find which data point this message belongs to.
  for (int i = 0; i < currentSettings.numDataPoints; i++) {
    DataPoint point = currentSettings.dataPoints[i];
    if (point.dataSourceType == DATA_SOURCE_MQTT && point.mqttTopic == topic) {
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, message);
        bool success = false;
        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
            if (error == DeserializationError::Ok) {
                // If the payload is JSON, parse it using the defined paths.
                auto fetch = [&](const char* path) {
                    return getJsonVariant(doc.as<JsonVariant>(), path).as<String>();
                };
                if(!point.monthPath.empty()) displayPages[i].month = fetch(point.monthPath.c_str()).c_str();
                if(!point.dayPath.empty()) displayPages[i].day = fetch(point.dayPath.c_str()).c_str();
                if(!point.yearPath.empty()) displayPages[i].year = fetch(point.yearPath.c_str()).c_str();
                if(!point.timePath.empty()) displayPages[i].time = fetch(point.timePath.c_str()).c_str();
                success = true;
            } else {
                // If the payload is not JSON (plain text), display it in the 'time' field.
                displayPages[i].month = "";
                displayPages[i].day = "";
                displayPages[i].year = "";
                displayPages[i].time = message.c_str();
                success = true;
            }

            if (success) {
                // On success, update the 'last good' cache and reset the failure counter.
                memcpy(&lastGoodDisplayPages[i], &displayPages[i], sizeof(MarqueeData));
                dataPointFetchFailures[i] = 0;
            } else {
                // On failure, increment the counter.
                dataPointFetchFailures[i]++;
                // After 3 failures, show an error message. Otherwise, show the last good data.
                if (dataPointFetchFailures[i] >= 3) {
                    displayPages[i].time = "MQTT FAIL";
                } else {
                    memcpy(&displayPages[i], &lastGoodDisplayPages[i], sizeof(MarqueeData));
                }
            }
            xSemaphoreGive(xDisplayDataMutex);
        }
        break; // Stop searching once the matching data point is found.
    }
  }
}

/**
 * @brief Fetches data for a single API data point. Runs in a dedicated FreeRTOS task.
 * @param p A void pointer to a FetchDataParams struct.
 */
void fetchDataTask(void* p) {
    struct FetchDataParams* params = (struct FetchDataParams*)p;
    int pointIndex = params->pointIndex;
    int totalRequests = params->totalRequests;
    delete params; // Clean up memory.

    DataPoint point = currentSettings.dataPoints[pointIndex];
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure(); // Bypassing certificate validation to save RAM and improve reliability.

    if (http.begin(client, point.url.c_str())) {
        // Add authentication header if provided.
        if (!point.authHeaderKey.empty() && !point.authHeaderValue.empty()) {
            http.addHeader(point.authHeaderKey.c_str(), point.authHeaderValue.c_str());
        }
        
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            DynamicJsonDocument doc(2048);
            DeserializationError error = deserializeJson(doc, payload);
            if (error == DeserializationError::Ok) {
                // Safely parse and update the shared display data.
                if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                    auto fetch = [&](const char* path) {
                        return getJsonVariant(doc.as<JsonVariant>(), path).as<String>();
                    };
                    if(!point.monthPath.empty()) displayPages[pointIndex].month = fetch(point.monthPath.c_str()).c_str();
                    if(!point.dayPath.empty()) displayPages[pointIndex].day = fetch(point.dayPath.c_str()).c_str();
                    if(!point.yearPath.empty()) displayPages[pointIndex].year = fetch(point.yearPath.c_str()).c_str();
                    if(!point.timePath.empty()) displayPages[pointIndex].time = fetch(point.timePath.c_str()).c_str();
                    
                    // Update cache and reset failure counter.
                    memcpy(&lastGoodDisplayPages[pointIndex], &displayPages[pointIndex], sizeof(MarqueeData));
                    dataPointFetchFailures[pointIndex] = 0;
                    xSemaphoreGive(xDisplayDataMutex);
                }
            } else {
                if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                    dataPointFetchFailures[pointIndex]++;
                    xSemaphoreGive(xDisplayDataMutex);
                }
            }
        } else {
            if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                dataPointFetchFailures[pointIndex]++;
                xSemaphoreGive(xDisplayDataMutex);
            }
        }
        http.end();
    } else {
        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
            dataPointFetchFailures[pointIndex]++;
            xSemaphoreGive(xDisplayDataMutex);
        }
    }
    
    // After 3 failures, display an error message.
    if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
        if (dataPointFetchFailures[pointIndex] >= 3) {
            displayPages[pointIndex].time = "API FAIL";
        }
        xSemaphoreGive(xDisplayDataMutex);
    }

    // Atomically increment the counter for completed requests.
    requestsCompleted++;
    // Once all requests are done, reset the global fetching flag.
    if (requestsCompleted >= totalRequests) {
        isFetchingData = false;
        ESP_LOGI("DataLink", "All API requests finished.");
    }

    vTaskDelete(NULL); // End the task.
}


/**
 * @brief Periodically triggers the fetching of all configured API data points.
 */
void fetchDataLink() {
    // Conditions to skip fetching.
    if (!timeSynchronized || !currentSettings.dataLinkEnabled || currentSettings.numDataPoints == 0 || isFetchingData) {
        return;
    }
    // Check if the refresh interval has passed.
    if (millis() - lastDataLinkFetch < (unsigned long)currentSettings.dataLinkRefreshInterval * 60 * 1000) {
        return;
    }
    
    lastDataLinkFetch = millis();
    isFetchingData = true;
    requestsCompleted = 0;
    
    // Count how many API requests we need to make (excluding MQTT points).
    int apiRequestsToMake = 0;
    for (int i = 0; i < currentSettings.numDataPoints; i++) {
        if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_API) {
            apiRequestsToMake++;
        }
    }

    if (apiRequestsToMake == 0) {
        isFetchingData = false;
        return;
    }

    ESP_LOGI("DataLink", "Starting parallel fetch for %d API data points.", apiRequestsToMake);
    // Create a separate, short-lived task for each API request to run them in parallel.
    for (int i = 0; i < currentSettings.numDataPoints; i++) {
        if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_API) {
            FetchDataParams* params = new FetchDataParams{i, apiRequestsToMake};
            xTaskCreate(fetchDataTask, "fetchDataTask", 8192, params, 1, NULL);
        }
    }
}

/**
 * @brief Kicks off the main time travel visual and audio sequence.
 */
void startTimeTravelAnimation() {
    ESP_LOGI("ANIMATION", "startTimeTravelAnimation function called.");
    if (isAnimating) {
        ESP_LOGW("ANIMATION", "Animation already in progress. Ignoring request.");
        return;
    }
    isAnimating = true;
    animationStartTime = millis();
    
    // Capture the REAL current time as the departure time for saving later.
    time_t now;
    time(&now);
    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
    tzset();
    localtime_r(&now, &realDepartureTimeInfo);

    // Set up the hardcoded iconic movie dates for the animation visuals.
    // This provides a consistent, screen-accurate experience for every time jump.
    animDestTimeInfo = {0};
    animDestTimeInfo.tm_year = 1955 - 1900; animDestTimeInfo.tm_mon = 10; animDestTimeInfo.tm_mday = 5;
    animDestTimeInfo.tm_hour = 6; animDestTimeInfo.tm_min = 0;

    animPresTimeInfo = {0};
    animPresTimeInfo.tm_year = 1985 - 1900; animPresTimeInfo.tm_mon = 9; animPresTimeInfo.tm_mday = 26;
    animPresTimeInfo.tm_hour = 1; animPresTimeInfo.tm_min = 21;

    animLastTimeInfo = {0};
    animLastTimeInfo.tm_year = 1985 - 1900; animLastTimeInfo.tm_mon = 9; animLastTimeInfo.tm_mday = 26;
    animLastTimeInfo.tm_hour = 1; animLastTimeInfo.tm_min = 20;

    ESP_LOGI("ANIMATION", "Animation state set to 'true'. Start time: %lu", animationStartTime);
    currentPhase = ANIM_POWER_UP;
    #if ENABLE_HARDWARE
    if (currentSettings.timeTravelSoundToggle) {
        playSound("FLUX_CAPACITOR_CHARGE");
        ESP_LOGI("ANIMATION", "FLUX_CAPACITOR_CHARGE sound played.");
    }
    #endif
}

/**
 * @brief The main state machine for handling the multi-phase time travel animation.
 */
void handleDisplayAnimation() {
  #if ENABLE_HARDWARE
  if (!isAnimating) return;
  unsigned long currentTime = millis();
  unsigned long elapsed = currentTime - animationStartTime;

  // Define durations for each phase for a dramatic sequence.
  const int ACCELERATION_DURATION = 4000;
  const int WHITE_FLASH_DURATION = 150;
  const int FLICKER_DURATION = 1000;
  const int TIME_BLUR_DURATION = 2000;
  const int ARRIVAL_ECHO_DURATION = 300;
  const int TOTAL_DURATION = ACCELERATION_DURATION + WHITE_FLASH_DURATION + FLICKER_DURATION + TIME_BLUR_DURATION + ARRIVAL_ECHO_DURATION;

  switch (currentPhase) {
    case ANIM_POWER_UP:
      // Phase 1: Accelerate to 88 MPH.
      if (currentTime - lastAnimationFrameTime > 50) { // Update ~20 times per second.
          float progress = (float)elapsed / ACCELERATION_DURATION;
          // Use a power function to create an "ease-in" effect, making the acceleration feel more natural.
          int speed = 88 * pow(progress, 2.5); 
          if (speed > 88) speed = 88;
          
          displaySpeed(speed); // Update the speedometer on the bottom row.
          
          // Flicker the destination and present time displays as if they are locking on.
          animateTemporalLockOn(destRow, animDestTimeInfo, 1955);
          animateTemporalLockOn(presRow, animPresTimeInfo, 1985);
          
          lastAnimationFrameTime = currentTime;
      }
      if (elapsed >= ACCELERATION_DURATION) {
          // Phase 2: White Flash Climax.
          flashAllDisplays();
          delay(WHITE_FLASH_DURATION); // Blocking delay is acceptable here for a precise flash effect.
          currentPhase = ANIM_FLICKER;
          if(currentSettings.timeTravelSoundToggle) playSound("ACCELERATION");
      }
      break;

    case ANIM_FLICKER:
      // Phase 3: Temporal Displacement Flicker.
      if (currentTime - lastAnimationFrameTime > 50) {
          // Show random characters on all displays.
          animateDisplayRowRandomly(destRow);
          animateDisplayRowRandomly(presRow);
          animateDisplayRowRandomly(lastRow);
          lastAnimationFrameTime = currentTime;
      }
      if (elapsed >= (ACCELERATION_DURATION + WHITE_FLASH_DURATION + FLICKER_DURATION)) {
          currentPhase = ANIM_TIME_ACCELERATION;
      }
      break;

    case ANIM_TIME_ACCELERATION:
      // Phase 4: Time Blur Effect.
      if (currentTime - lastAnimationFrameTime > 50) {
          unsigned long time_blur_elapsed = elapsed - (ACCELERATION_DURATION + WHITE_FLASH_DURATION + FLICKER_DURATION);
          // Rapidly cycle through years, months, and days on all displays.
          animateAllRowsTimelineSkim(time_blur_elapsed, TIME_BLUR_DURATION, 1955);
          lastAnimationFrameTime = currentTime;
      }
      if (elapsed >= (ACCELERATION_DURATION + WHITE_FLASH_DURATION + FLICKER_DURATION + TIME_BLUR_DURATION)) {
          currentPhase = ANIM_ARRIVAL;
      }
      break;

    case ANIM_ARRIVAL:
      // Phase 5: Arrival Echo.
      // Briefly show the destination time on the "Present Time" display to simulate the jolt of arrival.
      updateDisplayRow(presRow, animDestTimeInfo, 1955);
      if(currentSettings.timeTravelSoundToggle) playSound("ARRIVAL_THUD");
      currentPhase = ANIM_LANDING; // Immediately move to the final phase.
      break;

    case ANIM_LANDING:
      // Phase 6: Finalization.
      if (elapsed >= TOTAL_DURATION) {
          // Update the "Last Time Departed" with the REAL time captured at the start of the animation.
          currentSettings.lastTimeDepartedYear = realDepartureTimeInfo.tm_year + 1900;
          currentSettings.lastTimeDepartedMonth = realDepartureTimeInfo.tm_mon + 1;
          currentSettings.lastTimeDepartedDay = realDepartureTimeInfo.tm_mday;
          currentSettings.lastTimeDepartedHour = realDepartureTimeInfo.tm_hour;
          currentSettings.lastTimeDepartedMinute = realDepartureTimeInfo.tm_min;
          
          // End the animation and restore the normal clock display.
          isAnimating = false;
          currentPhase = ANIM_INACTIVE;
          updateNormalClockDisplay();
          
          // Activate the post-jump "temporal echo" effect.
          isEchoEffectActive = true;
          echoEffectStartTime = millis();
          lastEchoCheckTime = millis();
          ESP_LOGI("FX", "Temporal Echo effect activated.");
      }
      break;
      
    case ANIM_INACTIVE:
      break;
    }
  #endif
}

/**
 * @brief Handles a post-time-travel effect where the "Present Time" display occasionally flickers to show the "Last Time Departed".
 */
void handleTemporalEcho() {
  #if ENABLE_HARDWARE
  if (!isEchoEffectActive) {
    return;
  }

  // The effect lasts for 3 minutes (180,000 ms).
  if (millis() - echoEffectStartTime > 180000) {
    isEchoEffectActive = false;
    isFlickeringNow = false;
    ESP_LOGI("FX", "Temporal Echo effect deactivated.");
    return;
  }

  // If a flicker is currently active, wait for it to finish.
  if (isFlickeringNow) {
    if (millis() - flickerStartTime > 150) { // Flicker duration.
      isFlickeringNow = false;
      flickerDisplayIndex = -1;
      updateNormalClockDisplay(); // Restore the correct time.
    }
    return;
  }

  // Every 10 seconds, there is a 25% chance of a flicker occurring.
  if (millis() - lastEchoCheckTime > 10000) {
    lastEchoCheckTime = millis();
    if (random(100) < 25) {
      isFlickeringNow = true;
      flickerStartTime = millis();
      // Randomly choose one of the four display segments to flicker.
      flickerDisplayIndex = random(4);

      // Get the last departed time to display during the flicker.
      struct tm lastTimeDepartedInfo = {0};
      lastTimeDepartedInfo.tm_year = currentSettings.lastTimeDepartedYear - 1900;
      lastTimeDepartedInfo.tm_mon = currentSettings.lastTimeDepartedMonth - 1;
      lastTimeDepartedInfo.tm_mday = currentSettings.lastTimeDepartedDay;
      lastTimeDepartedInfo.tm_hour = currentSettings.lastTimeDepartedHour;
      lastTimeDepartedInfo.tm_min = currentSettings.lastTimeDepartedMinute;
      const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
      char buffer[5];
      ESP_LOGD("FX", "Echo Flicker: Display %d", flickerDisplayIndex);

      // Overwrite one segment of the "Present Time" row with the corresponding "Last Time Departed" data.
      switch (flickerDisplayIndex) {
        case 0:
          printToDisplay(presRow.month, months[lastTimeDepartedInfo.tm_mon], 1);
          presRow.month.writeDisplay();
          break;
        case 1:
          sprintf(buffer, "%02d", lastTimeDepartedInfo.tm_mday);
          printToDisplay(presRow.day, buffer, 2);
          presRow.day.writeDisplay();
          break;
        case 2:
          sprintf(buffer, "%04d", currentSettings.lastTimeDepartedYear);
          printToDisplay(presRow.year, buffer);
          presRow.year.writeDisplay();
          break;
        case 3:
          char timeBuffer[5];
          sprintf(timeBuffer, "%02d%02d", lastTimeDepartedInfo.tm_hour, lastTimeDepartedInfo.tm_min);
          presRow.time.clear();
          presRow.time.writeDigitAscii(0, timeBuffer[0]);
          presRow.time.writeDigitAscii(1, timeBuffer[1] | 0x80); // Add decimal point
          presRow.time.writeDigitAscii(2, timeBuffer[2]);
          presRow.time.writeDigitAscii(3, timeBuffer[3]);
          presRow.time.writeDisplay();
          break;
      }
    }
  }
  #endif
}

/**
 * @brief Handles the state machine for a major "malfunction" visual effect.
 */
void handleMalfunction() {
  #if ENABLE_HARDWARE
  if (!isMalfunctioning) return;
  unsigned long elapsed = millis() - malfunctionStartTime;
  switch (currentMalfunctionPhase) {
    case MAL_HAYWIRE:
      // Phase 1: Show chaotic numbers on all displays.
      if (elapsed < 3000) {
        if (millis() - lastAnimationFrameTime > 100) {
          printToDisplay(destRow.month, "888", 1); /* ... and so on ... */
          // ... (code to write '8's to all displays) ...
          lastAnimationFrameTime = millis();
        }
      } else {
        malfunctionStartTime = millis();
        currentMalfunctionPhase = MAL_ERROR_MESSAGE;
      }
      break;
    case MAL_ERROR_MESSAGE:
      // Phase 2: Show a specific error message.
      if (elapsed < 4000) {
        printToDisplay(destRow.month, "TIM", 1);
        printToDisplay(destRow.day, "CI", 2); printToDisplay(destRow.year, "RCUT"); printToDisplay(destRow.time, "OVER");
        printToDisplay(presRow.month, "LOA", 1); printToDisplay(presRow.day, "D", 2); presRow.year.clear(); presRow.time.clear();
        printToDisplay(lastRow.month, "FLX", 1);
        printToDisplay(lastRow.day, "OF", 2); printToDisplay(lastRow.year, "FLIN"); lastRow.time.clear();
        // ... (code to write all displays) ...
      } else {
        malfunctionStartTime = millis();
        currentMalfunctionPhase = MAL_REBOOT;
      }
      break;
    case MAL_REBOOT:
      // Phase 3: Blank the displays and trigger the standard boot sequence.
      blankAllDisplays();
      runBootSequence();
      break;
    case MAL_INACTIVE:
      break;
  }
  #endif
}

/**
 * @brief Kicks off the boot sequence animation.
 */
void runBootSequence() {
  bootState = BOOT_START;
  bootStateStartTime = millis();
}

/**
 * @brief Handles the state machine for the boot sequence animation.
 */
void handleBootSequence() {
  if (bootState == BOOT_INACTIVE || bootState == BOOT_COMPLETE) return;
  unsigned long elapsed = millis() - bootStateStartTime;
  // Each phase of the boot sequence lasts for 2 seconds.
  if (elapsed > 2000) {
    bootState = static_cast<BootSequenceState>(bootState + 1);
    bootStateStartTime = millis();
    if (bootState >= BOOT_COMPLETE) {
      bootState = BOOT_COMPLETE;
      updateNormalClockDisplay();
      return;
    }
  }
  #if ENABLE_HARDWARE
  switch (bootState) {
    case BOOT_88MPH:
      // (Handled by default display state on boot)
      break;
    case BOOT_RECALIBRATING:
      printToDisplay(destRow.month, "REC", 1); printToDisplay(destRow.day, "AL", 2); printToDisplay(destRow.year, "IBRA"); printToDisplay(destRow.time, "TING");
      destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();
      break;
    case BOOT_CAPACITOR:
      printToDisplay(presRow.month, "CAP", 1); printToDisplay(presRow.day, "AC", 2); printToDisplay(presRow.year, "ITOR"); printToDisplay(presRow.time, "FULL");
      presRow.month.writeDisplay();
      presRow.day.writeDisplay(); presRow.year.writeDisplay(); presRow.time.writeDisplay();
      break;
    default:
      break;
  }
  #endif
}

/**
 * @brief Restores the normal clock display after a short glitch effect is finished.
 */
void restoreDisplayAfterGlitch() {
  if (isGlitching && millis() - glitchStartTime > 150) { // Glitch duration.
    updateNormalClockDisplay();
    isGlitching = false;
  }
}

/**
 * @brief Periodically triggers random glitch or malfunction effects based on configured probability.
 */
void handleGlitchEffect() {
  // Do not trigger effects during animations, sleep, or other effects.
  if (isAnimating || isDisplayAsleep || isGlitching || isMalfunctioning || currentSettings.glitchEffectFrequency == 0) return;
  
  // Check for a glitch once per minute.
  if (millis() - lastGlitchTime > 60000) {
    lastGlitchTime = millis();
    // Check if a glitch should occur based on the configured frequency.
    if (random(100) < currentSettings.glitchEffectFrequency) {
      // Within a glitch, there's a smaller chance of a full malfunction.
      if (currentSettings.malfunctionFrequency > 0 && random(currentSettings.malfunctionFrequency) == 0) {
        isMalfunctioning = true;
        malfunctionStartTime = millis();
        currentMalfunctionPhase = MAL_HAYWIRE;
      } else {
        // Trigger a simple, short glitch.
        isGlitching = true;
        glitchStartTime = millis();
        #if ENABLE_HARDWARE
        animateDisplayRowRandomly(destRow);
        animateDisplayRowRandomly(presRow);
        animateDisplayRowRandomly(lastRow);
        #endif
      }
    }
  }
}

/**
 * @brief Handles cycling through saved destination time presets.
 */
void handlePresetCycling() {
    if (currentSettings.presetCycleInterval == 0 || isAnimating || isDisplayAsleep) return;
    if (millis() - lastPresetCycleTime > (unsigned long)currentSettings.presetCycleInterval * 60000) {
        lastPresetCycleTime = millis();
        // (Future implementation: Add logic here to load the next preset from NVS).
    }
}

/**
 * @brief Checks the current time against the configured sleep/wake schedule and updates the display state.
 */
void handleSleepSchedule() {
  if (!timeSynchronized) return;
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  
  // Convert current time and sleep/wake times to minutes from midnight for easy comparison.
  int now_minutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  int sleep_minutes = currentSettings.departureHour * 60 + currentSettings.departureMinute;
  int wake_minutes = currentSettings.arrivalHour * 60 + currentSettings.arrivalMinute;

  // Determine if we are currently within the sleep window.
  // This handles the "overnight" case where sleep time is later than wake time (e.g., sleep at 10pm, wake at 7am).
  bool shouldBeAsleep = (sleep_minutes < wake_minutes) ?
                        (now_minutes >= sleep_minutes && now_minutes < wake_minutes) : // Same-day sleep window.
                        (now_minutes >= sleep_minutes || now_minutes < wake_minutes); // Overnight sleep window.

  // Change state if needed.
  if (shouldBeAsleep && !isDisplayAsleep) {
    isDisplayAsleep = true;
    #if ENABLE_HARDWARE
    blankAllDisplays();
    playSound("SLEEP_ON");
    #endif
  } else if (!shouldBeAsleep && isDisplayAsleep) {
    isDisplayAsleep = false;
    #if ENABLE_HARDWARE
    updateNormalClockDisplay();
    playSound("CONFIRM_ON");
    #endif
  }
}

/**
 * @brief Updates all three display rows with their normal time data.
 */
void updateNormalClockDisplay() {
  if (isDisplayAsleep || isAnimating || isGlitching || isMalfunctioning) return;
  #if ENABLE_HARDWARE
  if (timeSynchronized) {
    time_t now;
    time(&now);
    struct tm timeinfo;
    
    // Set the environment timezone to the "Present" timezone to correctly display the middle row.
    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
    tzset();
    localtime_r(&now, &timeinfo);
    updateDisplayRow(presRow, timeinfo, timeinfo.tm_year + 1900);
    
    // Set the environment timezone to the "Destination" timezone to correctly display the top row.
    setenv("TZ", TZ_DATA[currentSettings.destinationTimezoneIndex].tzString, 1);
    tzset();
    localtime_r(&now, &timeinfo);
    updateDisplayRow(destRow, timeinfo, currentSettings.destinationYear);
    
    // "Last Time Departed" is a fixed point in time and doesn't use a timezone.
    struct tm lastTimeDepartedInfo = {0};
    lastTimeDepartedInfo.tm_year = currentSettings.lastTimeDepartedYear - 1900;
    lastTimeDepartedInfo.tm_mon = currentSettings.lastTimeDepartedMonth - 1;
    lastTimeDepartedInfo.tm_mday = currentSettings.lastTimeDepartedDay;
    lastTimeDepartedInfo.tm_hour = currentSettings.lastTimeDepartedHour;
    lastTimeDepartedInfo.tm_min = currentSettings.lastTimeDepartedMinute;
    updateDisplayRow(lastRow, lastTimeDepartedInfo, currentSettings.lastTimeDepartedYear);
  }
  #endif
}

/**
 * @brief Handles the multi-page display logic for the live weather mode.
 */
void handleWeatherDisplay() {
    #if ENABLE_HARDWARE
    if (!currentSettings.weatherModeEnabled) return;
    if (xSemaphoreTake(xDisplayDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (!currentWeatherData.dataValid) {
            // Display a default message if weather data is not available.
            printToDisplay(lastRow.month, "WEA", 1);
            printToDisplay(lastRow.day, "TH", 2);
            printToDisplay(lastRow.year, "ER");
            printToDisplay(lastRow.time, "----");
        } else {
            // Cycle through different pages of weather information.
            static int weatherPage = 0;
            static unsigned long lastPageChange = 0;
            char buffer[6];
            // Change page every 4 seconds.
            if (millis() - lastPageChange > 4000) {
                weatherPage = (weatherPage + 1) % 4; // Cycle through 4 pages.
                lastPageChange = millis();
            }
            
            const char* icon = getIconForWeatherCode(currentWeatherData.weatherCode);
            switch(weatherPage) {
                case 0: // Current Conditions
                    printToDisplay(lastRow.month, "NOW", 1);
                    printToDisplay(lastRow.day, icon, 2);
                    dtostrf(currentWeatherData.temperature, 4, 1, buffer);
                    printToDisplay(lastRow.year, buffer);
                    printToDisplay(lastRow.time, currentSettings.useMetricUnits ? "CEL" : "DEG");
                    digitalWrite(LAST_AM_PIN, LOW); // Use indicator LEDs to show the page.
                    digitalWrite(LAST_PM_PIN, LOW);
                    break;
                case 1: // Tomorrow's Forecast
                    printToDisplay(lastRow.month, "TMRW", 1);
                    printToDisplay(lastRow.day, getIconForWeatherCode(currentWeatherData.tomorrowWeatherCode), 2);
                    dtostrf(currentWeatherData.tomorrowHigh, 4, 0, buffer);
                    printToDisplay(lastRow.year, buffer);
                    dtostrf(currentWeatherData.tomorrowLow, 4, 0, buffer);
                    printToDisplay(lastRow.time, buffer);
                    digitalWrite(LAST_AM_PIN, HIGH); // AM LED on.
                    digitalWrite(LAST_PM_PIN, LOW);
                    break;
                case 2: // Wind and Rain
                    printToDisplay(lastRow.month, "WIND", 1);
                    dtostrf(currentWeatherData.maxWindSpeed, 2, 0, buffer);
                    strcat(buffer, "M");
                    printToDisplay(lastRow.day, buffer, 2);
                    printToDisplay(lastRow.year, "RAIN");
                    sprintf(buffer, "%d%%", currentWeatherData.precipitationProbability);
                    printToDisplay(lastRow.time, buffer);
                    digitalWrite(LAST_AM_PIN, LOW);
                    digitalWrite(LAST_PM_PIN, HIGH); // PM LED on.
                    break;
                case 3: // Sunrise / Sunset
                    struct tm timeinfo;
                    char timeStr[5];
                    printToDisplay(lastRow.month, "SUN", 1);
                    localtime_r(&currentWeatherData.sunrise, &timeinfo);
                    sprintf(timeStr, "%02d%02d", timeinfo.tm_hour, timeinfo.tm_min);
                    printToDisplay(lastRow.day, timeStr, 2);
                    localtime_r(&currentWeatherData.sunset, &timeinfo);
                    sprintf(timeStr, "%02d%02d", timeinfo.tm_hour, timeinfo.tm_min);
                    printToDisplay(lastRow.year, timeStr);
                    printToDisplay(lastRow.time, "RISE/SET");
                    digitalWrite(LAST_AM_PIN, HIGH); // Both LEDs on.
                    digitalWrite(LAST_PM_PIN, HIGH);
                    break;
            }
        }
        xSemaphoreGive(xDisplayDataMutex);
        // Write the updated content to the physical displays.
        lastRow.month.writeDisplay();
        lastRow.day.writeDisplay();
        lastRow.year.writeDisplay();
        lastRow.time.writeDisplay();
    }
    #endif
}

/**
 * @brief Handles the state machine for the Data Link marquee display.
 */
void updateMarqueeDisplay() {
    #if ENABLE_HARDWARE
    if (!currentSettings.dataLinkEnabled || currentSettings.numDataPoints == 0) return;
    DisplayRow* targetRow = &lastRow;
    if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
        // State: IDLE - Time to move to the next data point.
        if (marqueeState == M_IDLE) {
            currentPageIndex = (currentPageIndex + 1) % currentSettings.numDataPoints;
            marqueeScrollPosition = 0; // Reset scroll position for the new page.
            marqueeScrollPositionYear = 0;
            marqueeState = M_PAUSED; // Move to PAUSED state to show static text first.
            lastMarqueeStateChange = millis();
        }

        DataPoint point = currentSettings.dataPoints[currentPageIndex];
        // Display the static parts of the marquee.
        printToDisplay(targetRow->month, displayPages[currentPageIndex].month.c_str());
        if (!point.icon.empty()) {
            printToDisplay(targetRow->day, point.icon.c_str(), 2);
        } else {
            printToDisplay(targetRow->day, displayPages[currentPageIndex].day.c_str(), 2);
        }

        // Construct the full text for the potentially scrolling parts.
        std::string yearContent = point.yearPrefix + displayPages[currentPageIndex].year + point.yearSuffix;
        std::string timeContent = point.prefix + displayPages[currentPageIndex].time + point.suffix;
        
        xSemaphoreGive(xDisplayDataMutex); // Release mutex after reading shared data.

        // Create a "canvas" for the text with padding, so it scrolls in from the side.
        String yearCanvas = "   " + String(yearContent.c_str()) + "   ";
        if (yearCanvas.length() <= 4) {
            // If the text fits, display it statically.
            printToDisplay(targetRow->year, yearCanvas.c_str());
        } else {
            // If it's too long, display a 4-character "viewport" of the canvas.
            String yearViewport = yearCanvas.substring(marqueeScrollPositionYear, marqueeScrollPositionYear + 4);
            printToDisplay(targetRow->year, yearViewport.c_str());
        }

        String timeCanvas = "   " + String(timeContent.c_str()) + "   ";
        if (timeCanvas.length() <= 4) {
            printToDisplay(targetRow->time, timeCanvas.c_str());
        } else {
            String viewport = timeCanvas.substring(marqueeScrollPosition, marqueeScrollPosition + 4);
            printToDisplay(targetRow->time, viewport.c_str());
        }

        // State: PAUSED - Wait for 2 seconds before starting to scroll.
        if (marqueeState == M_PAUSED && millis() - lastMarqueeStateChange > 2000) {
            marqueeState = M_SCROLLING;
            lastMarqueeStateChange = millis();
        }

        // State: SCROLLING - Increment the scroll position over time.
        if (marqueeState == M_SCROLLING && millis() - lastMarqueeStateChange > (unsigned long)point.scrollSpeed) {
            lastMarqueeStateChange = millis();
            bool timeDone = false;
            bool yearDone = false;

            // Increment the scroll position for the 'time' field if needed.
            if (timeCanvas.length() > 4) {
                marqueeScrollPosition++;
                if (marqueeScrollPosition > timeCanvas.length() - 4) {
                    timeDone = true; // Scrolling is finished.
                }
            } else {
                timeDone = true;
            }

            // Increment the scroll position for the 'year' field if needed.
            if (yearCanvas.length() > 4) {
                marqueeScrollPositionYear++;
                if (marqueeScrollPositionYear > yearCanvas.length() - 4) {
                    yearDone = true; // Scrolling is finished.
                }
            } else {
                yearDone = true;
            }

            // Once both fields are done scrolling, go back to the IDLE state to switch pages.
            if (timeDone && yearDone) {
                marqueeState = M_IDLE;
            }
        }

        // Write the final content to the physical displays.
        targetRow->month.writeDisplay();
        targetRow->day.writeDisplay();
        targetRow->year.writeDisplay();
        targetRow->time.writeDisplay();
    }
    #endif
}