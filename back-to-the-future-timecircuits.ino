/**
 * @file back-to-the-future-timecircuits.ino
 * @brief Main application firmware for the ESP32-based Time Circuits display.
 * @details This file contains the primary setup() and loop() functions, global variable declarations,
 * and the core logic for coordinating all subsystems of the clock, including WiFi, web server,
 * display management, and event handling. The main loop is designed as a non-blocking state machine
 * to ensure smooth animations and responsive network handling.
 */

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
#include "EventManager.h"
#include "AnimationManager.h"
#include "DisplayManager.h"
#include "DataManager.h"
#include "MqttManager.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// --- FUNCTION PROTOTYPES ---
void handlePresetCycling();
void handleSleepSchedule();
void handleSequencer();

// --- GLOBAL VARIABLES & STATE ---
/** @brief Holds all user-configurable settings for the device. */
ClockSettings currentSettings;
/** @brief An array of structs holding the parsed data for each of the 5 Data Link marquee pages. */
MarqueeData displayPages[5];
/** @brief A cache to store the last successfully fetched data for the marquee, used for fallback on API failure. */
MarqueeData lastGoodDisplayPages[5];
/** @brief Struct holding the latest fetched weather data. */
WeatherData currentWeatherData;
/** @brief The last city name used for a successful geocoding lookup, to avoid redundant API calls. */
std::string lastCityName = "";
/** @brief A constant array of TimeZoneEntry structs, providing POSIX strings and display names for time zones. */
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
/** @brief A constant array of NTP server hostnames for time synchronization. */
const char *NTP_SERVERS[] = { "pool.ntp.org", "time.google.com", "time.nist.gov" };
const int NUM_NTP_SERVERS = sizeof(NTP_SERVERS) / sizeof(NTP_SERVERS[0]);
int currentNtpServerIndex = 0;
/** @brief Flag indicating if the device time has been successfully synchronized with an NTP server. */
bool timeSynchronized = false;
bool ntpSyncRequested = false;

// Core objects
WiFiManager wifiManager;
AsyncWebServer server(80);
Preferences preferences;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long lastMqttReconnectAttempt = 0;
bool mqttReconnectRequired = false;

// State Variables for animations, effects, and data handling
bool isAnimating = false;
unsigned long animationStartTime = 0;
unsigned long lastAnimationFrameTime = 0;
AnimationPhase currentPhase = ANIM_INACTIVE;
bool isDisplayAsleep = false;
BootSequenceState bootState = BOOT_INACTIVE;
unsigned long bootStateStartTime = 0;
unsigned long lastGlitchTime = 0;
bool isGlitching = false;
unsigned long glitchStartTime = 0;
unsigned long lastPresetCycleTime = 0;
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

// HA-ENHANCEMENT: Global state for message override
bool isMessageOverrideActive = false;
String overrideMessageLine1 = "";
String overrideMessageLine2 = "";
String overrideMessageLine3 = "";
// HA-MARQUEE: Global state for marquee override
bool isMarqueeOverrideActive = false;
String marqueeOverrideMessage = "";
unsigned long marqueeOverrideEndTime = 0;
// Mutex for protecting shared data between tasks
SemaphoreHandle_t xDisplayDataMutex;

// Sequencer state
SequenceStep sequence[20];
int currentSequenceStep = 0;
unsigned long sequenceStepStartTime = 0;
bool isSequenceActive = false;

/**
 * @brief Saves the current settings from the global `currentSettings` struct to non-volatile storage (NVS).
 */
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

/**
 * @brief Loads settings from NVS into the global `currentSettings` struct.
 */
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
	// Sanity check for timezone indices to prevent crashes if data is corrupted.
	if (currentSettings.presentTimezoneIndex < 0 || currentSettings.presentTimezoneIndex >= NUM_TIMEZONE_OPTIONS) {
		currentSettings.presentTimezoneIndex = 0;
	}
	if (currentSettings.destinationTimezoneIndex < 0 || currentSettings.destinationTimezoneIndex >= NUM_TIMEZONE_OPTIONS) {
		currentSettings.destinationTimezoneIndex = 0;
	}
	// Apply the loaded timezone setting.
	setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
	tzset();
}

/**
 * @brief Lists all files in the LittleFS filesystem to the Serial monitor for debugging.
 */
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

/**
 * @brief The main setup function, run once on boot.
 * @details Initializes all hardware and software subsystems in the correct order:
 * 1. Serial communication for debugging.
 * 2. LittleFS filesystem for web assets.
 * 3. Loads settings from non-volatile storage.
 * 4. Initializes hardware (displays, sound).
 * 5. Connects to WiFi using WiFiManager (captive portal on first boot).
 * 6. Starts the mDNS service (timecircuits.local).
 * 7. Sets up all web server routes and starts the server.
 * 8. Configures the NTP client for time synchronization.
 * 9. Configures the MQTT client for Home Assistant integration.
 * 10. Starts the initial boot-up animation sequence.
 */
void setup() {
	Serial.begin(115200);
	delay(1000);

	Serial.println(F("\n\n--- BOOTING ---"));
	// Initialize LittleFS for web files.
	if (!LittleFS.begin(true)) {
		ESP_LOGE("FS", "CRITICAL ERROR: LittleFS Mount Failed. Restarting in 10 seconds.");
		delay(10000);
		ESP.restart();
	}

	// Load all settings from NVS into memory.
	listAllFiles();
	loadSettings();
	// Create a mutex for safe multi-threaded access to shared display data.
	xDisplayDataMutex = xSemaphoreCreateMutex();
	#if ENABLE_HARDWARE
	// Initialize physical displays, sound module, etc.
	setupPhysicalDisplay();
	dfpSerial.begin(9600, SERIAL_8N1, DFP_RX_PIN, DFP_TX_PIN);
	if (myDFPlayer.begin(dfpSerial, true, false)) {
		myDFPlayer.volume(currentSettings.notificationVolume);
		setupSoundFiles();
	}
#endif

	// Connect to WiFi using WiFiManager, which creates a captive portal for first-time setup.
	wifiManager.autoConnect("BTTF-Clock-Setup");
	ESP_LOGI("WiFi", "WiFi connected! IP: %s", WiFi.localIP().toString().c_str());

	// Start mDNS service to allow accessing the device at http://timecircuits.local
	if (MDNS.begin("timecircuits")) {
		MDNS.addService("http", "tcp", 80);
	}

	// Set up all web server routes and start the server.
	setupWebRoutes();
	server.begin();
	// Increase the MQTT buffer size to handle large JSON discovery payloads.
	mqttClient.setBufferSize(2048);

	ESP_LOGI("Web", "HTTP server started.");
	// Initial time configuration.
	configTime(0, 0, NTP_SERVERS[0]);
	setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
	tzset();
	// Configure MQTT client if a broker address is set.
	setupMqtt();

	ESP_LOGI("Memory", "Free heap after setup: %u bytes", ESP.getFreeHeap());
	// Start the cinematic boot sequence on the displays.
	runBootSequence();
}

/**
 * @brief The main application loop. This function is the heart of the firmware's state machine.
 * @details This function is executed repeatedly and is designed to be completely non-blocking.
 * It handles the following responsibilities on each iteration:
 * 1.  Manages the MQTT connection, reconnecting if necessary.
 * 2.  Processes incoming MQTT messages via `mqttClient.loop()`.
 * 3.  Checks for timeouts on temporary marquee messages.
 * 4.  Executes the active step of a command sequence, if any.
 * 5.  Manages the primary display state machine:
 * - If malfunctioning, it handles the malfunction animation.
 * - If not animating, it handles normal operations: glitches, echos, sleep schedule, and updates the display content (clock, weather, or data link).
 * 6.  Manages the main time travel animation state machine.
 * 7.  Handles periodic NTP time synchronization.
 * 8.  Handles periodic state publishing to Home Assistant.
 */
void loop() {
	// Handle MQTT connection and message processing.
	if (WiFi.status() == WL_CONNECTED && !currentSettings.mqttBroker.empty()) {
		if (mqttReconnectRequired || !mqttClient.connected()) {
			unsigned long now = millis();
			// Attempt to reconnect every 5 seconds if disconnected.
			if (now - lastMqttReconnectAttempt > 5000) {
				lastMqttReconnectAttempt = now;
				setupMqtt();
				reconnectMqtt();
				mqttReconnectRequired = false;
			}
		}
		mqttClient.loop(); // Process incoming MQTT messages and maintain connection.
	}

    // Handle temporary marquee override timeout
    if (isMarqueeOverrideActive && marqueeOverrideEndTime > 0 && millis() > marqueeOverrideEndTime) {
        isMarqueeOverrideActive = false;
        marqueeOverrideEndTime = 0;
        publishAllHaStates(); // Update HA to reflect the change
    }

	handleFlashEffect();
    handleSequencer();
	// Run the boot sequence state machine (only does something on startup).
	handleBootSequence();
	// The main display logic is a state machine. Only one of these major states can be active.
	if (isMalfunctioning) {
		// Highest priority: if a malfunction is active, it takes over the display.
		handleMalfunction();
	} else if (!isAnimating) {
		// Normal operation state (not in a time travel animation).
		restoreDisplayAfterGlitch();
		// Reverts display to normal after a brief glitch effect.
		handleTemporalEcho();
		// Handles the random flickering effect for a few minutes after a time jump.
		// Only update the display if not in the middle of a flicker effect.
		if (!isFlickeringNow) {
			handleGlitchEffect();
			// Randomly triggers visual glitches or malfunctions.
			// Periodically fetch weather data if the weather mode is enabled.
			if (currentSettings.weatherModeEnabled) {
				static unsigned long lastWeatherFetch = 0;
				if (millis() - lastWeatherFetch > 300000) { // Fetch every 5 minutes (300,000 ms)
					lastWeatherFetch = millis();
					WeatherTaskParams* params = new WeatherTaskParams{ currentSettings.cityName, false };
					xTaskCreate(fetchWeatherDataTask, "fetchWeatherDataTask", 8192, params, 1, NULL);
				}
			}

			handlePresetCycling();
			// Handles cycling through destination time presets.
			handleSleepSchedule(); // Checks if the display should enter or exit sleep mode.
			// Logic for the bottom display row.
            if (isMessageOverrideActive) {
                displayOverrideMessage();
            } else if (isMarqueeOverrideActive) {
                displayMarqueeOverride();
            } else if (currentSettings.dataLinkEnabled) {
				fetchDataLink(); // Fetches data from APIs for the marquee.
				updateMarqueeDisplay();
				// Updates the marquee display.
			} else {
				updateNormalClockDisplay(); // Update all three rows with standard clock data.
				if (currentSettings.weatherModeEnabled) {
					handleWeatherDisplay();
					// If weather mode is on, it overrides the bottom row.
				}
			}
		}
	}

	// This runs regardless of the normal operation state to handle the time travel animation.
	handleDisplayAnimation();
    handleTemporalGlitch();
	// Handle the NTP sync glitch effect


	// Time synchronization logic.
	static unsigned long lastNtpUpdate = 0;
	static unsigned long lastHaStateUpdate = 0;

	// Sync time if requested via UI, if never synced before, or if it's been an hour since the last sync.
	if (ntpSyncRequested || (!timeSynchronized && millis() > 10000) || (timeSynchronized && millis() - lastNtpUpdate > 3600000)) {
		configTime(0, 0, NTP_SERVERS[currentNtpServerIndex]);
		setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
		tzset();
		struct tm timeinfo;
		if (getLocalTime(&timeinfo, 5000)) { // Attempt to get time with a 5-second timeout.
			if (!timeSynchronized) { // Only trigger glitch on the FIRST successful sync
                triggerTemporalGlitch();
			}
			timeSynchronized = true;
		}
		else {
			timeSynchronized = false;
		}
		// Cycle to the next NTP server in the list for robustness.
		currentNtpServerIndex = (currentNtpServerIndex + 1) % NUM_NTP_SERVERS;
		lastNtpUpdate = millis();
		ntpSyncRequested = false;
	}

    if (timeSynchronized && millis() - lastHaStateUpdate > 5000) { // Update HA states every 5 seconds
        publishAllHaStates();
        lastHaStateUpdate = millis();
    }
}

// --- SYSTEM & STATE HANDLERS ---

/**
 * @brief Handles the execution of a command sequence received via MQTT.
 * @details This function is a state machine that steps through an array of `SequenceStep` commands.
 * It supports commands for displaying text, waiting, playing sounds, and flashing segments.
 */
void handleSequencer() {
    if (!isSequenceActive) return;
    SequenceStep step = sequence[currentSequenceStep];
    unsigned long elapsed = millis() - sequenceStepStartTime;
    switch (step.command) {
        case SEQ_CMD_TEXT:
            updateDisplaySegment(step.targetRow, step.targetSegment, step.stringParam);
            currentSequenceStep++;
            sequenceStepStartTime = millis();
            break;
        case SEQ_CMD_FLASH:
            triggerFlashEffect(step.targetRow, step.targetSegment, step.intParam);
            currentSequenceStep++;
            sequenceStepStartTime = millis();
            break;
        case SEQ_CMD_SOUND:
            playSound(step.stringParam.c_str());
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

/**
 * @brief Handles the automatic cycling of destination time presets.
 * @details If enabled, this function will eventually load the next saved preset from NVS
 * after the configured interval has passed.
 */
void handlePresetCycling() {
    if (currentSettings.presetCycleInterval == 0 || isAnimating || isDisplayAsleep) return;
    if (millis() - lastPresetCycleTime > (unsigned long)currentSettings.presetCycleInterval * 60000) {
        lastPresetCycleTime = millis();
        // (Future implementation: Add logic here to load the next preset from NVS).
    }
}

/**
 * @brief Checks the current time against the user-configured sleep/wake schedule.
 * @details Puts the display to sleep or wakes it up by setting the `isDisplayAsleep` flag
 * and playing the appropriate sound effect.
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
  if (shouldBeAsleep && !isDisplayAsleep) {
    isDisplayAsleep = true;
    #if ENABLE_HARDWARE
    blankAllDisplays();
    playSound("SLEEP_ON");
    #endif
    updateHaStatus("Asleep");
  } else if (!shouldBeAsleep && isDisplayAsleep) {
    isDisplayAsleep = false;
    #if ENABLE_HARDWARE
    updateNormalClockDisplay();
    playSound("CONFIRM_ON");
    #endif
    updateHaStatus("Idle");
  }
}