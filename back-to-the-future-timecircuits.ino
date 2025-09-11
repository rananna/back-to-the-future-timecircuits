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
#include <freertos/queue.h>
#include <string>
#include <LCBUrl.h>
#include <ArduinoOTA.h>

#include "HardwareControl.h"
#include "web_server.h"
#include "api_templates.h"
#include "EventManager.h"
#include "AnimationManager.h"
#include "DisplayManager.h"
#include "DataManager.h"
#include "MqttManager.h"

// Audio Library Includes
#include "Audio.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// --- CONFIGURABLE CONSTANTS ---
const unsigned long WIFI_CONNECT_TIMEOUT = 15000; // 15 seconds
const unsigned int MQTT_INITIAL_RETRY_INTERVAL = 5000; // 5 seconds
const unsigned int MQTT_MAX_RETRY_INTERVAL = 60000; // 1 minute
const unsigned long NTP_INITIAL_SYNC_DELAY = 2000; // 2 seconds
const unsigned long DISPLAY_UPDATE_INTERVAL = 250; // Milliseconds between display updates.

// --- ASYNCHRONOUS WIFI STATE MANAGEMENT ---
enum WifiState {
  WIFI_STATE_CONNECTING,
  WIFI_STATE_START_PORTAL,
  WIFI_STATE_PORTAL_RUNNING,
  WIFI_STATE_CONNECTED
};
WifiState wifiState = WIFI_STATE_CONNECTING;
unsigned long wifiConnectStartTime = 0;
TaskHandle_t wifiManagerTaskHandle = NULL;
// --- ONE-TIME LOGGING FLAGS ---
bool logConnectingPrinted = false;
bool logPortalMsgPrinted = false;
bool logConnectedPrinted = false;
// --- MQTT EXPONENTIAL BACKOFF ---
unsigned long nextMqttReconnectAttempt = 0;
unsigned int mqttReconnectInterval = MQTT_INITIAL_RETRY_INTERVAL;
// --- THEMED BOOT SEQUENCE ---
BootSequenceState bootState = BOOT_INACTIVE;
int speedometerValue = 0;
// --- DISPLAY MODE STATE MACHINE ---
DisplayModeState currentDisplayMode = NORMAL_CLOCK;

// --- UNIFIED AUDIO GLOBALS ---
Audio audio;
char currentSoundFile[MAX_FILENAME_LENGTH] = "";


char MQTT_UNIQUE_ID[19];

// --- FUNCTION PROTOTYPES ---
void handlePresetCycling();
void handleSleepSchedule();
void handleSequencer();
bool isMarketOpen();
bool attemptHardwareInit();
void checkDataFetchStatusTask(void* p);
void startAudioStream(const char* url, bool is_tts);
void stopAudioStream();
void wifiManagerTask(void *pvParameters);
void updateDisplaySegment(int row, int segment, const std::string& text);
void testDecimalPointFlashing();
void handleScheduledAnimation();

// --- GLOBAL VARIABLES & CONSTANTS ---
ClockSettings currentSettings;
MarqueeData displayPages[5];
MarqueeData lastGoodDisplayPages[5];
WeatherData currentWeatherData;
StockData stockData[3];
unsigned long lastStockDataFetch = 0;
unsigned long lastDisplayUpdateTime = 0;
std::string lastCityName = "";
unsigned long bootTimestamp = 0;
bool hardwareInitialized = false;
const TimeZoneEntry TZ_DATA[] = {
	{ "UTC0", "UTC", "Etc/UTC", "Global" },
	{ "NST3:30NDT,M3.2.0,M11.1.0", "Newfoundland (St. John's)", "America/St_Johns", "Americas" },
	{ "AST4ADT,M3.2.0,M11.1.0", "Atlantic (Halifax)", "America/Halifax", "Americas" },
	{ "EST5EDT,M3.2.0,M11.1.0", "Eastern (New York)", "America/New_York", "Americas" },
	{ "CST6CDT,M3.2.0,M11.1.0", "Central (Chicago)", "America/Chicago", "Americas" },
	{ "MST7MDT,M3.2.0,M11.1.0", "Mountain (Denver)", "America/Denver", "Americas" },
	{ "PST8PDT,M3.2.0,M11.1.0", "Pacific (Los Angeles)", "America/Los_Angeles", "Americas" },
	{ "AKST9AKDT,M3.2.0,M11.1.0", "Alaska (Anchorage)", "America/Anchorage", "Americas" },
	{ "MST7", "Mountain (Phoenix, No DST)", "America/Phoenix", "Americas" },
	{ "HST10", "Hawaii (Honolulu, No DST)", "Pacific/Honolulu", "Americas" },
	{ "GMT0BST,M3.5.0/1,M10.5.0", "GMT/BST (London)", "Europe/London", "Europe" },
	{ "CET-1CEST,M3.5.0,M10.5.0", "CET/CEST (Berlin)", "Europe/Berlin", "Europe" },
	{ "EET-2EEST,M3.5.0/3,M10.5.0/4", "EET/EEST (Athens)", "Europe/Athens", "Europe" },
	{ "<+03>-3", "Moscow Standard Time", "Europe/Moscow", "Europe" },
	{ "<+03>-3", "Turkey Time (Istanbul)","Europe/Istanbul", "Europe" },
	{ "IST-5:30", "Indian Standard Time (Kolkata)", "Asia/Kolkata", "Asia" },
	{ "<+08>-8", "Singapore Standard Time", "Asia/Singapore", "Asia" },
	{ "CST-8", "China Standard Time (Shanghai)", "Asia/Shanghai", "Asia" },
	{ "KST-9", "Korea Standard Time (Seoul)", "Asia/Seoul", "Asia" },
	{ "JST-9", "Japan Standard Time (Tokyo)", "Asia/Tokyo", "Asia" },
	{ "<+04>-4", "Gulf Standard Time (Dubai)", "Asia/Dubai", "Asia" },
	{ "AWST-8", "AWST (Perth)", "Australia/Perth", "Australia & Oceania" },
	{ "AEST-10AEDT,M10.1.0,M4.1.0/3", "AEST/AEDT (Sydney)", "Australia/Sydney", "Australia & Oceania" },
	{ "NZST-12NZDT,M9.5.0,M4.1.0/3", "NZST/NZDT (Auckland)", "Pacific/Auckland", "Australia & Oceania" },
	{ "ChST-10", "Chamorro Time (Guam)", "Pacific/Guam", "Australia & Oceania" },
	{ "WAT-1", "West Africa Time (Lagos)", "Africa/Lagos", "Africa" },
	{ "SAST-2", "South Africa Standard Time", "Africa/Johannesburg", "Africa" },
	{ "EET-2","EET (Cairo)", "Africa/Cairo", "Africa" },
	{ "EAT-3", "East Africa Time (Nairobi)", "Africa/Nairobi", "Africa" },
	{ "<-03>3", "Brasilia Time (Sao Paulo)", "America/Sao_Paulo", "South America" },
	{ "<-03>3", "Argentina Time (Buenos Aires)", "America/Argentina/Buenos_Aires", "South America" }
};
const int NUM_TIMEZONE_OPTIONS = sizeof(TZ_DATA) / sizeof(TZ_DATA[0]);
const char TZ_JSON[] PROGMEM = "{\"Global\":[{\"value\":0,\"text\":\"UTC\",\"ianaTzName\":\"Etc/UTC\"}],\"Americas\":[{\"value\":1,\"text\":\"Newfoundland (St. John's)\",\"ianaTzName\":\"America/St_Johns\"},{\"value\":2,\"text\":\"Atlantic (Halifax)\",\"ianaTzName\":\"America/Halifax\"},{\"value\":3,\"text\":\"Eastern (New York)\",\"ianaTzName\":\"America/New_York\"},{\"value\":4,\"text\":\"Central (Chicago)\",\"ianaTzName\":\"America/Chicago\"},{\"value\":5,\"text\":\"Mountain (Denver)\",\"ianaTzName\":\"America/Denver\"},{\"value\":6,\"text\":\"Pacific (Los Angeles)\",\"ianaTzName\":\"America/Los_Angeles\"},{\"value\":7,\"text\":\"Alaska (Anchorage)\",\"ianaTzName\":\"America/Anchorage\"},{\"value\":8,\"text\":\"Mountain (Phoenix, No DST)\",\"ianaTzName\":\"America/Phoenix\"},{\"value\":9,\"text\":\"Hawaii (Honolulu, No DST)\",\"ianaTzName\":\"Pacific/Honolulu\"}],\"Europe\":[{\"value\":10,\"text\":\"GMT/BST (London)\",\"ianaTzName\":\"Europe/London\"},{\"value\":11,\"text\":\"CET/CEST (Berlin)\",\"ianaTzName\":\"Europe/Berlin\"},{\"value\":12,\"text\":\"EET/EEST (Athens)\",\"ianaTzName\":\"Europe/Athens\"},{\"value\":13,\"text\":\"Moscow Standard Time\",\"ianaTzName\":\"Europe/Moscow\"},{\"value\":14,\"text\":\"Turkey Time (Istanbul)\",\"ianaTzName\":\"Europe/Istanbul\"}],\"Asia\":[{\"value\":15,\"text\":\"Indian Standard Time (Kolkata)\",\"ianaTzName\":\"Asia/Kolkata\"},{\"value\":16,\"text\":\"Singapore Standard Time\",\"ianaTzName\":\"Asia/Singapore\"},{\"value\":17,\"text\":\"China Standard Time (Shanghai)\",\"ianaTzName\":\"Asia/Shanghai\"},{\"value\":18,\"text\":\"Korea Standard Time (Seoul)\",\"ianaTzName\":\"Asia/Seoul\"},{\"value\":19,\"text\":\"Japan Standard Time (Tokyo)\",\"ianaTzName\":\"Asia/Tokyo\"},{\"value\":20,\"text\":\"Gulf Standard Time (Dubai)\",\"ianaTzName\":\"Asia/Dubai\"}],\"Australia & Oceania\":[{\"value\":21,\"text\":\"AWST (Perth)\",\"ianaTzName\":\"Australia/Perth\"},{\"value\":23,\"text\":\"NZST/NZDT (Auckland)\",\"ianaTzName\":\"Pacific/Auckland\"},{\"value\":22,\"text\":\"AEST/AEDT (Sydney)\",\"ianaTzName\":\"Australia/Sydney\"},{\"value\":24,\"text\":\"Chamorro Time (Guam)\",\"ianaTzName\":\"Pacific/Guam\"}],\"Africa\":[{\"value\":25,\"text\":\"West Africa Time (Lagos)\",\"ianaTzName\":\"Africa/Lagos\"},{\"value\":26,\"text\":\"South Africa Standard Time\",\"ianaTzName\":\"Africa/Johannesburg\"},{\"value\":27,\"text\":\"EET (Cairo)\",\"ianaTzName\":\"Africa/Cairo\"},{\"value\":28,\"text\":\"East Africa Time (Nairobi)\",\"ianaTzName\":\"Africa/Nairobi\"}],\"South America\":[{\"value\":29,\"text\":\"Brasilia Time (Sao Paulo)\",\"ianaTzName\":\"America/Sao_Paulo\"},{\"value\":30,\"text\":\"Argentina Time (Buenos Aires)\",\"ianaTzName\":\"America/Argentina/Buenos_Aires\"}]}";
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
AnimationPhase currentPhase = ANIM_INACTIVE;
bool isStyledAnimating = false;
unsigned long styledAnimationStartTime = 0;
AnimationPhase currentStyledPhase = ANIM_INACTIVE;
bool isDisplayAsleep = false;
unsigned long bootStateStartTime = 0;
unsigned long lastGlitchTime = 0;
bool isGlitching = false;
unsigned long glitchStartTime = 0;
unsigned long lastPresetCycleTime = 0;
unsigned long lastScheduledAnimationTime = 0;
bool isEchoEffectActive = false;
unsigned long echoEffectStartTime = 0;
unsigned long lastEchoCheckTime = 0;
bool isFlickeringNow = false;
unsigned long flickerStartTime = 0;
int flickerDisplayIndex = -1;
MarqueeState marqueeState = M_IDLE;
unsigned long lastDataLinkFetch = 0;
unsigned long lastMarqueeStateChange = 0;
int marqueeScrollPosition = 0;
int marqueeScrollPositionYear = 0;
volatile bool isFetchingData = false;
int dataPointFetchFailures[5] = {0, 0, 0, 0, 0};
const int MAX_FETCH_FAILURES = 3;
bool isMalfunctioning = false;
unsigned long malfunctionStartTime = 0;
MalfunctionPhase currentMalfunctionPhase = MAL_INACTIVE;
volatile int requestsCompleted = 0;
int currentPageIndex = 0;
bool isMessageOverrideActive = false;
String overrideMessageLine1 = "";
String overrideMessageLine2 = "";
String overrideMessageLine3 = "";
bool isMarqueeOverrideActive = false;
String marqueeOverrideMessage = "";
unsigned long marqueeOverrideEndTime = 0;
SemaphoreHandle_t xDisplayDataMutex;

SequenceStep sequence[20];
int currentSequenceStep = 0;
unsigned long sequenceStepStartTime = 0;
bool isSequenceActive = false;

// --- NEW DISPLAY STATE MACHINE ---
enum DisplayState {
    STATE_NORMAL_CLOCK,
    STATE_MESSAGE_OVERRIDE,
    STATE_MALFUNCTION,
    STATE_ANIMATING,
    STATE_STOCK_TICKER,
    STATE_MARQUEE_OVERRIDE,
    STATE_DATA_LINK,
    STATE_WEATHER
};
DisplayState currentDisplayState = STATE_NORMAL_CLOCK;


// --- Callback function to handle audio events ---
void audio_info(Audio::msg_t m) {
    if (m.e == Audio::evt_eof) {
        Serial.printf("AUDIO_LOG: Finished playing sound: %s\n", currentSoundFile);
        currentSoundFile[0] = '\0'; // Clear the filename
        // Update Home Assistant that audio is idle
        if (mqttClient.connected()) {
            mqttClient.publish((String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/audio/state").c_str(), "IDLE", true);
        }
    }
}

/**
 * @brief A dedicated FreeRTOS task to handle audio processing.
 * @details This task runs in a continuous loop, calling audio.loop() to ensure
 * the I2S buffer is always fed with audio data.
 This prevents animations or other
 * long-running code in the main loop from starving the audio system and causing
 * stuttering or delays.
 * @param pvParameters Standard FreeRTOS task parameters (unused).
 */
void audioTask(void *pvParameters) {
  for (;;) {
    audio.loop();
    vTaskDelay(2 / portTICK_PERIOD_MS); // Run this task every 2 milliseconds
  }
}

void saveSettings() {
    Serial.println("--- Saving Settings (Dirty Flag Check) ---");
    preferences.begin(PREFERENCES_NAMESPACE, false);

    // Helper macro to reduce boilerplate
    #define SAVE_IF_CHANGED(key, type, value) \
        if (preferences.get##type(key, -1) != value) { \
            preferences.put##type(key, value); \
            Serial.printf("SAVING: %s -> %d\n", #key, value); \
        }

    #define SAVE_STRING_IF_CHANGED(key, value) \
        if (!preferences.isKey(key) || preferences.getString(key, "") != value.c_str()) { \
            preferences.putString(key, value.c_str()); \
            Serial.printf("SAVING: %s -> %s\n", #key, value.c_str()); \
        }

    SAVE_IF_CHANGED("destYear", Int, currentSettings.destinationYear);
    SAVE_IF_CHANGED("destTzIndex", Int, currentSettings.destinationTimezoneIndex);
    SAVE_IF_CHANGED("depHour", Int, currentSettings.departureHour);
    SAVE_IF_CHANGED("depMinute", Int, currentSettings.departureMinute);
    SAVE_IF_CHANGED("arrHour", Int, currentSettings.arrivalHour);
    SAVE_IF_CHANGED("arrMinute", Int, currentSettings.arrivalMinute);
    SAVE_IF_CHANGED("lastYear", Int, currentSettings.lastTimeDepartedYear);
    SAVE_IF_CHANGED("lastMonth", Int, currentSettings.lastTimeDepartedMonth);
    SAVE_IF_CHANGED("lastDay", Int, currentSettings.lastTimeDepartedDay);
    SAVE_IF_CHANGED("lastHour", Int, currentSettings.lastTimeDepartedHour);
    SAVE_IF_CHANGED("lastMinute", Int, currentSettings.lastTimeDepartedMinute);
    SAVE_IF_CHANGED("brightness", UChar, currentSettings.brightness);
    SAVE_IF_CHANGED("volume", UChar, currentSettings.notificationVolume);
    if (preferences.getBool("soundToggle", true) != currentSettings.timeTravelSoundToggle) {
        preferences.putBool("soundToggle", currentSettings.timeTravelSoundToggle);
        Serial.printf("SAVING: soundToggle -> %s\n", currentSettings.timeTravelSoundToggle ? "true" : "false");
    }

    SAVE_IF_CHANGED("presTzIndex", Int, currentSettings.presentTimezoneIndex);
    SAVE_IF_CHANGED("presetCycle", Int, currentSettings.presetCycleInterval);
    if (preferences.getBool("format24h", false) != currentSettings.displayFormat24h) {
        preferences.putBool("format24h", currentSettings.displayFormat24h);
        Serial.printf("SAVING: format24h -> %s\n", currentSettings.displayFormat24h ? "true" : "false");
    }
    
    SAVE_IF_CHANGED("theme", Int, currentSettings.theme);
    SAVE_IF_CHANGED("animInterval", Int, currentSettings.timeTravelAnimationInterval);
    SAVE_IF_CHANGED("animDuration", Int, currentSettings.timeTravelAnimationDuration);
    SAVE_IF_CHANGED("animStyle", Int, currentSettings.animationStyle);
    SAVE_IF_CHANGED("glitchFreq", Int, currentSettings.glitchEffectFrequency);
    SAVE_IF_CHANGED("malfuncFreq", Int, currentSettings.malfunctionFrequency);
    if (preferences.getBool("dlEnabled", false) != currentSettings.dataLinkEnabled) {
        preferences.putBool("dlEnabled", currentSettings.dataLinkEnabled);
        Serial.printf("SAVING: dlEnabled -> %s\n", currentSettings.dataLinkEnabled ? "true" : "false");
    }

    SAVE_IF_CHANGED("dlTargetRow", Int, currentSettings.dataLinkTargetRow);
    SAVE_IF_CHANGED("dlRefresh", Int, currentSettings.dataLinkRefreshInterval);
    SAVE_IF_CHANGED("numDataPoints", Int, currentSettings.numDataPoints);

    SAVE_STRING_IF_CHANGED("mqttBroker", currentSettings.mqttBroker);
    SAVE_IF_CHANGED("mqttPort", Int, currentSettings.mqttPort);
    SAVE_STRING_IF_CHANGED("mqttUser", currentSettings.mqttUser);
    SAVE_STRING_IF_CHANGED("mqttPass", currentSettings.mqttPassword);
    if (preferences.getBool("weatherMode", false) != currentSettings.weatherModeEnabled) {
        preferences.putBool("weatherMode", currentSettings.weatherModeEnabled);
        Serial.printf("SAVING: weatherMode -> %s\n", currentSettings.weatherModeEnabled ? "true" : "false");
    }

    SAVE_STRING_IF_CHANGED("cityName", currentSettings.cityName);
    if (preferences.getBool("useMetric", false) != currentSettings.useMetricUnits) {
        preferences.putBool("useMetric", currentSettings.useMetricUnits);
        Serial.printf("SAVING: useMetric -> %s\n", currentSettings.useMetricUnits ? "true" : "false");
    }
    
    // Note: Comparing floats can be tricky due to precision.
    // A small tolerance might be needed for production code, but this is fine for this use case.
    if (preferences.getFloat("latitude", 0.0) != currentSettings.latitude) {
        preferences.putFloat("latitude", currentSettings.latitude);
        Serial.printf("SAVING: latitude -> %f\n", currentSettings.latitude);
    }
    if (preferences.getFloat("longitude", 0.0) != currentSettings.longitude) {
        preferences.putFloat("longitude", currentSettings.longitude);
        Serial.printf("SAVING: longitude -> %f\n", currentSettings.longitude);
    }
    
    if (preferences.getBool("stModeEnabled", false) != currentSettings.stockTickerModeEnabled) {
        preferences.putBool("stModeEnabled", currentSettings.stockTickerModeEnabled);
        Serial.printf("SAVING: stModeEnabled -> %s\n", currentSettings.stockTickerModeEnabled ? "true" : "false");
    }

    SAVE_STRING_IF_CHANGED("stRow1Sym", currentSettings.stockRow1_symbol);
    SAVE_STRING_IF_CHANGED("stRow2Sym", currentSettings.stockRow2_symbol);
    SAVE_STRING_IF_CHANGED("stRow3Sym", currentSettings.stockRow3_symbol);
    SAVE_STRING_IF_CHANGED("fmpApiKey", currentSettings.financialModelingPrepApiKey);

	for (int i = 0; i < 5; i++) {
		String prefix = "dp" + String(i) + "_";
        SAVE_STRING_IF_CHANGED((prefix + "url").c_str(), currentSettings.dataPoints[i].url);
		SAVE_STRING_IF_CHANGED((prefix + "monthPath").c_str(), currentSettings.dataPoints[i].monthPath);
		SAVE_STRING_IF_CHANGED((prefix + "dayPath").c_str(), currentSettings.dataPoints[i].dayPath);
		SAVE_STRING_IF_CHANGED((prefix + "yearPath").c_str(), currentSettings.dataPoints[i].yearPath);
		SAVE_STRING_IF_CHANGED((prefix + "timePath").c_str(), currentSettings.dataPoints[i].timePath);
        SAVE_STRING_IF_CHANGED((prefix + "prefix").c_str(), currentSettings.dataPoints[i].prefix);
		SAVE_STRING_IF_CHANGED((prefix + "suffix").c_str(), currentSettings.dataPoints[i].suffix);
		SAVE_STRING_IF_CHANGED((prefix + "icon").c_str(), currentSettings.dataPoints[i].icon);
		SAVE_IF_CHANGED((prefix + "scroll").c_str(), Int, currentSettings.dataPoints[i].scrollSpeed);
        SAVE_IF_CHANGED((prefix + "srcType").c_str(), Int, (int)currentSettings.dataPoints[i].dataSourceType);
		SAVE_STRING_IF_CHANGED((prefix + "topic").c_str(), currentSettings.dataPoints[i].mqttTopic);
		SAVE_STRING_IF_CHANGED((prefix + "yearPrefix").c_str(), currentSettings.dataPoints[i].yearPrefix);
		SAVE_STRING_IF_CHANGED((prefix + "yearSuffix").c_str(), currentSettings.dataPoints[i].yearSuffix);
        SAVE_IF_CHANGED((prefix + "dispMode").c_str(), Int, (int)currentSettings.dataPoints[i].displayMode);
		SAVE_STRING_IF_CHANGED((prefix + "scrollTxt").c_str(), currentSettings.dataPoints[i].scrollingText);
		SAVE_STRING_IF_CHANGED((prefix + "authKey").c_str(), currentSettings.dataPoints[i].authHeaderKey);
		SAVE_STRING_IF_CHANGED((prefix + "authVal").c_str(), currentSettings.dataPoints[i].authHeaderValue);
        SAVE_IF_CHANGED((prefix + "httpMethod").c_str(), Int, (int)currentSettings.dataPoints[i].httpMethod);
		SAVE_STRING_IF_CHANGED((prefix + "reqBody").c_str(), currentSettings.dataPoints[i].requestBody);
		SAVE_STRING_IF_CHANGED((prefix + "apiKey").c_str(), currentSettings.dataPoints[i].apiExampleKey);
	}
	preferences.end();
    Serial.println("--- Settings Saved ---");
    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
	tzset();
}

void loadSettings() {
    Serial.println("--- Loading Settings ---");
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
		currentSettings.malfunctionFrequency = 0;
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
		currentSettings.stockTickerModeEnabled = false;
		currentSettings.stockRow1_symbol = "^GSPC";
		currentSettings.stockRow2_symbol = "^GSPTSE";
		currentSettings.stockRow3_symbol = "^IXIC";
		currentSettings.financialModelingPrepApiKey = "";
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
		currentSettings.dataLinkEnabled = preferences.getBool("dlEnabled");
		currentSettings.dataLinkTargetRow = preferences.getInt("dlTargetRow");
		currentSettings.dataLinkRefreshInterval = preferences.getInt("dlRefresh");
		currentSettings.numDataPoints = preferences.getInt("numDataPoints");
        String brokerStr = preferences.getString("mqttBroker", "");
        currentSettings.mqttBroker = brokerStr.c_str();
		Serial.printf("SETTINGS_LOG: Loaded MQTT Broker: [%s]\n", currentSettings.mqttBroker.c_str());
		currentSettings.mqttPort = preferences.getInt("mqttPort", 1883);
		Serial.printf("SETTINGS_LOG: Loaded MQTT Port: [%d]\n", currentSettings.mqttPort);
        String userStr = preferences.getString("mqttUser", "");
        currentSettings.mqttUser = userStr.c_str();
		Serial.printf("SETTINGS_LOG: Loaded MQTT User: [%s]\n", currentSettings.mqttUser.c_str());
        String passStr = preferences.getString("mqttPass", "");
        currentSettings.mqttPassword = passStr.c_str();
		currentSettings.weatherModeEnabled = preferences.getBool("weatherMode", false);
		String tempString = preferences.getString("cityName", "New York");
		currentSettings.cityName = tempString.c_str();
		currentSettings.useMetricUnits = preferences.getBool("useMetric", false);
		currentSettings.latitude = preferences.getFloat("latitude", 40.7128);
		currentSettings.longitude = preferences.getFloat("longitude", -74.0060);
		currentSettings.stockTickerModeEnabled = preferences.getBool("stModeEnabled", false);
		tempString = preferences.getString("stRow1Sym", "^GSPC");
		currentSettings.stockRow1_symbol = tempString.c_str();
		tempString = preferences.getString("stRow2Sym", "^GSPTSE");
		currentSettings.stockRow2_symbol = tempString.c_str();
		tempString = preferences.getString("stRow3Sym", "^IXIC");
		currentSettings.stockRow3_symbol = tempString.c_str();
		tempString = preferences.getString("fmpApiKey", "");
		currentSettings.financialModelingPrepApiKey = tempString.c_str();
		for (int i = 0; i < 5; i++) {
			String prefix = "dp" + String(i) + "_";
			if(preferences.isKey((prefix + "url").c_str())) currentSettings.dataPoints[i].url = preferences.getString((prefix + "url").c_str(), "").c_str();
			if(preferences.isKey((prefix + "monthPath").c_str())) currentSettings.dataPoints[i].monthPath = preferences.getString((prefix + "monthPath").c_str(), "").c_str();
			if(preferences.isKey((prefix + "dayPath").c_str())) currentSettings.dataPoints[i].dayPath = preferences.getString((prefix + "dayPath").c_str(), "").c_str();
			if(preferences.isKey((prefix + "yearPath").c_str())) currentSettings.dataPoints[i].yearPath = preferences.getString((prefix + "yearPath").c_str(), "").c_str();
			if(preferences.isKey((prefix + "timePath").c_str())) currentSettings.dataPoints[i].timePath = preferences.getString((prefix + "timePath").c_str(), "").c_str();
			if(preferences.isKey((prefix + "prefix").c_str())) currentSettings.dataPoints[i].prefix = preferences.getString((prefix + "prefix").c_str(), "").c_str();
			if(preferences.isKey((prefix + "suffix").c_str())) currentSettings.dataPoints[i].suffix = preferences.getString((prefix + "suffix").c_str(), "").c_str();
			if(preferences.isKey((prefix + "icon").c_str())) currentSettings.dataPoints[i].icon = preferences.getString((prefix + "icon").c_str(), "").c_str();
			if(preferences.isKey((prefix + "scroll").c_str())) currentSettings.dataPoints[i].scrollSpeed = preferences.getInt((prefix + "scroll").c_str());
			if(preferences.isKey((prefix + "srcType").c_str())) currentSettings.dataPoints[i].dataSourceType = (DataSourceType)preferences.getInt((prefix + "srcType").c_str());
			if(preferences.isKey((prefix + "topic").c_str())) currentSettings.dataPoints[i].mqttTopic = preferences.getString((prefix + "topic").c_str(), "").c_str();
			if(preferences.isKey((prefix + "yearPrefix").c_str())) currentSettings.dataPoints[i].yearPrefix = preferences.getString((prefix + "yearPrefix").c_str(), "").c_str();
			if(preferences.isKey((prefix + "yearSuffix").c_str())) currentSettings.dataPoints[i].yearSuffix = preferences.getString((prefix + "yearSuffix").c_str(), "").c_str();
			if(preferences.isKey((prefix + "dispMode").c_str())) currentSettings.dataPoints[i].displayMode = (DisplayMode)preferences.getInt((prefix + "dispMode").c_str(), 0);
			if(preferences.isKey((prefix + "scrollTxt").c_str())) currentSettings.dataPoints[i].scrollingText = preferences.getString((prefix + "scrollTxt").c_str(), "").c_str();
			if(preferences.isKey((prefix + "authKey").c_str())) currentSettings.dataPoints[i].authHeaderKey = preferences.getString((prefix + "authKey").c_str(), "").c_str();
			if(preferences.isKey((prefix + "authVal").c_str())) currentSettings.dataPoints[i].authHeaderValue = preferences.getString((prefix + "authVal").c_str(), "").c_str();
			if(preferences.isKey((prefix + "httpMethod").c_str())) currentSettings.dataPoints[i].httpMethod = (HttpMethod)preferences.getInt((prefix + "httpMethod").c_str(), 0);
			if(preferences.isKey((prefix + "reqBody").c_str())) currentSettings.dataPoints[i].requestBody = preferences.getString((prefix + "reqBody").c_str(), "").c_str();
			if(preferences.isKey((prefix + "apiKey").c_str())) currentSettings.dataPoints[i].apiExampleKey = preferences.getString((prefix + "apiKey").c_str(), "").c_str();
		}
	}
	preferences.end();
	Serial.println("--- Settings Loaded ---");
	if (currentSettings.presentTimezoneIndex < 0 || currentSettings.presentTimezoneIndex >= NUM_TIMEZONE_OPTIONS) {
		currentSettings.presentTimezoneIndex = 0;
	}
	if (currentSettings.destinationTimezoneIndex < 0 || currentSettings.destinationTimezoneIndex >= NUM_TIMEZONE_OPTIONS) {
		currentSettings.destinationTimezoneIndex = 0;
	}
	setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
	tzset();
}

void listAllFiles() {
	Serial.println(F("\n--- Listing all files in LittleFS ---"));
	File root = LittleFS.open("/");
	File file = root.openNextFile();
	while (file) {
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

bool attemptHardwareInit() {
    #if ENABLE_HARDWARE
    Serial.println(F("BOOT_LOG: Attempting to initialize hardware..."));
    setupPhysicalDisplay();
    Serial.println(F("BOOT_LOG: Physical display setup... OK"));
    return true; // Success
    #else
    Serial.println(F("BOOT_LOG: Hardware is disabled (ENABLE_HARDWARE = 0)"));
    return false; // Hardware is disabled, so it's not "initialized"
    #endif
}

void wifiManagerTask(void *pvParameters) {
  WiFiManager* wifiManager = (WiFiManager*)pvParameters;
  wifiManager->autoConnect("BTTF-Clock-Setup");
  vTaskDelete(NULL);
  wifiManagerTaskHandle = NULL;
}


void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println(F("\n\n--- BOOTING ---"));
    Serial.println(F("BOOT_LOG: Initializing Serial... OK"));
    delay(10);

    if (!LittleFS.begin(true, "/spiffs")) {
        ESP_LOGE("FS", "CRITICAL ERROR: LittleFS Mount Failed. Restarting in 10 seconds.");
        Serial.println(F("BOOT_LOG: LittleFS mount... FAILED!"));
        delay(10000);
        ESP.restart();
    }
    Serial.println(F("BOOT_LOG: LittleFS mount... OK"));
    delay(10);

    Serial.println(F("BOOT_LOG: Loading settings..."));
    loadSettings();
    Serial.println(F("BOOT_LOG: Settings loaded... OK"));
    delay(10);

    xDisplayDataMutex = xSemaphoreCreateMutex();
    Serial.println(F("BOOT_LOG: Mutex created... OK"));
    
    WiFi.mode(WIFI_STA);
    uint8_t mac[6];
    WiFi.macAddress(mac);
    sprintf(MQTT_UNIQUE_ID, "BTTF_TC_%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    WiFi.begin();
    wifiConnectStartTime = millis();
    wifiState = WIFI_STATE_CONNECTING;
    Serial.println("BOOT_LOG: Non-blocking WiFi connection initiated...");
    Serial.println(F("WEB_LOG: Setting up web routes..."));
    setupWebRoutes();
    Serial.println(F("WEB_LOG: Web routes configured."));

    hardwareInitialized = attemptHardwareInit();
    if (hardwareInitialized) {
        applyBrightness();
        Serial.println(F("BOOT_LOG: Initializing I2S Audio..."));
        audio.setPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DIN_PIN);
        audio.setVolume(currentSettings.notificationVolume);
        Audio::audio_info_callback = audio_info;
        Serial.println(F("BOOT_LOG: I2S Audio... OK"));
        
        xTaskCreatePinnedToCore(
            audioTask,          // Task function
            "AudioTask",        // Name of the task
            4096,               // FIX: Increased stack size from 2048 to 4096 words
         
           NULL,               // Task input parameter
            5,                  // Priority of the task (high)
            NULL,               // Task handle
       
     0                   // Core where the task should run (Core 0)
        );
    }

    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
    tzset();
    Serial.println(F("BOOT_LOG: Timezone configured."));

    setupMqtt();
    Serial.println(F("BOOT_LOG: MQTT setup initiated."));
    ESP_LOGI("Memory", "Free heap after setup: %u bytes", ESP.getFreeHeap());
    Serial.printf("BOOT_LOG: Free heap: %u bytes\n", ESP.getFreeHeap());

    ArduinoOTA.setHostname("bttf-time-circuits");
    ArduinoOTA.setPassword("1.21gigawatts");
    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else {
            type = "filesystem";
        }
        Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();

    // Clear any persistent manual display overrides from the previous session
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j) {
            updateDisplaySegment(i, j, "");
        }
    }

    Serial.println(F("--- BOOT COMPLETE ---"));
    bootTimestamp = millis();
}

// --- NEW STATE DETERMINATION FUNCTION ---
void updateDisplayState() {
    if (isMessageOverrideActive) {
        currentDisplayState = STATE_MESSAGE_OVERRIDE;
    } else if (isMalfunctioning) {
        currentDisplayState = STATE_MALFUNCTION;
    } else if (isAnimating) {
        currentDisplayState = STATE_ANIMATING;
    } else if (isMarqueeOverrideActive) {
        currentDisplayState = STATE_MARQUEE_OVERRIDE;
    } else if (currentSettings.stockTickerModeEnabled) {
        currentDisplayState = STATE_STOCK_TICKER;
    } else if (currentSettings.dataLinkEnabled) {
        currentDisplayState = STATE_DATA_LINK;
    } else if (currentSettings.weatherModeEnabled) {
        currentDisplayState = STATE_WEATHER;
    } else {
        currentDisplayState = STATE_NORMAL_CLOCK;
    }
}

// --- NEW DISPLAY HANDLER FUNCTION ---
void handleDisplay() {
    // These effects can run concurrently with the main display modes
    restoreDisplayAfterGlitch();
    handleTemporalEcho();
    handleGlitchEffect();
    handleSequencer();
    handlePresetCycling();
    handleSleepSchedule();

    // --- Main Display State Machine ---
    // This switch statement is the heart of the display logic. It ensures that only one
    // primary display mode is active at any given time, preventing conflicts. The state
    // is determined by the `updateDisplayState()` function, which prioritizes certain
    // states over others (e.g., an override message takes precedence over the clock).
    switch (currentDisplayState) {
        case STATE_MESSAGE_OVERRIDE:
            displayOverrideMessage();
            break;
        case STATE_MALFUNCTION:
            handleMalfunction();
            break;
        case STATE_ANIMATING:
            handleDisplayAnimation();
            break;
        case STATE_MARQUEE_OVERRIDE:
            displayMarqueeOverride();
            break;
        case STATE_STOCK_TICKER:
            if (isMarketOpen() && (millis() - lastStockDataFetch > 300000)) {
                lastStockDataFetch = millis();
                for (int i=0; i<3; ++i) {
                    FetchDataParams* params = new FetchDataParams{ i, 0 };
                    xTaskCreatePinnedToCore(fetchStockDataTask, "fetchStockDataTask", 8192, params, 1, NULL, 0);
                }
            }
            updateStockTickerDisplay();
            break;
        case STATE_DATA_LINK:
            fetchDataLink();
            updateMarqueeDisplay();
            break;
        case STATE_WEATHER:
            handleWeatherDisplay();
            break;
        case STATE_NORMAL_CLOCK:
        default:
            updateNormalClockDisplay();
            break;
    }
}


void loop() {
    vTaskDelay(1);
    
    // --- WiFi State Machine ---
    // This state machine manages the WiFi connection process in a non-blocking way.
    // It handles the initial connection attempt, starting the WiFiManager portal if
    // the connection fails, and managing the device reboot after successful portal configuration.
    switch (wifiState) {
        case WIFI_STATE_CONNECTING:
            if (!logConnectingPrinted) {
                Serial.println("WIFI_LOG: State is WIFI_STATE_CONNECTING.");
                logConnectingPrinted = true;
            }
            if (WiFi.status() == WL_CONNECTED) {
                wifiState = WIFI_STATE_CONNECTED;
            } else if (millis() - wifiConnectStartTime > WIFI_CONNECT_TIMEOUT) {
                wifiState = WIFI_STATE_START_PORTAL;
            }
            break;
        case WIFI_STATE_START_PORTAL:
            if (!logPortalMsgPrinted) {
                Serial.println("WIFI_LOG: State is WIFI_STATE_START_PORTAL. Starting WiFiManager.");
                logPortalMsgPrinted = true;
            }
            xTaskCreate(wifiManagerTask, "WiFiManager", 4096, &wifiManager, 1, &wifiManagerTaskHandle);
            wifiState = WIFI_STATE_PORTAL_RUNNING;
            break;
        case WIFI_STATE_PORTAL_RUNNING:
             if (WiFi.status() == WL_CONNECTED) {
                Serial.println("WIFI_LOG: WiFi connected via portal. Rebooting...");
                if(wifiManagerTaskHandle != NULL) {
                    vTaskDelete(wifiManagerTaskHandle);
                    wifiManagerTaskHandle = NULL;
                }
                static unsigned long reboot_time = 0;
                if (reboot_time == 0) reboot_time = millis();
                if (millis() - reboot_time > 2000) ESP.restart();
            }
            break;
        case WIFI_STATE_CONNECTED:
            if (!logConnectedPrinted) {
                ESP_LOGI("WiFi", "IP: %s", WiFi.localIP().toString().c_str());
                // Start the web server now that we are connected
                server.begin();
                ESP_LOGI("Web", "HTTP server started on successful connection.");

                logConnectedPrinted = true;
                if (MDNS.begin("timecircuits")) {
                    MDNS.addService("http", "tcp", 80);
                }
                ntpSyncRequested = true;
                runBootSequence();
            }
            if (!currentSettings.mqttBroker.empty()) {
                if (!mqttClient.connected()) {
                    unsigned long now = millis();
                    if (now > nextMqttReconnectAttempt) {
                        reconnectMqtt();
                        nextMqttReconnectAttempt = now + mqttReconnectInterval;
                        if (!mqttClient.connected()) {
                            mqttReconnectInterval *= 2;
                            if (mqttReconnectInterval > MQTT_MAX_RETRY_INTERVAL) {
                                mqttReconnectInterval = MQTT_MAX_RETRY_INTERVAL;
                            }
                        } else {
                             mqttReconnectInterval = MQTT_INITIAL_RETRY_INTERVAL;
                        }
                    }
                } else {
                    mqttClient.loop();
                }
            }
            
            handleScheduledAnimation();
            static unsigned long lastNtpUpdate = 0;
            if (ntpSyncRequested || (!timeSynchronized && millis() > NTP_INITIAL_SYNC_DELAY) || (timeSynchronized && millis() - lastNtpUpdate > 3600000)) {
                bool syncSuccess = false;
                int retries = 0;
                while (!syncSuccess && retries < NUM_NTP_SERVERS) {
                    configTime(0, 0, NTP_SERVERS[currentNtpServerIndex]);
                    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
                    tzset();
                    struct tm timeinfo;
                    if (getLocalTime(&timeinfo, 10000)) {
                        timeSynchronized = true;
                        syncSuccess = true;
                    } else {
                        currentNtpServerIndex = (currentNtpServerIndex + 1) % NUM_NTP_SERVERS;
                        retries++;
                    }
                }
                lastNtpUpdate = millis();
                ntpSyncRequested = false;
            }
            static unsigned long lastHaStateUpdate = 0;
            if (timeSynchronized && millis() - lastHaStateUpdate > 5000) {
                publishAllHaStates();
                lastHaStateUpdate = millis();
            }
            
            if (hardwareInitialized) {
                if (bootState != BOOT_INACTIVE) {
                    handleBootSequence();
                } else {
                    handleFlashEffect();

                    if (isAnimating) {
                        handleDisplayAnimation();
                    } else if (isStyledAnimating) {
                        handleStyledAnimation();
                    } else {
                        if (millis() - lastDisplayUpdateTime > DISPLAY_UPDATE_INTERVAL) {
                            lastDisplayUpdateTime = millis();
                            updateDisplayState();
                            handleDisplay();
                        }
                    }
                }
            }
            break;
    }
    ArduinoOTA.handle();
}
void handleSequencer() {
    if (!isSequenceActive) return;
    SequenceStep step = sequence[currentSequenceStep];
    unsigned long elapsed = millis() - sequenceStepStartTime;
    switch (step.command) {
        case SEQ_CMD_TEXT:
            if (hardwareInitialized) updateDisplaySegment(step.targetRow, step.targetSegment, step.stringParam);
            currentSequenceStep++;
            sequenceStepStartTime = millis();
            break;
        case SEQ_CMD_FLASH:
            if (hardwareInitialized) triggerFlashEffect(step.targetRow, step.targetSegment, step.intParam);
            currentSequenceStep++;
            sequenceStepStartTime = millis();
            break;
        case SEQ_CMD_SOUND:
            if (hardwareInitialized) playSound(step.stringParam.c_str());
            currentSequenceStep++;
			sequenceStepStartTime = millis();
            break;
        case SEQ_CMD_WAIT:
            if (elapsed >= (unsigned long)step.intParam) {
                sequenceStepStartTime = millis();
                currentSequenceStep++;
            }
            break;
        case SEQ_CMD_END:
            isSequenceActive = false;
            currentSequenceStep = 0;
            break;
    }
}

void handlePresetCycling() {
    if (currentSettings.presetCycleInterval == 0 || isAnimating || isDisplayAsleep) return;
    if (millis() - lastPresetCycleTime > (unsigned long)currentSettings.presetCycleInterval * 60000) {
        lastPresetCycleTime = millis();
    }
}

/**
 * @brief Checks if a scheduled time travel animation should be triggered.
 * @details This function is called in the main loop and uses the
 * `timeTravelAnimationInterval` setting to automatically start the
 * animation sequence after the specified number of minutes.
 */
void handleScheduledAnimation() {
    if (currentSettings.timeTravelAnimationInterval == 0 || isAnimating || isDisplayAsleep || isStyledAnimating) {
        return;
    }

    // NEW: Reset the timer once right after the boot sequence completes.
    if (lastScheduledAnimationTime == 0 && bootState == BOOT_INACTIVE) {
        lastScheduledAnimationTime = millis();
    }

    if (lastScheduledAnimationTime > 0 && (millis() - lastScheduledAnimationTime > (unsigned long)currentSettings.timeTravelAnimationInterval * 60000)) {
        startStyledAnimation();
        lastScheduledAnimationTime = millis();
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
    if (hardwareInitialized) {
        blankAllDisplays();
        playSound("/SLEEP_ON.mp3");
    }
    updateHaStatus("Asleep");
  } else if (!shouldBeAsleep && isDisplayAsleep) {
    isDisplayAsleep = false;
    if (hardwareInitialized) {
        updateNormalClockDisplay();
        playSound("/CONFIRM_ON.mp3");
    }
    updateHaStatus("Idle");
  }
}

bool isMarketOpen() {
    if (!timeSynchronized) return false;
    setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
    tzset();
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
    tzset();

    if (timeinfo.tm_wday < 1 || timeinfo.tm_wday > 5) {
        return false;
    }

    int current_minutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int market_open_minutes = 9 * 60 + 30;
    int market_close_minutes = 16 * 60;
    return (current_minutes >= market_open_minutes && current_minutes < market_close_minutes);
}