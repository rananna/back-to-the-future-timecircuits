/**
 * @file back-to-the-future-timecircuits.ino
 * @brief Main firmware for the ESP32-based Back to the Future Time Circuits clock.
 * @details This file contains the primary setup and loop functions for the device. It is
 * responsible for initializing all subsystems, managing the main application state, and
 * coordinating the various managers (Display, Animation, Data, etc.).
 *
 * Core functionalities managed here include:
 * - Wi-Fi connection and captive portal management.
 * - Loading and saving settings to non-volatile storage.
 * - Initializing hardware (displays, audio).
 * - The main application loop, which drives the display updates and event handling.
 * - NTP time synchronization.
 * - MQTT connection and reconnection logic.
 * - High-level state machines for display modes and system states.
 */

#include "esp_log.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
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
#include "SettingsManager.h"
#include "WiFiManagerHelper.h"
#include "NTPManager.h"

// Audio Library Includes
#include "Audio.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// --- SYSTEM CONSTANTS ---
const unsigned long WIFI_CONNECT_TIMEOUT = 15000;       // Time in ms to wait for WiFi before starting the captive portal.
const unsigned int MQTT_INITIAL_RETRY_INTERVAL = 5000;  // Initial time in ms to wait before first MQTT reconnect attempt.
const unsigned int MQTT_MAX_RETRY_INTERVAL = 60000;     // Maximum time in ms for the exponential backoff for MQTT reconnects.
const unsigned long NTP_INITIAL_SYNC_DELAY = 2000;      // Delay in ms after connecting to WiFi before the first NTP sync.
const unsigned long DISPLAY_UPDATE_INTERVAL = 250;      // Milliseconds between display updates for the main clock.

// --- MQTT STATE MANAGEMENT ---
// Handles the exponential backoff strategy for reconnecting to the MQTT broker.
unsigned long nextMqttReconnectAttempt = 0;  // Timestamp (millis) for the next scheduled reconnect attempt.
unsigned int mqttReconnectInterval = MQTT_INITIAL_RETRY_INTERVAL; // Current reconnect interval, increases on failure.

// --- STATE VARIABLES ---
BootSequenceState bootState = BOOT_INACTIVE; // Current phase of the cinematic boot sequence.
DisplayModeState currentDisplayMode = NORMAL_CLOCK; // Current primary mode of the display (e.g., clock, weather).

// --- AUDIO GLOBALS ---
Audio audio; // The global audio object from the ESP32-audioI2S library.
char currentSoundFile[MAX_FILENAME_LENGTH] = ""; // Filename of the audio file currently being played.

// --- DEVICE IDENTIFIERS ---
char MQTT_UNIQUE_ID[19]; // The unique identifier for this device, derived from its MAC address.

// --- FUNCTION PROTOTYPES ---
// Forward declarations for functions defined later in this file.
void handlePresetCycling();
void handleSleepSchedule();
void handleSequencer();
bool isMarketOpen();
bool attemptHardwareInit();
void checkDataFetchStatusTask(void* p);
void startAudioStream(const char* url, bool is_tts);
void stopAudioStream();
void updateDisplaySegment(int row, int segment, const std::string& text);
void testDecimalPointFlashing();
void handleScheduledAnimation();

// --- GLOBAL DATA STRUCTURES & SETTINGS ---
SettingsManager settingsManager;
MarqueeData displayPages[5];          // An array to hold the content for the 5 pages of the Data Link marquee.
MarqueeData lastGoodDisplayPages[5];  // A backup of the last valid marquee data to prevent displaying corrupted info.
WeatherData currentWeatherData;       // Holds the most recently fetched weather data.
StockData stockData[3];               // Holds the most recently fetched data for the three stock tickers.
unsigned long lastStockDataFetch = 0; // Timestamp of the last successful stock data fetch.
unsigned long lastDisplayUpdateTime = 0;// Timestamp of the last main display update, used for throttling.
std::string lastCityName = "";        // Caches the last city name used for a weather lookup to avoid redundant geocoding.
unsigned long bootTimestamp = 0;      // Timestamp (millis) when the setup() function completed.
bool hardwareInitialized = false;     // Flag indicating whether the physical hardware (displays, etc.) was successfully initialized.

// --- TIMEZONE DATA ---
// A comprehensive list of timezones supported by the clock.
// Each entry includes the POSIX TZ string, a user-friendly display name, the IANA timezone name, and a region for grouping in the UI.
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
WiFiManagerHelper wifiManagerHelper;
NTPManager ntpManager;
AsyncWebServer server(80);
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

// A more detailed state machine for the main display logic. This helps to cleanly
// separate the logic for each display mode and ensures only one mode is active at a time,
// preventing conflicts between different features trying to control the display.
enum DisplayState {
    STATE_NORMAL_CLOCK,       // Default state, showing the time.
    STATE_MESSAGE_OVERRIDE,
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


/**
 * @brief The main setup function, run once at boot.
 * @details Initializes all essential systems: Serial communication, LittleFS filesystem,
 * loading settings, creating the display data mutex, setting up Wi-Fi, configuring web
 * routes, initializing hardware (display and audio), setting the timezone, and starting
 * the MQTT client and OTA update service.
 */
void setup() {
    Serial.begin(115200);
    delay(1000); // Wait for serial monitor to connect.

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
    settingsManager.load();
    Serial.println(F("BOOT_LOG: Settings loaded... OK"));
    delay(10);

    xDisplayDataMutex = xSemaphoreCreateMutex();
    Serial.println(F("BOOT_LOG: Mutex created... OK"));

    uint8_t mac[6];
    WiFi.macAddress(mac);
    sprintf(MQTT_UNIQUE_ID, "BTTF_TC_%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    wifiManagerHelper.init();
    ntpManager.init();
    Serial.println(F("WEB_LOG: Setting up web routes..."));
    setupWebRoutes();
    Serial.println(F("WEB_LOG: Web routes configured."));

    hardwareInitialized = attemptHardwareInit();
    if (hardwareInitialized) {
        applyBrightness();
        Serial.println(F("BOOT_LOG: Initializing I2S Audio..."));
        audio.setPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DIN_PIN);
        audio.setVolume(settingsManager.settings.notificationVolume);
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

    setenv("TZ", TZ_DATA[settingsManager.settings.presentTimezoneIndex].tzString, 1);
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
    } else if (isAnimating) {
        currentDisplayState = STATE_ANIMATING;
    } else if (isMarqueeOverrideActive) {
        currentDisplayState = STATE_MARQUEE_OVERRIDE;
    } else if (settingsManager.settings.stockTickerModeEnabled) {
        currentDisplayState = STATE_STOCK_TICKER;
    } else if (settingsManager.settings.dataLinkEnabled) {
        currentDisplayState = STATE_DATA_LINK;
    } else if (settingsManager.settings.weatherModeEnabled) {
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


/**
 * @brief The main application loop.
 * @details This function is the heart of the firmware, executed continuously. It uses a
 * cooperative multitasking approach with `vTaskDelay(1)` to yield to other FreeRTOS tasks.
 * The loop manages several key state machines:
 * 1. WiFi Connection: Handles connecting, launching the portal on failure, and reconnecting.
 * 2. MQTT Client: Manages the MQTT connection and message loop.
 * 3. NTP Sync: Periodically synchronizes the internal clock with an NTP server.
 * 4. Display Rendering: Calls the main display handler, which then executes the current
 *    display mode's logic (clock, weather, animation, etc.).
 * 5. OTA Updates: Listens for Over-The-Air update requests.
 */
void loop() {
    vTaskDelay(1); // Yield to other tasks, making the system responsive.
    
    wifiManagerHelper.loop();
    ntpManager.loop();

    if (wifiManagerHelper.isConnected()) {
        if (!logConnectedPrinted) {
            ESP_LOGI("WiFi", "IP: %s", WiFi.localIP().toString().c_str());
            // Start the web server now that we are connected
            server.begin();
            ESP_LOGI("Web", "HTTP server started on successful connection.");

            logConnectedPrinted = true;
            if (MDNS.begin("timecircuits")) {
                MDNS.addService("http", "tcp", 80);
            }
            runBootSequence();
        }
        if (!settingsManager.settings.mqttBroker.empty()) {
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
                setenv("TZ", TZ_DATA[settingsManager.settings.presentTimezoneIndex].tzString, 1);
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
    if (settingsManager.settings.presetCycleInterval == 0 || isAnimating || isDisplayAsleep) return;
    if (millis() - lastPresetCycleTime > (unsigned long)settingsManager.settings.presetCycleInterval * 60000) {
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
    if (settingsManager.settings.timeTravelAnimationInterval == 0 || isAnimating || isDisplayAsleep || isStyledAnimating) {
        return;
    }

    // NEW: Reset the timer once right after the boot sequence completes.
    if (lastScheduledAnimationTime == 0 && bootState == BOOT_INACTIVE) {
        lastScheduledAnimationTime = millis();
    }

    if (lastScheduledAnimationTime > 0 && (millis() - lastScheduledAnimationTime > (unsigned long)settingsManager.settings.timeTravelAnimationInterval * 60000)) {
        startStyledAnimation();
        lastScheduledAnimationTime = millis();
    }
}

void handleSleepSchedule() {
  if (!ntpManager.isTimeSynchronized()) return;
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  int now_minutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  int sleep_minutes = settingsManager.settings.departureHour * 60 + settingsManager.settings.departureMinute;
  int wake_minutes = settingsManager.settings.arrivalHour * 60 + settingsManager.settings.arrivalMinute;

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
    if (!ntpManager.isTimeSynchronized()) return false;
    setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
    tzset();
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    setenv("TZ", TZ_DATA[settingsManager.settings.presentTimezoneIndex].tzString, 1);
    tzset();

    if (timeinfo.tm_wday < 1 || timeinfo.tm_wday > 5) {
        return false;
    }

    int current_minutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int market_open_minutes = 9 * 60 + 30;
    int market_close_minutes = 16 * 60;
    return (current_minutes >= market_open_minutes && current_minutes < market_close_minutes);
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


/**
 * @brief The main setup function, run once at boot.
 * @details Initializes all essential systems: Serial communication, LittleFS filesystem,
 * loading settings, creating the display data mutex, setting up Wi-Fi, configuring web
 * routes, initializing hardware (display and audio), setting the timezone, and starting
 * the MQTT client and OTA update service.
 */
void setup() {
    Serial.begin(115200);
    delay(1000); // Wait for serial monitor to connect.

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


/**
 * @brief The main application loop.
 * @details This function is the heart of the firmware, executed continuously. It uses a
 * cooperative multitasking approach with `vTaskDelay(1)` to yield to other FreeRTOS tasks.
 * The loop manages several key state machines:
 * 1. WiFi Connection: Handles connecting, launching the portal on failure, and reconnecting.
 * 2. MQTT Client: Manages the MQTT connection and message loop.
 * 3. NTP Sync: Periodically synchronizes the internal clock with an NTP server.
 * 4. Display Rendering: Calls the main display handler, which then executes the current
 *    display mode's logic (clock, weather, animation, etc.).
 * 5. OTA Updates: Listens for Over-The-Air update requests.
 */
void loop() {
    vTaskDelay(1); // Yield to other tasks, making the system responsive.
    
    // This state machine manages the WiFi connection process in a non-blocking way.
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
            static unsigned long lastHaStateUpdate = 0;
            if (ntpManager.isTimeSynchronized() && millis() - lastHaStateUpdate > 5000) {
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