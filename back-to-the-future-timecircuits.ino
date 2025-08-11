// --- Include header files ---
#include "esp_log.h"
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFiUdp.h>
#include <time.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <HTTPClient.h>

#include "HardwareControl.h"

// --- FIX FOR LED_BUILTIN ERROR ---
#ifndef LED_BUILTIN
  #define LED_BUILTIN 2
#endif

// =================================================================
// == GLOBAL DEFINITIONS & OBJECTS                                ==
// =================================================================

#define PREFERENCES_KEY "settings_v2"
#define THEME_PREF_KEY "ui_theme"

// --- Timing and Interval Constants ---
#define NTP_SUCCESS_INTERVAL_MS 3600000
#define ANIMATION_UPDATE_INTERVAL_MS 50
#define BOOT_STATE_CHANGE_INTERVAL_MS 2000
#define GLITCH_EFFECT_INTERVAL_MS 60000

// --- Global Objects & Variables ---
ClockSettings currentSettings;
ClockSettings defaultSettings = {
  1955, 4, 22, 0, 7, 0, 1, 21, 1985, 10, 26, 5, 15, true, 15, 10, false, THEME_TIME_CIRCUITS, 1,
  4000, ANIMATION_SEQUENTIAL_FLICKER, 0, 25, true, false, -80.52, 43.47,
  // Marquee Defaults
  false, 2, 10, 0, {}
};
const TimeZoneEntry TZ_DATA[] = {
  { "UTC0", "UTC", "Etc/UTC", "Global" },
  { "EST5EDT,M3.2.0,M11.1.0", "Eastern (New York)", "America/New_York", "Americas" },
  { "CST6CDT,M3.2.0,M11.1.0", "Central (Chicago)", "America/Chicago", "Americas" },
  { "MST7MDT,M3.2.0,M11.1.0", "Mountain (Denver)", "America/Denver", "Americas" },
  { "PST8PDT,M3.2.0,M11.1.0", "Pacific (Los Angeles)", "America/Los_Angeles", "Americas" },
  { "AKST9AKDT,M3.2.0,M11.1.0", "Alaska (Anchorage)", "America/Anchorage", "Americas" },
  { "MST7", "Mountain (Phoenix, No DST)", "America/Phoenix", "Americas" },
  { "HST10", "Hawaii (Honolulu, No DST)", "Pacific/Honolulu", "Americas" },
  { "GMT0BST,M3.5.0/1,M10.5.0", "GMT/BST (London)", "Europe/Africa" },
  { "CET-1CEST,M3.5.0,M10.5.0", "CET/CEST (Berlin)", "Europe/Africa" }
};
const int NUM_TIMEZONE_OPTIONS = sizeof(TZ_DATA) / sizeof(TZ_DATA[0]);

// --- Pre-generated constant strings stored in Flash Memory (PROGMEM) ---
const char TZ_JSON[] PROGMEM = "{\"Global\":[{\"value\":0,\"text\":\"UTC\",\"ianaTzName\":\"Etc/UTC\"}],\"Americas\":[{\"value\":1,\"text\":\"Eastern (New York)\",\"ianaTzName\":\"America/New_York\"},{\"value\":2,\"text\":\"Central (Chicago)\",\"ianaTzName\":\"America/Chicago\"},{\"value\":3,\"text\":\"Mountain (Denver)\",\"ianaTzName\":\"America/Denver\"},{\"value\":4,\"text\":\"Pacific (Los Angeles)\",\"ianaTzName\":\"America/Los_Angeles\"},{\"value\":5,\"text\":\"Alaska (Anchorage)\",\"ianaTzName\":\"America/Anchorage\"},{\"value\":6,\"text\":\"Mountain (Phoenix, No DST)\",\"ianaTzName\":\"America/Phoenix\"},{\"value\":7,\"text\":\"Hawaii (Honolulu, No DST)\",\"ianaTzName\":\"Pacific/Honolulu\"}],\"Europe/Africa\":[{\"value\":8,\"text\":\"GMT/BST (London)\",\"ianaTzName\":\"Europe/London\"},{\"value\":9,\"text\":\"CET/CEST (Berlin)\",\"ianaTzName\":\"Europe/Berlin\"}]}";
const char STYLE_0[] PROGMEM = "Sequential Flicker";
const char STYLE_1[] PROGMEM = "Random Flicker";
const char STYLE_2[] PROGMEM = "All Displays Random";
const char STYLE_3[] PROGMEM = "Counting Up";
const char STYLE_4[] PROGMEM = "Wave Flicker";
const char* const ANIMATION_STYLE_NAMES[] PROGMEM = { STYLE_0, STYLE_1, STYLE_2, STYLE_3, STYLE_4 };
const char *NTP_SERVERS[] = { "pool.ntp.org", "time.google.com", "time.nist.gov" };
const int NUM_NTP_SERVERS = sizeof(NTP_SERVERS) / sizeof(NTP_SERVERS[0]);
int currentNtpServerIndex = 0;
bool timeSynchronized = false;
bool ntpSyncRequested = false;
WiFiManager wifiManager;
AsyncWebServer server(80);
Preferences preferences;
// Animation and sleep state
bool isAnimating = false;
unsigned long animationStartTime = 0;
unsigned long lastAnimationFrameTime = 0;
enum AnimationPhase { ANIM_INACTIVE, ANIM_FLICKER, ANIM_COMPLETE };
AnimationPhase currentPhase = ANIM_INACTIVE;
bool isDisplayAsleep = false;
byte initialBrightness = 0;
#define MDNS_HOSTNAME "timecircuits"
enum BootSequenceState { BOOT_INACTIVE, BOOT_START, BOOT_88MPH, BOOT_RECALIBRATING, BOOT_CAPACITOR, BOOT_COMPLETE };
BootSequenceState bootState = BOOT_INACTIVE;
unsigned long bootStateStartTime = 0;
unsigned long lastGlitchTime = 0;
bool isGlitching = false;
unsigned long glitchStartTime = 0;
unsigned long lastPresetCycleTime = 0;
int currentPresetIndex = 0;
float currentWindSpeed = 0.0;
// Marquee Display Engine Variables
enum MarqueeState { M_IDLE, M_PAUSED, M_SCROLLING };
MarqueeState marqueeState = M_IDLE;
String displayPages[5];
String lastGoodDisplayPages[5];
int currentPageIndex = 0;
int currentPointToFetch = 0;
unsigned long lastDataLinkFetch = 0;
unsigned long lastMarqueeStateChange = 0;
int marqueeScrollPosition = 0;
bool isFetchingData = false;
int dataPointFetchFailures[5] = {0, 0, 0, 0, 0};
const int MAX_FETCH_FAILURES = 3;

// --- ADDED: Malfunction State Variables ---
bool isMalfunctioning = false;
unsigned long malfunctionStartTime = 0;
enum MalfunctionPhase { MAL_INACTIVE, MAL_HAYWIRE, MAL_ERROR_MESSAGE, MAL_REBOOT };
MalfunctionPhase currentMalfunctionPhase = MAL_INACTIVE;

// Forward declarations
void startTimeTravelAnimation();
void handleDisplayAnimation();
void handleBootSequence();
void handleGlitchEffect();
void handleMalfunction(); // <-- ADDED
void restoreDisplayAfterGlitch();
void handlePresetCycling();
void handleSleepSchedule();
void updateNormalClockDisplay();
void fetchDataLink();
void updateMarqueeDisplay();
void fetchWindSpeed();
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

void saveSettings() {
  preferences.putBytes(PREFERENCES_KEY, &currentSettings, sizeof(currentSettings));
  setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
  tzset();
}

void loadSettings() {
  if (preferences.getBytesLength(PREFERENCES_KEY) != sizeof(currentSettings)) {
    currentSettings = defaultSettings;
    saveSettings();
  } else {
    preferences.getBytes(PREFERENCES_KEY, &currentSettings, sizeof(currentSettings));
  }
  // Sanity check loaded timezone indices
  if (currentSettings.presentTimezoneIndex < 0 || currentSettings.presentTimezoneIndex >= NUM_TIMEZONE_OPTIONS) {
    currentSettings.presentTimezoneIndex = 0; // Default to UTC
  }
  if (currentSettings.destinationTimezoneIndex < 0 || currentSettings.destinationTimezoneIndex >= NUM_TIMEZONE_OPTIONS) {
    currentSettings.destinationTimezoneIndex = 0; // Default to UTC
  }
  setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
  tzset();
}

void setupWebRoutes() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(LittleFS, "/index.html", "text/html"); });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(LittleFS, "/style.css", "text/css"); });
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(LittleFS, "/script.js", "application/javascript"); });
  server.on("/api/isReady", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(200, "text/plain", "READY"); });
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
    doc["malfunctionFrequency"] = currentSettings.malfunctionFrequency; // <-- ADDED
    doc["timeTravelSoundToggle"] = currentSettings.timeTravelSoundToggle;
    doc["timeTravelVolumeFade"] = currentSettings.timeTravelVolumeFade;
    doc["presetCycleInterval"] = currentSettings.presetCycleInterval;
    doc["displayFormat24h"] = currentSettings.displayFormat24h;
String jsonString;
    serializeJson(doc, jsonString);
    request->send(200, "application/json", jsonString);
  });
  server.on("/api/settings/datalink", HTTP_GET, [](AsyncWebServerRequest *request) {
    String response = "{";
    response += "\"dataLinkEnabled\":" + String(currentSettings.dataLinkEnabled ? "true" : "false") + ",";
    response += "\"dataLinkTargetRow\":" + String(currentSettings.dataLinkTargetRow) + ",";
    response += "\"dataLinkRefreshInterval\":" + String(currentSettings.dataLinkRefreshInterval) + ",";
    response += "\"numDataPoints\":" + String(currentSettings.numDataPoints) + ",";
    response += "\"dataPoints\":[";
    for (int i = 0; i < currentSettings.numDataPoints; i++) {
        response += "{";
        
response += "\"url\":\"" + String(currentSettings.dataPoints[i].url) + "\",";
  
        response += "\"label\":\"" + String(currentSettings.dataPoints[i].label) + "\",";
        response += "\"jsonPath\":\"" + String(currentSettings.dataPoints[i].jsonPath) + "\",";
        response += "\"format\":\"" + String(currentSettings.dataPoints[i].format) + "\",";
        response += "\"icon\":\"" + String(currentSettings.dataPoints[i].icon) + "\",";
response += "\"scrollSpeed\":" + String(currentSettings.dataPoints[i].scrollSpeed) + ",";
        response += "\"isLiveData\":" + String(currentSettings.dataPoints[i].isLiveData ? "true" : "false") + ",";
        response += "\"liveDataTag\":\"" + String(currentSettings.dataPoints[i].liveDataTag) + "\"";
        response += "}";
        if (i < currentSettings.numDataPoints - 1) response += ",";
    }
    response += "]}";
    request->send(200, "application/json", response);
  });
  server.on("/api/timezones", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "application/json", TZ_JSON);
  });
  server.on("/api/getPresets", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", preferences.getString("customPresets", "[]"));
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
    currentSettings.destinationYear = getParamInt("destinationYear", currentSettings.destinationYear);
    currentSettings.destinationTimezoneIndex = getParamInt("destinationTimezoneIndex", currentSettings.destinationTimezoneIndex);
    if 
(request->hasParam("lastTimeDepartedYear", true)) {
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
// <-- ADDED
    currentSettings.notificationVolume = getParamInt("notificationVolume", currentSettings.notificationVolume);
currentSettings.timeTravelSoundToggle = (getParamValue("timeTravelSoundToggle") == "true");
    currentSettings.timeTravelVolumeFade = (getParamValue("timeTravelVolumeFade") == "true");
    currentSettings.presentTimezoneIndex = getParamInt("presentTimezoneIndex", currentSettings.presentTimezoneIndex);
    currentSettings.displayFormat24h = (getParamValue("displayFormat24h") == "true");
currentSettings.dataLinkEnabled = (getParamValue("dataLinkEnabled") == "true");
    currentSettings.dataLinkTargetRow = getParamInt("dataLinkTargetRow", currentSettings.dataLinkTargetRow);
    currentSettings.dataLinkRefreshInterval = getParamInt("dataLinkRefreshInterval", currentSettings.dataLinkRefreshInterval);
if (request->hasParam("numDataPoints", true)) {
        int numDataPoints = getParamInt("numDataPoints", currentSettings.numDataPoints);
        if (numDataPoints > 5) numDataPoints = 5;
        currentSettings.numDataPoints = numDataPoints;
        for (int i = 0; i < currentSettings.numDataPoints; i++) {
            String url = getParamValue("dp_url_" + String(i));
            strncpy(currentSettings.dataPoints[i].url, url.c_str(), sizeof(currentSettings.dataPoints[i].url) - 1);
            currentSettings.dataPoints[i].url[sizeof(currentSettings.dataPoints[i].url) - 1] = '\0';
            String label = getParamValue("dp_label_" + String(i));
            strncpy(currentSettings.dataPoints[i].label, label.c_str(), sizeof(currentSettings.dataPoints[i].label) - 1);
            currentSettings.dataPoints[i].label[sizeof(currentSettings.dataPoints[i].label) - 1] = '\0';
            String path = getParamValue("dp_path_" + String(i));
            strncpy(currentSettings.dataPoints[i].jsonPath, path.c_str(), sizeof(currentSettings.dataPoints[i].jsonPath) - 1);
            currentSettings.dataPoints[i].jsonPath[sizeof(currentSettings.dataPoints[i].jsonPath) - 1] = '\0';
            String format = getParamValue("dp_format_" + String(i));
            strncpy(currentSettings.dataPoints[i].format, format.c_str(), sizeof(currentSettings.dataPoints[i].format) - 1);
            currentSettings.dataPoints[i].format[sizeof(currentSettings.dataPoints[i].format) - 1] = '\0';
            String icon = getParamValue("dp_icon_" + String(i));
            strncpy(currentSettings.dataPoints[i].icon, icon.c_str(), sizeof(currentSettings.dataPoints[i].icon) - 1);
            currentSettings.dataPoints[i].icon[sizeof(currentSettings.dataPoints[i].icon) - 1] = '\0';
            String tag = getParamValue("dp_liveDataTag_" + String(i));
            strncpy(currentSettings.dataPoints[i].liveDataTag, tag.c_str(), sizeof(currentSettings.dataPoints[i].liveDataTag) - 1);
            currentSettings.dataPoints[i].liveDataTag[sizeof(currentSettings.dataPoints[i].liveDataTag) - 1] = '\0';
            currentSettings.dataPoints[i].scrollSpeed = getParamInt("dp_scrollSpeed_" + String(i), 150);
            currentSettings.dataPoints[i].isLiveData = (getParamValue("dp_isLiveData_" + String(i)) == "true");
        }
    }
    saveSettings();
    #if ENABLE_HARDWARE
    myDFPlayer.volume(currentSettings.notificationVolume);
    #endif
    request->send(200, "text/plain", "Settings Saved! Engaging Time Circuits...");
    startTimeTravelAnimation();
  });
  server.on("/api/addPreset", HTTP_POST, [](AsyncWebServerRequest *request){
    String presetsJson = preferences.getString("customPresets", "[]");
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, presetsJson);
    JsonArray presets = doc.as<JsonArray>();
    JsonObject newPreset = presets.createNestedObject();
    newPreset["name"] = request->getParam("name", true)->value();
    newPreset["value"] = request->getParam("value", true)->value();
    String newPresetsJson;
    serializeJson(doc, newPresetsJson);
    preferences.putString("customPresets", newPresetsJson);
    request->send(200, "text/plain", "Custom preset saved!");
  });
  server.on("/api/updatePreset", HTTP_POST, [](AsyncWebServerRequest *request){
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
    request->send(200, "text/plain", "Preset updated!");
  });
  server.on("/api/deletePreset", HTTP_POST, [](AsyncWebServerRequest *request){
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
    request->send(200, "text/plain", "Preset deleted!");
  });
  server.on("/api/resetSettings", HTTP_POST, [](AsyncWebServerRequest *request){
    preferences.remove("customPresets");
    preferences.remove(THEME_PREF_KEY);
    currentSettings = defaultSettings;
    saveSettings();
    request->send(200, "text/plain", "Settings have been reset to default.");
  });
  server.on("/api/syncTime", HTTP_POST, [](AsyncWebServerRequest *request){
    ntpSyncRequested = true;
    request->send(200, "text/plain", "NTP time sync requested.");
  });
  server.on("/api/getTheme", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", preferences.getString(THEME_PREF_KEY, "theme-time-circuits"));
  });
  server.on("/api/setTheme", HTTP_POST, [](AsyncWebServerRequest *request){
    String theme = request->getParam("theme", true)->value();
    preferences.putString(THEME_PREF_KEY, theme);
    request->send(200, "text/plain", "Theme saved.");
  });
  server.on("/api/testDataPoint", HTTP_POST, [](AsyncWebServerRequest *request){
    String url = request->getParam("url", true)->value();
    String path = request->getParam("path", true)->value();
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        DynamicJsonDocument doc(8192);
        DeserializationError error = deserializeJson(doc, http.getStream());
        if (error == DeserializationError::Ok) {
            JsonVariant value = getJsonVariant(doc.as<JsonVariant>(), path.c_str());
        
    if (!value.isNull()) {
                request->send(200, "application/json", "{\"success\":true, \"value\":\"" + value.as<String>() + "\"}");
            } else {
                request->send(200, "application/json", "{\"success\":false, \"error\":\"JSON Path not found.\"}");
            }
        } else {
            request->send(200, 
"application/json", "{\"success\":false, \"error\":\"JSON Parsing Failed.\"}");
        }
    } else {
        request->send(200, "application/json", "{\"success\":false, \"error\":\"HTTP Error: " + String(httpCode) + "\"}");
    }
    http.end();
  });
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n--- BOOTING ---");
  if (!LittleFS.begin(true)) { ESP_LOGE("FS", "CRITICAL ERROR: LittleFS Mount Failed."); while(1); }
  preferences.begin("bttf-clock", false);
  loadSettings();
  #if ENABLE_HARDWARE
  setupPhysicalDisplay();
  dfpSerial.begin(9600, SERIAL_8N1, DFP_RX_PIN, DFP_TX_PIN);
  if (myDFPlayer.begin(dfpSerial, true, false)) {
      myDFPlayer.volume(currentSettings.notificationVolume);
      setupSoundFiles();
  }
  #endif
  wifiManager.autoConnect("BTTF-Clock-Setup");
  ESP_LOGI("WiFi", "WiFi connected! IP: %s", WiFi.localIP().toString().c_str());
  if (MDNS.begin(MDNS_HOSTNAME)) { MDNS.addService("http", "tcp", 80); }
  setupWebRoutes();
  server.begin();
  ESP_LOGI("Web", "HTTP server started.");
  configTime(0, 0, NTP_SERVERS[0]);
  setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
  tzset();
  runBootSequence();
}

void loop() {
  ArduinoOTA.handle();
  handleBootSequence();
  if (isMalfunctioning) {
    handleMalfunction();
  } else if (!isAnimating) {
    restoreDisplayAfterGlitch();
    handleGlitchEffect();
    handlePresetCycling();
    handleSleepSchedule();
    if (currentSettings.dataLinkEnabled) {
      fetchDataLink();
      updateMarqueeDisplay();
    } else {
      updateNormalClockDisplay();
    }
  }

  handleDisplayAnimation();
  static unsigned long lastNtpUpdate = 0;
  if (ntpSyncRequested || (!timeSynchronized && millis() > 10000) || (timeSynchronized && millis() - lastNtpUpdate > NTP_SUCCESS_INTERVAL_MS)) {
    configTime(0, 0, NTP_SERVERS[currentNtpServerIndex]);
    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
    tzset();
    struct tm timeinfo;
    if(getLocalTime(&timeinfo, 5000)){ timeSynchronized = true;
    }
    else { timeSynchronized = false; }
    currentNtpServerIndex = (currentNtpServerIndex + 1) % NUM_NTP_SERVERS;
    lastNtpUpdate = millis();
    ntpSyncRequested = false;
  }
}

void startTimeTravelAnimation() {
    if (isAnimating) { return;
    }
    isAnimating = true;
animationStartTime = millis();
    currentPhase = ANIM_FLICKER;
    #if ENABLE_HARDWARE
    if (currentSettings.timeTravelSoundToggle) {
        playSound(SOUND_ACCELERATION);
    }
    #endif
}

void handleDisplayAnimation() {
  #if ENABLE_HARDWARE
  if (!isAnimating) return;
  unsigned long currentTime = millis();
  unsigned long elapsed = currentTime - animationStartTime;
  switch (currentPhase) {
    case ANIM_FLICKER:
        if (elapsed >= currentSettings.timeTravelAnimationDuration) {
            currentPhase = ANIM_COMPLETE;
        } else if (currentTime - lastAnimationFrameTime > ANIMATION_UPDATE_INTERVAL_MS) {
            animateDisplayRowRandomly(destRow);
            animateDisplayRowRandomly(presRow);
            animateDisplayRowRandomly(lastRow);
            lastAnimationFrameTime = currentTime;
        }
        break;
    case ANIM_COMPLETE:
      isAnimating = false;
      updateNormalClockDisplay();
      #if ENABLE_HARDWARE
      if(currentSettings.timeTravelSoundToggle){
        playSound(SOUND_ARRIVAL_THUD);
      }
      #endif
      break;
  }
  #endif
}

// --- BUG FIX: Corrected Malfunction Handler ---
void handleMalfunction() {
  #if ENABLE_HARDWARE
  if (!isMalfunctioning) return;
  unsigned long elapsed = millis() - malfunctionStartTime;

  switch (currentMalfunctionPhase) {
    case MAL_HAYWIRE:
      // Step 1: Displays go haywire for 3 seconds
      if (elapsed < 3000) {
        if (millis() - lastAnimationFrameTime > 100) {
          // Flash "8888" or random garbage on all displays
          destRow.month.print("8888");
          destRow.day.print("8888"); destRow.year.print("8888"); destRow.time.print("8888");
          presRow.month.print("8888"); presRow.day.print("8888"); presRow.year.print("8888"); presRow.time.print("8888");
          lastRow.month.print("8888"); lastRow.day.print("8888"); lastRow.year.print("8888"); lastRow.time.print("8888");
          
          destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();
          presRow.month.writeDisplay(); presRow.day.writeDisplay(); presRow.year.writeDisplay(); presRow.time.writeDisplay();
          lastRow.month.writeDisplay();
          lastRow.day.writeDisplay(); lastRow.year.writeDisplay(); lastRow.time.writeDisplay();
          
          lastAnimationFrameTime = millis();
        }
      } else {
        malfunctionStartTime = millis(); // Reset timer for next phase
        currentMalfunctionPhase = MAL_ERROR_MESSAGE;
      }
      break;

    case MAL_ERROR_MESSAGE:
      // Step 2: Show error message for 4 seconds
      if (elapsed < 4000) {
        // Display "TIME CIRCUIT OVERLOAD" across the top two rows
        destRow.month.print("TIME");
        destRow.day.print("CIRC"); destRow.year.print("UIT "); destRow.time.print("OVER");
        presRow.month.print("LOAD"); presRow.day.clear(); presRow.year.clear(); presRow.time.clear();
        
        // Display "FLUX OFFLINE" on the bottom row
        lastRow.month.print("FLUX");
        lastRow.day.print("OFFL"); lastRow.year.print("INE "); lastRow.time.clear();

        destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();
        presRow.month.writeDisplay(); presRow.day.writeDisplay(); presRow.year.writeDisplay(); presRow.time.writeDisplay();
        lastRow.month.writeDisplay(); lastRow.day.writeDisplay(); lastRow.year.writeDisplay(); lastRow.time.writeDisplay();
      } else {
        malfunctionStartTime = millis();
        currentMalfunctionPhase = MAL_REBOOT;
      }
      break;

    case MAL_REBOOT:
      // Step 3: Simulate a reboot
      blankAllDisplays(); // Turn off all displays
      runBootSequence();  // Trigger the existing boot sequence
      
      // FIX: Removed the following two lines to allow the boot sequence to complete.
      // isMalfunctioning = false;
      // currentMalfunctionPhase = MAL_INACTIVE;
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
  if (elapsed > BOOT_STATE_CHANGE_INTERVAL_MS) {
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
      display88MphSpeed(88.0);
      break;
    case BOOT_RECALIBRATING:
      destRow.month.print("RECA"); destRow.day.print("LIBR"); destRow.year.print("ATIN"); destRow.time.print("G");
      destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();
      break;
    case BOOT_CAPACITOR:
      presRow.month.print("CAPA"); presRow.day.print("CITO"); presRow.year.print("R"); presRow.time.print("FULL");
      presRow.month.writeDisplay(); presRow.day.writeDisplay(); presRow.year.writeDisplay(); presRow.time.writeDisplay();
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
  if (millis() - lastGlitchTime > GLITCH_EFFECT_INTERVAL_MS) {
    lastGlitchTime = millis();
    if (random(100) < currentSettings.glitchEffectFrequency) {
      
      // MODIFIED: Check for a major malfunction
      if (currentSettings.malfunctionFrequency > 0 && random(currentSettings.malfunctionFrequency) == 0) {
        isMalfunctioning = true;
        malfunctionStartTime = millis();
        currentMalfunctionPhase = MAL_HAYWIRE;
      } else {
        isGlitching = true;
        glitchStartTime = millis();
        #if ENABLE_HARDWARE
        // Simple glitch
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
        // Future implementation: Logic to cycle presets
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
    playSound(SOUND_SLEEP_ON);
    #endif
  } else if (!shouldBeAsleep && isDisplayAsleep) {
    isDisplayAsleep = false;
    #if ENABLE_HARDWARE
    updateNormalClockDisplay();
    playSound(SOUND_CONFIRM_ON);
    #endif
  }
}

void updateNormalClockDisplay() {
  if (isDisplayAsleep || isAnimating || isGlitching || isMalfunctioning) return; // <-- ADDED isMalfunctioning
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

void fetchWindSpeed() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String apiURL = "http://api.open-meteo.com/v1/forecast?latitude=" + String(currentSettings.latitude, 2) + "&longitude=" + String(currentSettings.longitude, 2) + "&current_weather=true";
  http.begin(apiURL);
  if (http.GET() == HTTP_CODE_OK) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getStream());
    if (doc.containsKey("current_weather")) {
      currentWindSpeed = doc["current_weather"]["windspeed"];
    }
  }
  http.end();
}

void fetchDataLink() {
    if (!currentSettings.dataLinkEnabled || currentSettings.numDataPoints == 0 || isFetchingData) return;
    if (millis() - lastDataLinkFetch < (unsigned long)currentSettings.dataLinkRefreshInterval * 60 * 1000 / currentSettings.numDataPoints) return;
    isFetchingData = true;
    lastDataLinkFetch = millis();
    DataPoint point = currentSettings.dataPoints[currentPointToFetch];
    String fetchedValue = "";
    bool fetchSuccess = false;
    if (point.isLiveData) {
        if (String(point.liveDataTag) == "WIND_SPEED") {
            fetchWindSpeed();
            char speedStr[8];
            dtostrf(currentWindSpeed * 0.621371, 4, 1, speedStr);
            fetchedValue = String(speedStr);
            fetchSuccess = true;
        } else { fetchedValue = "LIVE ERR"; }
    } else {
        HTTPClient http;
        http.begin(point.url);
        if (http.GET() == HTTP_CODE_OK) {
            DynamicJsonDocument doc(8192);
            if (deserializeJson(doc, http.getStream()) == DeserializationError::Ok) {
                JsonVariant value = getJsonVariant(doc.as<JsonVariant>(), point.jsonPath);
                if (!value.isNull()) {
                    fetchedValue = value.as<String>();
                    if (fetchedValue.length() > 256) fetchedValue = fetchedValue.substring(0, 256);
                    fetchSuccess = true;
                } else { fetchedValue = "PATH ERR";
                }
            } else { fetchedValue = "JSON ERR";
            }
        } else { fetchedValue = "HTTP ERR";
        }
        http.end();
    }
    if (fetchSuccess) {
        String format = String(point.format);
        format.replace("%L", point.label);
        format.replace("%V", fetchedValue);
        displayPages[currentPointToFetch] = format;
        lastGoodDisplayPages[currentPointToFetch] = format;
        dataPointFetchFailures[currentPointToFetch] = 0;
    } else {
        dataPointFetchFailures[currentPointToFetch]++;
        if (dataPointFetchFailures[currentPointToFetch] >= MAX_FETCH_FAILURES) {
            displayPages[currentPointToFetch] = String(point.label) + " | API FAIL";
        } else {
            displayPages[currentPointToFetch] = lastGoodDisplayPages[currentPointToFetch];
        }
    }
    currentPointToFetch = (currentPointToFetch + 1) % currentSettings.numDataPoints;
    isFetchingData = false;
}

void updateMarqueeDisplay() {
    #if ENABLE_HARDWARE
    if (!currentSettings.dataLinkEnabled || currentSettings.numDataPoints == 0) return;
    DisplayRow* targetRow = &lastRow;
    if (currentSettings.dataLinkTargetRow == 0) targetRow = &destRow;
    if (currentSettings.dataLinkTargetRow == 1) targetRow = &presRow;
    if (marqueeState == M_IDLE) {
        currentPageIndex = (currentPageIndex + 1) % currentSettings.numDataPoints;
        marqueeScrollPosition = 0;
        marqueeState = M_PAUSED;
        lastMarqueeStateChange = millis();
    }
    DataPoint point = currentSettings.dataPoints[currentPageIndex];
    String content = displayPages[currentPageIndex];
    String staticPart = content;
    String scrollPart = "";
    int pipePos = content.indexOf('|');
    if (pipePos != -1) {
        staticPart = content.substring(0, pipePos);
        scrollPart = content.substring(pipePos + 1);
        scrollPart.trim();
    }
    staticPart.trim();
    targetRow->month.clear();
    targetRow->month.print(staticPart.c_str());
    targetRow->month.writeDisplay();
    drawIcon(targetRow->day, point.icon);
    String canvas = "   " + scrollPart + "   ";
    if (canvas.length() <= 8) {
        targetRow->year.clear();
        targetRow->time.clear();
        targetRow->year.print(canvas.substring(0,4));
        targetRow->time.print(canvas.substring(4));
        targetRow->year.writeDisplay();
        targetRow->time.writeDisplay();
        if (marqueeState == M_PAUSED && millis() - lastMarqueeStateChange > 5000) {
             marqueeState = M_IDLE;
        }
    } else {
        if (marqueeState == M_PAUSED && millis() - lastMarqueeStateChange > 3000) {
            marqueeState = M_SCROLLING;
            lastMarqueeStateChange = millis();
        }
        if (marqueeState == M_SCROLLING && millis() - lastMarqueeStateChange > (unsigned long)point.scrollSpeed) {
            marqueeScrollPosition++;
            lastMarqueeStateChange = millis();
            if (marqueeScrollPosition > canvas.length() - 8) {
                marqueeState = M_IDLE;
            }
        }
        String viewport = canvas.substring(marqueeScrollPosition, marqueeScrollPosition + 8);
        targetRow->year.clear();
        targetRow->time.clear();
        targetRow->year.print(viewport.substring(0,4));
        targetRow->time.print(viewport.substring(4));
        targetRow->year.writeDisplay();
        targetRow->time.writeDisplay();
    }
    #endif
}