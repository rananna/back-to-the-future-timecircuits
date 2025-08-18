// Forcing a recompile to resolve build cache issues.
#include "web_server.h"
#include "api_templates.h"
#include <AsyncJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <string>

AsyncWebSocket ws("/ws");
WeatherData currentWeatherData; // <-- ADD THIS LINE

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
    doc["dataLinkTargetRow"] = currentSettings.dataLinkTargetRow;
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
  server.on("/api/saveSettings", HTTP_POST, [](AsyncWebServerRequest *request){
    auto getParamInt = [&](const String& name, int defaultValue) -> int {
        if (request->hasParam(name, true)) return request->getParam(name, true)->value().toInt();
        return defaultValue;
    };
    auto getParamValue = [&](const String& name) -> String {
        if (request->hasParam(name, true)) return request->getParam(name, true)->value();
        return "";
    };
    auto getParamFloat = [&](const String& name, float defaultValue) -> float {
        if (request->hasParam(name, true)) return request->getParam(name, true)->value().toFloat();
        return defaultValue;
    };

    std::string oldMqttBroker = currentSettings.mqttBroker;
    int oldMqttPort = currentSettings.mqttPort;

    currentSettings.destinationYear = getParamInt("destinationYear", currentSettings.destinationYear);
    currentSettings.destinationTimezoneIndex = getParamInt("destinationTimezoneIndex", currentSettings.destinationTimezoneIndex);

    if (request->hasParam("lastTimeDepartedYear", true)) {
        currentSettings.lastTimeDepartedYear = getParamInt("lastTimeDepartedYear", currentSettings.lastTimeDepartedYear);
        currentSettings.lastTimeDepartedMonth = getParamInt("lastTimeDepartedMonth", currentSettings.lastTimeDepartedMonth);
        currentSettings.lastTimeDepartedDay = getParamInt("lastTimeDepartedDay", currentSettings.lastTimeDepartedDay);
        currentSettings.lastTimeDepartedHour = getParamInt("lastTimeDepartedHour", currentSettings.lastTimeDepartedHour);
        currentSettings.lastTimeDepartedMinute = getParamInt("lastTimeDepartedMinute", currentSettings.lastTimeDepartedMinute);
    }
    currentSettings.presetCycleInterval = getParamInt("presetCycleInterval", currentSettings.presetCycleInterval);
    currentSettings.departureHour = getParamInt("departureHour", currentSettings.departureHour);
    currentSettings.departureMinute = getParamInt("departureMinute", currentSettings.departureMinute);
    currentSettings.arrivalHour = getParamInt("arrivalHour", currentSettings.arrivalHour);
    currentSettings.arrivalMinute = getParamInt("arrivalMinute", currentSettings.arrivalMinute);
    currentSettings.brightness = getParamInt("brightness", currentSettings.brightness);
    currentSettings.timeTravelAnimationDuration = getParamInt("timeTravelAnimationDuration", currentSettings.timeTravelAnimationDuration);
    currentSettings.timeTravelAnimationInterval = getParamInt("timeTravelAnimationInterval", currentSettings.timeTravelAnimationInterval);
    currentSettings.animationStyle = getParamInt("animationStyle", currentSettings.animationStyle);
    currentSettings.glitchEffectFrequency = getParamInt("glitchEffectFrequency", currentSettings.glitchEffectFrequency);
    currentSettings.malfunctionFrequency = getParamInt("malfunctionFrequency", currentSettings.malfunctionFrequency);
    currentSettings.notificationVolume = getParamInt("notificationVolume", currentSettings.notificationVolume);
    currentSettings.timeTravelSoundToggle = (getParamValue("timeTravelSoundToggle") == "true");
    currentSettings.timeTravelVolumeFade = (getParamValue("timeTravelVolumeFade") == "true");
    currentSettings.presentTimezoneIndex = getParamInt("presentTimezoneIndex", currentSettings.presentTimezoneIndex);
    currentSettings.displayFormat24h = (getParamValue("displayFormat24h") == "true");

    currentSettings.dataLinkEnabled = (getParamValue("dataLinkEnabled") == "true");
    currentSettings.dataLinkTargetRow = getParamInt("dataLinkTargetRow", currentSettings.dataLinkTargetRow);
    currentSettings.dataLinkRefreshInterval = getParamInt("dataLinkRefreshInterval", currentSettings.dataLinkRefreshInterval);
    
    currentSettings.mqttBroker = getParamValue("mqttBroker").c_str();
    currentSettings.mqttPort = getParamInt("mqttPort", 1883);
    currentSettings.mqttUser = getParamValue("mqttUser").c_str();
    currentSettings.mqttPassword = getParamValue("mqttPassword").c_str();

    currentSettings.weatherModeEnabled = (getParamValue("weatherModeEnabled") == "true");
    currentSettings.cityName = getParamValue("cityName").c_str();
    currentSettings.useMetricUnits = (getParamValue("useMetricUnits") == "true");

    if (request->hasParam("numDataPoints", true)) {
        int numDataPoints = getParamInt("numDataPoints", 0);
        if (numDataPoints > 5) numDataPoints = 5;
        currentSettings.numDataPoints = numDataPoints;
        for (int i = 0; i < 5; i++) {
            if (i < numDataPoints) {
                currentSettings.dataPoints[i].dataSourceType = (DataSourceType)getParamInt("dp_dataSourceType_" + String(i), 0);
                currentSettings.dataPoints[i].url = getParamValue("dp_url_" + String(i)).c_str();
                currentSettings.dataPoints[i].monthPath = getParamValue("dp_monthPath_" + String(i)).c_str();
                currentSettings.dataPoints[i].dayPath = getParamValue("dp_dayPath_" + String(i)).c_str();
                currentSettings.dataPoints[i].yearPath = getParamValue("dp_yearPath_" + String(i)).c_str();
                currentSettings.dataPoints[i].timePath = getParamValue("dp_timePath_" + String(i)).c_str();
                currentSettings.dataPoints[i].prefix = getParamValue("dp_prefix_" + String(i)).c_str();
                currentSettings.dataPoints[i].suffix = getParamValue("dp_suffix_" + String(i)).c_str();
                currentSettings.dataPoints[i].icon = getParamValue("dp_icon_" + String(i)).c_str();
                currentSettings.dataPoints[i].scrollSpeed = getParamInt("dp_scrollSpeed_" + String(i), 150);
                currentSettings.dataPoints[i].mqttTopic = getParamValue("dp_mqttTopic_" + String(i)).c_str();
                currentSettings.dataPoints[i].yearPrefix = getParamValue("dp_yearPrefix_" + String(i)).c_str();
                currentSettings.dataPoints[i].yearSuffix = getParamValue("dp_yearSuffix_" + String(i)).c_str();
                currentSettings.dataPoints[i].displayMode = (DisplayMode)getParamInt("dp_displayMode_" + String(i), 0);
                currentSettings.dataPoints[i].scrollingText = getParamValue("dp_scrollingText_" + String(i)).c_str();
                currentSettings.dataPoints[i].authHeaderKey = getParamValue("dp_authHeaderKey_" + String(i)).c_str();
                currentSettings.dataPoints[i].authHeaderValue = getParamValue("dp_authHeaderValue_" + String(i)).c_str();
                currentSettings.dataPoints[i].apiExampleKey = getParamValue("dp_apiExampleKey_" + String(i)).c_str();
            } else {
                currentSettings.dataPoints[i] = {};
                preferences.begin(PREFERENCES_NAMESPACE, false);
                String prefix = "dp" + String(i) + "_";
                preferences.remove((prefix + "url").c_str());
                preferences.remove((prefix + "monthPath").c_str());
                preferences.remove((prefix + "dayPath").c_str());
                preferences.remove((prefix + "yearPath").c_str());
                preferences.remove((prefix + "timePath").c_str());
                preferences.remove((prefix + "prefix").c_str());
                preferences.remove((prefix + "suffix").c_str());
                preferences.remove((prefix + "icon").c_str());
                preferences.remove((prefix + "scroll").c_str());
                preferences.remove((prefix + "srcType").c_str());
                preferences.remove((prefix + "topic").c_str());
                preferences.remove((prefix + "yearPrefix").c_str());
                preferences.remove((prefix + "yearSuffix").c_str());
                preferences.remove((prefix + "dispMode").c_str());
                preferences.remove((prefix + "scrollTxt").c_str());
                preferences.remove((prefix + "authKey").c_str());
                preferences.remove((prefix + "authVal").c_str());
                preferences.remove((prefix + "httpMethod").c_str());
                preferences.remove((prefix + "reqBody").c_str());
                preferences.remove((prefix + "apiKey").c_str());
                preferences.end();
            }
        }
    }

    saveSettings();
    if (oldMqttBroker != currentSettings.mqttBroker || oldMqttPort != currentSettings.mqttPort) {
        if (mqttClient.connected()) {
            mqttClient.disconnect();
        }
        mqttReconnectRequired = true;
    }

    #if ENABLE_HARDWARE
    myDFPlayer.volume(currentSettings.notificationVolume);
    #endif

    request->send(200, "text/plain", "Settings Saved! Engaging Time Circuits...");
    startTimeTravelAnimation();
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