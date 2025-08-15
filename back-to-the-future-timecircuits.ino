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
#include <AsyncJson.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <PubSubClient.h>

#include "HardwareControl.h"
#include "web_server.h" // Include the new web server header

#ifndef LED_BUILTIN
  #define LED_BUILTIN 2
#endif

#define THEME_PREF_KEY "ui_theme"
#define API_TEMPLATES_FILE "/api_templates.json"
#define PREFERENCES_NAMESPACE "bttf-clock"

#define NTP_SUCCESS_INTERVAL_MS 3600000
#define ANIMATION_UPDATE_INTERVAL_MS 50
#define BOOT_STATE_CHANGE_INTERVAL_MS 2000
#define GLITCH_EFFECT_INTERVAL_MS 60000
#define MQTT_RECONNECT_INTERVAL_MS 5000

#define MQTT_STATUS_TOPIC "bttf-clock/status"
#define MQTT_LWT_MESSAGE "offline"

ClockSettings currentSettings;
DynamicJsonDocument apiTemplatesDoc(4096);

const TimeZoneEntry TZ_DATA[] = {
  { "UTC0", "UTC", "Etc/UTC", "Global" },
  { "EST5EDT,M3.2.0,M11.1.0", "Eastern (New York)", "America/New_York", "Americas" },
  { "CST6CDT,M3.2.0,M11.1.0", "Central (Chicago)", "America/Chicago", "Americas" },
  { "MST7MDT,M3.2.0,M11.1.0", "Mountain (Denver)", "America/Denver", "Americas" },
  { "PST8PDT,M3.2.0,M11.1.0", "Pacific (Los Angeles)", "America/Los_Angeles", "Americas" },
  { "AKST9AKDT,M3.2.0,M11.1.0", "Alaska (Anchorage)", "America/Anchorage", "Americas" },
  { "MST7", "Mountain (Phoenix, No DST)", "America/Phoenix", "Americas" },
  { "HST10", "Hawaii (Honolulu, No DST)", "Pacific/Honolulu", "Americas" },
  { "GMT0BST,M3.5.0/1,M10.5.0", "GMT/BST (London)", "Europe/London", "Europe/Africa" },
  { "CET-1CEST,M3.5.0,M10.5.0", "CET/CEST (Berlin)", "Europe/Berlin", "Europe/Africa" }
};
const int NUM_TIMEZONE_OPTIONS = sizeof(TZ_DATA) / sizeof(TZ_DATA[0]);

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

WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long lastMqttReconnectAttempt = 0;
bool mqttReconnectRequired = false;

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
enum MarqueeState { M_IDLE, M_PAUSED, M_SCROLLING };
MarqueeState marqueeState = M_IDLE;
String displayPages[5][4]; // For Month, Day, Year, Time
String lastGoodDisplayPages[5][4];
int currentPageIndex = 0;
int currentPointToFetch = 0;
unsigned long lastDataLinkFetch = 0;
unsigned long lastMarqueeStateChange = 0;
int marqueeScrollPosition = 0;
int marqueeScrollPositionYear = 0;
int fullRowMarqueeScrollPos = 0; // <-- NEW for scrolling text
bool isFetchingData = false;
int dataPointFetchFailures[5] = {0, 0, 0, 0, 0};
const int MAX_FETCH_FAILURES = 3;
bool isMalfunctioning = false;
unsigned long malfunctionStartTime = 0;
enum MalfunctionPhase { MAL_INACTIVE, MAL_HAYWIRE, MAL_ERROR_MESSAGE, MAL_REBOOT };
MalfunctionPhase currentMalfunctionPhase = MAL_INACTIVE;

void startTimeTravelAnimation();
void handleDisplayAnimation();
void handleBootSequence();
void handleGlitchEffect();
void handleMalfunction();
void restoreDisplayAfterGlitch();
void handlePresetCycling();
void handleSleepSchedule();
void updateNormalClockDisplay();
void fetchDataLink();
void updateMarqueeDisplay();
void loadApiTemplates();
void runBootSequence();
void setupMqtt();
void mqttCallback(char* topic, byte* payload, unsigned int length);
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
  preferences.begin(PREFERENCES_NAMESPACE, false);

  preferences.putInt("destYear", currentSettings.destinationYear);
  preferences.putInt("destTzIndex", currentSettings.destinationTimezoneIndex);
  preferences.putInt("depHour", currentSettings.departureHour);
  preferences.putInt("depMinute", currentSettings.departureMinute);
  preferences.putInt("arrHour", currentSettings.arrivalHour);
  preferences.putInt("arrMinute", currentSettings.arrivalMinute);
  preferences.putInt("lastYear", currentSettings.lastTimeDepartedYear);
  preferences.putInt("lastMonth", currentSettings.lastTimeDepartedMonth);
  preferences.putInt("lastDay", currentSettings.lastTimeDepartedDay);
  preferences.putInt("lastHour", currentSettings.lastTimeDepartedHour);
  preferences.putInt("lastMinute", currentSettings.lastTimeDepartedMinute);
  preferences.putUChar("brightness", currentSettings.brightness);
  preferences.putUChar("volume", currentSettings.notificationVolume);
  preferences.putBool("soundToggle", currentSettings.timeTravelSoundToggle);
  preferences.putInt("presTzIndex", currentSettings.presentTimezoneIndex);
  preferences.putInt("presetCycle", currentSettings.presetCycleInterval);
  preferences.putBool("format24h", currentSettings.displayFormat24h);
  preferences.putInt("theme", currentSettings.theme);
  preferences.putInt("animInterval", currentSettings.timeTravelAnimationInterval);
  preferences.putInt("animDuration", currentSettings.timeTravelAnimationDuration);
  preferences.putInt("animStyle", currentSettings.animationStyle);
  preferences.putInt("glitchFreq", currentSettings.glitchEffectFrequency);
  preferences.putInt("malfuncFreq", currentSettings.malfunctionFrequency);
  preferences.putBool("volFade", currentSettings.timeTravelVolumeFade);

  preferences.putBool("dlEnabled", currentSettings.dataLinkEnabled);
  preferences.putInt("dlTargetRow", currentSettings.dataLinkTargetRow);
  preferences.putInt("dlRefresh", currentSettings.dataLinkRefreshInterval);
  preferences.putInt("numDataPoints", currentSettings.numDataPoints);

  preferences.putString("mqttBroker", currentSettings.mqttBroker);
  preferences.putInt("mqttPort", currentSettings.mqttPort);
  preferences.putString("mqttUser", currentSettings.mqttUser);
  preferences.putString("mqttPass", currentSettings.mqttPassword);
  for (int i = 0; i < 5; i++) {
    String prefix = "dp" + String(i) + "_";
    preferences.putString((prefix + "url").c_str(), currentSettings.dataPoints[i].url);
    preferences.putString((prefix + "monthPath").c_str(), currentSettings.dataPoints[i].monthPath);
    preferences.putString((prefix + "dayPath").c_str(), currentSettings.dataPoints[i].dayPath);
    preferences.putString((prefix + "yearPath").c_str(), currentSettings.dataPoints[i].yearPath);
    preferences.putString((prefix + "timePath").c_str(), currentSettings.dataPoints[i].timePath);
    preferences.putString((prefix + "prefix").c_str(), currentSettings.dataPoints[i].prefix);
    preferences.putString((prefix + "suffix").c_str(), currentSettings.dataPoints[i].suffix);
    preferences.putString((prefix + "icon").c_str(), currentSettings.dataPoints[i].icon);
    preferences.putInt((prefix + "scroll").c_str(), currentSettings.dataPoints[i].scrollSpeed);
    preferences.putInt((prefix + "srcType").c_str(), (int)currentSettings.dataPoints[i].dataSourceType);
    preferences.putString((prefix + "topic").c_str(), currentSettings.dataPoints[i].mqttTopic);
    preferences.putString((prefix + "yearPrefix").c_str(), currentSettings.dataPoints[i].yearPrefix);
    preferences.putString((prefix + "yearSuffix").c_str(), currentSettings.dataPoints[i].yearSuffix);
    preferences.putInt((prefix + "dispMode").c_str(), (int)currentSettings.dataPoints[i].displayMode); // <-- NEW
    preferences.putString((prefix + "scrollTxt").c_str(), currentSettings.dataPoints[i].scrollingText); // <-- NEW
  }

  preferences.end();

  setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
  tzset();
}

void loadSettings() {
  preferences.begin(PREFERENCES_NAMESPACE, true);
  bool needsInit = !preferences.isKey("destYear");
  preferences.end();
  if (needsInit) {
    ESP_LOGI("SETTINGS", "No settings found. Initializing with defaults.");
    currentSettings.destinationYear = 1955;
    currentSettings.destinationTimezoneIndex = 4;
    currentSettings.departureHour = 22;
    currentSettings.departureMinute = 0;
    currentSettings.arrivalHour = 7;
    currentSettings.arrivalMinute = 0;
    currentSettings.lastTimeDepartedYear = 1985;
    currentSettings.lastTimeDepartedMonth = 10;
    currentSettings.lastTimeDepartedDay = 26;
    currentSettings.lastTimeDepartedHour = 1;
    currentSettings.lastTimeDepartedMinute = 21;
    currentSettings.brightness = 5;
    currentSettings.notificationVolume = 15;
    currentSettings.timeTravelSoundToggle = true;
    currentSettings.presentTimezoneIndex = 1;
    currentSettings.presetCycleInterval = 10;
    currentSettings.displayFormat24h = false;
    currentSettings.theme = THEME_TIME_CIRCUITS;
    currentSettings.timeTravelAnimationInterval = 15;
    currentSettings.timeTravelAnimationDuration = 4000;
    currentSettings.animationStyle = ANIMATION_SEQUENTIAL_FLICKER;
    currentSettings.glitchEffectFrequency = 0;
    currentSettings.malfunctionFrequency = 25;
    currentSettings.timeTravelVolumeFade = true;
    currentSettings.dataLinkEnabled = false;
    currentSettings.dataLinkTargetRow = 2;
    currentSettings.dataLinkRefreshInterval = 10;
    currentSettings.numDataPoints = 0;
    strcpy(currentSettings.mqttBroker, "broker.emqx.io");
    currentSettings.mqttPort = 1883;
    strcpy(currentSettings.mqttUser, "");
    strcpy(currentSettings.mqttPassword, "");
    for (int i = 0; i < 5; i++) {
        memset(&currentSettings.dataPoints[i], 0, sizeof(DataPoint));
    }
    saveSettings();
  } else {
    ESP_LOGI("SETTINGS", "Loading settings from NVS.");
    preferences.begin(PREFERENCES_NAMESPACE, true);
    currentSettings.destinationYear = preferences.getInt("destYear");
    currentSettings.destinationTimezoneIndex = preferences.getInt("destTzIndex");
    currentSettings.departureHour = preferences.getInt("depHour");
    currentSettings.departureMinute = preferences.getInt("depMinute");
    currentSettings.arrivalHour = preferences.getInt("arrHour");
    currentSettings.arrivalMinute = preferences.getInt("arrMinute");
    currentSettings.lastTimeDepartedYear = preferences.getInt("lastYear");
    currentSettings.lastTimeDepartedMonth = preferences.getInt("lastMonth");
    currentSettings.lastTimeDepartedDay = preferences.getInt("lastDay");
    currentSettings.lastTimeDepartedHour = preferences.getInt("lastHour");
    currentSettings.lastTimeDepartedMinute = preferences.getInt("lastMinute");
    currentSettings.brightness = preferences.getUChar("brightness");
    currentSettings.notificationVolume = preferences.getUChar("volume");
    currentSettings.timeTravelSoundToggle = preferences.getBool("soundToggle");
    currentSettings.presentTimezoneIndex = preferences.getInt("presTzIndex");
    currentSettings.presetCycleInterval = preferences.getInt("presetCycle");
    currentSettings.displayFormat24h = preferences.getBool("format24h");
    currentSettings.theme = preferences.getInt("theme", THEME_TIME_CIRCUITS);
    currentSettings.timeTravelAnimationInterval = preferences.getInt("animInterval");
    currentSettings.timeTravelAnimationDuration = preferences.getInt("animDuration");
    currentSettings.animationStyle = preferences.getInt("animStyle");
    currentSettings.glitchEffectFrequency = preferences.getInt("glitchFreq");
    currentSettings.malfunctionFrequency = preferences.getInt("malfuncFreq");
    currentSettings.timeTravelVolumeFade = preferences.getBool("volFade");
    currentSettings.dataLinkEnabled = preferences.getBool("dlEnabled");
    currentSettings.dataLinkTargetRow = preferences.getInt("dlTargetRow");
    currentSettings.dataLinkRefreshInterval = preferences.getInt("dlRefresh");
    currentSettings.numDataPoints = preferences.getInt("numDataPoints");

    strncpy(currentSettings.mqttBroker, preferences.getString("mqttBroker", "").c_str(), sizeof(currentSettings.mqttBroker) - 1);
    currentSettings.mqttPort = preferences.getInt("mqttPort");
    strncpy(currentSettings.mqttUser, preferences.getString("mqttUser", "").c_str(), sizeof(currentSettings.mqttUser) - 1);
    strncpy(currentSettings.mqttPassword, preferences.getString("mqttPass", "").c_str(), sizeof(currentSettings.mqttPassword) - 1);
    for (int i = 0; i < 5; i++) {
      String prefix = "dp" + String(i) + "_";
      strncpy(currentSettings.dataPoints[i].url, preferences.getString((prefix + "url").c_str(), "").c_str(), sizeof(currentSettings.dataPoints[i].url) - 1);
      strncpy(currentSettings.dataPoints[i].monthPath, preferences.getString((prefix + "monthPath").c_str(), "").c_str(), sizeof(currentSettings.dataPoints[i].monthPath) - 1);
      strncpy(currentSettings.dataPoints[i].dayPath, preferences.getString((prefix + "dayPath").c_str(), "").c_str(), sizeof(currentSettings.dataPoints[i].dayPath) - 1);
      strncpy(currentSettings.dataPoints[i].yearPath, preferences.getString((prefix + "yearPath").c_str(), "").c_str(), sizeof(currentSettings.dataPoints[i].yearPath) - 1);
      strncpy(currentSettings.dataPoints[i].timePath, preferences.getString((prefix + "timePath").c_str(), "").c_str(), sizeof(currentSettings.dataPoints[i].timePath) - 1);
      strncpy(currentSettings.dataPoints[i].prefix, preferences.getString((prefix + "prefix").c_str(), "").c_str(), sizeof(currentSettings.dataPoints[i].prefix) - 1);
      strncpy(currentSettings.dataPoints[i].suffix, preferences.getString((prefix + "suffix").c_str(), "").c_str(), sizeof(currentSettings.dataPoints[i].suffix) - 1);
      strncpy(currentSettings.dataPoints[i].icon, preferences.getString((prefix + "icon").c_str(), "").c_str(), sizeof(currentSettings.dataPoints[i].icon) - 1);
      currentSettings.dataPoints[i].scrollSpeed = preferences.getInt((prefix + "scroll").c_str());
      currentSettings.dataPoints[i].dataSourceType = (DataSourceType)preferences.getInt((prefix + "srcType").c_str());
      strncpy(currentSettings.dataPoints[i].mqttTopic, preferences.getString((prefix + "topic").c_str(), "").c_str(), sizeof(currentSettings.dataPoints[i].mqttTopic) - 1);
      strncpy(currentSettings.dataPoints[i].yearPrefix, preferences.getString((prefix + "yearPrefix").c_str(), "").c_str(), sizeof(currentSettings.dataPoints[i].yearPrefix) - 1);
      strncpy(currentSettings.dataPoints[i].yearSuffix, preferences.getString((prefix + "yearSuffix").c_str(), "").c_str(), sizeof(currentSettings.dataPoints[i].yearSuffix) - 1);
      currentSettings.dataPoints[i].displayMode = (DisplayMode)preferences.getInt((prefix + "dispMode").c_str(), FOUR_COLUMN); // <-- NEW
      strncpy(currentSettings.dataPoints[i].scrollingText, preferences.getString((prefix + "scrollTxt").c_str(), "").c_str(), sizeof(currentSettings.dataPoints[i].scrollingText) - 1); // <-- NEW
    }
    preferences.end();
  }

  if (currentSettings.presentTimezoneIndex < 0 || currentSettings.presentTimezoneIndex >= NUM_TIMEZONE_OPTIONS) {
    currentSettings.presentTimezoneIndex = 0;
  }
  if (currentSettings.destinationTimezoneIndex < 0 || currentSettings.destinationTimezoneIndex >= NUM_TIMEZONE_OPTIONS) {
    currentSettings.destinationTimezoneIndex = 0;
  }
  setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
  tzset();
}

void loadApiTemplates() {
  File file = LittleFS.open(API_TEMPLATES_FILE, "r");
  if (file) {
    DeserializationError error = deserializeJson(apiTemplatesDoc, file);
    if (error) {
      ESP_LOGE("Templates", "Failed to parse API templates file.");
    }
    file.close();
  } else {
    ESP_LOGW("Templates", "API templates file not found.");
  }
}

void setupMqtt() {
  if (strlen(currentSettings.mqttBroker) == 0) {
    ESP_LOGI("MQTT", "Broker not configured. Skipping MQTT setup.");
    return;
  }
  mqttClient.setServer(currentSettings.mqttBroker, currentSettings.mqttPort);
  mqttClient.setCallback(mqttCallback);
  ESP_LOGI("MQTT", "Client configured for broker %s:%d", currentSettings.mqttBroker, currentSettings.mqttPort);
}

void reconnectMqtt() {
  if (strlen(currentSettings.mqttBroker) == 0) return;

  if (!mqttClient.connected()) {
    ESP_LOGI("MQTT", "Attempting MQTT connection...");
    String clientId = "BTTF-Clock-";
    clientId += String(random(0xffff), HEX);

    bool connected = false;
    if (strlen(currentSettings.mqttUser) > 0) {
        connected = mqttClient.connect(clientId.c_str(), currentSettings.mqttUser, currentSettings.mqttPassword, MQTT_STATUS_TOPIC, 1, true, MQTT_LWT_MESSAGE);
    } else {
        connected = mqttClient.connect(clientId.c_str(), MQTT_STATUS_TOPIC, 1, true, MQTT_LWT_MESSAGE);
    }

    if (connected) {
      ESP_LOGI("MQTT", "Connected to broker!");
      mqttClient.publish(MQTT_STATUS_TOPIC, "online", true);
      for (int i = 0; i < currentSettings.numDataPoints; i++) {
        if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_MQTT && strlen(currentSettings.dataPoints[i].mqttTopic) > 0) {
          mqttClient.subscribe(currentSettings.dataPoints[i].mqttTopic);
          ESP_LOGI("MQTT", "Subscribed to topic: %s", currentSettings.dataPoints[i].mqttTopic);
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
    if (point.dataSourceType == DATA_SOURCE_MQTT && strcmp(point.mqttTopic, topic) == 0) {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, message);

        bool success = false;
        if (error == DeserializationError::Ok) {
            if (point.displayMode == SCROLLING_TEXT) {
                displayPages[i][0] = getJsonVariant(doc.as<JsonVariant>(), point.scrollingText).as<String>();
            } else {
                auto fetch = [&](const char* path) { return getJsonVariant(doc.as<JsonVariant>(), path).as<String>(); };
                displayPages[i][0] = strlen(point.monthPath) > 0 ? fetch(point.monthPath) : "";
                displayPages[i][1] = strlen(point.dayPath) > 0 ? fetch(point.dayPath) : "";
                displayPages[i][2] = strlen(point.yearPath) > 0 ? fetch(point.yearPath) : "";
                displayPages[i][3] = strlen(point.timePath) > 0 ? fetch(point.timePath) : "";
            }
            success = true;
        } else {
            // If it's not JSON, treat the whole payload as the value
            if(point.displayMode == SCROLLING_TEXT) {
                displayPages[i][0] = message;
            } else {
                displayPages[i][0] = ""; displayPages[i][1] = ""; displayPages[i][2] = "";
                displayPages[i][3] = message;
            }
            success = true;
        }

        if (success) {
            for(int j=0; j<4; ++j) lastGoodDisplayPages[i][j] = displayPages[i][j];
            dataPointFetchFailures[i] = 0;
        } else {
            dataPointFetchFailures[i]++;
            if (dataPointFetchFailures[i] >= MAX_FETCH_FAILURES) {
                if(point.displayMode == SCROLLING_TEXT) displayPages[i][0] = "MQTT FAIL";
                else displayPages[i][3] = "MQTT FAIL";
            } else {
                 for(int j=0; j<4; ++j) displayPages[i][j] = lastGoodDisplayPages[i][j];
            }
        }
        break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n--- BOOTING ---");
  if (!LittleFS.begin(true)) { ESP_LOGE("FS", "CRITICAL ERROR: LittleFS Mount Failed."); while(1); }

  loadSettings();
  loadApiTemplates();
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
  if (MDNS.begin(MDNS_HOSTNAME)) { MDNS.addService("http", "tcp", 80);
  }
  setupWebRoutes();
  server.begin();
  ESP_LOGI("Web", "HTTP server started.");
  configTime(0, 0, NTP_SERVERS[0]);
  setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
  tzset();
  setupMqtt();
  runBootSequence();
}

void loop() {
  ArduinoOTA.handle();
  if (WiFi.status() == WL_CONNECTED && strlen(currentSettings.mqttBroker) > 0) {
    if (mqttReconnectRequired || !mqttClient.connected()) {
      unsigned long now = millis();
      if (now - lastMqttReconnectAttempt > MQTT_RECONNECT_INTERVAL_MS) {
        lastMqttReconnectAttempt = now;
        setupMqtt();
        reconnectMqtt();
        mqttReconnectRequired = false;
      }
    }
    mqttClient.loop();
  }

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
        playSound("ACCELERATION");
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
      if(currentSettings.timeTravelSoundToggle){
        playSound("ARRIVAL_THUD");
      }
      break;
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
      // display88MphSpeed(88.0);
      // This function is not defined in the provided code
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
  if (millis() - lastGlitchTime > GLITCH_EFFECT_INTERVAL_MS) {
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

void fetchDataLink() {
    if (!currentSettings.dataLinkEnabled || currentSettings.numDataPoints == 0 || isFetchingData) return;
    DataPoint point = currentSettings.dataPoints[currentPointToFetch];
    if (point.dataSourceType == DATA_SOURCE_MQTT) {
        currentPointToFetch = (currentPointToFetch + 1) % currentSettings.numDataPoints;
        return;
    }

    if (millis() - lastDataLinkFetch < (unsigned long)currentSettings.dataLinkRefreshInterval * 60 * 1000 / currentSettings.numDataPoints) return;
    isFetchingData = true;
    lastDataLinkFetch = millis();

    HTTPClient http;
    http.begin(point.url);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(8192);
        if (deserializeJson(doc, payload) == DeserializationError::Ok) {
            if(point.displayMode == SCROLLING_TEXT) {
                displayPages[currentPointToFetch][0] = getJsonVariant(doc.as<JsonVariant>(), point.scrollingText).as<String>();
            } else {
                auto fetch = [&](const char* path) { return getJsonVariant(doc.as<JsonVariant>(), path).as<String>(); };
                displayPages[currentPointToFetch][0] = strlen(point.monthPath) > 0 ? fetch(point.monthPath) : "";
                displayPages[currentPointToFetch][1] = strlen(point.dayPath) > 0 ? fetch(point.dayPath) : "";
                displayPages[currentPointToFetch][2] = strlen(point.yearPath) > 0 ? fetch(point.yearPath) : "";
                displayPages[currentPointToFetch][3] = strlen(point.timePath) > 0 ? fetch(point.timePath) : "";
            }
            for(int j=0; j<4; ++j) lastGoodDisplayPages[currentPointToFetch][j] = displayPages[currentPointToFetch][j];
            dataPointFetchFailures[currentPointToFetch] = 0;
        } else {
             if(point.displayMode == SCROLLING_TEXT) displayPages[currentPointToFetch][0] = "JSON ERR";
             else displayPages[currentPointToFetch][3] = "JSON ERR";
        }
    } else {
        if(point.displayMode == SCROLLING_TEXT) displayPages[currentPointToFetch][0] = "HTTP ERR";
        else displayPages[currentPointToFetch][3] = "HTTP ERR";
    }
    http.end();

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
        marqueeScrollPositionYear = 0;
        fullRowMarqueeScrollPos = 0;
        marqueeState = M_PAUSED;
        lastMarqueeStateChange = millis();
    }

    DataPoint point = currentSettings.dataPoints[currentPageIndex];

    if (point.displayMode == SCROLLING_TEXT) {
        String textToScroll = "   " + displayPages[currentPageIndex][0] + "   ";
        String viewport = textToScroll.substring(fullRowMarqueeScrollPos, fullRowMarqueeScrollPos + 16);
        
        printToDisplay(targetRow->month, viewport.substring(0, 4).c_str());
        printToDisplay(targetRow->day, viewport.substring(4, 8).c_str());
        printToDisplay(targetRow->year, viewport.substring(8, 12).c_str());
        printToDisplay(targetRow->time, viewport.substring(12, 16).c_str());
        
        if (marqueeState == M_SCROLLING && millis() - lastMarqueeStateChange > (unsigned long)point.scrollSpeed) {
            lastMarqueeStateChange = millis();
            fullRowMarqueeScrollPos++;
            if (fullRowMarqueeScrollPos > textToScroll.length() - 16) {
                marqueeState = M_IDLE;
            }
        }
    } else { // FOUR_COLUMN
        printToDisplay(targetRow->month, displayPages[currentPageIndex][0].c_str());
        printToDisplay(targetRow->day, displayPages[currentPageIndex][1].c_str());

        String yearContent = String(point.yearPrefix) + displayPages[currentPageIndex][2] + String(point.yearSuffix);
        String timeContent = String(point.prefix) + displayPages[currentPageIndex][3] + String(point.suffix);
        String yearCanvas = "   " + yearContent + "   ";
        if (yearCanvas.length() <= 4) {
            printToDisplay(targetRow->year, yearCanvas.c_str());
        } else {
            String yearViewport = yearCanvas.substring(marqueeScrollPositionYear, marqueeScrollPositionYear + 4);
            printToDisplay(targetRow->year, yearViewport.c_str());
        }

        String timeCanvas = "   " + timeContent + "   ";
        if (timeCanvas.length() <= 4) {
            printToDisplay(targetRow->time, timeCanvas.c_str());
        } else {
            String viewport = timeCanvas.substring(marqueeScrollPosition, marqueeScrollPosition + 4);
            printToDisplay(targetRow->time, viewport.c_str());
        }

        if (marqueeState == M_SCROLLING && millis() - lastMarqueeStateChange > (unsigned long)point.scrollSpeed) {
            lastMarqueeStateChange = millis();
            bool timeDone = (timeCanvas.length() <= 4);
            bool yearDone = (yearCanvas.length() <= 4);

            if (!timeDone) {
                marqueeScrollPosition++;
                if (marqueeScrollPosition > timeCanvas.length() - 4) timeDone = true;
            }
            if (!yearDone) {
                marqueeScrollPositionYear++;
                if (marqueeScrollPositionYear > yearCanvas.length() - 4) yearDone = true;
            }
            if (timeDone && yearDone) marqueeState = M_IDLE;
        }
    }

    if (marqueeState == M_PAUSED && millis() - lastMarqueeStateChange > 2000) {
        marqueeState = M_SCROLLING;
        lastMarqueeStateChange = millis();
    }

    targetRow->month.writeDisplay();
    targetRow->day.writeDisplay();
    targetRow->year.writeDisplay();
    targetRow->time.writeDisplay();
    #endif
}