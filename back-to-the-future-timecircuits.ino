// Forcing a recompile to resolve build cache issues.
#include "esp_log.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFiUdp.h>
#include <time.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <LittleFS.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <freertos/semphr.h>
#include <string>

#include "HardwareControl.h"
#include "web_server.h"
#include "api_templates.h"

#ifndef LED_BUILTIN
  #define LED_BUILTIN 2
#endif

#define NTP_SUCCESS_INTERVAL_MS 3600000
#define ANIMATION_UPDATE_INTERVAL_MS 50
#define BOOT_STATE_CHANGE_INTERVAL_MS 2000
#define GLITCH_EFFECT_INTERVAL_MS 60000
#define MQTT_RECONNECT_INTERVAL_MS 5000

#define MQTT_STATUS_TOPIC "bttf-clock/status"
#define MQTT_LWT_MESSAGE "offline"

ClockSettings currentSettings;
struct MarqueeData {
  std::string month;
  std::string day;
  std::string year;
  std::string time;
};

MarqueeData displayPages[5];
MarqueeData lastGoodDisplayPages[5];

WeatherData currentWeatherData;
std::string lastCityName = "";

const TimeZoneEntry TZ_DATA[] = {
  // Global
  { "UTC0", "UTC", "Etc/UTC", "Global" },

  // Americas
  { "NST3:30NDT,M3.2.0,M11.1.0", "Newfoundland (St. John's)", "America/St_Johns", "Americas" },
  { "AST4ADT,M3.2.0,M11.1.0", "Atlantic (Halifax)", "America/Halifax", "Americas" },
  { "EST5EDT,M3.2.0,M11.1.0", "Eastern (New York)", "America/New_York", "Americas" },
  { "CST6CDT,M3.2.0,M11.1.0", "Central (Chicago)", "America/Chicago", "Americas" },
  { "MST7MDT,M3.2.0,M11.1.0", "Mountain (Denver)", "America/Denver", "Americas" },
  { "PST8PDT,M3.2.0,M11.1.0", "Pacific (Los Angeles)", "America/Los_Angeles", "Americas" },
  { "AKST9AKDT,M3.2.0,M11.1.0", "Alaska (Anchorage)", "America/Anchorage", "Americas" },
  { "MST7", "Mountain (Phoenix, No DST)", "America/Phoenix", "Americas" },
  { "HST10", "Hawaii (Honolulu, No DST)", "Pacific/Honolulu", "Americas" },

  // Europe
  { "GMT0BST,M3.5.0/1,M10.5.0", "GMT/BST (London)", "Europe/London", "Europe" },
  { "CET-1CEST,M3.5.0,M10.5.0", "CET/CEST (Berlin)", "Europe/Berlin", "Europe" },
  { "EET-2EEST,M3.5.0/3,M10.5.0/4", "EET/EEST (Athens)", "Europe/Athens", "Europe" },
  { "<+03>-3", "Moscow Standard Time", "Europe/Moscow", "Europe" },
  { "<+03>-3", "Turkey Time (Istanbul)", "Europe/Istanbul", "Europe" },

  // Asia
  { "IST-5:30", "Indian Standard Time (Kolkata)", "Asia/Kolkata", "Asia" },
  { "<+08>-8", "Singapore Standard Time", "Asia/Singapore", "Asia" },
  { "CST-8", "China Standard Time (Shanghai)", "Asia/Shanghai", "Asia" },
  { "KST-9", "Korea Standard Time (Seoul)", "Asia/Seoul", "Asia" },
  { "JST-9", "Japan Standard Time (Tokyo)", "Asia/Tokyo", "Asia" },
  { "<+04>-4", "Gulf Standard Time (Dubai)", "Asia/Dubai", "Asia" },
  
  // Australia & Oceania
  { "AWST-8", "AWST (Perth)", "Australia/Perth", "Australia & Oceania" },
  { "AEST-10AEDT,M10.1.0,M4.1.0/3", "AEST/AEDT (Sydney)", "Australia/Sydney", "Australia & Oceania" },
  { "NZST-12NZDT,M9.5.0,M4.1.0/3", "NZST/NZDT (Auckland)", "Pacific/Auckland", "Australia & Oceania" },
  { "ChST-10", "Chamorro Time (Guam)", "Pacific/Guam", "Australia & Oceania" },
  
  // Africa
  { "WAT-1", "West Africa Time (Lagos)", "Africa/Lagos", "Africa" },
  { "SAST-2", "South Africa Standard Time", "Africa/Johannesburg", "Africa" },
  { "EET-2", "EET (Cairo)", "Africa/Cairo", "Africa" },
  { "EAT-3", "East Africa Time (Nairobi)", "Africa/Nairobi", "Africa" },

  // South America
  { "<-03>3", "Brasilia Time (Sao Paulo)", "America/Sao_Paulo", "South America" },
  { "<-03>3", "Argentina Time (Buenos Aires)", "America/Argentina/Buenos_Aires", "South America" }
};
const int NUM_TIMEZONE_OPTIONS = sizeof(TZ_DATA) / sizeof(TZ_DATA[0]);

const char TZ_JSON[] PROGMEM = "{\"Global\":[{\"value\":0,\"text\":\"UTC\",\"ianaTzName\":\"Etc/UTC\"}],\"Americas\":[{\"value\":1,\"text\":\"Newfoundland (St. John's)\",\"ianaTzName\":\"America/St_Johns\"},{\"value\":2,\"text\":\"Atlantic (Halifax)\",\"ianaTzName\":\"America/Halifax\"},{\"value\":3,\"text\":\"Eastern (New York)\",\"ianaTzName\":\"America/New_York\"},{\"value\":4,\"text\":\"Central (Chicago)\",\"ianaTzName\":\"America/Chicago\"},{\"value\":5,\"text\":\"Mountain (Denver)\",\"ianaTzName\":\"America/Denver\"},{\"value\":6,\"text\":\"Pacific (Los Angeles)\",\"ianaTzName\":\"America/Los_Angeles\"},{\"value\":7,\"text\":\"Alaska (Anchorage)\",\"ianaTzName\":\"America/Anchorage\"},{\"value\":8,\"text\":\"Mountain (Phoenix, No DST)\",\"ianaTzName\":\"America/Phoenix\"},{\"value\":9,\"text\":\"Hawaii (Honolulu, No DST)\",\"ianaTzName\":\"Pacific/Honolulu\"}],\"Europe\":[{\"value\":10,\"text\":\"GMT/BST (London)\",\"ianaTzName\":\"Europe/London\"},{\"value\":11,\"text\":\"CET/CEST (Berlin)\",\"ianaTzName\":\"Europe/Berlin\"},{\"value\":12,\"text\":\"EET/EEST (Athens)\",\"ianaTzName\":\"Europe/Athens\"},{\"value\":13,\"text\":\"Moscow Standard Time\",\"ianaTzName\":\"Europe/Moscow\"},{\"value\":14,\"text\":\"Turkey Time (Istanbul)\",\"ianaTzName\":\"Europe/Istanbul\"}],\"Asia\":[{\"value\":15,\"text\":\"Indian Standard Time (Kolkata)\",\"ianaTzName\":\"Asia/Kolkata\"},{\"value\":16,\"text\":\"Singapore Standard Time\",\"ianaTzName\":\"Asia/Singapore\"},{\"value\":17,\"text\":\"China Standard Time (Shanghai)\",\"ianaTzName\":\"Asia/Shanghai\"},{\"value\":18,\"text\":\"Korea Standard Time (Seoul)\",\"ianaTzName\":\"Asia/Seoul\"},{\"value\":19,\"text\":\"Japan Standard Time (Tokyo)\",\"ianaTzName\":\"Asia/Tokyo\"},{\"value\":20,\"text\":\"Gulf Standard Time (Dubai)\",\"ianaTzName\":\"Asia/Dubai\"}],\"Australia & Oceania\":[{\"value\":21,\"text\":\"AWST (Perth)\",\"ianaTzName\":\"Australia/Perth\"},{\"value\":22,\"text\":\"AEST/AEDT (Sydney)\",\"ianaTzName\":\"Australia/Sydney\"},{\"value\":23,\"text\":\"NZST/NZDT (Auckland)\",\"ianaTzName\":\"Pacific/Auckland\"},{\"value\":24,\"text\":\"Chamorro Time (Guam)\",\"ianaTzName\":\"Pacific/Guam\"}],\"Africa\":[{\"value\":25,\"text\":\"West Africa Time (Lagos)\",\"ianaTzName\":\"Africa/Lagos\"},{\"value\":26,\"text\":\"South Africa Standard Time\",\"ianaTzName\":\"Africa/Johannesburg\"},{\"value\":27,\"text\":\"EET (Cairo)\",\"ianaTzName\":\"Africa/Cairo\"},{\"value\":28,\"text\":\"East Africa Time (Nairobi)\",\"ianaTzName\":\"Africa/Nairobi\"}],\"South America\":[{\"value\":29,\"text\":\"Brasilia Time (Sao Paulo)\",\"ianaTzName\":\"America/Sao_Paulo\"},{\"value\":30,\"text\":\"Argentina Time (Buenos Aires)\",\"ianaTzName\":\"America/Argentina/Buenos_Aires\"}]}";
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

// --- NEW: Temporal Echo Effect State Variables ---
bool isEchoEffectActive = false;
unsigned long echoEffectStartTime = 0;
unsigned long lastEchoCheckTime = 0;
bool isFlickeringNow = false;
unsigned long flickerStartTime = 0;
int flickerDisplayIndex = -1; // 0=Month, 1=Day, 2=Year, 3=Time
// --- END NEW VARIABLES ---

float currentWindSpeed = 0.0;
enum MarqueeState { M_IDLE, M_PAUSED, M_SCROLLING };
MarqueeState marqueeState = M_IDLE;
int currentPageIndex = 0;
unsigned long lastDataLinkFetch = 0;
unsigned long lastMarqueeStateChange = 0;
int marqueeScrollPosition = 0;
int marqueeScrollPositionYear = 0;
volatile bool isFetchingData = false;
int dataPointFetchFailures[5] = {0, 0, 0, 0, 0};
const int MAX_FETCH_FAILURES = 3;
bool isMalfunctioning = false;
unsigned long malfunctionStartTime = 0;
enum MalfunctionPhase { MAL_INACTIVE, MAL_HAYWIRE, MAL_ERROR_MESSAGE, MAL_REBOOT };
MalfunctionPhase currentMalfunctionPhase = MAL_INACTIVE;
volatile int requestsCompleted = 0;

// Mutex for protecting shared display data
SemaphoreHandle_t xDisplayDataMutex;
// Struct to pass parameters to the data fetching task
struct FetchDataParams {
    int pointIndex;
    int totalRequests;
};

struct WeatherTaskParams {
    std::string cityName;
    bool forceGeocode;
};

void fetchDataTask(void* p);
void startTimeTravelAnimation();
void handleDisplayAnimation();
void handleTemporalEcho();
void handleBootSequence();
void handleGlitchEffect();
void handleMalfunction();
void restoreDisplayAfterGlitch();
void handlePresetCycling();
void handleSleepSchedule();
void updateNormalClockDisplay();
void fetchDataLink();
void updateMarqueeDisplay();
void listAllFiles();
void runBootSequence();
void setupMqtt();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void fetchWeatherData(WeatherTaskParams* params);

// NEW FUNCTION to encode URL parameters
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
  preferences.putString("mqttBroker", currentSettings.mqttBroker.c_str());
  preferences.putInt("mqttPort", currentSettings.mqttPort);
  preferences.putString("mqttUser", currentSettings.mqttUser.c_str());
  preferences.putString("mqttPass", currentSettings.mqttPassword.c_str());
  preferences.putBool("weatherMode", currentSettings.weatherModeEnabled);
  preferences.putString("cityName", currentSettings.cityName.c_str());
  preferences.putBool("useMetric", currentSettings.useMetricUnits);
  preferences.putFloat("latitude", currentSettings.latitude);
  preferences.putFloat("longitude", currentSettings.longitude);
  for (int i = 0; i < 5; i++) {
    String prefix = "dp" + String(i) + "_";
    preferences.putString((prefix + "url").c_str(), currentSettings.dataPoints[i].url.c_str());
    preferences.putString((prefix + "monthPath").c_str(), currentSettings.dataPoints[i].monthPath.c_str());
    preferences.putString((prefix + "dayPath").c_str(), currentSettings.dataPoints[i].dayPath.c_str());
    preferences.putString((prefix + "yearPath").c_str(), currentSettings.dataPoints[i].yearPath.c_str());
    preferences.putString((prefix + "timePath").c_str(), currentSettings.dataPoints[i].timePath.c_str());
    preferences.putString((prefix + "prefix").c_str(), currentSettings.dataPoints[i].prefix.c_str());
    preferences.putString((prefix + "suffix").c_str(), currentSettings.dataPoints[i].suffix.c_str());
    preferences.putString((prefix + "icon").c_str(), currentSettings.dataPoints[i].icon.c_str());
    preferences.putInt((prefix + "scroll").c_str(), currentSettings.dataPoints[i].scrollSpeed);
    preferences.putInt((prefix + "srcType").c_str(), (int)currentSettings.dataPoints[i].dataSourceType);
    preferences.putString((prefix + "topic").c_str(), currentSettings.dataPoints[i].mqttTopic.c_str());
    preferences.putString((prefix + "yearPrefix").c_str(), currentSettings.dataPoints[i].yearPrefix.c_str());
    preferences.putString((prefix + "yearSuffix").c_str(), currentSettings.dataPoints[i].yearSuffix.c_str());
    preferences.putInt((prefix + "dispMode").c_str(), (int)currentSettings.dataPoints[i].displayMode);
    preferences.putString((prefix + "scrollTxt").c_str(), currentSettings.dataPoints[i].scrollingText.c_str());
    preferences.putString((prefix + "authKey").c_str(), currentSettings.dataPoints[i].authHeaderKey.c_str());
    preferences.putString((prefix + "authVal").c_str(), currentSettings.dataPoints[i].authHeaderValue.c_str());
    preferences.putInt((prefix + "httpMethod").c_str(), (int)currentSettings.dataPoints[i].httpMethod);
    preferences.putString((prefix + "reqBody").c_str(), currentSettings.dataPoints[i].requestBody.c_str());
    preferences.putString((prefix + "apiKey").c_str(), currentSettings.dataPoints[i].apiExampleKey.c_str());
  }
  preferences.end();
  setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
  tzset();
}

void loadSettings() {
  preferences.begin(PREFERENCES_NAMESPACE, true);
  bool needsInit = !preferences.isKey("destYear");
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
    currentSettings.mqttBroker = "broker.emqx.io";
    currentSettings.mqttPort = 1883;
    currentSettings.mqttUser = "";
    currentSettings.mqttPassword = "";
    currentSettings.weatherModeEnabled = false;
    currentSettings.cityName = "New York";
    currentSettings.useMetricUnits = false;
    currentSettings.latitude = 40.7128;
    currentSettings.longitude = -74.0060;
    for (int i = 0; i < 5; i++) {
        currentSettings.dataPoints[i] = {};
    }
    saveSettings();
  } else {
    ESP_LOGI("SETTINGS", "Loading settings from NVS.");
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
    currentSettings.mqttBroker = preferences.getString("mqttBroker", "").c_str();
    currentSettings.mqttPort = preferences.getInt("mqttPort");
    currentSettings.mqttUser = preferences.getString("mqttUser", "").c_str();
    currentSettings.mqttPassword = preferences.getString("mqttPass", "").c_str();
    currentSettings.weatherModeEnabled = preferences.getBool("weatherMode", false);
    currentSettings.cityName = preferences.getString("cityName", "New York").c_str();
    currentSettings.useMetricUnits = preferences.getBool("useMetric", false);
    currentSettings.latitude = preferences.getFloat("latitude", 40.7128);
    currentSettings.longitude = preferences.getFloat("longitude", -74.0060);

    for (int i = 0; i < 5; i++) {
      String prefix = "dp" + String(i) + "_";
      currentSettings.dataPoints[i].url = preferences.getString((prefix + "url").c_str(), "").c_str();
      currentSettings.dataPoints[i].monthPath = preferences.getString((prefix + "monthPath").c_str(), "").c_str();
      currentSettings.dataPoints[i].dayPath = preferences.getString((prefix + "dayPath").c_str(), "").c_str();
      currentSettings.dataPoints[i].yearPath = preferences.getString((prefix + "yearPath").c_str(), "").c_str();
      currentSettings.dataPoints[i].timePath = preferences.getString((prefix + "timePath").c_str(), "").c_str();
      currentSettings.dataPoints[i].prefix = preferences.getString((prefix + "prefix").c_str(), "").c_str();
      currentSettings.dataPoints[i].suffix = preferences.getString((prefix + "suffix").c_str(), "").c_str();
      currentSettings.dataPoints[i].icon = preferences.getString((prefix + "icon").c_str(), "").c_str();
      currentSettings.dataPoints[i].scrollSpeed = preferences.getInt((prefix + "scroll").c_str());
      currentSettings.dataPoints[i].dataSourceType = (DataSourceType)preferences.getInt((prefix + "srcType").c_str());
      currentSettings.dataPoints[i].mqttTopic = preferences.getString((prefix + "topic").c_str(), "").c_str();
      currentSettings.dataPoints[i].yearPrefix = preferences.getString((prefix + "yearPrefix").c_str(), "").c_str();
      currentSettings.dataPoints[i].yearSuffix = preferences.getString((prefix + "yearSuffix").c_str(), "").c_str();
      currentSettings.dataPoints[i].displayMode = (DisplayMode)preferences.getInt((prefix + "dispMode").c_str(), 0);
      currentSettings.dataPoints[i].scrollingText = preferences.getString((prefix + "scrollTxt").c_str(), "").c_str();
      currentSettings.dataPoints[i].authHeaderKey = preferences.getString((prefix + "authKey").c_str(), "").c_str();
      currentSettings.dataPoints[i].authHeaderValue = preferences.getString((prefix + "authVal").c_str(), "").c_str();
      currentSettings.dataPoints[i].httpMethod = (HttpMethod)preferences.getInt((prefix + "httpMethod").c_str(), 0);
      currentSettings.dataPoints[i].requestBody = preferences.getString((prefix + "reqBody").c_str(), "").c_str();
      currentSettings.dataPoints[i].apiExampleKey = preferences.getString((prefix + "apiKey").c_str(), "").c_str();
    }
  }
  preferences.end();
  if (currentSettings.presentTimezoneIndex < 0 || currentSettings.presentTimezoneIndex >= NUM_TIMEZONE_OPTIONS) {
    currentSettings.presentTimezoneIndex = 0;
  }
  if (currentSettings.destinationTimezoneIndex < 0 || currentSettings.destinationTimezoneIndex >= NUM_TIMEZONE_OPTIONS) {
    currentSettings.destinationTimezoneIndex = 0;
  }
  setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
  tzset();
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
                     "&daily=temperature_2m_max,temperature_2m_min" + 
                     "&hourly=temperature_2m,weather_code" +
                     "&forecast_days=1" +
                     "&temperature_unit=" + tempUnit + "&wind_speed_unit=" + speedUnit;
        if (http.begin(client, weatherUrl)) {
            int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                DynamicJsonDocument doc(2048);
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

                        time_t now;
                        time(&now);
                        struct tm timeinfo;
                        localtime_r(&now, &timeinfo);
                        int currentHour = timeinfo.tm_hour;
                        for (int j = 0; j < 3; j++) {
                            int forecastHour = currentHour + j + 1;
                            if (forecastHour < 24) {
                                currentWeatherData.hourlyTemp[j] = doc["hourly"]["temperature_2m"][forecastHour];
                                currentWeatherData.hourlyCode[j] = doc["hourly"]["weather_code"][forecastHour];
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


void listAllFiles() {
  Serial.println(F("\n--- Listing all files in LittleFS ---"));
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while(file){
      Serial.print(F("  FILE: "));
      Serial.print(file.name());
      Serial.print(F("\tSIZE: "));
      Serial.println(file.size());
      file.close();
      file = root.openNextFile();
  }
  Serial.println(F("--- End of file list ---\n"));
  root.close();
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
        connected = mqttClient.connect(clientId.c_str(), currentSettings.mqttUser.c_str(), currentSettings.mqttPassword.c_str(), MQTT_STATUS_TOPIC, 1, true, MQTT_LWT_MESSAGE);
    } else {
        connected = mqttClient.connect(clientId.c_str(), MQTT_STATUS_TOPIC, 1, true, MQTT_LWT_MESSAGE);
    }

    if (connected) {
      ESP_LOGI("MQTT", "Connected to broker!");
      mqttClient.publish(MQTT_STATUS_TOPIC, "online", true);
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
                weatherPage = (weatherPage + 1) % 8;
                lastPageChange = millis();
            }
            
            const char* icon = getIconForWeatherCode(currentWeatherData.weatherCode);
            switch(weatherPage) {
                case 0: // Temperature
                    printToDisplay(lastRow.month, "TEM", 1);
                    printToDisplay(lastRow.day, icon, 2);
                    dtostrf(currentWeatherData.temperature, 4, 1, buffer);
                    printToDisplay(lastRow.year, buffer);
                    printToDisplay(lastRow.time, currentSettings.useMetricUnits ? "CEL" : "DEG");
                    break;
                case 1: // Apparent Temperature
                    printToDisplay(lastRow.month, "FEE", 1);
                    printToDisplay(lastRow.day, icon, 2);
                    dtostrf(currentWeatherData.apparentTemperature, 4, 1, buffer);
                    printToDisplay(lastRow.year, buffer);
                    printToDisplay(lastRow.time, currentSettings.useMetricUnits ? "CEL" : "DEG");
                    break;
                case 2: // Wind Speed
                    printToDisplay(lastRow.month, "WIN", 1);
                    printToDisplay(lastRow.day, icon, 2);
                    dtostrf(currentWeatherData.windSpeed, 4, 1, buffer);
                    printToDisplay(lastRow.year, buffer);
                    printToDisplay(lastRow.time, currentSettings.useMetricUnits ? "KMH" : "MPH");
                    break;
                case 3: // Humidity
                    printToDisplay(lastRow.month, "HUM", 1);
                    printToDisplay(lastRow.day, icon, 2);
                    sprintf(buffer, "%d", currentWeatherData.humidity);
                    printToDisplay(lastRow.year, buffer);
                    printToDisplay(lastRow.time, "%");
                    break;
                case 4: // Daily Forecast
                    printToDisplay(lastRow.month, "HI", 1);
                    printToDisplay(lastRow.day, "LO", 2);
                    dtostrf(currentWeatherData.dailyHigh, 4, 0, buffer);
                    printToDisplay(lastRow.year, buffer);
                    dtostrf(currentWeatherData.dailyLow, 4, 0, buffer);
                    printToDisplay(lastRow.time, buffer);
                    break;
                case 5: // Forecast for Hour + 1
                    printToDisplay(lastRow.month, "HR", 1);
                    printToDisplay(lastRow.day, "+1", 2);
                    dtostrf(currentWeatherData.hourlyTemp[0], 4, 1, buffer);
                    printToDisplay(lastRow.year, buffer);
                    printToDisplay(lastRow.time, getIconForWeatherCode(currentWeatherData.hourlyCode[0]));
                    break;
                case 6: // Forecast for Hour + 2
                    printToDisplay(lastRow.month, "HR", 1);
                    printToDisplay(lastRow.day, "+2", 2);
                    dtostrf(currentWeatherData.hourlyTemp[1], 4, 1, buffer);
                    printToDisplay(lastRow.year, buffer);
                    printToDisplay(lastRow.time, getIconForWeatherCode(currentWeatherData.hourlyCode[1]));
                    break;
                case 7: // Forecast for Hour + 3
                    printToDisplay(lastRow.month, "HR", 1);
                    printToDisplay(lastRow.day, "+3", 2);
                    dtostrf(currentWeatherData.hourlyTemp[2], 4, 1, buffer);
                    printToDisplay(lastRow.year, buffer);
                    printToDisplay(lastRow.time, getIconForWeatherCode(currentWeatherData.hourlyCode[2]));
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
                if (dataPointFetchFailures[i] >= MAX_FETCH_FAILURES) {
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
    FetchDataParams* params = (FetchDataParams*)p;
    int pointIndex = params->pointIndex;
    int totalRequests = params->totalRequests;
    delete params;

    DataPoint point = currentSettings.dataPoints[pointIndex];
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();
    // Bypassing certificate validation

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
                // Lock mutex before writing to shared data
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
        if (dataPointFetchFailures[pointIndex] >= MAX_FETCH_FAILURES) {
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


void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println(F("\n\n--- BOOTING ---"));
  if (!LittleFS.begin(true)) {
    ESP_LOGE("FS", "CRITICAL ERROR: LittleFS Mount Failed.");
    while(1);
  }
  
  listAllFiles();
  loadSettings();
  // Create the mutex for thread-safe data access
  xDisplayDataMutex = xSemaphoreCreateMutex();

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
  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
  }
  setupWebRoutes();
  server.begin();
  ESP_LOGI("Web", "HTTP server started.");
  configTime(0, 0, NTP_SERVERS[0]);
  setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
  tzset();
  setupMqtt();
  ESP_LOGI("Memory", "Free heap after setup: %u bytes", ESP.getFreeHeap());
  runBootSequence();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED && !currentSettings.mqttBroker.empty()) {
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
    
    handleTemporalEcho();
    if (!isFlickeringNow) {
      handleGlitchEffect();
      
      if (currentSettings.weatherModeEnabled) {
        static unsigned long lastWeatherFetch = 0;
        if (millis() - lastWeatherFetch > 300000) { // Fetch every 5 minutes
          lastWeatherFetch = millis();
          WeatherTaskParams* params = new WeatherTaskParams{currentSettings.cityName, false};
          xTaskCreate(fetchWeatherDataTask, "fetchWeatherDataTask", 8192, params, 1, NULL);
        }
      }

      handlePresetCycling();
      handleSleepSchedule();
      if (currentSettings.dataLinkEnabled) {
        fetchDataLink();
        updateMarqueeDisplay();
      } else {
        updateNormalClockDisplay();
        if (currentSettings.weatherModeEnabled) {
            handleWeatherDisplay();
        }
      }
    }
  }

  handleDisplayAnimation();
  static unsigned long lastNtpUpdate = 0;
  if (ntpSyncRequested || (!timeSynchronized && millis() > 10000) || (timeSynchronized && millis() - lastNtpUpdate > NTP_SUCCESS_INTERVAL_MS)) {
    configTime(0, 0, NTP_SERVERS[currentNtpServerIndex]);
    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
    tzset();
    struct tm timeinfo;
    if(getLocalTime(&timeinfo, 5000)){
      timeSynchronized = true;
    } else {
      timeSynchronized = false;
    }
    currentNtpServerIndex = (currentNtpServerIndex + 1) % NUM_NTP_SERVERS;
    lastNtpUpdate = millis();
    ntpSyncRequested = false;
  }
}

void startTimeTravelAnimation() {
    if (isAnimating) { return; }
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
            switch(currentSettings.animationStyle) {
                case ANIMATION_SEQUENTIAL_FLICKER:
                case ANIMATION_RANDOM_FLICKER:
                case ANIMATION_ALL_DISPLAYS_RANDOM:
                case ANIMATION_COUNTING_UP:
           
                    animateDisplayRowRandomly(destRow);
                    animateDisplayRowRandomly(presRow);
                    animateDisplayRowRandomly(lastRow);
                    break;
                case ANIMATION_WAVE_FLICKER:
                     animateWaveformCollapse(elapsed, currentSettings.timeTravelAnimationDuration);
                     break;
                case ANIMATION_TORNADO_FLICKER:
                    animateTornadoFlicker();
                    break;
                case ANIMATION_CAPACITOR_CHARGE_UP:
                    animateCapacitorChargeUp(elapsed, currentSettings.timeTravelAnimationDuration);
                    break;
                case ANIMATION_DIGITAL_RAIN:
                    animateDigitalRain(elapsed, currentSettings.timeTravelAnimationDuration);
                    break;
                case ANIMATION_WAVEFORM_COLLAPSE:
                    animateWaveformCollapse(elapsed, currentSettings.timeTravelAnimationDuration);
                    break;
                case ANIMATION_TIMELINE_SKIM:
                    animateTimelineSkim(elapsed, currentSettings.timeTravelAnimationDuration, currentSettings.destinationYear);
                    break;
            }
            lastAnimationFrameTime = currentTime;
        }
        break;
    case ANIM_COMPLETE:
      isAnimating = false;
      updateNormalClockDisplay();
      if(currentSettings.timeTravelSoundToggle){
        playSound("ARRIVAL_THUD");
      }
      
      // --- NEW: Trigger the Temporal Echo Effect ---
      isEchoEffectActive = true;
      echoEffectStartTime = millis();
      lastEchoCheckTime = millis();
      ESP_LOGI("FX", "Temporal Echo effect activated.");
      // --- END TRIGGER ---

      break;
  }
  #endif
}

/****************************************************************
 * Handles the "Temporal Echo" effect after a time jump.
 * For a few minutes, a random display on the "Present Time"
 * row will occasionally flicker to show the "Last Time Departed"
 * value for a split second.
 ****************************************************************/
void handleTemporalEcho() {
  #if ENABLE_HARDWARE
  // If the effect isn't active, do nothing.
  if (!isEchoEffectActive) {
    return;
  }

  // Effect Duration: Deactivate after 3 minutes (180,000 ms)
  if (millis() - echoEffectStartTime > 180000) {
    isEchoEffectActive = false;
    isFlickeringNow = false; // Ensure flicker is also off
    ESP_LOGI("FX", "Temporal Echo effect deactivated.");
    return;
  }

  // --- State 1: We are currently in a flicker ---
  if (isFlickeringNow) {
    // The flicker lasts for a split second (150 ms)
    if (millis() - flickerStartTime > 150) {
      isFlickeringNow = false;
      flickerDisplayIndex = -1;
      updateNormalClockDisplay(); // Restore the correct time immediately
    }
    return;
    // Do nothing else while a flicker is happening
  }

  // --- State 2: Check if it's time to *maybe* start a new flicker ---
  // Check every 10 seconds for a chance to flicker
  if (millis() - lastEchoCheckTime > 10000) {
    lastEchoCheckTime = millis();
    // Give it a 25% chance of happening every 10 seconds to make it feel random
    if (random(100) < 25) {
      isFlickeringNow = true;
      flickerStartTime = millis();
      flickerDisplayIndex = random(4); // Pick one of the 4 displays to flicker

      // Prepare the "Last Time Departed" data to be displayed
      struct tm lastTimeDepartedInfo = {0};
      lastTimeDepartedInfo.tm_year = currentSettings.lastTimeDepartedYear - 1900;
      lastTimeDepartedInfo.tm_mon = currentSettings.lastTimeDepartedMonth - 1;
      lastTimeDepartedInfo.tm_mday = currentSettings.lastTimeDepartedDay;
      lastTimeDepartedInfo.tm_hour = currentSettings.lastTimeDepartedHour;
      lastTimeDepartedInfo.tm_min = currentSettings.lastTimeDepartedMinute;
      const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
      char buffer[5];
      ESP_LOGD("FX", "Echo Flicker: Display %d", flickerDisplayIndex);

      // Overwrite just ONE segment of the PRESENT row with the PAST data
      switch (flickerDisplayIndex) {
        case 0: // Month
          printToDisplay(presRow.month, months[lastTimeDepartedInfo.tm_mon], 1);
          presRow.month.writeDisplay();
          break;
        case 1: // Day
          sprintf(buffer, "%02d", lastTimeDepartedInfo.tm_mday);
          printToDisplay(presRow.day, buffer, 2);
          presRow.day.writeDisplay();
          break;
        case 2: // Year
          sprintf(buffer, "%04d", currentSettings.lastTimeDepartedYear);
          printToDisplay(presRow.year, buffer);
          presRow.year.writeDisplay();
          break;
        case 3: // Time
          char timeBuffer[5];
          sprintf(timeBuffer, "%02d%02d", lastTimeDepartedInfo.tm_hour, lastTimeDepartedInfo.tm_min);
          presRow.time.clear();
          presRow.time.writeDigitAscii(0, timeBuffer[0]);
          presRow.time.writeDigitAscii(1, timeBuffer[1] | 0x80);
          // Add decimal point
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