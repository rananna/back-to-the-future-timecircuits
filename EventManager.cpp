/**
 * @file EventManager.cpp
 * @brief Implements the core logic for state management, animations, and data fetching.
 */

#include "EventManager.h"
#include "web_server.h" // For access to extern variables and tasks
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// HA-ENHANCEMENT: Moved definitions to the header for global visibility.
#define MQTT_UNIQUE_ID "bttf_timecircuits_01"
#define MQTT_DEVICE_TYPE "bttf-clock"
#define MQTT_BASE_TOPIC "homeassistant"


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
extern PubSubClient mqttClient;
extern bool timeSynchronized;
extern int currentPageIndex;

// HA-ENHANCEMENT: Extern declarations for new override state
extern bool isMessageOverrideActive;
extern String overrideMessageLine1;
extern String overrideMessageLine2;
extern String overrideMessageLine3;

// HA-MARQUEE: Extern declarations for the dynamic marquee override.
extern bool isMarqueeOverrideActive;
extern String marqueeOverrideMessage;


// ADDED: Extern declaration for the Time Zone data array to make it visible to this file.
extern const TimeZoneEntry TZ_DATA[];

// HA-ERROR-CHECK: Flag to ensure discovery messages are only sent once per boot cycle.
bool haDiscoveryPublished = false;


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
 * @brief HA-ERROR-CHECK: Publishes an empty retained message to a config topic to clear it in HA.
 * This is an invaluable development tool to remove old, renamed, or orphaned entities.
 * @param component The HA component type (e.g., "switch", "sensor").
 * @param unique_id_suffix The suffix of the unique_id of the entity to clear (e.g., "power", "status").
 */
void clearHaEntity(const char* component, const char* unique_id_suffix) {
    String object_id = String(MQTT_UNIQUE_ID) + "_" + unique_id_suffix;
    String topic = String(MQTT_BASE_TOPIC) + "/" + component + "/" + object_id + "/config";
    ESP_LOGW("HA_CLEANUP", "Clearing stale HA entity: %s", topic.c_str());
    // Publishing an empty, retained payload to the config topic is the official way to remove an entity.
    if (mqttClient.connected()) {
        if (!mqttClient.publish(topic.c_str(), "", true)) {
            ESP_LOGE("HA_CLEANUP", "Failed to clear HA entity %s. Message may be too large for buffer.", topic.c_str());
        }
    }
}

/**
 * @brief HA-ENHANCEMENT: Constructs and publishes all MQTT discovery messages for Home Assistant.
 */
void publishHaAutoDiscovery() {
    ESP_LOGI("HA_DISCOVERY", "Publishing Home Assistant auto-discovery messages...");

    // This is the base topic for all state and command messages from this device.
    String device_base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;

    // Create a reusable JSON object for the device information.
    StaticJsonDocument<512> device_doc;
    JsonObject device = device_doc.to<JsonObject>();
    device["identifiers"] = MQTT_UNIQUE_ID;
    device["name"] = "Time Circuits Display";
    device["model"] = "BTTF Clock v1";
    device["manufacturer"] = "Doc Brown Industries";
    device["sw_version"] = "2.0";

    // Create a reusable JSON array for the availability information.
    StaticJsonDocument<256> availability_doc;
    JsonArray availability = availability_doc.to<JsonArray>();
    JsonObject availability_topic = availability.createNestedObject();
    availability_topic["topic"] = device_base_topic + "/status";
    availability_topic["payload_available"] = "online";
    availability_topic["payload_not_available"] = "offline";

    // --- Create a temporary document for each entity ---
    DynamicJsonDocument doc(1024);
    String topic;
    String payload;
    
    // --- Entity: Status Sensor ---
    doc.clear();
    doc["name"] = "Status";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_status";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_status";
    doc["state_topic"] = device_base_topic + "/status/state";
    doc["icon"] = "mdi:clock-outline";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/sensor/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    // --- Entity: Destination Year (Number Input) ---
    doc.clear();
    doc["name"] = "Destination Year";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_dest_year";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_dest_year";
    doc["command_topic"] = device_base_topic + "/destination_year/command";
    doc["state_topic"] = device_base_topic + "/destination_year/state";
    doc["min"] = 1000;
    doc["max"] = 9999;
    doc["step"] = 1;
    doc["mode"] = "box";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/number/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    // --- Entity: Message Override Switch ---
    doc.clear();
    doc["name"] = "Message Override";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_override_switch";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_override_switch";
    doc["command_topic"] = device_base_topic + "/override/command";
    doc["state_topic"] = device_base_topic + "/override/state";
    doc["icon"] = "mdi:message-alert-outline";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/switch/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    // --- Entity: Override Message Text Input ---
    doc.clear();
    doc["name"] = "Override Message";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_override_text";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_override_text";
    doc["command_topic"] = device_base_topic + "/override_text/command";
    doc["state_topic"] = device_base_topic + "/override_text/state";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/text/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
    
    // --- Device Triggers for Automations ---
    doc.clear();
    doc["automation_type"] = "trigger";
    doc["topic"] = device_base_topic + "/events";
    doc["type"] = "animation_started";
    doc["subtype"] = "event";
    doc["device"] = device;
    topic = String(MQTT_BASE_TOPIC) + "/device_automation/" + MQTT_UNIQUE_ID + "/anim_started/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
    
    doc.clear();
    doc["automation_type"] = "trigger";
    doc["topic"] = device_base_topic + "/events";
    doc["type"] = "animation_completed";
    doc["subtype"] = "event";
    doc["device"] = device;
    topic = String(MQTT_BASE_TOPIC) + "/device_automation/" + MQTT_UNIQUE_ID + "/anim_completed/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    // --- HA-MARQUEE: Add Text Entity for Dynamic Marquee Control ---
    doc.clear();
    doc["name"] = "Marquee Message";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_marquee_message";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_marquee_message";
    doc["command_topic"] = device_base_topic + "/marquee/command";
    doc["state_topic"] = device_base_topic + "/marquee/state";
    doc["icon"] = "mdi:sign-text";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/text/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    // --- Entity: Power Switch ---
    doc.clear();
    doc["name"] = "Power";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_power";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_power";
    doc["command_topic"] = device_base_topic + "/power/command";
    doc["state_topic"] = device_base_topic + "/power/state";
    doc["icon"] = "mdi:power";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/switch/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    // --- Entity: Brightness (Number Input) ---
    doc.clear();
    doc["name"] = "Brightness";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_brightness";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_brightness";
    doc["command_topic"] = device_base_topic + "/brightness/command";
    doc["state_topic"] = device_base_topic + "/brightness/state";
    doc["min"] = 0;
    doc["max"] = 7;
    doc["step"] = 1;
    doc["mode"] = "slider";
    doc["icon"] = "mdi:brightness-6";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/number/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    // --- Entity: Animation Trigger (Button) ---
    doc.clear();
    doc["name"] = "Trigger Animation";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_trigger_animation";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_trigger_animation";
    doc["command_topic"] = device_base_topic + "/animation/command";
    doc["payload_press"] = "START";
    doc["icon"] = "mdi:movie-play-outline";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/button/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    ESP_LOGI("HA_DISCOVERY", "Finished publishing all discovery messages.");
}

/**
 * @brief Attempts to reconnect to the MQTT broker if the connection is lost.
 */
void reconnectMqtt() {
  if (currentSettings.mqttBroker.empty()) return;
  if (!mqttClient.connected()) {
    ESP_LOGI("MQTT", "Attempting MQTT connection...");
    String clientId = "BTTF-Clock-";
    clientId += String(random(0xffff), HEX);
    String availability_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/status";
    
    if (mqttClient.connect(clientId.c_str(), currentSettings.mqttUser.c_str(), currentSettings.mqttPassword.c_str(), availability_topic.c_str(), 1, true, "offline")) {
        ESP_LOGI("MQTT", "Connected to broker!");
        mqttClient.publish(availability_topic.c_str(), "online", true);
        
        // HA-ERROR-CHECK: Only publish the full discovery configuration once per boot.
        if (!haDiscoveryPublished) {
            publishHaAutoDiscovery();
            haDiscoveryPublished = true;
        }
        
        // Always publish current states on any reconnect to ensure HA is in sync.
        publishAllHaStates();

        String command_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/+/command";
        mqttClient.subscribe(command_topic.c_str());
        ESP_LOGI("MQTT", "Subscribed to command topic: %s", command_topic.c_str());

        for (int i = 0; i < currentSettings.numDataPoints; i++) {
          if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_MQTT && !currentSettings.dataPoints[i].mqttTopic.empty()) {
            mqttClient.subscribe(currentSettings.dataPoints[i].mqttTopic.c_str());
          }
        }
    } else {
      ESP_LOGE("MQTT", "Failed to connect, rc=%d. Will try again in 5 seconds.", mqttClient.state());
    }
  }
}

/**
 * @brief Callback function that is executed when an MQTT message is received.
 */
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message = "";
    message.reserve(length);
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    ESP_LOGI("MQTT", "Message arrived [%s] %s", topic, message.c_str());

    String topicStr = String(topic);
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/";
    bool stateChanged = false;
    
    // Check if it's a command topic
    if (topicStr.endsWith("/command")) {
        String component_topic = topicStr.substring(base_topic.length());
        String component = component_topic.substring(0, component_topic.indexOf('/'));

        // HA-ERROR-CHECK: Create a mutable copy of the payload for safe string parsing.
        char msg_copy[length + 1];
        strncpy(msg_copy, (char*)payload, length);
        msg_copy[length] = '\0';

        // Handle component commands with robust error checking
        if (component == "power") {
            if (message == "ON") isDisplayAsleep = false;
            else if (message == "OFF") isDisplayAsleep = true;
            stateChanged = true;
        } 
        else if (component == "brightness") {
            char* endptr;
            long val = strtol(msg_copy, &endptr, 10);
            if (*endptr != '\0') {
                ESP_LOGE("MQTT_ERROR", "Invalid brightness command (not a number): '%s'", msg_copy);
                return;
            }
            int brightness = (int)val;
            if (brightness >= 0 && brightness <= 7) {
                currentSettings.brightness = brightness;
                saveSettings();
                stateChanged = true;
            } else {
                ESP_LOGW("MQTT_WARN", "Brightness value out of range: %d", brightness);
            }
        }
        else if (component == "marquee") {
            isMarqueeOverrideActive = (message.length() > 0);
            marqueeOverrideMessage = message;
            stateChanged = true;
        }
        else if (component == "destination_year") {
            char* endptr;
            long val = strtol(msg_copy, &endptr, 10);
            if (*endptr != '\0') {
                ESP_LOGE("MQTT_ERROR", "Invalid year command (not a number): '%s'", msg_copy);
                return;
            }
            int year = (int)val;
            if (year >= 1000 && year <= 9999) {
                currentSettings.destinationYear = year;
                saveSettings();
                stateChanged = true;
            } else {
                ESP_LOGW("MQTT_WARN", "Destination year out of range: %d", year);
            }
        }
        else if (component == "override") {
            isMessageOverrideActive = (message == "ON");
            stateChanged = true;
        }
        else if (component == "override_text") {
            int first_newline = message.indexOf('\n');
            int second_newline = message.indexOf('\n', first_newline + 1);
            if (first_newline != -1) {
                overrideMessageLine1 = message.substring(0, first_newline);
                if (second_newline != -1) {
                    overrideMessageLine2 = message.substring(first_newline + 1, second_newline);
                    overrideMessageLine3 = message.substring(second_newline + 1);
                } else {
                    overrideMessageLine2 = message.substring(first_newline + 1);
                    overrideMessageLine3 = "";
                }
            } else {
                overrideMessageLine1 = message;
                overrideMessageLine2 = "";
                overrideMessageLine3 = "";
            }
            stateChanged = true;
        }
        else if (component == "animation" && message == "START") {
            startTimeTravelAnimation();
        }
    }
    // Fallback to check for data point topics if no command topic matched
    else {
        for (int i = 0; i < currentSettings.numDataPoints; i++) {
            if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_MQTT && topicStr == currentSettings.dataPoints[i].mqttTopic.c_str()) {
                // Your existing data point logic here...
                break;
            }
        }
    }
    if (stateChanged) {
        publishAllHaStates();
    }
}

/**
 * @brief HA-MARQUEE: New display function for the marquee override mode.
 */
void displayMarqueeOverride() {
    #if ENABLE_HARDWARE
    String textToDisplay = marqueeOverrideMessage;
    
    if (textToDisplay.length() > 13) {
        textToDisplay = "  " + textToDisplay + "  ";
    }

    static unsigned long lastScrollTime = 0;
    static int scrollPosition = 0;

    if (millis() - lastScrollTime > currentSettings.dataPoints[currentPageIndex].scrollSpeed) { // Scroll Speed
        lastScrollTime = millis();
        
        String viewport = textToDisplay.substring(scrollPosition, scrollPosition + 13);
        
        printToDisplay(lastRow.month, viewport.substring(0, 3).c_str(), 0);
        printToDisplay(lastRow.day, viewport.substring(3, 5).c_str(), 0);
        printToDisplay(lastRow.year, viewport.substring(5, 9).c_str(), 0);
        printToDisplay(lastRow.time, viewport.substring(9, 13).c_str(), 0);

        lastRow.month.writeDisplay();
        lastRow.day.writeDisplay();
        lastRow.year.writeDisplay();
        lastRow.time.writeDisplay();

        if (textToDisplay.length() > 13) {
            scrollPosition++;
            if (scrollPosition > textToDisplay.length() - 13) {
                scrollPosition = 0;
            }
        } else {
            scrollPosition = 0;
        }
    }
    #endif
}

/**
 * @brief HA-ENHANCEMENT: New display function for message override mode.
 */
void displayOverrideMessage() {
    #if ENABLE_HARDWARE
    // Display the override message, splitting it across the three rows.
    // Line 1 on Destination Row (top)
    printToDisplay(destRow.month, overrideMessageLine1.substring(0, 3).c_str(), 1);
    printToDisplay(destRow.day, overrideMessageLine1.substring(3, 5).c_str(), 2);
    printToDisplay(destRow.year, overrideMessageLine1.substring(5, 9).c_str());
    printToDisplay(destRow.time, overrideMessageLine1.substring(9, 13).c_str());
    destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();

    // Line 2 on Present Row (middle)
    printToDisplay(presRow.month, overrideMessageLine2.substring(0, 3).c_str(), 1);
    printToDisplay(presRow.day, overrideMessageLine2.substring(3, 5).c_str(), 2);
    printToDisplay(presRow.year, overrideMessageLine2.substring(5, 9).c_str());
    printToDisplay(presRow.time, overrideMessageLine2.substring(9, 13).c_str());
    presRow.month.writeDisplay(); presRow.day.writeDisplay(); presRow.year.writeDisplay(); presRow.time.writeDisplay();

    // Line 3 on Last Departed Row (bottom)
    printToDisplay(lastRow.month, overrideMessageLine3.substring(0, 3).c_str(), 1);
    printToDisplay(lastRow.day, overrideMessageLine3.substring(3, 5).c_str(), 2);
    printToDisplay(lastRow.year, overrideMessageLine3.substring(5, 9).c_str());
    printToDisplay(lastRow.time, overrideMessageLine3.substring(9, 13).c_str());
    lastRow.month.writeDisplay(); lastRow.day.writeDisplay(); lastRow.year.writeDisplay(); lastRow.time.writeDisplay();
    #endif
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
          int speed = 88 * pow(progress, 2.5); 
          if (speed > 88) speed = 88;
          
          displaySpeed(speed);
          
          animateTemporalLockOn(destRow, animDestTimeInfo, 1955);
          animateTemporalLockOn(presRow, animPresTimeInfo, 1985);
          
          lastAnimationFrameTime = currentTime;
      }
      if (elapsed >= ACCELERATION_DURATION) {
          flashAllDisplays();
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
          animateAllRowsTimelineSkim(time_blur_elapsed, TIME_BLUR_DURATION, 1955);
          lastAnimationFrameTime = currentTime;
      }
      if (elapsed >= (ACCELERATION_DURATION + WHITE_FLASH_DURATION + FLICKER_DURATION + TIME_BLUR_DURATION)) {
          currentPhase = ANIM_ARRIVAL;
      }
      break;

    case ANIM_ARRIVAL:
      updateDisplayRow(presRow, animDestTimeInfo, 1955);
      if(currentSettings.timeTravelSoundToggle) playSound("ARRIVAL_THUD");
      currentPhase = ANIM_LANDING;
      break;

    case ANIM_LANDING:
      if (elapsed >= TOTAL_DURATION) {
          currentSettings.lastTimeDepartedYear = realDepartureTimeInfo.tm_year + 1900;
          currentSettings.lastTimeDepartedMonth = realDepartureTimeInfo.tm_mon + 1;
          currentSettings.lastTimeDepartedDay = realDepartureTimeInfo.tm_mday;
          currentSettings.lastTimeDepartedHour = realDepartureTimeInfo.tm_hour;
          currentSettings.lastTimeDepartedMinute = realDepartureTimeInfo.tm_min;
          
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

/**
 * @brief Handles a post-time-travel effect where the "Present Time" display occasionally flickers to show the "Last Time Departed".
 */
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
      if (elapsed < 3000) {
        if (millis() - lastAnimationFrameTime > 100) {
          printToDisplay(destRow.month, "888", 1);
          printToDisplay(destRow.day, "88", 2);
          printToDisplay(destRow.year, "8888");
          printToDisplay(destRow.time, "8888");
          destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();
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
  
  int now_minutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  int sleep_minutes = currentSettings.departureHour * 60 + currentSettings.departureMinute;
  int wake_minutes = currentSettings.arrivalHour * 60 + currentSettings.arrivalMinute;

  bool shouldBeAsleep = (sleep_minutes < wake_minutes) ?
                        (now_minutes >= sleep_minutes && now_minutes < wake_minutes) : // Same-day sleep window.
                        (now_minutes >= sleep_minutes || now_minutes < wake_minutes); // Overnight sleep window.

  // Get the correct base topic for publishing state.
  String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;

  if (shouldBeAsleep && !isDisplayAsleep) {
    isDisplayAsleep = true;
    #if ENABLE_HARDWARE
    blankAllDisplays();
    playSound("SLEEP_ON");
    #endif
    // HA-IMPROVEMENT: Report state change with the CORRECT topic
    mqttClient.publish((base_topic + "/power/state").c_str(), "OFF", true);
    updateHaStatus("Asleep");
  } else if (!shouldBeAsleep && isDisplayAsleep) {
    isDisplayAsleep = false;
    #if ENABLE_HARDWARE
    updateNormalClockDisplay();
    playSound("CONFIRM_ON");
    #endif
    // HA-IMPROVEMENT: Report state change with the CORRECT topic
    mqttClient.publish((base_topic + "/power/state").c_str(), "ON", true);
    updateHaStatus("Idle");
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
    
    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
    tzset();
    localtime_r(&now, &timeinfo);
    updateDisplayRow(presRow, timeinfo, timeinfo.tm_year + 1900);
    
    setenv("TZ", TZ_DATA[currentSettings.destinationTimezoneIndex].tzString, 1);
    tzset();
    localtime_r(&now, &timeinfo);
    updateDisplayRow(destRow, timeinfo, currentSettings.destinationYear);
    
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
            printToDisplay(lastRow.month, "WEA", 1);
            printToDisplay(lastRow.day, "TH", 2);
            printToDisplay(lastRow.year, "ER");
            printToDisplay(lastRow.time, "----");
        } else {
            static int weatherPage = 0;
            static unsigned long lastPageChange = 0;
            char buffer[6];
            if (millis() - lastPageChange > 4000) {
                weatherPage = (weatherPage + 1) % 4;
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
                    strcat(buffer, "M");
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

/**
 * @brief Handles the state machine for the Data Link marquee display.
 */
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
        
        xSemaphoreGive(xDisplayDataMutex);

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