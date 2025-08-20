#include "EventManager.h"
#include "web_server.h" // For access to extern variables and tasks
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// --- EXTERN DECLARATIONS ---
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

// NEW: Structs to hold time info for the animation
struct tm realDepartureTimeInfo; // The actual time the animation starts
struct tm animDestTimeInfo;      // Hardcoded movie destination time
struct tm animPresTimeInfo;      // Hardcoded movie present time
struct tm animLastTimeInfo;      // Hardcoded movie last departed time


// --- FUNCTION IMPLEMENTATIONS ---

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
    char path_copy[128];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    JsonVariant current = root;
    char* context = NULL;
    char* token = strtok_r(path_copy, ".[]", &context);
    while (token != NULL) {
        if (current.isNull()) return JsonVariant();
        if (current.is<JsonObject>()) {
            current = current[token];
        } else if (current.is<JsonArray>()) {
            current = current[atoi(token)];
        } else {
            return JsonVariant();
        }
        token = strtok_r(NULL, ".[]", &context);
    }
    return current;
}

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
    delay(duration);
    #endif
}

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

void fetchWeatherData(WeatherTaskParams* params) {
    std::string taskCityName = params->cityName;
    bool forceGeocode = params->forceGeocode;
    delete params; // Clean up memory

    if (taskCityName.empty()) {
        ESP_LOGE("Weather", "City name is empty, cannot fetch weather.");
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
        for (int i = 0; i < 3; i++) { // Retry up to 3 times
            ESP_LOGI("Weather", "Geocoding attempt %d for %s", i + 1, taskCityName.c_str());
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
                            lastCityName = taskCityName; // Update the cache
                            xSemaphoreGive(xDisplayDataMutex);
                        }
                        ESP_LOGI("Weather", "Geocoded %s to Lat: %f, Lon: %f", taskCityName.c_str(), currentSettings.latitude, currentSettings.longitude);
                        geocodeSuccess = true;
                        http.end();
                        break; 
                    }
                }
                http.end();
            }
            delay(1000); // Wait 1 second before retrying
        }

        if (!geocodeSuccess) {
            ESP_LOGE("Weather", "Geocoding failed for city: %s after all retries.", taskCityName.c_str());
            showTemporaryMessage("GEO", "", "FAIL", "", 2000);
            if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                currentWeatherData.dataValid = false;
                xSemaphoreGive(xDisplayDataMutex);
            }
            return;
        }
    }

    bool weatherSuccess = false;
    for (int i = 0; i < 3; i++) { // Retry up to 3 times
        ESP_LOGI("Weather", "Weather fetch attempt %d", i + 1);
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
                        // Current weather
                        currentWeatherData.temperature = doc["current"]["temperature_2m"];
                        currentWeatherData.apparentTemperature = doc["current"]["apparent_temperature"];
                        currentWeatherData.windSpeed = doc["current"]["wind_speed_10m"];
                        currentWeatherData.humidity = doc["current"]["relative_humidity_2m"];
                        currentWeatherData.weatherCode = doc["current"]["weather_code"];
                        // Today's daily forecast
                        currentWeatherData.dailyHigh = doc["daily"]["temperature_2m_max"][0];
                        currentWeatherData.dailyLow = doc["daily"]["temperature_2m_min"][0];
                        currentWeatherData.sunrise = doc["daily"]["sunrise"][0];
                        currentWeatherData.sunset = doc["daily"]["sunset"][0];
                        currentWeatherData.precipitationProbability = doc["daily"]["precipitation_probability_max"][0];
                        currentWeatherData.maxWindSpeed = doc["daily"]["wind_speed_10m_max"][0];
                        // Tomorrow's forecast
                        currentWeatherData.tomorrowHigh = doc["daily"]["temperature_2m_max"][1];
                        currentWeatherData.tomorrowLow = doc["daily"]["temperature_2m_min"][1];
                        currentWeatherData.tomorrowWeatherCode = doc["daily"]["weather_code"][1];

                        // Hourly forecast
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
                    xSemaphoreGive(xDisplayDataMutex);
                }
                http.end();
                if (weatherSuccess) break;
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

void setupMqtt() {
  if (currentSettings.mqttBroker.empty()) {
    ESP_LOGI("MQTT", "Broker not configured. Skipping MQTT setup.");
    return;
  }
  mqttClient.setServer(currentSettings.mqttBroker.c_str(), currentSettings.mqttPort);
  mqttClient.setCallback(mqttCallback);
  ESP_LOGI("MQTT", "Client configured for broker %s:%d", currentSettings.mqttBroker.c_str(), currentSettings.mqttPort);
}

void reconnectMqtt() {
  if (currentSettings.mqttBroker.empty()) return;
  if (!mqttClient.connected()) {
    ESP_LOGI("MQTT", "Attempting MQTT connection...");
    String clientId = "BTTF-Clock-";
    clientId += String(random(0xffff), HEX);
    bool connected = false;
    if (!currentSettings.mqttUser.empty()) {
        connected = mqttClient.connect(clientId.c_str(), currentSettings.mqttUser.c_str(), currentSettings.mqttPassword.c_str(), "bttf-clock/status", 1, true, "offline");
    } else {
        connected = mqttClient.connect(clientId.c_str(), "bttf-clock/status", 1, true, "offline");
    }

    if (connected) {
      ESP_LOGI("MQTT", "Connected to broker!");
      mqttClient.publish("bttf-clock/status", "online", true);
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

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  ESP_LOGI("MQTT", "Message arrived [%s] %s", topic, message.c_str());

  for (int i = 0; i < currentSettings.numDataPoints; i++) {
    DataPoint point = currentSettings.dataPoints[i];
    if (point.dataSourceType == DATA_SOURCE_MQTT && point.mqttTopic == topic) {
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, message);
        bool success = false;
        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
            if (error == DeserializationError::Ok) {
                auto fetch = [&](const char* path) {
                    return getJsonVariant(doc.as<JsonVariant>(), path).as<String>();
                };
                if(!point.monthPath.empty()) displayPages[i].month = fetch(point.monthPath.c_str()).c_str();
                if(!point.dayPath.empty()) displayPages[i].day = fetch(point.dayPath.c_str()).c_str();
                if(!point.yearPath.empty()) displayPages[i].year = fetch(point.yearPath.c_str()).c_str();
                if(!point.timePath.empty()) displayPages[i].time = fetch(point.timePath.c_str()).c_str();
                success = true;
            } else {
                displayPages[i].month = "";
                displayPages[i].day = "";
                displayPages[i].year = "";
                displayPages[i].time = message.c_str();
                success = true;
            }

            if (success) {
                memcpy(&lastGoodDisplayPages[i], &displayPages[i], sizeof(MarqueeData));
                dataPointFetchFailures[i] = 0;
            } else {
                dataPointFetchFailures[i]++;
                if (dataPointFetchFailures[i] >= 3) {
                    displayPages[i].time = "MQTT FAIL";
                } else {
                    memcpy(&displayPages[i], &lastGoodDisplayPages[i], sizeof(MarqueeData));
                }
            }
            xSemaphoreGive(xDisplayDataMutex);
        }
        break;
    }
  }
}

void fetchDataTask(void* p) {
    struct FetchDataParams* params = (struct FetchDataParams*)p;
    int pointIndex = params->pointIndex;
    int totalRequests = params->totalRequests;
    delete params;

    DataPoint point = currentSettings.dataPoints[pointIndex];
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure(); // Bypassing certificate validation

    if (http.begin(client, point.url.c_str())) {
        if (!point.authHeaderKey.empty() && !point.authHeaderValue.empty()) {
            http.addHeader(point.authHeaderKey.c_str(), point.authHeaderValue.c_str());
        }
        
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            DynamicJsonDocument doc(2048);
            DeserializationError error = deserializeJson(doc, payload);
            if (error == DeserializationError::Ok) {
                if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                    auto fetch = [&](const char* path) {
                        return getJsonVariant(doc.as<JsonVariant>(), path).as<String>();
                    };
                    if(!point.monthPath.empty()) displayPages[pointIndex].month = fetch(point.monthPath.c_str()).c_str();
                    if(!point.dayPath.empty()) displayPages[pointIndex].day = fetch(point.dayPath.c_str()).c_str();
                    if(!point.yearPath.empty()) displayPages[pointIndex].year = fetch(point.yearPath.c_str()).c_str();
                    if(!point.timePath.empty()) displayPages[pointIndex].time = fetch(point.timePath.c_str()).c_str();
                    
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
    
    if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
        if (dataPointFetchFailures[pointIndex] >= 3) {
            displayPages[pointIndex].time = "API FAIL";
        }
        xSemaphoreGive(xDisplayDataMutex);
    }

    requestsCompleted++;
    if (requestsCompleted >= totalRequests) {
        isFetchingData = false;
        ESP_LOGI("DataLink", "All API requests finished.");
    }

    vTaskDelete(NULL);
}


void fetchDataLink() {
    if (!timeSynchronized || !currentSettings.dataLinkEnabled || currentSettings.numDataPoints == 0 || isFetchingData) {
        return;
    }
    if (millis() - lastDataLinkFetch < (unsigned long)currentSettings.dataLinkRefreshInterval * 60 * 1000) {
        return;
    }
    
    lastDataLinkFetch = millis();
    isFetchingData = true;
    requestsCompleted = 0;
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
    for (int i = 0; i < currentSettings.numDataPoints; i++) {
        if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_API) {
            FetchDataParams* params = new FetchDataParams{i, apiRequestsToMake};
            xTaskCreate(fetchDataTask, "fetchDataTask", 8192, params, 1, NULL);
        }
    }
}

// UPDATED: startTimeTravelAnimation now sets up the iconic movie dates for the animation.
void startTimeTravelAnimation() {
    ESP_LOGI("ANIMATION", "startTimeTravelAnimation function called.");
    if (isAnimating) {
        ESP_LOGW("ANIMATION", "Animation already in progress. Ignoring request.");
        return;
    }
    isAnimating = true;
    animationStartTime = millis();
    
    // Capture the REAL current time as the departure time for saving later
    time_t now;
    time(&now);
    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
    tzset();
    localtime_r(&now, &realDepartureTimeInfo);

    // Set up the hardcoded iconic movie dates for the animation visuals
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

// REWRITTEN: The animation handler is now a highly detailed, multi-stage state machine.
void handleDisplayAnimation() {
  #if ENABLE_HARDWARE
  if (!isAnimating) return;
  unsigned long currentTime = millis();
  unsigned long elapsed = currentTime - animationStartTime;

  // Define durations for each phase for a more dramatic sequence
  const int ACCELERATION_DURATION = 4000;
  const int WHITE_FLASH_DURATION = 150;
  const int FLICKER_DURATION = 1000;
  const int TIME_BLUR_DURATION = 2000;
  const int ARRIVAL_ECHO_DURATION = 300;
  const int TOTAL_DURATION = ACCELERATION_DURATION + WHITE_FLASH_DURATION + FLICKER_DURATION + TIME_BLUR_DURATION + ARRIVAL_ECHO_DURATION;

  switch (currentPhase) {
    case ANIM_POWER_UP:
      if (currentTime - lastAnimationFrameTime > 50) {
          float progress = (float)elapsed / ACCELERATION_DURATION;
          int speed = 88 * pow(progress, 2.5); // Use a power of 2.5 for a more aggressive ease-in
          if (speed > 88) speed = 88;
          
          displaySpeed(speed);
          
          // Use the "Temporal Lock-On" effect with the iconic movie dates
          animateTemporalLockOn(destRow, animDestTimeInfo, 1955);
          animateTemporalLockOn(presRow, animPresTimeInfo, 1985);
          
          lastAnimationFrameTime = currentTime;
      }
      if (elapsed >= ACCELERATION_DURATION) {
          flashAllDisplays(); // White Flash Climax
          delay(WHITE_FLASH_DURATION);
          currentPhase = ANIM_FLICKER;
          if(currentSettings.timeTravelSoundToggle) playSound("ACCELERATION");
      }
      break;

    case ANIM_FLICKER:
      if (currentTime - lastAnimationFrameTime > 50) {
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
      if (currentTime - lastAnimationFrameTime > 50) {
          unsigned long time_blur_elapsed = elapsed - (ACCELERATION_DURATION + WHITE_FLASH_DURATION + FLICKER_DURATION);
          // Use the new coordinated time blur
          animateAllRowsTimelineSkim(time_blur_elapsed, TIME_BLUR_DURATION, 1955);
          lastAnimationFrameTime = currentTime;
      }
      if (elapsed >= (ACCELERATION_DURATION + WHITE_FLASH_DURATION + FLICKER_DURATION + TIME_BLUR_DURATION)) {
          currentPhase = ANIM_ARRIVAL;
      }
      break;

    case ANIM_ARRIVAL:
      // Display the "Arrival Echo" using the iconic destination time
      updateDisplayRow(presRow, animDestTimeInfo, 1955);
      if(currentSettings.timeTravelSoundToggle) playSound("ARRIVAL_THUD");
      currentPhase = ANIM_LANDING; // Immediately move to landing to wait out the delay
      break;

    case ANIM_LANDING:
      if (elapsed >= TOTAL_DURATION) {
          // Update the Last Time Departed with the REAL captured time
          currentSettings.lastTimeDepartedYear = realDepartureTimeInfo.tm_year + 1900;
          currentSettings.lastTimeDepartedMonth = realDepartureTimeInfo.tm_mon + 1;
          currentSettings.lastTimeDepartedDay = realDepartureTimeInfo.tm_mday;
          currentSettings.lastTimeDepartedHour = realDepartureTimeInfo.tm_hour;
          currentSettings.lastTimeDepartedMinute = realDepartureTimeInfo.tm_min;
          
          // Finalize animation and restore correct times
          isAnimating = false;
          currentPhase = ANIM_INACTIVE;
          updateNormalClockDisplay();
          
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


void handleTemporalEcho() {
  #if ENABLE_HARDWARE
  if (!isEchoEffectActive) {
    return;
  }

  if (millis() - echoEffectStartTime > 180000) {
    isEchoEffectActive = false;
    isFlickeringNow = false;
    ESP_LOGI("FX", "Temporal Echo effect deactivated.");
    return;
  }

  if (isFlickeringNow) {
    if (millis() - flickerStartTime > 150) {
      isFlickeringNow = false;
      flickerDisplayIndex = -1;
      updateNormalClockDisplay();
    }
    return;
  }

  if (millis() - lastEchoCheckTime > 10000) {
    lastEchoCheckTime = millis();
    if (random(100) < 25) {
      isFlickeringNow = true;
      flickerStartTime = millis();
      flickerDisplayIndex = random(4);

      struct tm lastTimeDepartedInfo = {0};
      lastTimeDepartedInfo.tm_year = currentSettings.lastTimeDepartedYear - 1900;
      lastTimeDepartedInfo.tm_mon = currentSettings.lastTimeDepartedMonth - 1;
      lastTimeDepartedInfo.tm_mday = currentSettings.lastTimeDepartedDay;
      lastTimeDepartedInfo.tm_hour = currentSettings.lastTimeDepartedHour;
      lastTimeDepartedInfo.tm_min = currentSettings.lastTimeDepartedMinute;
      const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
      char buffer[5];
      ESP_LOGD("FX", "Echo Flicker: Display %d", flickerDisplayIndex);

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
          presRow.time.writeDigitAscii(1, timeBuffer[1] | 0x80);
          presRow.time.writeDigitAscii(2, timeBuffer[2]);
          presRow.time.writeDigitAscii(3, timeBuffer[3]);
          presRow.time.writeDisplay();
          break;
      }
    }
  }
  #endif
}

void handleMalfunction() {
  #if ENABLE_HARDWARE
  if (!isMalfunctioning) return;
  unsigned long elapsed = millis() - malfunctionStartTime;
  switch (currentMalfunctionPhase) {
    case MAL_HAYWIRE:
      if (elapsed < 3000) {
        if (millis() - lastAnimationFrameTime > 100) {
          printToDisplay(destRow.month, "888", 1);
          printToDisplay(destRow.day, "88", 2); printToDisplay(destRow.year, "8888"); printToDisplay(destRow.time, "8888");
          printToDisplay(presRow.month, "888", 1); printToDisplay(presRow.day, "88", 2); printToDisplay(presRow.year, "8888"); printToDisplay(presRow.time, "8888");
          printToDisplay(lastRow.month, "888", 1);
          printToDisplay(lastRow.day, "88", 2); printToDisplay(lastRow.year, "8888");
          printToDisplay(lastRow.time, "8888");
          destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();
          presRow.month.writeDisplay(); presRow.day.writeDisplay(); presRow.year.writeDisplay(); presRow.time.writeDisplay();
          lastRow.month.writeDisplay(); lastRow.day.writeDisplay(); lastRow.year.writeDisplay(); lastRow.time.writeDisplay();
          lastAnimationFrameTime = millis();
        }
      } else {
        malfunctionStartTime = millis();
        currentMalfunctionPhase = MAL_ERROR_MESSAGE;
      }
      break;
    case MAL_ERROR_MESSAGE:
      if (elapsed < 4000) {
        printToDisplay(destRow.month, "TIM", 1);
        printToDisplay(destRow.day, "CI", 2); printToDisplay(destRow.year, "RCUT"); printToDisplay(destRow.time, "OVER");
        printToDisplay(presRow.month, "LOA", 1); printToDisplay(presRow.day, "D", 2); presRow.year.clear(); presRow.time.clear();
        printToDisplay(lastRow.month, "FLX", 1);
        printToDisplay(lastRow.day, "OF", 2); printToDisplay(lastRow.year, "FLIN"); lastRow.time.clear();
        destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();
        presRow.month.writeDisplay(); presRow.day.writeDisplay(); presRow.year.writeDisplay(); presRow.time.writeDisplay();
        lastRow.month.writeDisplay(); lastRow.day.writeDisplay(); lastRow.year.writeDisplay(); lastRow.time.writeDisplay();
      } else {
        malfunctionStartTime = millis();
        currentMalfunctionPhase = MAL_REBOOT;
      }
      break;
    case MAL_REBOOT:
      blankAllDisplays();
      runBootSequence();
      break;
    case MAL_INACTIVE:
      // Should not be reached.
      break;
  }
  #endif
}

void runBootSequence() {
  bootState = BOOT_START;
  bootStateStartTime = millis();
}

void handleBootSequence() {
  if (bootState == BOOT_INACTIVE || bootState == BOOT_COMPLETE) return;
  unsigned long elapsed = millis() - bootStateStartTime;
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
      // Handled by default case
      break;
    case BOOT_RECALIBRATING:
      printToDisplay(destRow.month, "REC", 1); printToDisplay(destRow.day, "AL", 2); printToDisplay(destRow.year, "IBRA"); printToDisplay(destRow.time, "TING");
      destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay();
      destRow.time.writeDisplay();
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

void restoreDisplayAfterGlitch() {
  if (isGlitching && millis() - glitchStartTime > 150) {
    updateNormalClockDisplay();
    isGlitching = false;
  }
}

void handleGlitchEffect() {
  if (isAnimating || isDisplayAsleep || isGlitching || isMalfunctioning || currentSettings.glitchEffectFrequency == 0) return;
  if (millis() - lastGlitchTime > 60000) {
    lastGlitchTime = millis();
    if (random(100) < currentSettings.glitchEffectFrequency) {
      if (currentSettings.malfunctionFrequency > 0 && random(currentSettings.malfunctionFrequency) == 0) {
        isMalfunctioning = true;
        malfunctionStartTime = millis();
        currentMalfunctionPhase = MAL_HAYWIRE;
      } else {
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

void handlePresetCycling() {
    if (currentSettings.presetCycleInterval == 0 || isAnimating || isDisplayAsleep) return;
    if (millis() - lastPresetCycleTime > (unsigned long)currentSettings.presetCycleInterval * 60000) {
        lastPresetCycleTime = millis();
        // Add logic here to cycle through presets if needed
    }
}

void handleSleepSchedule() {
  if (!timeSynchronized) return;
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  int now_minutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  int sleep_minutes = currentSettings.departureHour * 60 + currentSettings.departureMinute;
  int wake_minutes = currentSettings.arrivalHour * 60 + currentSettings.arrivalMinute;
  bool shouldBeAsleep = (sleep_minutes < wake_minutes) ?
                        (now_minutes >= sleep_minutes && now_minutes < wake_minutes) :
                        (now_minutes >= sleep_minutes || now_minutes < wake_minutes);
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

void updateNormalClockDisplay() {
  if (isDisplayAsleep || isAnimating || isGlitching || isMalfunctioning) return;
  #if ENABLE_HARDWARE
  if (timeSynchronized) {
    time_t now;
    time(&now);
    struct tm timeinfo;
    
    // Set to present timezone to update present time display
    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
    tzset();
    localtime_r(&now, &timeinfo);
    updateDisplayRow(presRow, timeinfo, timeinfo.tm_year + 1900);
    
    // Set to destination timezone for destination display
    setenv("TZ", TZ_DATA[currentSettings.destinationTimezoneIndex].tzString, 1);
    tzset();
    localtime_r(&now, &timeinfo);
    updateDisplayRow(destRow, timeinfo, currentSettings.destinationYear);
    
    // Last Time Departed doesn't use a timezone, it's a fixed point in time
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

void handleWeatherDisplay() {
    #if ENABLE_HARDWARE
    if (!currentSettings.weatherModeEnabled) return;
    if (xSemaphoreTake(xDisplayDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (!currentWeatherData.dataValid) {
            printToDisplay(lastRow.month, "WEA", 1);
            printToDisplay(lastRow.day, "TH", 2);
            printToDisplay(lastRow.year, "ER");
            printToDisplay(lastRow.time, "----");
        } else {
            static int weatherPage = 0;
            static unsigned long lastPageChange = 0;
            char buffer[6];
            if (millis() - lastPageChange > 4000) {
                weatherPage = (weatherPage + 1) % 4; // Cycle through 4 pages
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
                    digitalWrite(LAST_AM_PIN, LOW);
                    digitalWrite(LAST_PM_PIN, LOW);
                    break;
                case 1: // Tomorrow's Forecast
                    printToDisplay(lastRow.month, "TMRW", 1);
                    printToDisplay(lastRow.day, getIconForWeatherCode(currentWeatherData.tomorrowWeatherCode), 2);
                    dtostrf(currentWeatherData.tomorrowHigh, 4, 0, buffer);
                    printToDisplay(lastRow.year, buffer);
                    dtostrf(currentWeatherData.tomorrowLow, 4, 0, buffer);
                    printToDisplay(lastRow.time, buffer);
                    digitalWrite(LAST_AM_PIN, HIGH);
                    digitalWrite(LAST_PM_PIN, LOW);
                    break;
                case 2: // Wind and Rain
                    printToDisplay(lastRow.month, "WIND", 1);
                    dtostrf(currentWeatherData.maxWindSpeed, 2, 0, buffer);
                    strcat(buffer, "M"); // Assuming MPH/KPH for display
                    printToDisplay(lastRow.day, buffer, 2);
                    printToDisplay(lastRow.year, "RAIN");
                    sprintf(buffer, "%d%%", currentWeatherData.precipitationProbability);
                    printToDisplay(lastRow.time, buffer);
                    digitalWrite(LAST_AM_PIN, LOW);
                    digitalWrite(LAST_PM_PIN, HIGH);
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
                    digitalWrite(LAST_AM_PIN, HIGH);
                    digitalWrite(LAST_PM_PIN, HIGH);
                    break;
            }
        }
        xSemaphoreGive(xDisplayDataMutex);
        lastRow.month.writeDisplay();
        lastRow.day.writeDisplay();
        lastRow.year.writeDisplay();
        lastRow.time.writeDisplay();
    }
    #endif
}

void updateMarqueeDisplay() {
    #if ENABLE_HARDWARE
    if (!currentSettings.dataLinkEnabled || currentSettings.numDataPoints == 0) return;
    DisplayRow* targetRow = &lastRow;
    if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
        if (marqueeState == M_IDLE) {
            currentPageIndex = (currentPageIndex + 1) % currentSettings.numDataPoints;
            marqueeScrollPosition = 0;
            marqueeScrollPositionYear = 0;
            marqueeState = M_PAUSED;
            lastMarqueeStateChange = millis();
        }

        DataPoint point = currentSettings.dataPoints[currentPageIndex];
        printToDisplay(targetRow->month, displayPages[currentPageIndex].month.c_str());
        if (!point.icon.empty()) {
            printToDisplay(targetRow->day, point.icon.c_str(), 2);
        } else {
            printToDisplay(targetRow->day, displayPages[currentPageIndex].day.c_str(), 2);
        }

        std::string yearContent = point.yearPrefix + displayPages[currentPageIndex].year + point.yearSuffix;
        std::string timeContent = point.prefix + displayPages[currentPageIndex].time + point.suffix;
        
        xSemaphoreGive(xDisplayDataMutex); // Release mutex after reading shared data

        String yearCanvas = "   " + String(yearContent.c_str()) + "   ";
        if (yearCanvas.length() <= 4) {
            printToDisplay(targetRow->year, yearCanvas.c_str());
        } else {
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

        if (marqueeState == M_PAUSED && millis() - lastMarqueeStateChange > 2000) {
            marqueeState = M_SCROLLING;
            lastMarqueeStateChange = millis();
        }

        if (marqueeState == M_SCROLLING && millis() - lastMarqueeStateChange > (unsigned long)point.scrollSpeed) {
            lastMarqueeStateChange = millis();
            bool timeDone = false;
            bool yearDone = false;

            if (timeCanvas.length() > 4) {
                marqueeScrollPosition++;
                if (marqueeScrollPosition > timeCanvas.length() - 4) {
                    timeDone = true;
                }
            } else {
                timeDone = true;
            }

            if (yearCanvas.length() > 4) {
                marqueeScrollPositionYear++;
                if (marqueeScrollPositionYear > yearCanvas.length() - 4) {
                    yearDone = true;
                }
            } else {
                yearDone = true;
            }

            if (timeDone && yearDone) {
                marqueeState = M_IDLE;
            }
        }

        targetRow->month.writeDisplay();
        targetRow->day.writeDisplay();
        targetRow->year.writeDisplay();
        targetRow->time.writeDisplay();
    }
    #endif
}
