/**
 * @file back-to-the-future-timecircuits.ino
 * @brief Main firmware for the ESP32-based Back to the Future Time Circuits clock.
 * @details This file contains the primary setup and loop functions for the device. It is
 * responsible for initializing all subsystems, managing the main application state, and
 * coordinating the various managers (Display, Animation, Data, etc.).
 */

#include "DebugLog.h"
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
#include "MqttManager.h"
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_mac.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <cctype>
#include <LCBUrl.h>
#include <ArduinoOTA.h>
#include "esp_task_wdt.h"

#include "MqttManager.h"
#include "HardwareControl.h"
#include "web_server.h"
#include "api_templates.h"
#include "EventManager.h"
#include "AnimationTypes.h"
#include "AnimationManager.h"
#include "DisplayManager.h"
#include "DataManager.h"
#include "StockManager.h"
#include "timezone.h"

#include <vector>

/**
 * @struct Preset
 * @brief A memory-safe structure to hold the details of a single preset time.
 * @details Uses a fixed-size char array for the name to prevent heap fragmentation.
 */
struct Preset {
    char name[48];
    int year;
    int month;
    int day;
    int hour;
    int minute;

    bool operator==(const ClockSettings& settings) const {
        return year == settings.lastTimeDepartedYear &&
               month == settings.lastTimeDepartedMonth &&
               day == settings.lastTimeDepartedDay &&
               hour == settings.lastTimeDepartedHour &&
               minute == settings.lastTimeDepartedMinute;
    }
};

const std::vector<Preset> moviePresets = {
    {"Einstein's Test (1985)", 1985, 10, 26, 1, 20},
    {"Marty's First Jump (1985)", 1985, 10, 26, 1, 35},
    {"Arrival in Past (1955)", 1955, 11, 5, 6, 0},
    {"Lightning Strike (1955)", 1955, 11, 12, 22, 4},
    {"Future Arrival (2015)", 2015, 10, 21, 16, 29},
    {"Old Biff Gives Almanac (1955)", 1955, 11, 12, 18, 38},
    {"Doc's Arrival in Old West (1885)", 1885, 1, 1, 0, 0},
    {"Marty's Arrival in Old West (1885)", 1885, 9, 2, 8, 0},
    {"Train Push to Future (1885)", 1885, 9, 7, 9, 0}
};


// Audio Library Includes
#include "Audio.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

#define WDT_TIMEOUT 20

// --- SYSTEM CONSTANTS ---
const unsigned long WIFI_CONNECT_TIMEOUT = 15000;
const unsigned int MQTT_INITIAL_RETRY_INTERVAL = 5000;
const unsigned int MQTT_MAX_RETRY_INTERVAL = 60000;
const unsigned int MQTT_MAX_FAILS = 5;
const unsigned long MQTT_HOLDOFF_DURATION = 300000;
const unsigned long NTP_INITIAL_SYNC_DELAY = 2000;
const unsigned long DISPLAY_UPDATE_INTERVAL = 250;
const unsigned long HARDWARE_INIT_RETRY_INTERVAL = 30000;
const unsigned long STOCK_MANAGER_RESET_INTERVAL = 2 * 60 * 60 * 1000;

enum WifiState { WIFI_STATE_CONNECTING, WIFI_STATE_START_PORTAL, WIFI_STATE_PORTAL_RUNNING, WIFI_STATE_CONNECTED };
WifiState wifiState = WIFI_STATE_CONNECTING;
unsigned long wifiConnectStartTime = 0;
TaskHandle_t wifiManagerTaskHandle = NULL;
bool logConnectingPrinted = false;
bool logPortalMsgPrinted = false;
bool logConnectedPrinted = false;

unsigned long nextMqttReconnectAttempt = 0;
unsigned int mqttReconnectInterval = MQTT_INITIAL_RETRY_INTERVAL;
bool initialMqttConnectionAttempted = false;
unsigned int mqttConsecutiveFails = 0;
unsigned long mqttHoldoffUntil = 0;
bool mDnsIsActive = false;

BootSequenceState bootState = BOOT_INACTIVE;
DisplayModeState currentDisplayMode = NORMAL_CLOCK;

Audio audio;
bool isPlayingSound = false;
bool isSoundFromMqtt = false;
StockManager stockManager;
char currentSoundFile[MAX_FILENAME_LENGTH] = "";

char MQTT_UNIQUE_ID[21];

void handlePresetCycling();
void handleSleepSchedule();
bool attemptHardwareInit();
void onHardwareInitialized();
void checkDataFetchStatusTask(void* p);
void wifiManagerTask(void *pvParameters);
void updateDisplaySegment(int row, int segment, const char* text);

ClockSettings currentSettings;
MarqueeData displayPages[5];
MarqueeData lastGoodDisplayPages[5];
WeatherData currentWeatherData;
unsigned long lastDisplayUpdateTime = 0;
char lastCityName[64] = "";
unsigned long bootTimestamp = 0;
bool hardwareInitialized = false;
unsigned long lastHardwareInitAttempt = 0;

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
unsigned long lastPresetCycleTime = 0;
bool isEchoEffectActive = false;
unsigned long echoEffectStartTime = 0;
unsigned long lastEchoCheckTime = 0;
bool isFlickeringNow = false;
unsigned long flickerStartTime = 0;
int flickerDisplayIndex = -1;
MarqueeState marqueeState = M_START_PAGE;
unsigned long lastDataLinkFetch = 0;
unsigned long lastMarqueeStateChange = 0;
int marqueeScrollPosition = 0;
int marqueeScrollPositionYear = 0;
volatile bool isFetchingData = false;
volatile bool isFetchingWeather = false;
volatile bool justFinishedAnimation = false;

int dataPointFetchFailures[5] = {0, 0, 0, 0, 0};
const int MAX_FETCH_FAILURES = 3;
volatile int requestsCompleted = 0;
int currentPageIndex = 0;
bool isMessageOverrideActive = false;
char overrideMessageLine1[128] = "";
char overrideMessageLine2[128] = "";
char overrideMessageLine3[128] = "";
SemaphoreHandle_t xDisplayDataMutex;
SemaphoreHandle_t xAnimationStartMutex;
SemaphoreHandle_t xTimeLibMutex;
SemaphoreHandle_t xDisplayHardwareMutex;

SequencerTrack sequencerTracks[3];

enum DisplayState { STATE_NORMAL_CLOCK, STATE_MESSAGE_OVERRIDE, STATE_ANIMATING, STATE_STOCK_TICKER, STATE_DATA_LINK, STATE_WEATHER };
DisplayState currentDisplayState = STATE_NORMAL_CLOCK;

void audioTask(void *pvParameters) {
  for (;;) {
    audio.loop();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void applySettingsFromJson(const JsonObject& obj);

void applyAndSaveSettings(JsonVariant& json) {
    JsonObject obj = json.as<JsonObject>();
    applySettingsFromJson(obj);
    saveSettings();
    audio.setVolume(currentSettings.notificationVolume);
}

void handleWeatherTimeout() {
    Log_printf(LOG_LEVEL_WARN, "Weather fetch timed out. Disabling weather mode.");
    currentWeatherData.dataValid = false;
    strncpy(currentWeatherData.errorReason, "FETCH TIMEOUT", sizeof(currentWeatherData.errorReason));
    currentSettings.displayMode = DMS_NORMAL_CLOCK;
    saveSettings();
    if (mqttClient.connected()) {
        publishDisplayMode(currentSettings.displayMode);
    }
    resetWeatherFetchState();
}

void applySettingsFromJson(const JsonObject& obj) {
    bool needsMqttReconnect = false;
    char oldMqttBroker[64];
    strncpy(oldMqttBroker, currentSettings.mqttBroker, sizeof(oldMqttBroker));
    // ... (rest of change detection variables) ...

    // --- Apply settings from JSON (logic remains the same, but uses strncpy for char arrays) ---
    if (!obj["mqttBroker"].isNull()) {
        strncpy(currentSettings.mqttBroker, obj["mqttBroker"], sizeof(currentSettings.mqttBroker) - 1);
        currentSettings.mqttBroker[sizeof(currentSettings.mqttBroker) - 1] = '\0';
    }
    // ... (repeat for all string settings) ...

    // --- FIX: Logic for Data Link settings remains the same, but uses strncpy ---
    if (!obj["dataPoints"].isNull()) {
        // ...
        if (!dp["scrollingText"].isNull()) {
            const char* newText = dp["scrollingText"];
            if (strcmp(currentSettings.dataPoints[i].scrollingText, newText) != 0) {
                strncpy(currentSettings.dataPoints[i].scrollingText, newText, sizeof(currentSettings.dataPoints[i].scrollingText) - 1);
                isMarqueeBufferDirty = true;
            }
        }
        // ... (repeat for prefix/suffix) ...
    }

    if (strcmp(oldMqttBroker, currentSettings.mqttBroker) != 0) {
        needsMqttReconnect = true;
    }
    // ... (rest of change detection logic) ...
}

void saveSettings() {
    preferences.begin(PREFERENCES_NAMESPACE, false);
    // ... (saving logic remains mostly the same, but uses .c_str() on char arrays where needed if library expects it, though it's direct put for Preferences) ...
    preferences.putString("mqttBroker", currentSettings.mqttBroker);
    preferences.putString("cityName", currentSettings.cityName);
    // ...
    preferences.end();
    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
	tzset();
}

void loadSettings() {
    // ... (loading logic remains the same, but uses strncpy to load into char arrays) ...
    String tempString = preferences.getString("mqttBroker", "broker.emqx.io");
    strncpy(currentSettings.mqttBroker, tempString.c_str(), sizeof(currentSettings.mqttBroker) - 1);
    // ... (repeat for all string settings) ...
}

// ... (other setup functions remain the same) ...

void setup() {
    Serial.begin(115200);
    delay(1000);
    randomSeed(analogRead(0));
    // ... (watchdog, MAC, LittleFS, etc.) ...
    loadSettings();
    // ... (rest of setup) ...
}

// ... (main loop logic remains largely the same) ...

/**
 * @brief Constructs a list of presets in a memory-safe way.
 * @details This function is now memory-safe. It uses a static vector to avoid
 * reallocating memory on every call. It reads custom presets from NVS and parses
 * them into the static vector, which is passed by reference to the caller.
 * @param allPresets A reference to a static vector<Preset> to be populated.
 */
void getFullPresetList(std::vector<Preset>& allPresets) {
    allPresets.clear();
    allPresets = moviePresets;

    preferences.begin(PREFERENCES_NAMESPACE, true);
    String presetsJson = preferences.getString("customPresets", "[]");
    preferences.end();

    static JsonDocument doc;
    doc.clear();
    DeserializationError error = deserializeJson(doc, presetsJson);

    if (!error) {
        JsonArray customPresets = doc.as<JsonArray>();
        for (JsonObject presetObj : customPresets) {
            Preset p;
            const char* value_str = presetObj["value"];
            if (sscanf(value_str, "%d-%d-%d-%d-%d", &p.year, &p.month, &p.day, &p.hour, &p.minute) == 5) {
                const char* name_str = presetObj["name"];
                strncpy(p.name, name_str, sizeof(p.name) - 1);
                p.name[sizeof(p.name) - 1] = '\0';
                allPresets.push_back(p);
            }
        }
    } else {
        Log_printf(LOG_LEVEL_ERROR, "Failed to parse custom presets JSON: %s", error.c_str());
    }
}

/**
 * @brief Handles the automatic cycling of presets in a memory-safe way.
 * @details This function is now memory-safe. It uses a static vector for presets
 * to prevent heap fragmentation from repeated allocations in a background task.
 */
void handlePresetCycling() {
    if (justFinishedAnimation || !bootSequenceCompleted || isAnySequenceActive() || currentDisplayState != STATE_NORMAL_CLOCK) {
        return;
    }
    if (lastPresetCycleTime == 0 && bootState == BOOT_INACTIVE) {
        lastPresetCycleTime = millis();
    }
    if (currentSettings.presetCycleInterval == 0 || isDisplayAsleep) {
        return;
    }

    if (millis() - lastPresetCycleTime > (unsigned long)currentSettings.presetCycleInterval * 60000) {
        lastPresetCycleTime = millis();

        Log_printf(LOG_LEVEL_INFO, "Preset cycle triggered.");

        static std::vector<Preset> allPresets;
        getFullPresetList(allPresets);

        if (allPresets.empty()) {
            Log_printf(LOG_LEVEL_WARN, "No presets available to cycle.");
            return;
        }

        int currentIndex = -1;
        for (size_t i = 0; i < allPresets.size(); ++i) {
            if (allPresets[i] == currentSettings) {
                currentIndex = i;
                break;
            }
        }

        int nextIndex = (currentIndex == -1) ? 0 : (currentIndex + 1) % allPresets.size();
        const Preset& nextPreset = allPresets[nextIndex];
        Log_printf(LOG_LEVEL_INFO, "Cycling to next preset: %s", nextPreset.name);

        currentSettings.lastTimeDepartedYear = nextPreset.year;
        currentSettings.lastTimeDepartedMonth = nextPreset.month;
        currentSettings.lastTimeDepartedDay = nextPreset.day;
        currentSettings.lastTimeDepartedHour = nextPreset.hour;
        currentSettings.lastTimeDepartedMinute = nextPreset.minute;

        broadcastPresetUpdate(nextPreset.name, nextPreset.year, nextPreset.month, nextPreset.day, nextPreset.hour, nextPreset.minute);

        if (currentSettings.timeTravelSoundToggle) {
            playSound("electric_sparks.mp3", false, -1);
        }
        triggerAnimation(currentSettings.animationSequence);
    }
}

// ... (rest of file remains the same) ...

void loop() {
    esp_task_wdt_reset();
    // ...
    // The main loop calls handlePresetCycling, which is now memory-safe.
    // ...
}