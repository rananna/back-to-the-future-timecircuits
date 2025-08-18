// Forcing a recompile to resolve build cache issues.
#include "web_server.h"
#include "api_templates.h"
#include <AsyncJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <string>

AsyncWebSocket ws("/ws");

// Declare the new function that will be defined in the .ino file
void forceFetchWeatherDataTask(void* p);

// This function runs in a separate task to prevent blocking
void makeApiRequestTask(void* p) {
    ApiTestParams* params = (ApiTestParams*)p;
    String urlStr = params->url;
    String authKey = params->authKey;
    String authValue = params->authValue;
    uint32_t clientId = params->clientId;
    delete params; // Clean up the params object immediately

    HTTPClient http;
    WiFiClientSecure client;
    
    client.setInsecure();

    if (http.begin(client, urlStr)) {
        if (authKey.length() > 0 && authValue.length() > 0) {
            http.addHeader(authKey, authValue);
        }
        
        int httpCode = http.GET();
        String responseString;
        DynamicJsonDocument responseJson(4096);
        responseJson["action"] = "apiResult";

        if (httpCode > 0) {
            if (httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                DynamicJsonDocument payloadDoc(4096);
                DeserializationError error = deserializeJson(payloadDoc, payload);
                if (error == DeserializationError::Ok) {
                    responseJson["status"] = "success";
                    responseJson["payload"] = payloadDoc.as<JsonVariant>();
                } else {
                    responseJson["status"] = "error";
                    responseJson["payload"] = "JSON Parsing Failed: " + String(error.c_str());
                }
            } else {
                responseJson["status"] = "error";
                responseJson["payload"] = "HTTP Error: " + String(httpCode);
            }
        } else {
            responseJson["status"] = "error";
            responseJson["payload"] = "Request Failed: " + http.errorToString(httpCode);
        }
        
        http.end();
        serializeJson(responseJson, responseString);
        ws.text(clientId, responseString);
    } else {
        ESP_LOGE("API_TASK", "Unable to connect to %s", urlStr.c_str());
    }

    vTaskDelete(NULL); // End the task
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        ESP_LOGI("WebSocket", "Client #%u connected from %s", client->id(), client->remoteIP().toString().c_str());
    } else if (type == WS_EVT_DISCONNECT) {
        ESP_LOGI("WebSocket", "Client #%u disconnected", client->id());
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            data[len] = 0;
            
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, (char*)data);

            String action = doc["action"];
            if (action == "testApi") {
                if (!timeSynchronized) {
                    ESP_LOGE("WebSocket", "Time not sync'd, API call aborted.");
                    String responseString;
                    DynamicJsonDocument responseJson(256);
                    responseJson["action"] = "apiResult";
                    responseJson["status"] = "error";
                    responseJson["payload"] = "Time not sync'd. Go to System->Sync Time.";
                    serializeJson(responseJson, responseString);
                    ws.text(client->id(), responseString);
                    return;
                }

                String url = doc["data"]["url"];
                String authKey = doc["data"]["authKey"];
                String authValue = doc["data"]["authValue"];

                ApiTestParams* params = new ApiTestParams{url, authKey, authValue, client->id()};

                BaseType_t taskCreated = xTaskCreate(makeApiRequestTask, "apiTestTask", 8192, params, 1, NULL);
                if (taskCreated != pdPASS) {
                    ESP_LOGE("WebSocket", "Failed to create API test task. Deleting params to prevent leak.");
                    delete params;
                }
            }
        }
    }
}

void setupWebRoutes() {
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(LittleFS, "/index.html", "text/html"); });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(LittleFS, "/style.css", "text/css"); });
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(LittleFS, "/script.js", "application/javascript"); });
  server.on("/api/isReady", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(200, "text/plain", "READY"); });
  
  server.on("/api/greatScott", HTTP_POST, [](AsyncWebServerRequest *request){
    #if ENABLE_HARDWARE
    playSound("EASTER_EGG");
    #endif
    request->send(200, "text/plain", "Great Scott!");
  });
  server.on("/api/settings/timecircuits", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<256> doc;
    doc["destinationYear"] = currentSettings.destinationYear;
    doc["destinationTimezoneIndex"] = currentSettings.destinationTimezoneIndex;
    doc["lastTimeDepartedYear"] = currentSettings.lastTimeDepartedYear;
    doc["lastTimeDepartedMonth"] = currentSettings.lastTimeDepartedMonth;
    doc["lastTimeDepartedDay"] = currentSettings.lastTimeDepartedDay;
    doc["lastTimeDepartedHour"] = currentSettings.lastTimeDepartedHour;
    doc["lastTimeDepartedMinute"] = currentSettings.lastTimeDepartedMinute;
    doc["presentTimezoneIndex"] = currentSettings.presentTimezoneIndex;
    String jsonString;
    serializeJson(doc, jsonString);
    request->send(200, "application/json", jsonString);
  });
  server.on("/api/settings/temporal", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<384> doc;
    doc["departureHour"] = currentSettings.departureHour;
    doc["departureMinute"] = currentSettings.departureMinute;
    doc["arrivalHour"] = currentSettings.arrivalHour;
    doc["arrivalMinute"] = currentSettings.arrivalMinute;
    doc["brightness"] = currentSettings.brightness;
    doc["notificationVolume"] = currentSettings.notificationVolume;
    doc["timeTravelAnimationDuration"] = currentSettings.timeTravelAnimationDuration;
    doc["timeTravelAnimationInterval"] = currentSettings.timeTravelAnimationInterval;
    doc["animationStyle"] = currentSettings.animationStyle;
    doc["glitchEffectFrequency"] = currentSettings.glitchEffectFrequency;
    doc["malfunctionFrequency"] = currentSettings.malfunctionFrequency;
    doc["timeTravelSoundToggle"] = currentSettings.timeTravelSoundToggle;
    doc["timeTravelVolumeFade"] = currentSettings.timeTravelVolumeFade;
    doc["presetCycleInterval"] = currentSettings.presetCycleInterval;
    doc["displayFormat24h"] = currentSettings.displayFormat24h;
    String jsonString;
    serializeJson(doc, jsonString);
    request->send(200, "application/json", jsonString);
  });
  server.on("/api/settings/datalink", HTTP_GET, [](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(2048);
    doc["dataLinkEnabled"] = currentSettings.dataLinkEnabled;
    doc["dataLinkRefreshInterval"] = currentSettings.dataLinkRefreshInterval;
    doc["numDataPoints"] = currentSettings.numDataPoints;
    doc["mqttBroker"] = currentSettings.mqttBroker.c_str();
    doc["mqttPort"] = currentSettings.mqttPort;
    doc["mqttUser"] = currentSettings.mqttUser.c_str();
    doc["mqttPassword"] = currentSettings.mqttPassword.c_str();
    doc["weatherModeEnabled"] = currentSettings.weatherModeEnabled;
    doc["cityName"] = currentSettings.cityName.c_str();
    doc["useMetricUnits"] = currentSettings.useMetricUnits;

    JsonArray dataPoints = doc.createNestedArray("dataPoints");
    for (int i = 0; i < currentSettings.numDataPoints; i++) {
        JsonObject dp = dataPoints.createNestedObject();
        dp["url"] = currentSettings.dataPoints[i].url.c_str();
        dp["monthPath"] = currentSettings.dataPoints[i].monthPath.c_str();
        dp["dayPath"] = currentSettings.dataPoints[i].dayPath.c_str();
        dp["yearPath"] = currentSettings.dataPoints[i].yearPath.c_str();
        dp["timePath"] = currentSettings.dataPoints[i].timePath.c_str();
        dp["prefix"] = currentSettings.dataPoints[i].prefix.c_str();
        dp["suffix"] = currentSettings.dataPoints[i].suffix.c_str();
        dp["icon"] = currentSettings.dataPoints[i].icon.c_str();
        dp["scrollSpeed"] = currentSettings.dataPoints[i].scrollSpeed;
        dp["dataSourceType"] = (int)currentSettings.dataPoints[i].dataSourceType;
        dp["mqttTopic"] = currentSettings.dataPoints[i].mqttTopic.c_str();
        dp["yearPrefix"] = currentSettings.dataPoints[i].yearPrefix.c_str();
        dp["yearSuffix"] = currentSettings.dataPoints[i].yearSuffix.c_str();
        dp["displayMode"] = (int)currentSettings.dataPoints[i].displayMode;
        dp["scrollingText"] = currentSettings.dataPoints[i].scrollingText.c_str();
        dp["authHeaderKey"] = currentSettings.dataPoints[i].authHeaderKey.c_str();
        dp["authHeaderValue"] = currentSettings.dataPoints[i].authHeaderValue.c_str();
        dp["apiExampleKey"] = currentSettings.dataPoints[i].apiExampleKey.c_str();
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });
  server.on("/api/timezones", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "application/json", TZ_JSON);
  });
  server.on("/api/getPresets", HTTP_GET, [](AsyncWebServerRequest *request) {
    preferences.begin(PREFERENCES_NAMESPACE, true);
    String presets = preferences.getString("customPresets", "[]");
    preferences.end();
    request->send(200, "application/json", presets);
  });
  server.on("/api/time", HTTP_GET, [](AsyncWebServerRequest *request) {
    time_t now;
    time(&now);
    StaticJsonDocument<128> doc;
    doc["unixTime"] = now;
    doc["timeSynchronized"] = timeSynchronized;
    String jsonString;
    serializeJson(doc, jsonString);
    request->send(200, "application/json", jsonString);
  });
  server.on("/api/weather", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (currentWeatherData.dataValid) {
        StaticJsonDocument<256> doc;
        doc["temperature"] = currentWeatherData.temperature;
        doc["apparentTemperature"] = currentWeatherData.apparentTemperature;
        doc["windSpeed"] = currentWeatherData.windSpeed;
        doc["humidity"] = currentWeatherData.humidity;
        doc["weatherCode"] = currentWeatherData.weatherCode;
        doc["dailyHigh"] = currentWeatherData.dailyHigh;
        doc["dailyLow"] = currentWeatherData.dailyLow;
        String jsonString;
        serializeJson(doc, jsonString);
        request->send(200, "application/json", jsonString);
    } else {
        request->send(503, "application/json", "{\"error\":\"Weather data not available\"}");
    }
  });
  server.on("/api/weather/refresh", HTTP_POST, [](AsyncWebServerRequest *request){
    xTaskCreate(forceFetchWeatherDataTask, "forceFetchWeatherDataTask", 8192, NULL, 1, NULL);
    request->send(202, "text/plain", "Weather refresh triggered");
  });
  
  // New JSON handler for saving settings
  AsyncCallbackJsonWebHandler* saveSettingsHandler = new AsyncCallbackJsonWebHandler("/api/saveSettings", [](AsyncWebServerRequest *request, JsonVariant &json) {
    ESP_LOGI("SAVE_SETTINGS", "Received save request. Free heap: %u", ESP.getFreeHeap());
    JsonObject obj = json.as<JsonObject>();

    std::string oldMqttBroker = currentSettings.mqttBroker;
    int oldMqttPort = currentSettings.mqttPort;

    // Time Circuits & Temporal
    ESP_LOGI("SAVE_SETTINGS", "Parsing temporal settings...");
    currentSettings.destinationYear = obj["destinationYear"] | currentSettings.destinationYear;
    currentSettings.destinationTimezoneIndex = obj["destinationTimezoneIndex"] | currentSettings.destinationTimezoneIndex;
    currentSettings.lastTimeDepartedYear = obj["lastTimeDepartedYear"] | currentSettings.lastTimeDepartedYear;
    currentSettings.lastTimeDepartedMonth = obj["lastTimeDepartedMonth"] | currentSettings.lastTimeDepartedMonth;
    currentSettings.lastTimeDepartedDay = obj["lastTimeDepartedDay"] | currentSettings.lastTimeDepartedDay;
    currentSettings.lastTimeDepartedHour = obj["lastTimeDepartedHour"] | currentSettings.lastTimeDepartedHour;
    currentSettings.lastTimeDepartedMinute = obj["lastTimeDepartedMinute"] | currentSettings.lastTimeDepartedMinute;
    currentSettings.presetCycleInterval = obj["presetCycleInterval"] | currentSettings.presetCycleInterval;
    currentSettings.departureHour = obj["departureHour"] | currentSettings.departureHour;
    currentSettings.departureMinute = obj["departureMinute"] | currentSettings.departureMinute;
    currentSettings.arrivalHour = obj["arrivalHour"] | currentSettings.arrivalHour;
    currentSettings.arrivalMinute = obj["arrivalMinute"] | currentSettings.arrivalMinute;
    currentSettings.brightness = obj["brightness"] | currentSettings.brightness;
    currentSettings.timeTravelAnimationDuration = obj["timeTravelAnimationDuration"] | currentSettings.timeTravelAnimationDuration;
    currentSettings.timeTravelAnimationInterval = obj["timeTravelAnimationInterval"] | currentSettings.timeTravelAnimationInterval;
    currentSettings.animationStyle = obj["animationStyle"] | currentSettings.animationStyle;
    currentSettings.glitchEffectFrequency = obj["glitchEffectFrequency"] | currentSettings.glitchEffectFrequency;
    currentSettings.malfunctionFrequency = obj["malfunctionFrequency"] | currentSettings.malfunctionFrequency;
    currentSettings.notificationVolume = obj["notificationVolume"] | currentSettings.notificationVolume;
    currentSettings.timeTravelSoundToggle = obj["timeTravelSoundToggle"] | currentSettings.timeTravelSoundToggle;
    currentSettings.timeTravelVolumeFade = obj["timeTravelVolumeFade"] | currentSettings.timeTravelVolumeFade;
    currentSettings.presentTimezoneIndex = obj["presentTimezoneIndex"] | currentSettings.presentTimezoneIndex;
    currentSettings.displayFormat24h = obj["displayFormat24h"] | currentSettings.displayFormat24h;

    // Data Link & Weather
    ESP_LOGI("SAVE_SETTINGS", "Parsing Data Link settings...");
    currentSettings.dataLinkEnabled = obj["dataLinkEnabled"] | currentSettings.dataLinkEnabled;
    currentSettings.dataLinkRefreshInterval = obj["dataLinkRefreshInterval"] | currentSettings.dataLinkRefreshInterval;
    if (obj.containsKey("mqttBroker")) currentSettings.mqttBroker = obj["mqttBroker"].as<std::string>();
    currentSettings.mqttPort = obj["mqttPort"] | 1883;
    if (obj.containsKey("mqttUser")) currentSettings.mqttUser = obj["mqttUser"].as<std::string>();
    if (obj.containsKey("mqttPassword")) currentSettings.mqttPassword = obj["mqttPassword"].as<std::string>();
    currentSettings.weatherModeEnabled = obj["weatherModeEnabled"] | currentSettings.weatherModeEnabled;
    if (obj.containsKey("cityName")) currentSettings.cityName = obj["cityName"].as<std::string>();
    currentSettings.useMetricUnits = obj["useMetricUnits"] | currentSettings.useMetricUnits;

    // Data Points
    ESP_LOGI("SAVE_SETTINGS", "Parsing Data Points...");
    currentSettings.numDataPoints = obj["numDataPoints"] | 0;
    if (obj.containsKey("dataPoints")) {
        JsonArray dataPoints = obj["dataPoints"];
        for (int i = 0; i < 5; i++) {
            if (i < currentSettings.numDataPoints && i < dataPoints.size()) {
                JsonObject dp = dataPoints[i];
                if (dp.containsKey("dataSourceType")) currentSettings.dataPoints[i].dataSourceType = (DataSourceType)(dp["dataSourceType"].as<int>());
                if (dp.containsKey("url")) currentSettings.dataPoints[i].url = dp["url"].as<std::string>();
                if (dp.containsKey("monthPath")) currentSettings.dataPoints[i].monthPath = dp["monthPath"].as<std::string>();
                if (dp.containsKey("dayPath")) currentSettings.dataPoints[i].dayPath = dp["dayPath"].as<std::string>();
                if (dp.containsKey("yearPath")) currentSettings.dataPoints[i].yearPath = dp["yearPath"].as<std::string>();
                if (dp.containsKey("timePath")) currentSettings.dataPoints[i].timePath = dp["timePath"].as<std::string>();
                if (dp.containsKey("prefix")) currentSettings.dataPoints[i].prefix = dp["prefix"].as<std::string>();
                if (dp.containsKey("suffix")) currentSettings.dataPoints[i].suffix = dp["suffix"].as<std::string>();
                if (dp.containsKey("icon")) currentSettings.dataPoints[i].icon = dp["icon"].as<std::string>();
                currentSettings.dataPoints[i].scrollSpeed = dp["scrollSpeed"] | 150;
                if (dp.containsKey("mqttTopic")) currentSettings.dataPoints[i].mqttTopic = dp["mqttTopic"].as<std::string>();
                if (dp.containsKey("yearPrefix")) currentSettings.dataPoints[i].yearPrefix = dp["yearPrefix"].as<std::string>();
                if (dp.containsKey("yearSuffix")) currentSettings.dataPoints[i].yearSuffix = dp["yearSuffix"].as<std::string>();
                if (dp.containsKey("displayMode")) currentSettings.dataPoints[i].displayMode = (DisplayMode)(dp["displayMode"].as<int>());
                if (dp.containsKey("scrollingText")) currentSettings.dataPoints[i].scrollingText = dp["scrollingText"].as<std::string>();
                if (dp.containsKey("authHeaderKey")) currentSettings.dataPoints[i].authHeaderKey = dp["authHeaderKey"].as<std::string>();
                if (dp.containsKey("authHeaderValue")) currentSettings.dataPoints[i].authHeaderValue = dp["authHeaderValue"].as<std::string>();
                if (dp.containsKey("apiExampleKey")) currentSettings.dataPoints[i].apiExampleKey = dp["apiExampleKey"].as<std::string>();
            } else {
                currentSettings.dataPoints[i] = {}; // Clear unused data points
            }
        }
    }
    ESP_LOGI("SAVE_SETTINGS", "Parsing complete. Calling saveSettings()...");
    saveSettings();
    ESP_LOGI("SAVE_SETTINGS", "saveSettings() returned. Free heap: %u", ESP.getFreeHeap());

    if (oldMqttBroker != currentSettings.mqttBroker || oldMqttPort != currentSettings.mqttPort) {
        if (mqttClient.connected()) {
            mqttClient.disconnect();
        }
        mqttReconnectRequired = true;
    }

    #if ENABLE_HARDWARE
    myDFPlayer.volume(currentSettings.notificationVolume);
    #endif

    request->send(200, "text/plain", "Settings Saved!");
    ESP_LOGI("SAVE_SETTINGS", "Save request handler finished.");
  });
  server.addHandler(saveSettingsHandler);

  server.on("/api/triggerAnimation", HTTP_POST, [](AsyncWebServerRequest *request){
    ESP_LOGI("ANIMATION", "Animation triggered via API.");
    startTimeTravelAnimation();
    request->send(200, "text/plain", "Animation triggered!");
  });

  server.on("/api/addPreset", HTTP_POST, [](AsyncWebServerRequest *request){
    preferences.begin(PREFERENCES_NAMESPACE, false);
    String presetsJson = preferences.getString("customPresets", "[]");
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, presetsJson);
    if (error) {
        request->send(500, "text/plain", "Failed to parse presets");
        preferences.end();
        return;
    }
    JsonArray presets = doc.as<JsonArray>();
    JsonObject newPreset = presets.createNestedObject();
    newPreset["name"] = request->getParam("name", true)->value();
    newPreset["value"] = request->getParam("value", true)->value();

    String newPresetsJson;
    serializeJson(doc, newPresetsJson);
    preferences.putString("customPresets", newPresetsJson);
    preferences.end();
    request->send(200, "text/plain", "Custom preset saved!");
  });
  server.on("/api/updatePreset", HTTP_POST, [](AsyncWebServerRequest *request){
    preferences.begin(PREFERENCES_NAMESPACE, false);
    String name = request->getParam("name", true)->value();
    String newName = request->getParam("newName", true)->value();
    String value = request->getParam("value", true)->value();
    String presetsJson = preferences.getString("customPresets", "[]");
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, presetsJson);
    JsonArray presets = doc.as<JsonArray>();
    for (JsonObject preset : presets) {
        if (preset["name"] == name) {
            preset["name"] = newName;
            preset["value"] = value;
            break;
        }
    }
    String newPresetsJson;
    serializeJson(doc, newPresetsJson);
    preferences.putString("customPresets", newPresetsJson);
    preferences.end();
    request->send(200, "text/plain", "Preset updated!");
  });
  server.on("/api/deletePreset", HTTP_POST, [](AsyncWebServerRequest *request){
    preferences.begin(PREFERENCES_NAMESPACE, false);
    String name = request->getParam("name", true)->value();
    String presetsJson = preferences.getString("customPresets", "[]");
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, presetsJson);
    JsonArray presets = doc.as<JsonArray>();
    for (int i = 0; i < presets.size(); i++) {
        if (presets[i]["name"] == name) {
            presets.remove(i);
            break;
        }
    }
    String newPresetsJson;
    serializeJson(doc, newPresetsJson);
    preferences.putString("customPresets", newPresetsJson);
    preferences.end();
    request->send(200, "text/plain", "Preset deleted!");
  });
  server.on("/api/resetSettings", HTTP_POST, [](AsyncWebServerRequest *request){
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.clear();
    preferences.end();
    preferences.remove("customPresets");
    preferences.remove(THEME_PREF_KEY);
    LittleFS.remove("/api_templates.json");
    loadSettings();
    request->send(200, "text/plain", "Settings have been reset to default.");
  });
  server.on("/api/syncTime", HTTP_POST, [](AsyncWebServerRequest *request){
    ntpSyncRequested = true;
    request->send(200, "text/plain", "NTP time sync requested.");
  });
  server.on("/api/getTheme", HTTP_GET, [](AsyncWebServerRequest *request){
      preferences.begin(PREFERENCES_NAMESPACE, true);
      String themeName = preferences.getString(THEME_PREF_KEY, "theme-time-circuits");
      preferences.end();
      request->send(200, "text/plain", themeName);
  });
  server.on("/api/api_examples", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "application/json", apiTemplates);
  });
  server.on("/api/setTheme", HTTP_POST, [](AsyncWebServerRequest *request){
    String theme = request->getParam("theme", true)->value();

    int themeEnum = THEME_TIME_CIRCUITS;
    if (theme == "theme-outatime") themeEnum = THEME_OUTATIME;
    else if (theme == "theme-88mph") themeEnum = THEME_88MPH;
    else if (theme == "theme-plutonium-glow") themeEnum = THEME_PLUTONIUM_GLOW;
    else if (theme == "theme-mr-fusion") themeEnum = THEME_MR_FUSION;
    else if (theme == "theme-clock-tower") themeEnum = THEME_CLOCK_TOWER;

    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.putString(THEME_PREF_KEY, theme);
    preferences.putInt("theme", themeEnum);
    preferences.end();

    currentSettings.theme = themeEnum;

    request->send(200, "text/plain", "Theme saved.");
  });
  
  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(404, "text/plain", "Not found");
  });
}