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
bool isMarketOpen();

// --- GLOBAL VARIABLES & CONSTANTS ---
ClockSettings currentSettings;
MarqueeData displayPages[5];
MarqueeData lastGoodDisplayPages[5];
WeatherData currentWeatherData;
StockData stockData[3];
unsigned long lastStockDataFetch = 0;
std::string lastCityName = "";

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
	{ "<+03>-3", "Turkey Time (Istanbul)", "Europe/Istanbul", "Europe" },
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
	{ "EET-2", "EET (Cairo)", "Africa/Cairo", "Africa" },
	{ "EAT-3", "East Africa Time (Nairobi)", "Africa/Nairobi", "Africa" },
	{ "<-03>3", "Brasilia Time (Sao Paulo)", "America/Sao_Paulo", "South America" },
	{ "<-03>3", "Argentina Time (Buenos Aires)", "America/Argentina/Buenos_Aires", "South America" }
};
const int NUM_TIMEZONE_OPTIONS = sizeof(TZ_DATA) / sizeof(TZ_DATA[0]);
const char TZ_JSON[] PROGMEM = "{\"Global\":[{\"value\":0,\"text\":\"UTC\",\"ianaTzName\":\"Etc/UTC\"}],\"Americas\":[{\"value\":1,\"text\":\"Newfoundland (St. John's)\",\"ianaTzName\":\"America/St_Johns\"},{\"value\":2,\"text\":\"Atlantic (Halifax)\",\"ianaTzName\":\"America/Halifax\"},{\"value\":3,\"text\":\"Eastern (New York)\",\"ianaTzName\":\"America/New_York\"},{\"value\":4,\"text\":\"Central (Chicago)\",\"ianaTzName\":\"America/Chicago\"},{\"value\":5,\"text\":\"Mountain (Denver)\",\"ianaTzName\":\"America/Denver\"},{\"value\":6,\"text\":\"Pacific (Los Angeles)\",\"ianaTzName\":\"America/Los_Angeles\"},{\"value\":7,\"text\":\"Alaska (Anchorage)\",\"ianaTzName\":\"America/Anchorage\"},{\"value\":8,\"text\":\"Mountain (Phoenix, No DST)\",\"ianaTzName\":\"America/Phoenix\"},{\"value\":9,\"text\":\"Hawaii (Honolulu, No DST)\",\"ianaTzName\":\"Pacific/Honolulu\"}],\"Europe\":[{\"value\":10,\"text\":\"GMT/BST (London)\",\"ianaTzName\":\"Europe/London\"},{\"value\":11,\"text\":\"CET/CEST (Berlin)\",\"ianaTzName\":\"Europe/Berlin\"},{\"value\":12,\"text\":\"EET/EEST (Athens)\",\"ianaTzName\":\"Europe/Athens\"},{\"value\":13,\"text\":\"Moscow Standard Time\",\"ianaTzName\":\"Europe/Moscow\"},{\"value\":14,\"text\":\"Turkey Time (Istanbul)\",\"ianaTzName\":\"Europe/Istanbul\"}],\"Asia\":[{\"value\":15,\"text\":\"Indian Standard Time (Kolkata)\",\"ianaTzName\":\"Asia/Kolkata\"},{\"value\":16,\"text\":\"Singapore Standard Time\",\"ianaTzName\":\"Asia/Singapore\"},{\"value\":17,\"text\":\"China Standard Time (Shanghai)\",\"ianaTzName\":\"Asia/Shanghai\"},{\"value\":18,\"text\":\"Korea Standard Time (Seoul)\",\"ianaTzName\":\"Asia/Seoul\"},{\"value\":19,\"text\":\"Japan Standard Time (Tokyo)\",\"ianaTzName\":\"Asia/Tokyo\"},{\"value\":20,\"text\":\"Gulf Standard Time (Dubai)\",\"ianaTzName\":\"Asia/Dubai\"}],\"Australia & Oceania\":[{\"value\":21,\"text\":\"AWST (Perth)\",\"ianaTzName\":\"Australia/Perth\"},{\"value\":22,\"text\":\"AEST/AEDT (Sydney)\",\"ianaTzName\":\"Australia/Sydney\"},{\"value\":23,\"text\":\"NZST/NZDT (Auckland)\",\"ianaTzName\":\"Pacific/Auckland\"},{\"value\":24,\"text\":\"Chamorro Time (Guam)\",\"ianaTzName\":\"Pacific/Guam\"}],\"Africa\":[{\"value\":25,\"text\":\"West Africa Time (Lagos)\",\"ianaTzName\":\"Africa/Lagos\"},{\"value\":26,\"text\":\"South Africa Standard Time\",\"ianaTzName\":\"Africa/Johannesburg\"},{\"value\":27,\"text\":\"EET (Cairo)\",\"ianaTzName\":\"Africa/Cairo\"},{\"value\":28,\"text\":\"East Africa Time (Nairobi)\",\"ianaTzName\":\"Africa/Nairobi\"}],\"South America\":[{\"value\":29,\"text\":\"Brasilia Time (Sao Paulo)\",\"ianaTzName\":\"America/Sao_Paulo\"},{\"value\":30,\"text\":\"Argentina Time (Buenos Aires)\",\"ianaTzName\":\"America/Argentina/Buenos_Aires\"}]}";
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

void saveSettings() {
    Serial.println("--- Saving Settings ---");
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

	preferences.putBool("stModeEnabled", currentSettings.stockTickerModeEnabled);
	preferences.putString("stRow1Sym", currentSettings.stockRow1_symbol.c_str());
	preferences.putString("stRow2Sym", currentSettings.stockRow2_symbol.c_str());
	preferences.putString("stRow3Sym", currentSettings.stockRow3_symbol.c_str());
    Serial.printf("Saving avApiKey: [%s]\n", currentSettings.alphaVantageApiKey.c_str());
	preferences.putString("avApiKey", currentSettings.alphaVantageApiKey.c_str());

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
		currentSettings.stockTickerModeEnabled = false;
		currentSettings.stockRow1_symbol = "^GSPC";
		currentSettings.stockRow2_symbol = "^GSPTSE";
		currentSettings.stockRow3_symbol = "^IXIC";
		currentSettings.alphaVantageApiKey = "";
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
		String tempString;

		tempString = preferences.getString("mqttBroker", "");
		currentSettings.mqttBroker = tempString.c_str();
		tempString = preferences.getString("mqttUser", "");
		currentSettings.mqttUser = tempString.c_str();

		tempString = preferences.getString("mqttPass", "");
		currentSettings.mqttPassword = tempString.c_str();

		currentSettings.weatherModeEnabled = preferences.getBool("weatherMode", false);
		tempString = preferences.getString("cityName", "New York");
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

		tempString = preferences.getString("avApiKey", "");
        Serial.printf("Loading avApiKey from Preferences: [%s]\n", tempString.c_str());
		currentSettings.alphaVantageApiKey = tempString.c_str();
        Serial.printf("Loaded avApiKey into currentSettings: [%s]\n", currentSettings.alphaVantageApiKey.c_str());

		for (int i = 0; i < 5; i++) {
			String prefix = "dp" + String(i) + "_";
			tempString = preferences.getString((prefix + "url").c_str(), "");
			currentSettings.dataPoints[i].url = tempString.c_str();

			tempString = preferences.getString((prefix + "monthPath").c_str(), "");
			currentSettings.dataPoints[i].monthPath = tempString.c_str();
			tempString = preferences.getString((prefix + "dayPath").c_str(), "");
			currentSettings.dataPoints[i].dayPath = tempString.c_str();

			tempString = preferences.getString((prefix + "yearPath").c_str(), "");
			currentSettings.dataPoints[i].yearPath = tempString.c_str();
			tempString = preferences.getString((prefix + "timePath").c_str(), "");
			currentSettings.dataPoints[i].timePath = tempString.c_str();

			tempString = preferences.getString((prefix + "prefix").c_str(), "");
			currentSettings.dataPoints[i].prefix = tempString.c_str();
			tempString = preferences.getString((prefix + "suffix").c_str(), "");
			currentSettings.dataPoints[i].suffix = tempString.c_str();

			tempString = preferences.getString((prefix + "icon").c_str(), "");
			currentSettings.dataPoints[i].icon = tempString.c_str();
			currentSettings.dataPoints[i].scrollSpeed = preferences.getInt((prefix + "scroll").c_str());
			currentSettings.dataPoints[i].dataSourceType = (DataSourceType)preferences.getInt((prefix + "srcType").c_str());

			tempString = preferences.getString((prefix + "topic").c_str(), "");
			currentSettings.dataPoints[i].mqttTopic = tempString.c_str();
			tempString = preferences.getString((prefix + "yearPrefix").c_str(), "");
			currentSettings.dataPoints[i].yearPrefix = tempString.c_str();

			tempString = preferences.getString((prefix + "yearSuffix").c_str(), "");
			currentSettings.dataPoints[i].yearSuffix = tempString.c_str();
			currentSettings.dataPoints[i].displayMode = (DisplayMode)preferences.getInt((prefix + "dispMode").c_str(), 0);

			tempString = preferences.getString((prefix + "scrollTxt").c_str(), "");
			currentSettings.dataPoints[i].scrollingText = tempString.c_str();
			tempString = preferences.getString((prefix + "authKey").c_str(), "");
			currentSettings.dataPoints[i].authHeaderKey = tempString.c_str();

			tempString = preferences.getString((prefix + "authVal").c_str(), "");
			currentSettings.dataPoints[i].authHeaderValue = tempString.c_str();
			currentSettings.dataPoints[i].httpMethod = (HttpMethod)preferences.getInt((prefix + "httpMethod").c_str(), 0);
			
			tempString = preferences.getString((prefix + "reqBody").c_str(), "");
			currentSettings.dataPoints[i].requestBody = tempString.c_str();
			tempString = preferences.getString((prefix + "apiKey").c_str(), "");
			currentSettings.dataPoints[i].apiExampleKey = tempString.c_str();
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

void setup() {
	Serial.begin(115200);
	delay(1000);

	Serial.println(F("\n\n--- BOOTING ---"));
	if (!LittleFS.begin(true)) {
		ESP_LOGE("FS", "CRITICAL ERROR: LittleFS Mount Failed. Restarting in 10 seconds.");
		delay(10000);
		ESP.restart();
	}

	listAllFiles();
	loadSettings();
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

	if (MDNS.begin("timecircuits")) {
		MDNS.addService("http", "tcp", 80);
	}

	setupWebRoutes();
	server.begin();
	mqttClient.setBufferSize(2048);

	ESP_LOGI("Web", "HTTP server started.");
	configTime(0, 0, NTP_SERVERS[0]);
	setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
	tzset();
	setupMqtt();

	ESP_LOGI("Memory", "Free heap after setup: %u bytes", ESP.getFreeHeap());
	runBootSequence();
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

void loop() {
	if (WiFi.status() == WL_CONNECTED && !currentSettings.mqttBroker.empty()) {
		if (mqttReconnectRequired || !mqttClient.connected()) {
			unsigned long now = millis();
			if (now - lastMqttReconnectAttempt > 5000) {
				lastMqttReconnectAttempt = now;
				setupMqtt();
				reconnectMqtt();
				mqttReconnectRequired = false;
			}
		}
		mqttClient.loop();
	}

    if (isMarqueeOverrideActive && marqueeOverrideEndTime > 0 && millis() > marqueeOverrideEndTime) {
        isMarqueeOverrideActive = false;
        marqueeOverrideEndTime = 0;
        publishAllHaStates();
    }

	handleFlashEffect();
    handleSequencer();
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
				if (millis() - lastWeatherFetch > 300000) {
					lastWeatherFetch = millis();
					WeatherTaskParams* params = new WeatherTaskParams{ currentSettings.cityName, false };
					xTaskCreate(fetchWeatherDataTask, "fetchWeatherDataTask", 8192, params, 1, NULL);
				}
			}

			handlePresetCycling();
			handleSleepSchedule();
			if (currentSettings.stockTickerModeEnabled) {
                if (isMarketOpen()) {
                    if (millis() - lastStockDataFetch > 300000) { 
                        lastStockDataFetch = millis();
                        for (int i=0; i<3; ++i) {
                            FetchDataParams* params = new FetchDataParams{ i, 0 };
                            xTaskCreate(fetchStockDataTask, "fetchStockDataTask", 8192, params, 1, NULL);
                        }
                    }
                }
                updateStockTickerDisplay();
            } else if (isMessageOverrideActive) {
                displayOverrideMessage();
            } else if (isMarqueeOverrideActive) {
                displayMarqueeOverride();
			} else if (currentSettings.dataLinkEnabled) {
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
    handleTemporalGlitch();

	static unsigned long lastNtpUpdate = 0;
	static unsigned long lastHaStateUpdate = 0;

	if (ntpSyncRequested || (!timeSynchronized && millis() > 10000) || (timeSynchronized && millis() - lastNtpUpdate > 3600000)) {
		configTime(0, 0, NTP_SERVERS[currentNtpServerIndex]);
		setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
		tzset();
		struct tm timeinfo;
		if (getLocalTime(&timeinfo, 5000)) {
			if (!timeSynchronized) {
                triggerTemporalGlitch();
            }
			timeSynchronized = true;
		}
		else {
			timeSynchronized = false;
		}
		currentNtpServerIndex = (currentNtpServerIndex + 1) % NUM_NTP_SERVERS;
		lastNtpUpdate = millis();
		ntpSyncRequested = false;
	}

    if (timeSynchronized && millis() - lastHaStateUpdate > 5000) {
        publishAllHaStates();
        lastHaStateUpdate = millis();
    }
}

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