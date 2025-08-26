#include "esp_log.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <time.h>
#include <AsyncJson.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <string>
#include <Update.h>
#include <ArduinoOTA.h>
#include "esp32-hal-psram.h"
#include <Preferences.h>
#include <PubSubClient.h>
#include "WifiManagernew.h" // Added for the new WifiManager functions
// --- Core Project Includes ---
#include "types.h"
#include "globals.h"
#include "web_server.h"
#include "config.h"
#include "HardwareControl.h"
#include "api_templates.h"
#include "EventManager.h"
#include "AnimationManager.h"
#include "DisplayManager.h"
#include "DataManager.h"
#include "MqttManager.h"
#include "logger.h"


// Audio Library Includes
#include "AudioFileSourceLittleFS.h"
#include "AudioOutputI2S.h"
#include "AudioGeneratorMP3.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// --- FUNCTION PROTOTYPES ---
void handlePresetCycling();
void handleSleepSchedule();
void handleSequencer();
bool isMarketOpen();
bool attemptHardwareInit();
void handleAudio();
void checkDataFetchStatusTask(void* p);
void playTtsFromUrl(const char* url);
void ttsDownloadTask(void* parameter);
void playRadioStream(const char* url);
void stopRadioStream();
void logMessage(LogLevel level, const char* source, const char* format, ...);
void fetchStockDataTask(void* p);
void forceFetchWeatherDataTask(void* p);
void fetchDataLink();
void broadcastLog(const char* message);
void setupWebRoutes();
void handleFlashEffect();
void updateStockTickerDisplay();
void displayOverrideMessage();
void displayMarqueeOverride();
void updateMarqueeDisplay();
void updateNormalClockDisplay();
void handleWeatherDisplay();
void restoreDisplayAfterGlitch();
void handleTemporalEcho();
void handleGlitchEffect();
void handleBootSequence();
void handleMalfunction();
void triggerTemporalGlitch();
void triggerFlashEffect(int row, int segment, unsigned long duration = 1000);
void runBootSequence();
void saveSettings();
void wifiProvisioning();
// --- GLOBALS ---
bool isProvisioningMode = false;
unsigned long lastConnectionAttempt = 0;
const long connectionTimeoutMs = 15000;
const char* WIFI_PREF_NAMESPACE = "wifi_creds";

void saveWiFiCredentials(const char* ssid, const char* password) {
    Preferences preferences;
    preferences.begin(WIFI_PREF_NAMESPACE, false);
    preferences.putString("ssid", ssid);
    preferences.putString("pass", password);
    preferences.end();
    logMessage(LOG_INFO, "WIFI", "Credentials saved.");
}

void saveSettings() {
    logMessage(LOG_INFO, "SETTINGS", "--- Saving Settings ---");
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
	preferences.putInt("dlTargetRow", 2);
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
    logMessage(LOG_INFO, "SETTINGS", "Saving avApiKey: [%s]", currentSettings.alphaVantageApiKey.c_str());
	preferences.putString("avApiKey", currentSettings.alphaVantageApiKey.c_str());
    preferences.putBool("loggingEnabled", currentSettings.loggingEnabled);
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
		preferences.putString((prefix + "apiKey").c_str(), currentSettings.dataPoints[i].apiExampleKey.c_str());
	}
	preferences.end();
    logMessage(LOG_INFO, "SETTINGS", "--- Settings Saved ---");
    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
	tzset();
}

void loadSettings() {
    logMessage(LOG_INFO, "SETTINGS", "--- Loading Settings ---");
    preferences.begin(PREFERENCES_NAMESPACE, true);
    bool needsInit = !preferences.isKey("destYear");
	if (needsInit) {
		logMessage(LOG_INFO, "SETTINGS", "No settings found. Initializing with defaults.");
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
		currentSettings.theme = 0;
		currentSettings.timeTravelAnimationInterval = 15;
        currentSettings.timeTravelAnimationDuration = 4000;
		currentSettings.animationStyle = 0;
		currentSettings.glitchEffectFrequency = 0;
		currentSettings.malfunctionFrequency = 25;
		currentSettings.timeTravelVolumeFade = true;
		currentSettings.dataLinkEnabled = false;
        currentSettings.dataLinkRefreshInterval = 10;
		currentSettings.numDataPoints = 0;
		currentSettings.mqttBroker = "broker.emqx.io";
		currentSettings.mqttPort = 1883;
		currentSettings.mqttUser = "";
        currentSettings.mqttPassword = "";
		currentSettings.weatherModeEnabled = false;
		currentSettings.cityName = "New York";
		currentSettings.useMetricUnits = false;
        currentSettings.stockTickerModeEnabled = false;
		currentSettings.stockRow1_symbol = "^GSPC";
		currentSettings.stockRow2_symbol = "^GSPTSE";
		currentSettings.stockRow3_symbol = "^IXIC";
		currentSettings.alphaVantageApiKey = "";
        currentSettings.loggingEnabled = true;
		for (int i = 0; i < 5; i++) {
			currentSettings.dataPoints[i] = {};
        }
		saveSettings();
    } else {
		logMessage(LOG_INFO, "SETTINGS", "Loading settings from NVS.");
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
		currentSettings.theme = preferences.getInt("theme", 0);
        currentSettings.timeTravelAnimationInterval = preferences.getInt("animInterval");
		currentSettings.timeTravelAnimationDuration = preferences.getInt("animDuration");
		currentSettings.animationStyle = preferences.getInt("animStyle");
		currentSettings.glitchEffectFrequency = preferences.getInt("glitchFreq");
		currentSettings.malfunctionFrequency = preferences.getInt("malfuncFreq");
		currentSettings.timeTravelVolumeFade = preferences.getBool("volFade");
        currentSettings.dataLinkEnabled = preferences.getBool("dlEnabled");
		currentSettings.dataLinkRefreshInterval = preferences.getInt("dlRefresh");
		currentSettings.numDataPoints = preferences.getInt("numDataPoints");
		String tempString;
		tempString = preferences.getString("mqttBroker", "");
        currentSettings.mqttBroker = tempString;
		tempString = preferences.getString("mqttUser", "");
		currentSettings.mqttUser = tempString;

		tempString = preferences.getString("mqttPass", "");
		currentSettings.mqttPassword = tempString;
        currentSettings.weatherModeEnabled = preferences.getBool("weatherMode", false);
		tempString = preferences.getString("cityName", "New York");
		currentSettings.cityName = tempString;

		currentSettings.useMetricUnits = preferences.getBool("useMetric", false);
		currentSettings.stockTickerModeEnabled = preferences.getBool("stModeEnabled", false);

		tempString = preferences.getString("stRow1Sym", "^GSPC");
		currentSettings.stockRow1_symbol = tempString;
		tempString = preferences.getString("stRow2Sym", "^GSPTSE");
        currentSettings.stockRow2_symbol = tempString;
		tempString = preferences.getString("stRow3Sym", "^IXIC");
		currentSettings.stockRow3_symbol = tempString;

		tempString = preferences.getString("avApiKey", "");
		logMessage(LOG_INFO, "SETTINGS", "Loading avApiKey from Preferences: [%s]", tempString.c_str());
		currentSettings.alphaVantageApiKey = tempString;
        logMessage(LOG_INFO, "SETTINGS", "Loaded avApiKey into currentSettings: [%s]", currentSettings.alphaVantageApiKey.c_str());
		currentSettings.loggingEnabled = preferences.getBool("loggingEnabled", true);
		for (int i = 0; i < 5; i++) {
			String prefix = "dp" + String(i) + "_";
			tempString = preferences.getString((prefix + "url").c_str(), "");
			currentSettings.dataPoints[i].url = tempString;

			tempString = preferences.getString((prefix + "monthPath").c_str(), "");
			currentSettings.dataPoints[i].monthPath = tempString;
			tempString = preferences.getString((prefix + "dayPath").c_str(), "");
			currentSettings.dataPoints[i].dayPath = tempString;

			tempString = preferences.getString((prefix + "yearPath").c_str(), "");
			currentSettings.dataPoints[i].yearPath = tempString;
			tempString = preferences.getString((prefix + "timePath").c_str(), "");
			currentSettings.dataPoints[i].timePath = tempString;

			tempString = preferences.getString((prefix + "prefix").c_str(), "");
			currentSettings.dataPoints[i].prefix = tempString;
			tempString = preferences.getString((prefix + "suffix").c_str(), "");
			currentSettings.dataPoints[i].suffix = tempString;

			tempString = preferences.getString((prefix + "icon").c_str(), "");
			currentSettings.dataPoints[i].icon = tempString;
			currentSettings.dataPoints[i].scrollSpeed = preferences.getInt((prefix + "scroll").c_str());
			currentSettings.dataPoints[i].dataSourceType = (DataSourceType)preferences.getInt((prefix + "srcType").c_str());

			tempString = preferences.getString((prefix + "topic").c_str(), "");
			currentSettings.dataPoints[i].mqttTopic = tempString;
			tempString = preferences.getString((prefix + "yearPrefix").c_str(), "");
			currentSettings.dataPoints[i].yearPrefix = tempString;

			tempString = preferences.getString((prefix + "yearSuffix").c_str(), "");
			currentSettings.dataPoints[i].yearSuffix = tempString;
			currentSettings.dataPoints[i].displayMode = (DisplayMode)preferences.getInt((prefix + "dispMode").c_str(), 0);

			tempString = preferences.getString((prefix + "scrollTxt").c_str(), "");
			currentSettings.dataPoints[i].scrollingText = tempString;
			tempString = preferences.getString((prefix + "authKey").c_str(), "");
			currentSettings.dataPoints[i].authHeaderKey = tempString;

			tempString = preferences.getString((prefix + "authVal").c_str(), "");
			currentSettings.dataPoints[i].authHeaderValue = tempString;
			tempString = preferences.getString((prefix + "apiKey").c_str(), "");
			currentSettings.dataPoints[i].apiExampleKey = tempString;
		}
	}
	preferences.end();
	logMessage(LOG_INFO, "SETTINGS", "--- Settings Loaded ---");
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
	logMessage(LOG_INFO, "FS", "--- Listing all files in LittleFS ---");
	File root = LittleFS.open("/");
	File file = root.openNextFile();
	while (file) {
		logMessage(LOG_INFO, "FS", "  FILE: %s\tSIZE: %d", file.name(), file.size());
		file.close();
		file = root.openNextFile();
	}
	logMessage(LOG_INFO, "FS", "--- End of file list ---\n");
    root.close();
}

bool attemptHardwareInit() {
    #if ENABLE_HARDWARE
    logMessage(LOG_INFO, "BOOT", "Attempting to initialize hardware...");
    setupPhysicalDisplay();
    logMessage(LOG_INFO, "BOOT", "Physical display setup... OK");
    return true; // Success
    #else
    logMessage(LOG_WARN, "BOOT", "Hardware is disabled (ENABLE_HARDWARE = 0)");
    return false; // Hardware is disabled, so it's not "initialized"
    #endif
}

void wifiProvisioning() {
    Preferences preferences;
    preferences.begin(WIFI_PREF_NAMESPACE, true);
    String ssid = preferences.getString("ssid", "");
    String password = preferences.getString("pass", "");
    preferences.end();
    if (ssid.length() > 0) {
        logMessage(LOG_INFO, "WIFI", "Connecting to saved network: %s", ssid.c_str());
		WiFi.begin(ssid.c_str(), password.c_str());
        isProvisioningMode = false;
    } else {
        logMessage(LOG_INFO, "WIFI", "No saved credentials, starting AP mode.");
		const char* apName = "BTTF-Clock-Setup";
        const char* apPassword = "password";
		// Use a simple password for initial setup
        WiFi.softAP(apName, apPassword);
		IPAddress apIP(192, 168, 4, 1);
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        MDNS.begin("timecircuits");
        MDNS.addService("http", "tcp", 80);
        isProvisioningMode = true;
    }
    lastConnectionAttempt = millis();
}

void setup() {
	Serial.begin(921600);
	delay(1000);
    if (psramInit()) {
        logMessage(LOG_INFO, "BOOT", "PSRAM initialized... OK");
		logMessage(LOG_INFO, "MEM", "Free PSRAM: %u bytes", ESP.getFreePsram());
    } else {
        logMessage(LOG_ERROR, "BOOT", "PSRAM initialization... FAILED!");
    }

	logMessage(LOG_INFO, "BOOT", "\n\n--- BOOTING ---");
	logMessage(LOG_INFO, "BOOT", "Initializing Serial... OK");
    delay(10);
    if (!LittleFS.begin(true)) {
		logMessage(LOG_CRITICAL, "FS", "CRITICAL ERROR: LittleFS Mount Failed. Restarting in 10 seconds.");
		logMessage(LOG_ERROR, "BOOT", "LittleFS mount... FAILED!");
		delay(10000);
		ESP.restart();
    }
    logMessage(LOG_INFO, "BOOT", "LittleFS mount... OK");
    delay(10); 

	listAllFiles();
	logMessage(LOG_INFO, "BOOT", "Loading settings...");
	loadSettings();
    logMessage(LOG_INFO, "BOOT", "Settings loaded... OK");
    delay(10);

	xDisplayDataMutex = xSemaphoreCreateMutex();
    xAudioMutex = xSemaphoreCreateMutex();
	logMessage(LOG_INFO, "BOOT", "Mutex created... OK");
    // Replaced WiFiManager with custom function
    wifiProvisioning();

    ArduinoOTA.setHostname("timecircuits");
    ArduinoOTA.begin();

	logMessage(LOG_INFO, "WEB", "Setting up web routes...");
	setupWebRoutes();
    logger.setBroadcastCallback(broadcastLog);
    logMessage(LOG_INFO, "WEB", "Web routes configured. Starting server...");
	server.begin();
    logMessage(LOG_INFO, "WEB", "HTTP server started.");
    logMessage(LOG_INFO, "WEB", "Web server started... OK");
    hardwareInitialized = attemptHardwareInit();
	if(hardwareInitialized) {
        out = new AudioOutputI2S();
        out->SetPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DIN_PIN);
		out->SetGain((float)currentSettings.notificationVolume / 30.0f);
        mp3 = new AudioGeneratorMP3();
        logMessage(LOG_INFO, "BOOT", "I2S Audio... OK");
    }

	configTime(0, 0, NTP_SERVERS[0]);
    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
    tzset();
    logMessage(LOG_INFO, "TIME", "Timezone configured.");
    setupMqtt();
    logMessage(LOG_INFO, "MQTT", "MQTT setup initiated.");
	logMessage(LOG_INFO, "MEM", "Free heap after setup: %u bytes", ESP.getFreeHeap());
    logMessage(LOG_INFO, "MEM", "Free PSRAM: %u bytes", ESP.getFreePsram());
    logMessage(LOG_INFO, "BOOT", "Free heap: %u bytes\n", ESP.getFreeHeap());
    logMessage(LOG_INFO, "BOOT", "Calling runBootSequence()...");
    runBootSequence();
    logMessage(LOG_INFO, "BOOT", "--- BOOT COMPLETE ---");
    bootTimestamp = millis();
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
    ArduinoOTA.handle();

    if (isProvisioningMode) {
        // Keep the device in AP mode
    } else {
        // Handle reconnection logic for STA mode
        if (WiFi.status() != WL_CONNECTED && millis() - lastConnectionAttempt > 5000) {
            logMessage(LOG_WARN, "WIFI", "Connection lost, attempting to reconnect...");
			wifiProvisioning();
        }
    }

    if (hardwareInitialized) {
        handleAudio();
    }

	if (millis() - bootTimestamp > 15000) { 
        if (WiFi.status() == WL_CONNECTED && !currentSettings.mqttBroker.isEmpty()) {
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
    }

    if (isMarqueeOverrideActive && marqueeOverrideEndTime > 0 && millis() > marqueeOverrideEndTime) {
        isMarqueeOverrideActive = false;
		marqueeOverrideEndTime = 0;
        publishAllHaStates();
    }

    if (hardwareInitialized) {
        handleFlashEffect();
        handleBootSequence();
		if (isMalfunctioning) {
            handleMalfunction();
		}
    }
    
    handleSequencer();
	if (!isMalfunctioning && !isAnimating) {
        if (hardwareInitialized) {
		    restoreDisplayAfterGlitch();
            handleTemporalEcho();
		}
		if (!isFlickeringNow) {
			handleGlitchEffect();
			if (currentSettings.weatherModeEnabled) {
				static unsigned long lastWeatherFetch = 0;
                if (millis() - lastWeatherFetch > 300000) {
					lastWeatherFetch = millis();
					WeatherTaskParams* params = new WeatherTaskParams{ std::string(currentSettings.cityName.c_str()), false };
                    xTaskCreatePinnedToCore(fetchWeatherDataTask, "fetchWeatherDataTask", 8192, params, 1, NULL, 0);
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
							xTaskCreatePinnedToCore(fetchStockDataTask, "fetchStockDataTask", 8192, params, 1, NULL, 0);
                        }
                    }
                }
                if (hardwareInitialized) updateStockTickerDisplay();
			} else if (isMessageOverrideActive) {
                if (hardwareInitialized) displayOverrideMessage();
			} else if (isMarqueeOverrideActive) {
                if (hardwareInitialized) displayMarqueeOverride();
			} else if (currentSettings.dataLinkEnabled) {
				fetchDataLink();
				if (hardwareInitialized) updateMarqueeDisplay();
			} else {
				if (hardwareInitialized) updateNormalClockDisplay();
				if (currentSettings.weatherModeEnabled) {
					if (hardwareInitialized) handleWeatherDisplay();
				}
			}
		}
	}
    
    if (hardwareInitialized) {
	    handleDisplayAnimation();
        handleTemporalGlitch();
    }

	static unsigned long lastNtpUpdate = 0;
	static unsigned long lastHaStateUpdate = 0;
	if (ntpSyncRequested || (!timeSynchronized && millis() > 10000) || (timeSynchronized && millis() - lastNtpUpdate > 3600000)) {
		configTime(0, 0, NTP_SERVERS[0]);
		setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
		tzset();
		struct tm timeinfo;
		if (getLocalTime(&timeinfo, 5000)) {
			if (!timeSynchronized) {
                if (hardwareInitialized) triggerTemporalGlitch();
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

// ... other functions

void handleAudio() {
    if (xSemaphoreTake(xAudioMutex, 0) == pdTRUE) {
        if (isStreamingRadio) {
        } else if (isPlayingSound && mp3->isRunning()) {
            if (!mp3->loop()) {
                mp3->stop();
                isPlayingSound = false;
                digitalWrite(I2S_SD_PIN, LOW); // Disable amplifier
                delete file;
                file = nullptr;
                if (!ttsFile.isEmpty()) {
                    LittleFS.remove(ttsFile);
                    ttsFile = "";
                }
            }
        }
        xSemaphoreGive(xAudioMutex);
    }
}

void ttsDownloadTask(void* parameter) {
    const char* url = (const char*)parameter;
    if (xSemaphoreTake(xAudioMutex, portMAX_DELAY) == pdTRUE) {
        logMessage(LOG_INFO, "AUDIO", "Downloading TTS audio from: %s", url);
        HTTPClient http;
        WiFiClient client;
        http.begin(client, url);

        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            File ttsFileHandle = LittleFS.open("/tts.mp3", "w");
            if (ttsFileHandle) {
                http.writeToStream(&ttsFileHandle);
                ttsFileHandle.close();
                ttsFile = "/tts.mp3";
                logMessage(LOG_INFO, "AUDIO", "TTS file downloaded successfully.");
                playSound(ttsFile.c_str());
            } else {
                logMessage(LOG_ERROR, "AUDIO", "Failed to open file for writing.");
                ttsFile = "";
            }
        } else {
            logMessage(LOG_ERROR, "AUDIO", "HTTP GET failed with code: %d", httpCode);
            ttsFile = "";
        }
        http.end();
        xSemaphoreGive(xAudioMutex);
    }
    delete[] (char*)url;
    vTaskDelete(NULL);
}

void handleSequencer() {
    if (!isSequenceActive) return;
    SequenceStep step = sequence[currentSequenceStep];
    unsigned long elapsed = millis() - sequenceStepStartTime;
	switch (step.command) {
        case SEQ_CMD_TEXT:
            if (hardwareInitialized) updateDisplaySegment(step.targetRow, step.targetSegment, step.stringParam.c_str());
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


void playRadioStream(const char* url) {
    stopRadioStream();
	if (xSemaphoreTake(xAudioMutex, portMAX_DELAY) == pdTRUE) {
        logMessage(LOG_INFO, "AUDIO", "Starting radio stream from: %s", url);
		logMessage(LOG_WARN, "AUDIO", "Please use the MQTT 'play_radio' command instead.");
        isStreamingRadio = true;
        xSemaphoreGive(xAudioMutex);
	}
}

void stopRadioStream() {
    if (!isStreamingRadio) return;
    if (xSemaphoreTake(xAudioMutex, portMAX_DELAY) == pdTRUE) {
        logMessage(LOG_INFO, "AUDIO", "Stopping radio stream.");
		logMessage(LOG_WARN, "AUDIO", "Please use the MQTT 'stop_radio' command instead.");
        isStreamingRadio = false;
        xSemaphoreGive(xAudioMutex);
	}
}

void logMessage(LogLevel level, const char* source, const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    const char* levelStr;
    switch(level) {
        case LOG_INFO: levelStr = "INFO";
		break;
        case LOG_WARN: levelStr = "WARN";
        break;
        case LOG_ERROR: levelStr = "ERROR"; break;
        case LOG_CRITICAL: levelStr = "CRITICAL"; break;
		default: levelStr = "UNKNOWN"; break;
    }
    Serial.printf("[%lu] [%s] %s: %s\n", millis(), source, levelStr, buffer);
	if (currentSettings.loggingEnabled) {
		logger.pushMessage(level, source, (const char*)buffer);
	}
}