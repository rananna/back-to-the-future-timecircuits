#include "web_server.h"

// This is the full function moved from the .ino file.
// It sets up all the HTTP endpoints for the web interface.
void setupWebRoutes() {
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
    DynamicJsonDocument doc(4096);
    doc["dataLinkEnabled"] = currentSettings.dataLinkEnabled;
    doc["dataLinkTargetRow"] = currentSettings.dataLinkTargetRow;
    doc["dataLinkRefreshInterval"] = currentSettings.dataLinkRefreshInterval;
    doc["numDataPoints"] = currentSettings.numDataPoints;
    doc["mqttBroker"] = currentSettings.mqttBroker;
    doc["mqttPort"] = currentSettings.mqttPort;
    doc["mqttUser"] = currentSettings.mqttUser;
    doc["mqttPassword"] = currentSettings.mqttPassword;

    JsonArray dataPoints = doc.createNestedArray("dataPoints");
    for (int i = 0; i < currentSettings.numDataPoints; i++) {
        JsonObject dp = dataPoints.createNestedObject();
        dp["url"] = currentSettings.dataPoints[i].url;
        dp["monthPath"] = currentSettings.dataPoints[i].monthPath;
        dp["dayPath"] = currentSettings.dataPoints[i].dayPath;
        dp["yearPath"] = currentSettings.dataPoints[i].yearPath;
        dp["timePath"] = currentSettings.dataPoints[i].timePath;
        dp["prefix"] = currentSettings.dataPoints[i].prefix;
        dp["suffix"] = currentSettings.dataPoints[i].suffix;
        dp["icon"] = currentSettings.dataPoints[i].icon;
        dp["scrollSpeed"] = currentSettings.dataPoints[i].scrollSpeed;
        dp["dataSourceType"] = (int)currentSettings.dataPoints[i].dataSourceType;
        dp["mqttTopic"] = currentSettings.dataPoints[i].mqttTopic;
        dp["yearPrefix"] = currentSettings.dataPoints[i].yearPrefix;
        dp["yearSuffix"] = currentSettings.dataPoints[i].yearSuffix;
        dp["displayMode"] = (int)currentSettings.dataPoints[i].displayMode; // <-- NEW
        dp["scrollingText"] = currentSettings.dataPoints[i].scrollingText; // <-- NEW
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
  server.on("/api/saveSettings", HTTP_POST, [](AsyncWebServerRequest *request){
    auto getParamInt = [&](const String& name, int defaultValue) -> int {
        if (request->hasParam(name, true)) return request->getParam(name, true)->value().toInt();
        return defaultValue;
    };
    auto getParamValue = [&](const String& name) -> String {
        if (request->hasParam(name, true)) return request->getParam(name, true)->value();
        return "";
    };

    String oldMqttBroker = currentSettings.mqttBroker;
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
    strncpy(currentSettings.mqttBroker, getParamValue("mqttBroker").c_str(), sizeof(currentSettings.mqttBroker) - 1);
    currentSettings.mqttPort = getParamInt("mqttPort", 1883);
    strncpy(currentSettings.mqttUser, getParamValue("mqttUser").c_str(), sizeof(currentSettings.mqttUser) - 1);
    strncpy(currentSettings.mqttPassword, getParamValue("mqttPassword").c_str(), sizeof(currentSettings.mqttPassword) - 1);
    if (request->hasParam("numDataPoints", true)) {
        int numDataPoints = getParamInt("numDataPoints", 0);
        if (numDataPoints > 5) numDataPoints = 5;
        currentSettings.numDataPoints = numDataPoints;
        for (int i = 0; i < 5; i++) {
            if (i < numDataPoints) {
                currentSettings.dataPoints[i].dataSourceType = (DataSourceType)getParamInt("dp_dataSourceType_" + String(i), 0);
                strncpy(currentSettings.dataPoints[i].url, getParamValue("dp_url_" + String(i)).c_str(), sizeof(currentSettings.dataPoints[i].url) - 1);
                strncpy(currentSettings.dataPoints[i].monthPath, getParamValue("dp_monthPath_" + String(i)).c_str(), sizeof(currentSettings.dataPoints[i].monthPath) - 1);
                strncpy(currentSettings.dataPoints[i].dayPath, getParamValue("dp_dayPath_" + String(i)).c_str(), sizeof(currentSettings.dataPoints[i].dayPath) - 1);
                strncpy(currentSettings.dataPoints[i].yearPath, getParamValue("dp_yearPath_" + String(i)).c_str(), sizeof(currentSettings.dataPoints[i].yearPath) - 1);
                strncpy(currentSettings.dataPoints[i].timePath, getParamValue("dp_timePath_" + String(i)).c_str(), sizeof(currentSettings.dataPoints[i].timePath) - 1);
                strncpy(currentSettings.dataPoints[i].prefix, getParamValue("dp_prefix_" + String(i)).c_str(), sizeof(currentSettings.dataPoints[i].prefix) - 1);
                strncpy(currentSettings.dataPoints[i].suffix, getParamValue("dp_suffix_" + String(i)).c_str(), sizeof(currentSettings.dataPoints[i].suffix) - 1);
                strncpy(currentSettings.dataPoints[i].icon, getParamValue("dp_icon_" + String(i)).c_str(), sizeof(currentSettings.dataPoints[i].icon) - 1);
                currentSettings.dataPoints[i].scrollSpeed = getParamInt("dp_scrollSpeed_" + String(i), 150);
                strncpy(currentSettings.dataPoints[i].mqttTopic, getParamValue("dp_mqttTopic_" + String(i)).c_str(), sizeof(currentSettings.dataPoints[i].mqttTopic) - 1);
                strncpy(currentSettings.dataPoints[i].yearPrefix, getParamValue("dp_yearPrefix_" + String(i)).c_str(), sizeof(currentSettings.dataPoints[i].yearPrefix) - 1);
                strncpy(currentSettings.dataPoints[i].yearSuffix, getParamValue("dp_yearSuffix_" + String(i)).c_str(), sizeof(currentSettings.dataPoints[i].yearSuffix) - 1);
                currentSettings.dataPoints[i].displayMode = (DisplayMode)getParamInt("dp_displayMode_" + String(i), 0); // <-- NEW
                strncpy(currentSettings.dataPoints[i].scrollingText, getParamValue("dp_scrollingText_" + String(i)).c_str(), sizeof(currentSettings.dataPoints[i].scrollingText) - 1); // <-- NEW
            } else {
                memset(&currentSettings.dataPoints[i], 0, sizeof(DataPoint));
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
    String value = request->getParam("value", true)->value();
    String presetsJson = preferences.getString("customPresets", "[]");
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, presetsJson);
    JsonArray presets = doc.as<JsonArray>();
    for (JsonObject preset : presets) {
        if (preset["name"] == name) {
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
    LittleFS.remove(API_TEMPLATES_FILE);
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
  server.on("/api/setTheme", HTTP_POST, [](AsyncWebServerRequest *request){
    String theme = request->getParam("theme", true)->value();

    int themeEnum = THEME_TIME_CIRCUITS; // Default
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
  server.on("/api/testDataPoint", HTTP_POST, [](AsyncWebServerRequest *request){
    String url = request->getParam("url", true)->value();
    String path = request->hasParam("path", true) ? request->getParam("path", true)->value() : "";
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    String errorMsg = "";

    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            DynamicJsonDocument doc(8192);

            DeserializationError error = deserializeJson(doc, payload);
            if (error == DeserializationError::Ok) {
                if (path.length() == 0) {
                    request->send(200, "application/json", "{\"success\":true, \"value\":" + payload + "}");
                } else {
                    JsonVariant value = getJsonVariant(doc.as<JsonVariant>(), path.c_str());
                    if (!value.isNull()) {
                        request->send(200, "application/json", "{\"success\":true, \"value\":\"" + value.as<String>() + "\"}");
                    } else {
                        errorMsg = "JSON Path not found.";
                    }
                }
            } else {
                errorMsg = "JSON Parsing Failed: " + String(error.c_str());
            }
        } else {
            errorMsg = "HTTP Error: " + String(httpCode);
        }
    } else {
        errorMsg = "HTTP request failed.";
    }

    if (errorMsg != "") {
        request->send(200, "application/json", "{\"success\":false, \"error\":\"" + errorMsg + "\"}");
    }
    http.end();
  });
}