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
#include "StockManager.h"
#include "timezone.h"

#include <vector>

// --- PRESET DEFINITIONS ---
// A structure to hold the details of a single preset time.
struct Preset {
    std::string name;
    int year;
    int month;
    int day;
    int hour;
    int minute;

    // Helper to check for equality, useful for finding the current preset
    bool operator==(const ClockSettings& settings) const {
        return year == settings.lastTimeDepartedYear &&
               month == settings.lastTimeDepartedMonth &&
               day == settings.lastTimeDepartedDay &&
               hour == settings.lastTimeDepartedHour &&
               minute == settings.lastTimeDepartedMinute;
    }
};

// A constant vector containing all the presets from the movies.
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

// --- SYSTEM CONSTANTS ---
const unsigned long WIFI_CONNECT_TIMEOUT = 15000;       // Time in ms to wait for WiFi before starting the captive portal.
const unsigned int MQTT_INITIAL_RETRY_INTERVAL = 5000;  // Initial time in ms to wait before first MQTT reconnect attempt.
const unsigned int MQTT_MAX_RETRY_INTERVAL = 60000;     // Maximum time in ms for the exponential backoff for MQTT reconnects.
const unsigned long NTP_INITIAL_SYNC_DELAY = 2000;      // Delay in ms after connecting to WiFi before the first NTP sync.
const unsigned long DISPLAY_UPDATE_INTERVAL = 250;      // Milliseconds between display updates for the main clock.
const unsigned long HARDWARE_INIT_RETRY_INTERVAL = 30000; // Time in ms to wait before retrying hardware init.
const unsigned long STOCK_MANAGER_RESET_INTERVAL = 2 * 60 * 60 * 1000; // 2 hours

// --- WIFI STATE MANAGEMENT ---
// Manages the asynchronous, non-blocking WiFi connection process.
enum WifiState {
  WIFI_STATE_CONNECTING,      // Actively trying to connect with saved credentials.
  WIFI_STATE_START_PORTAL,    // Connection failed, preparing to launch the WiFiManager captive portal.
  WIFI_STATE_PORTAL_RUNNING,  // Captive portal is active and awaiting user configuration.
  WIFI_STATE_CONNECTED        // WiFi connection is successful.
};
WifiState wifiState = WIFI_STATE_CONNECTING; // Current state of the WiFi connection.
unsigned long wifiConnectStartTime = 0;      // Timestamp (millis) when the WiFi connection process began.
TaskHandle_t wifiManagerTaskHandle = NULL;   // Handle for the FreeRTOS task running the WiFiManager portal.
bool logConnectingPrinted = false;           // Flag to ensure the "Connecting..." message is logged only once.
bool logPortalMsgPrinted = false;            // Flag to ensure the "Portal Starting..." message is logged only once.
bool logConnectedPrinted = false;            // Flag to ensure the "Connected" message is logged only once.

// --- MQTT STATE MANAGEMENT ---
// Handles the exponential backoff strategy for reconnecting to the MQTT broker.
unsigned long nextMqttReconnectAttempt = 0;  // Timestamp (millis) for the next scheduled reconnect attempt.
unsigned int mqttReconnectInterval = MQTT_INITIAL_RETRY_INTERVAL; // Current reconnect interval, increases on failure.

// --- STATE VARIABLES ---
BootSequenceState bootState = BOOT_INACTIVE; // Current phase of the cinematic boot sequence.
DisplayModeState currentDisplayMode = NORMAL_CLOCK; // Current primary mode of the display (e.g., clock, weather).

// --- AUDIO GLOBALS ---
Audio audio; // The global audio object from the ESP32-audioI2S library.
StockManager stockManager;
char currentSoundFile[MAX_FILENAME_LENGTH] = ""; // Filename of the audio file currently being played.

// --- DEVICE IDENTIFIERS ---
char MQTT_UNIQUE_ID[19]; // The unique identifier for this device, derived from its MAC address.

// --- FUNCTION PROTOTYPES ---
// Forward declarations for functions defined later in this file.
void handlePresetCycling();
void handleSleepSchedule();
void handleSequencer();
bool attemptHardwareInit();
void onHardwareInitialized();
void checkDataFetchStatusTask(void* p);
void startAudioStream(const char* url, bool is_tts);
void stopAudioStream();
void wifiManagerTask(void *pvParameters);
void updateDisplaySegment(int row, int segment, const std::string& text);
void testDecimalPointFlashing();
void handleScheduledAnimation();

// --- GLOBAL DATA STRUCTURES & SETTINGS ---

ClockSettings currentSettings;        // Holds all user-configurable settings for the clock.
MarqueeData displayPages[5];          // An array to hold the content for the 5 pages of the Data Link marquee.
MarqueeData lastGoodDisplayPages[5];  // A backup of the last valid marquee data to prevent displaying corrupted info.
WeatherData currentWeatherData;       // Holds the most recently fetched weather data.
unsigned long lastDisplayUpdateTime = 0;// Timestamp of the last main display update, used for throttling.
std::string lastCityName = "";        // Caches the last city name used for a weather lookup to avoid redundant geocoding.
unsigned long bootTimestamp = 0;      // Timestamp (millis) when the setup() function completed.
bool hardwareInitialized = false;     // Flag indicating whether the physical hardware (displays, etc.) was successfully initialized.
unsigned long lastHardwareInitAttempt = 0; // Timestamp for the last hardware initialization attempt.

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

// --- Asynchronous Settings Save Mechanism ---
// This flag is set by the web server when a save request is received.
// The main loop then handles the actual saving process in the background.
volatile bool saveSettingsRequested = false;
// This string buffers the JSON payload from the web UI to be saved.
String settingsToSaveJson;

bool isDisplayAsleep = false;
unsigned long bootStateStartTime = 0;
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
volatile bool isFetchingWeather = false;

// Flags to track the completion of the very first stock data fetch at boot.
// This allows us to log a confirmation message once the initial data is ready.
bool initialStockFetchTriggered = false;
bool initialStockFetchCompletedLogged = false;

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
SemaphoreHandle_t xAnimationStartMutex;
SemaphoreHandle_t xTimeLibMutex;
SemaphoreHandle_t xDisplayHardwareMutex;

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
        Log_printf(LOG_LEVEL_INFO, "Finished playing sound: %s", currentSoundFile);
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
    vTaskDelay(10 / portTICK_PERIOD_MS); // Run this task every 2 milliseconds
  }
}

// Forward declaration for the function that applies settings from a JSON object.
void applySettingsFromJson(const JsonObject& obj);

// This helper function now contains the memory-intensive JSON parsing.
// It returns true on success and false on failure.
bool applyAndSaveSettings() {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, settingsToSaveJson);

    // Clear the large JSON string from memory as soon as it has been parsed.
    settingsToSaveJson = "";

    if (error) {
        Log_printf(LOG_LEVEL_ERROR, "applyAndSaveSettings: deserializeJson() failed: %s", error.c_str());
        return false;
    }

    JsonObject obj = doc.as<JsonObject>();

    // Apply the new settings from the JSON object.
    applySettingsFromJson(obj);

    // Save the newly applied settings to NVS.
    saveSettings();

    // Set the volume (might have changed).
    audio.setVolume(currentSettings.notificationVolume);

    return true;
}

/**
 * @brief Handles the asynchronous saving of settings requested from the web UI.
 * @details This function is called from the main loop when the `saveSettingsRequested`
 * flag is true. It calls a helper function to perform the memory-intensive parsing
 * and saving, and once that is complete and the memory has been freed, it triggers
 * the confirmation animation.
 */
void handleBackgroundSave() {
    saveSettingsRequested = false; // Reset the flag immediately.
    Log_printf(LOG_LEVEL_INFO, "--- Background Save Started ---");

    // The 8KB DynamicJsonDocument is allocated and freed entirely inside this function call.
    if (applyAndSaveSettings()) {
        // By the time we are here, the large memory buffer is gone.
        // Now it's safe to start the memory-intensive confirmation animation.
        startStyledAnimation();
    }

    Log_printf(LOG_LEVEL_INFO, "--- Background Save Finished ---");
}

/**
 * @brief Handles the complete logic for a weather fetch timeout.
 * @details This function is called by the DisplayManager when a weather data
 * fetch fails to complete within the timeout period. It logs the event, sets a
 * displayable error message, disables weather mode to prevent retries, saves this
 * change to NVS, broadcasts the change to the UI via MQTT, and resets the
 * internal fetch state machine.
 */
void handleWeatherTimeout() {
    Log_printf(LOG_LEVEL_WARN, "Weather fetch timed out. Disabling weather mode.");
    // The xDisplayDataMutex is already taken by the calling function (handleWeatherDisplay)
    currentWeatherData.dataValid = false;
    currentWeatherData.errorReason = "FETCH TIMEOUT";

    // Disable weather mode to prevent getting stuck in a timeout loop
    currentSettings.weatherModeEnabled = false;
    saveSettings(); // Persist the change

    // Broadcast the change to the web UI via MQTT
    if (mqttClient.connected()) {
        mqttClient.publish((String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/weatherModeEnabled/state").c_str(), "false", true);
    }

    resetWeatherFetchState();
}


/**
 * @brief Parses a JSON object and applies its values to the global `currentSettings` struct.
 * @details This function contains the core logic for validating and updating the application's
 * settings based on a JSON object, typically received from the web interface. It includes
 * logic to detect changes that require service reconnections (e.g., MQTT).
 * @param obj A const reference to the JsonObject containing the new settings.
 */
void applySettingsFromJson(const JsonObject& obj) {
    Log_printf(LOG_LEVEL_DEBUG, "Applying settings from JSON object.");

    // --- Input Validation Lambdas ---
    auto validateAndSet = [&](const char* key, int& setting, int min, int max) {
        if (!obj[key].isNull()) {
            int value = obj[key].as<int>();
            if (value >= min && value <= max) {
                setting = value;
            } else {
                Log_printf(LOG_LEVEL_WARN, "Validation failed for %s: value %d is out of range (%d-%d).", key, value, min, max);
            }
        }
    };
    auto validateAndSetUChar = [&](const char* key, uint8_t& setting, uint8_t min, uint8_t max) {
        if (!obj[key].isNull()) {
            uint8_t value = obj[key].as<uint8_t>();
            if (value >= min && value <= max) {
                setting = value;
            } else {
                Log_printf(LOG_LEVEL_WARN, "Validation failed for %s: value %u is out of range (%u-%u).", key, value, min, max);
            }
        }
    };

    // --- Detect Changes for Service Reconnections ---
    bool needsMqttReconnect = false;
    std::string oldMqttBroker = currentSettings.mqttBroker;
    int oldMqttPort = currentSettings.mqttPort;
    std::string oldMqttUser = currentSettings.mqttUser;
    std::string oldMqttPass = currentSettings.mqttPassword;
    int oldNumDataPoints = currentSettings.numDataPoints;
    DataPoint oldDataPoints[5];
    for(int i=0; i<5; ++i) {
        oldDataPoints[i] = currentSettings.dataPoints[i];
    }
    std::string oldCityName = currentSettings.cityName;
    bool oldWeatherModeEnabled = currentSettings.weatherModeEnabled;

    // --- Apply All Settings from JSON ---
    validateAndSet("destinationYear", currentSettings.destinationYear, 0, 9999);
    validateAndSet("destinationTimezoneIndex", currentSettings.destinationTimezoneIndex, 0, NUM_TIMEZONE_OPTIONS - 1);
    validateAndSet("lastTimeDepartedYear", currentSettings.lastTimeDepartedYear, 0, 9999);
    validateAndSet("lastTimeDepartedMonth", currentSettings.lastTimeDepartedMonth, 1, 12);
    validateAndSet("lastTimeDepartedDay", currentSettings.lastTimeDepartedDay, 1, 31);
    validateAndSet("lastTimeDepartedHour", currentSettings.lastTimeDepartedHour, 0, 23);
    validateAndSet("lastTimeDepartedMinute", currentSettings.lastTimeDepartedMinute, 0, 59);
    validateAndSet("presetCycleInterval", currentSettings.presetCycleInterval, 0, 1440);
    validateAndSet("departureHour", currentSettings.departureHour, 0, 23);
    validateAndSet("departureMinute", currentSettings.departureMinute, 0, 59);
    validateAndSet("arrivalHour", currentSettings.arrivalHour, 0, 23);
    validateAndSet("arrivalMinute", currentSettings.arrivalMinute, 0, 59);
    validateAndSetUChar("brightness", currentSettings.brightness, 0, 15);
    if (hardwareInitialized) {
        applyBrightness();
    }
    validateAndSet("timeTravelAnimationDuration", currentSettings.timeTravelAnimationDuration, 0, 30000);
    validateAndSet("timeTravelAnimationInterval", currentSettings.timeTravelAnimationInterval, 0, 1440);
    validateAndSet("animationStyle", currentSettings.animationStyle, 0, 22);
    validateAndSetUChar("notificationVolume", currentSettings.notificationVolume, 0, 21);
    if (!obj["timeTravelSoundToggle"].isNull()) currentSettings.timeTravelSoundToggle = obj["timeTravelSoundToggle"];
    validateAndSet("presentTimezoneIndex", currentSettings.presentTimezoneIndex, 0, NUM_TIMEZONE_OPTIONS - 1);
    if (!obj["displayFormat24h"].isNull()) currentSettings.displayFormat24h = obj["displayFormat24h"];
    if (!obj["dataLinkEnabled"].isNull()) currentSettings.dataLinkEnabled = obj["dataLinkEnabled"];
    currentSettings.dataLinkRefreshInterval = obj["dataLinkRefreshInterval"] | currentSettings.dataLinkRefreshInterval;
    if (!obj["mqttBroker"].isNull()) currentSettings.mqttBroker = obj["mqttBroker"].as<std::string>();
    currentSettings.mqttPort = obj["mqttPort"] | 1883;
    if (!obj["mqttUser"].isNull()) currentSettings.mqttUser = obj["mqttUser"].as<std::string>();
    if (!obj["mqttPassword"].isNull()) currentSettings.mqttPassword = obj["mqttPassword"].as<std::string>();
    currentSettings.weatherModeEnabled = obj["weatherModeEnabled"] | currentSettings.weatherModeEnabled;
    if (!obj["cityName"].isNull()) {
        std::string newCityName = obj["cityName"].as<std::string>();
        if (newCityName != oldCityName) {
            lastCityName = "";
            if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                currentWeatherData.dataValid = false;
                xSemaphoreGive(xDisplayDataMutex);
            }
        }
        currentSettings.cityName = newCityName;
    }
    if (!obj["latitude"].isNull()) {
        currentSettings.latitude = obj["latitude"].as<float>();
    }
    if (!obj["longitude"].isNull()) {
        currentSettings.longitude = obj["longitude"].as<float>();
    }
    currentSettings.useMetricUnits = obj["useMetricUnits"] | currentSettings.useMetricUnits;
    currentSettings.stockTickerModeEnabled = obj["stockTickerModeEnabled"] | currentSettings.stockTickerModeEnabled;
    stockManager.setEnabled(currentSettings.stockTickerModeEnabled);
    if (!obj["financialModelingPrepApiKey"].isNull()) {
        currentSettings.financialModelingPrepApiKey = obj["financialModelingPrepApiKey"].as<std::string>();
        stockManager.setApiKey(currentSettings.financialModelingPrepApiKey.c_str());
    }
    if (!obj["stockAssets"].isNull()) {
        JsonArray arr = obj["stockAssets"].as<JsonArray>();
        std::vector<String> symbols;
        for (JsonVariant v : arr) {
            symbols.push_back(v.as<String>());
        }
        stockManager.updateAndSaveAssets(symbols);
    }

    int numPoints = obj["numDataPoints"] | 0;
    currentSettings.numDataPoints = (numPoints < 0) ? 0 : (numPoints > 5 ? 5 : numPoints);
    if (!obj["dataPoints"].isNull()) {
        JsonArray dataPoints = obj["dataPoints"];
        for (int i = 0; i < 5; i++) {
            if (i < currentSettings.numDataPoints && i < dataPoints.size()) {
                JsonObject dp = dataPoints[i];
                if (!dp["dataSourceType"].isNull()) currentSettings.dataPoints[i].dataSourceType = (DataSourceType)(dp["dataSourceType"].as<int>());
                if (!dp["url"].isNull()) currentSettings.dataPoints[i].url = dp["url"].as<std::string>();
                if (!dp["monthPath"].isNull()) currentSettings.dataPoints[i].monthPath = dp["monthPath"].as<std::string>();
                if (!dp["dayPath"].isNull()) currentSettings.dataPoints[i].dayPath = dp["dayPath"].as<std::string>();
                if (!dp["yearPath"].isNull()) currentSettings.dataPoints[i].yearPath = dp["yearPath"].as<std::string>();
                if (!dp["timePath"].isNull()) currentSettings.dataPoints[i].timePath = dp["timePath"].as<std::string>();
                if (!dp["prefix"].isNull()) currentSettings.dataPoints[i].prefix = dp["prefix"].as<std::string>();
                if (!dp["suffix"].isNull()) currentSettings.dataPoints[i].suffix = dp["suffix"].as<std::string>();
                if (!dp["icon"].isNull()) currentSettings.dataPoints[i].icon = dp["icon"].as<std::string>();
                currentSettings.dataPoints[i].scrollSpeed = dp["scrollSpeed"] | 150;
                if (!dp["mqttTopic"].isNull()) currentSettings.dataPoints[i].mqttTopic = dp["mqttTopic"].as<std::string>();
                if (!dp["yearPrefix"].isNull()) currentSettings.dataPoints[i].yearPrefix = dp["yearPrefix"].as<std::string>();
                if (!dp["yearSuffix"].isNull()) currentSettings.dataPoints[i].yearSuffix = dp["yearSuffix"].as<std::string>();
                if (!dp["displayMode"].isNull()) currentSettings.dataPoints[i].displayMode = (DisplayMode)(dp["displayMode"].as<int>());
                if (!dp["scrollingText"].isNull()) currentSettings.dataPoints[i].scrollingText = dp["scrollingText"].as<std::string>();
                if (!dp["authHeaderKey"].isNull()) currentSettings.dataPoints[i].authHeaderKey = dp["authHeaderKey"].as<std::string>();
                if (!dp["authHeaderValue"].isNull()) currentSettings.dataPoints[i].authHeaderValue = dp["authHeaderValue"].as<std::string>();
                if (!dp["apiExampleKey"].isNull()) currentSettings.dataPoints[i].apiExampleKey = dp["apiExampleKey"].as<std::string>();
            } else {
                currentSettings.dataPoints[i] = {}; // Clear unused data points
            }
        }
    }

    // --- Finalize MQTT Reconnect Logic ---
    if (oldMqttBroker != currentSettings.mqttBroker ||
        oldMqttPort != currentSettings.mqttPort ||
        oldMqttUser != currentSettings.mqttUser ||
        oldMqttPass != currentSettings.mqttPassword ||
        oldNumDataPoints != currentSettings.numDataPoints) {
        needsMqttReconnect = true;
    } else {
        for(int i=0; i<5; ++i) {
            if (oldDataPoints[i].dataSourceType != currentSettings.dataPoints[i].dataSourceType ||
                oldDataPoints[i].mqttTopic != currentSettings.dataPoints[i].mqttTopic) {
                needsMqttReconnect = true;
                break;
            }
        }
    }
    if (needsMqttReconnect) {
        Log_printf(LOG_LEVEL_INFO, "MQTT settings changed. Forcing reconnect.");
        if (mqttClient.connected()) {
            mqttClient.disconnect();
        }
        mqttReconnectRequired = true;
    }

    // If weather mode was just turned off, reset the fetch state
    if (oldWeatherModeEnabled && !currentSettings.weatherModeEnabled) {
        resetWeatherFetchState();
    }
}


/**
 * @brief Saves the current settings to non-volatile storage (NVS).
 * @details This function uses the Preferences library to persist the `currentSettings`
 * object. To minimize unnecessary writes to the flash memory, it checks each setting
 * against its previously saved value and only writes if the value has actually changed.
 */
void saveSettings() {
    Log_printf(LOG_LEVEL_INFO, "--- Saving Settings (Dirty Flag Check) ---");
    preferences.begin(PREFERENCES_NAMESPACE, false); // Open preferences in read-write mode.

    // Helper macro to reduce boilerplate code for saving numeric types.
    // It gets the existing value, compares it to the new value, and puts the new value if different.
    #define SAVE_IF_CHANGED(key, type, value) \
        if (preferences.get##type(key, -1) != value) { \
            preferences.put##type(key, value); \
            Log_printf(LOG_LEVEL_DEBUG, "SAVING: %s -> %d", #key, value); \
        }

    // Helper macro for saving string types. It handles the case where the key might not exist yet.
    #define SAVE_STRING_IF_CHANGED(key, value) \
        if (!preferences.isKey(key) || preferences.getString(key, "") != value.c_str()) { \
            preferences.putString(key, value.c_str()); \
            Log_printf(LOG_LEVEL_DEBUG, "SAVING: %s -> %s", #key, value.c_str()); \
        }

    // --- Save each setting using the helper macros ---
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
        Log_printf(LOG_LEVEL_DEBUG, "SAVING: soundToggle -> %s", currentSettings.timeTravelSoundToggle ? "true" : "false");
    }

    SAVE_IF_CHANGED("presTzIndex", Int, currentSettings.presentTimezoneIndex);
    SAVE_IF_CHANGED("presetCycle", Int, currentSettings.presetCycleInterval);
    if (preferences.getBool("format24h", false) != currentSettings.displayFormat24h) {
        preferences.putBool("format24h", currentSettings.displayFormat24h);
        Log_printf(LOG_LEVEL_DEBUG, "SAVING: format24h -> %s", currentSettings.displayFormat24h ? "true" : "false");
    }
    
    SAVE_IF_CHANGED("theme", Int, currentSettings.theme);
    SAVE_IF_CHANGED("animInterval", Int, currentSettings.timeTravelAnimationInterval);
    SAVE_IF_CHANGED("animDuration", Int, currentSettings.timeTravelAnimationDuration);
    SAVE_IF_CHANGED("animStyle", Int, currentSettings.animationStyle);
    if (preferences.getBool("dlEnabled", false) != currentSettings.dataLinkEnabled) {
        preferences.putBool("dlEnabled", currentSettings.dataLinkEnabled);
        Log_printf(LOG_LEVEL_DEBUG, "SAVING: dlEnabled -> %s", currentSettings.dataLinkEnabled ? "true" : "false");
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
        Log_printf(LOG_LEVEL_DEBUG, "SAVING: weatherMode -> %s", currentSettings.weatherModeEnabled ? "true" : "false");
    }

    SAVE_STRING_IF_CHANGED("cityName", currentSettings.cityName);
    if (preferences.getBool("useMetric", false) != currentSettings.useMetricUnits) {
        preferences.putBool("useMetric", currentSettings.useMetricUnits);
        Log_printf(LOG_LEVEL_DEBUG, "SAVING: useMetric -> %s", currentSettings.useMetricUnits ? "true" : "false");
    }
    
    // Note: Comparing floats can be tricky due to precision.
    // A small tolerance might be needed for production code, but this is fine for this use case.
    if (preferences.getFloat("latitude", 0.0) != currentSettings.latitude) {
        preferences.putFloat("latitude", currentSettings.latitude);
        Log_printf(LOG_LEVEL_DEBUG, "SAVING: latitude -> %f", currentSettings.latitude);
    }
    if (preferences.getFloat("longitude", 0.0) != currentSettings.longitude) {
        preferences.putFloat("longitude", currentSettings.longitude);
        Log_printf(LOG_LEVEL_DEBUG, "SAVING: longitude -> %f", currentSettings.longitude);
    }
    
    if (preferences.getBool("stModeEnabled", false) != currentSettings.stockTickerModeEnabled) {
        preferences.putBool("stModeEnabled", currentSettings.stockTickerModeEnabled);
        Log_printf(LOG_LEVEL_DEBUG, "SAVING: stModeEnabled -> %s", currentSettings.stockTickerModeEnabled ? "true" : "false");
    }

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
    Log_printf(LOG_LEVEL_INFO, "--- Settings Saved ---");

    // After saving, immediately apply the timezone setting to the system.
    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
	tzset();
}

/**
 * @brief Loads settings from non-volatile storage (NVS) into the `currentSettings` object.
 * @details This function reads all configuration data from flash memory using the
 * Preferences library. If it detects that this is the first boot (i.e., no settings
 * have been saved yet), it initializes the `currentSettings` object with a set of
 * sensible defaults and then calls `saveSettings()` to create the initial configuration.
 */
void loadSettings() {
    Log_printf(LOG_LEVEL_INFO, "--- Loading Settings ---");
    preferences.begin(PREFERENCES_NAMESPACE, true); // Open preferences in read-only mode.

    // Check if a key exists. If not, we assume it's the first boot.
	bool needsInit = !preferences.isKey("destYear");
    if (needsInit) {
        // --- INITIALIZE WITH DEFAULT VALUES ---
		Log_printf(LOG_LEVEL_INFO, "No settings found. Initializing with defaults.");
		currentSettings.destinationYear = 1955;
		currentSettings.destinationTimezoneIndex = 4; // Default to Pacific Time
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
		currentSettings.financialModelingPrepApiKey = "";
        stockManager.clearAssets();
		for (int i = 0; i < 5; i++) {
			currentSettings.dataPoints[i] = {};
		}
		saveSettings();
	} else {
		Log_printf(LOG_LEVEL_INFO, "Loading settings from NVS.");
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
		currentSettings.dataLinkEnabled = preferences.getBool("dlEnabled");
		currentSettings.dataLinkTargetRow = preferences.getInt("dlTargetRow");
		currentSettings.dataLinkRefreshInterval = preferences.getInt("dlRefresh");
		currentSettings.numDataPoints = preferences.getInt("numDataPoints");
        String brokerStr = preferences.getString("mqttBroker", "");
        currentSettings.mqttBroker = brokerStr.c_str();
		Log_printf(LOG_LEVEL_DEBUG, "Loaded MQTT Broker: [%s]", currentSettings.mqttBroker.c_str());
		currentSettings.mqttPort = preferences.getInt("mqttPort", 1883);
		Log_printf(LOG_LEVEL_DEBUG, "Loaded MQTT Port: [%d]", currentSettings.mqttPort);
        String userStr = preferences.getString("mqttUser", "");
        currentSettings.mqttUser = userStr.c_str();
		Log_printf(LOG_LEVEL_DEBUG, "Loaded MQTT User: [%s]", currentSettings.mqttUser.c_str());
        String passStr = preferences.getString("mqttPass", "");
        currentSettings.mqttPassword = passStr.c_str();
		currentSettings.weatherModeEnabled = preferences.getBool("weatherMode", false);
		String tempString = preferences.getString("cityName", "New York");
		currentSettings.cityName = tempString.c_str();
		currentSettings.useMetricUnits = preferences.getBool("useMetric", false);
		currentSettings.latitude = preferences.getFloat("latitude", 40.7128);
		currentSettings.longitude = preferences.getFloat("longitude", -74.0060);
		currentSettings.stockTickerModeEnabled = preferences.getBool("stModeEnabled", false);
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

    // Initialize the StockManager with the loaded settings
    stockManager.setApiKey(currentSettings.financialModelingPrepApiKey.c_str());
    stockManager.setEnabled(currentSettings.stockTickerModeEnabled);
    stockManager.loadAssets();

	Log_printf(LOG_LEVEL_INFO, "--- Settings Loaded ---");
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
	Log_printf(LOG_LEVEL_INFO, "--- Listing all files in LittleFS ---");
	File root = LittleFS.open("/");
	File file = root.openNextFile();
	while (file) {
		Log_printf(LOG_LEVEL_INFO, "  FILE: %s\tSIZE: %d", file.name(), file.size());
		file.close();
		file = root.openNextFile();
	}
	Log_printf(LOG_LEVEL_INFO, "--- End of file list ---");
	root.close();
}

bool attemptHardwareInit() {
    #if ENABLE_HARDWARE
    Log_printf(LOG_LEVEL_INFO, "Attempting to initialize hardware...");
    if (setupPhysicalDisplay()) {
        Log_printf(LOG_LEVEL_INFO, "Physical display setup... OK");
        return true; // Success
    } else {
        Log_printf(LOG_LEVEL_ERROR, "Physical display setup... FAILED");
        return false; // Failure
    }
    #else
    Log_printf(LOG_LEVEL_WARN, "Hardware is disabled (ENABLE_HARDWARE = 0)");
    return true; // Keep original logic: if disabled, it's "not failed"
    #endif
}

void onHardwareInitialized() {
    Log_printf(LOG_LEVEL_INFO, "Hardware successfully initialized. Running post-init tasks.");
    applyBrightness();
    Log_printf(LOG_LEVEL_INFO, "Initializing I2S Audio...");
    audio.setPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DIN_PIN);
    audio.setVolume(currentSettings.notificationVolume);
    Audio::audio_info_callback = audio_info;
    Log_printf(LOG_LEVEL_INFO, "I2S Audio... OK");

    xTaskCreatePinnedToCore(
        audioTask,          // Task function
        "AudioTask",        // Name of the task
        4096,               // Increased stack size
        NULL,               // Task input parameter
        5,                  // Priority of the task
        NULL,               // Task handle
        0                   // Core where the task should run
    );
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

    xSerialMutex = xSemaphoreCreateMutex(); // For thread-safe logging
    Log_printf(LOG_LEVEL_INFO, "--- BOOTING ---");
    Log_printf(LOG_LEVEL_INFO, "Initializing Serial... OK");

    if (!LittleFS.begin(true, "/spiffs")) {
        Log_printf(LOG_LEVEL_ERROR, "CRITICAL ERROR: LittleFS Mount Failed. Restarting in 10 seconds.");
        delay(10000);
        ESP.restart();
    }
    Log_printf(LOG_LEVEL_INFO, "LittleFS mount... OK");

    Log_printf(LOG_LEVEL_INFO, "Loading settings...");
    loadSettings();
    Log_printf(LOG_LEVEL_INFO, "Settings loaded... OK");

    xDisplayDataMutex = xSemaphoreCreateMutex();
    xAnimationStartMutex = xSemaphoreCreateMutex();
    xTimeLibMutex = xSemaphoreCreateMutex();
    xDisplayHardwareMutex = xSemaphoreCreateMutex();
    Log_printf(LOG_LEVEL_INFO, "Mutexes created... OK");
    
    WiFi.mode(WIFI_STA);
    uint8_t mac[6];
    WiFi.macAddress(mac);
    sprintf(MQTT_UNIQUE_ID, "BTTF_TC_%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Manually configure DNS servers to fix potential network issues
    IPAddress primaryDNS(8, 8, 8, 8);
    IPAddress secondaryDNS(8, 8, 4, 4);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, primaryDNS, secondaryDNS);

    WiFi.begin();
    wifiConnectStartTime = millis();
    wifiState = WIFI_STATE_CONNECTING;
    Log_printf(LOG_LEVEL_INFO, "Non-blocking WiFi connection initiated...");
    Log_printf(LOG_LEVEL_INFO, "Setting up web routes...");
    setupWebRoutes();
    Log_printf(LOG_LEVEL_INFO, "Web routes configured.");

    hardwareInitialized = attemptHardwareInit();
    if (hardwareInitialized) {
        onHardwareInitialized();
    }

    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
    tzset();
    Log_printf(LOG_LEVEL_INFO, "Timezone configured.");

    setupMqtt();
    Log_printf(LOG_LEVEL_INFO, "MQTT setup initiated.");

    stockManager.begin();
    Log_printf(LOG_LEVEL_INFO, "StockManager setup initiated.");

    Log_printf(LOG_LEVEL_INFO, "Free heap after setup: %u bytes", ESP.getFreeHeap());

    ArduinoOTA.setHostname("bttf-time-circuits");
    ArduinoOTA.setPassword("1.21gigawatts");
    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else {
            type = "filesystem";
        }
        Log_printf(LOG_LEVEL_INFO, "OTA Update Start: %s", type.c_str());
    });
    ArduinoOTA.onEnd([]() {
        Log_printf(LOG_LEVEL_INFO, "OTA Update End");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Log_printf(LOG_LEVEL_DEBUG, "OTA Progress: %u%%", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        const char* error_str = "Unknown";
        if (error == OTA_AUTH_ERROR) error_str = "Auth Failed";
        else if (error == OTA_BEGIN_ERROR) error_str = "Begin Failed";
        else if (error == OTA_CONNECT_ERROR) error_str = "Connect Failed";
        else if (error == OTA_RECEIVE_ERROR) error_str = "Receive Failed";
        else if (error == OTA_END_ERROR) error_str = "End Failed";
        Log_printf(LOG_LEVEL_ERROR, "OTA Error[%u]: %s", error, error_str);
    });
    ArduinoOTA.begin();

    // Clear any persistent manual display overrides from the previous session
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j) {
            updateDisplaySegment(i, j, "");
        }
    }

    Log_printf(LOG_LEVEL_INFO, "--- BOOT COMPLETE ---");
    bootTimestamp = millis();
}

// --- NEW STATE DETERMINATION FUNCTION ---
void updateDisplayState() {
    static DisplayState previousDisplayState = STATE_NORMAL_CLOCK;
    DisplayState newDisplayState;

    if (isMessageOverrideActive) {
        newDisplayState = STATE_MESSAGE_OVERRIDE;
    } else if (isAnimating) {
        newDisplayState = STATE_ANIMATING;
    } else if (isMarqueeOverrideActive) {
        newDisplayState = STATE_MARQUEE_OVERRIDE;
    } else if (currentSettings.stockTickerModeEnabled) {
        newDisplayState = STATE_STOCK_TICKER;
    } else if (currentSettings.dataLinkEnabled) {
        newDisplayState = STATE_DATA_LINK;
    } else if (currentSettings.weatherModeEnabled) {
        newDisplayState = STATE_WEATHER;
    } else {
        newDisplayState = STATE_NORMAL_CLOCK;
    }

    if (newDisplayState != previousDisplayState) {
        Log_printf(LOG_LEVEL_INFO, "Display state changed from %d to %d", previousDisplayState, newDisplayState);
        if (newDisplayState == STATE_STOCK_TICKER) {
            // Reset the stock ticker state machine when entering the mode
            if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                stockState = SD_CONNECTING;
                xSemaphoreGive(xDisplayDataMutex);
            }
        }
        previousDisplayState = newDisplayState;
    }

    currentDisplayState = newDisplayState;
}

// --- NEW DISPLAY HANDLER FUNCTION ---
void handleDisplay() {
    // These effects can run concurrently with the main display modes
    handleTemporalEcho();
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
            updateStockTickerDisplay();
            break;
        case STATE_DATA_LINK:
            fetchDataLink();
            updateMarqueeDisplay();
            break;
        case STATE_WEATHER:
            // Weather mode is now handled by the handleWeatherDisplay function
            handleWeatherDisplay();
            break;
        case STATE_NORMAL_CLOCK:
        default:
            // In the normal clock mode, update all three rows as usual.
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
void handleBackgroundSave(); // Forward declaration

void loop() {
    vTaskDelay(1); // Yield to other tasks, making the system responsive.
    
    // Check if a settings save has been requested by the web server
    if (saveSettingsRequested) {
        handleBackgroundSave();
    }

    // This state machine manages the WiFi connection process in a non-blocking way.
    // This state machine manages the WiFi connection process in a non-blocking way.
    // It handles the initial connection attempt, starting the WiFiManager portal if
    // the connection fails, and managing the device reboot after successful portal configuration.
    switch (wifiState) {
        case WIFI_STATE_CONNECTING:
            if (!logConnectingPrinted) {
                Log_printf(LOG_LEVEL_INFO, "WiFi State: CONNECTING");
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
                Log_printf(LOG_LEVEL_INFO, "WiFi State: START_PORTAL. Starting WiFiManager.");
                logPortalMsgPrinted = true;
            }
            xTaskCreate(wifiManagerTask, "WiFiManager", 4096, &wifiManager, 1, &wifiManagerTaskHandle);
            wifiState = WIFI_STATE_PORTAL_RUNNING;
            break;
        case WIFI_STATE_PORTAL_RUNNING:
             if (WiFi.status() == WL_CONNECTED) {
                Log_printf(LOG_LEVEL_INFO, "WiFi connected via portal. Rebooting...");
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
                Log_printf(LOG_LEVEL_INFO, "WiFi State: CONNECTED. IP: %s", WiFi.localIP().toString().c_str());
                // Start the web server now that we are connected
                server.begin();
                Log_printf(LOG_LEVEL_INFO, "HTTP server started on successful connection.");

                logConnectedPrinted = true;
                if (MDNS.begin("timecircuits")) {
                    MDNS.addService("http", "tcp", 80);
                }

                // Now that WiFi is connected, trigger the initial stock fetch if enabled.
                if (currentSettings.stockTickerModeEnabled) {
                    Log_printf(LOG_LEVEL_INFO, "Stock ticker mode is enabled, triggering initial data fetch.");
                    stockManager.fetchData();
                    initialStockFetchTriggered = true; // Mark that the first fetch has started.
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
            
            stockManager.loop();

            // --- START: MODIFICATION - Log initial stock fetch completion ---
            if (initialStockFetchTriggered && !initialStockFetchCompletedLogged && !stockManager.isFetching()) {
                initialStockFetchCompletedLogged = true; // Set flag immediately to prevent re-entry
                Log_printf(LOG_LEVEL_INFO, "Initial asynchronous stock data fetch has completed. Checking results...");

                const auto& assets = stockManager.getAssets();
                if (assets.empty()) {
                    Log_printf(LOG_LEVEL_INFO, "  -> No stock assets are configured.");
                } else {
                    int success_count = 0;
                    for (const auto& asset : assets) {
                        if (asset.data_valid) {
                            Log_printf(LOG_LEVEL_INFO, "  - %s: OK", asset.symbol.c_str());
                            success_count++;
                        } else {
                            Log_printf(LOG_LEVEL_WARN, "  - %s: FAILED (%s)", asset.symbol.c_str(), asset.error_reason.c_str());
                        }
                    }
                    Log_printf(LOG_LEVEL_INFO, "  -> Fetch result: %d of %d assets updated successfully.", success_count, assets.size());
                }
            }
            // --- END: MODIFICATION ---

            // --- START: MODIFICATION - Periodic Stock Manager Reset ---
            static unsigned long lastStockManagerReset = 0;
            if (currentSettings.stockTickerModeEnabled) {
                if (lastStockManagerReset == 0) {
                    lastStockManagerReset = millis();
                }
                unsigned long now = millis();
                if (now - lastStockManagerReset > STOCK_MANAGER_RESET_INTERVAL) {
                    Log_printf(LOG_LEVEL_INFO, "Performing periodic reset of StockManager to prevent heap fragmentation.");
                    // Re-initialize the stock manager from the master settings object.
                    stockManager.setApiKey(currentSettings.financialModelingPrepApiKey.c_str());
                    stockManager.loadAssets();
                    stockManager.setEnabled(true); // Re-enable the manager after reset
                    lastStockManagerReset = now;
                }
            } else {
                lastStockManagerReset = 0; // Reset the timer if stock ticker mode is disabled
            }
            // --- END: MODIFICATION ---

            handleScheduledAnimation();
            static unsigned long lastNtpUpdate = 0;
            if (ntpSyncRequested || (!timeSynchronized && millis() > NTP_INITIAL_SYNC_DELAY) || (timeSynchronized && millis() - lastNtpUpdate > 3600000)) {
                if (xSemaphoreTake(xTimeLibMutex, portMAX_DELAY) == pdTRUE) {
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
                    xSemaphoreGive(xTimeLibMutex);
                }
            }
            static unsigned long lastHaStateUpdate = 0;
            if (timeSynchronized && millis() - lastHaStateUpdate > 5000) {
                publishAllHaStates();
                lastHaStateUpdate = millis();
            }
            
            if (hardwareInitialized) {
                if (bootState != BOOT_INACTIVE) {
                    if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                        handleBootSequence();
                        xSemaphoreGive(xDisplayDataMutex);
                    }
                } else {
                    handleFlashEffect();

                    if (isAnimating) {
                        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                            handleDisplayAnimation();
                            xSemaphoreGive(xDisplayDataMutex);
                        }
                    } else if (isStyledAnimating) {
                        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                            handleStyledAnimation();
                            xSemaphoreGive(xDisplayDataMutex);
                        }
                    } else {
                        if (millis() - lastDisplayUpdateTime > DISPLAY_UPDATE_INTERVAL) {
                            lastDisplayUpdateTime = millis();
                            updateDisplayState();
                            handleDisplay();
                        }
                    }
                }
            } else {
                // Hardware failed to initialize, retry periodically
                unsigned long now = millis();
                if (now - lastHardwareInitAttempt > HARDWARE_INIT_RETRY_INTERVAL) {
                    Log_printf(LOG_LEVEL_WARN, "Retrying hardware initialization...");
                    lastHardwareInitAttempt = now;
                    hardwareInitialized = attemptHardwareInit();
                    if (hardwareInitialized) {
                        Log_printf(LOG_LEVEL_INFO, "Hardware initialized successfully on retry.");
                        onHardwareInitialized();
                    } else {
                        Log_printf(LOG_LEVEL_WARN, "Hardware initialization retry failed.");
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

#include <cstdio> // For sscanf

/**
 * @brief Constructs a complete list of presets, combining movie presets and custom ones.
 * @details This function first copies the hardcoded `moviePresets` vector. It then reads
 * the `customPresets` JSON string from NVS, parses it, and appends each valid
 * custom preset to the list.
 * @return A std::vector<Preset> containing all available presets.
 */
std::vector<Preset> getFullPresetList() {
    std::vector<Preset> allPresets = moviePresets; // Start with the movie presets

    preferences.begin(PREFERENCES_NAMESPACE, true); // Read-only
    String presetsJson = preferences.getString("customPresets", "[]");
    preferences.end();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, presetsJson);

    if (!error) {
        JsonArray customPresets = doc.as<JsonArray>();
        for (JsonObject presetObj : customPresets) {
            std::string value = presetObj["value"].as<std::string>();
            int year, month, day, hour, minute;
            // Use sscanf to safely parse the date-time string
            if (sscanf(value.c_str(), "%d-%d-%d-%d-%d", &year, &month, &day, &hour, &minute) == 5) {
                allPresets.push_back({presetObj["name"].as<std::string>(), year, month, day, hour, minute});
            }
        }
    } else {
        Log_printf(LOG_LEVEL_ERROR, "Failed to parse custom presets JSON: %s", error.c_str());
    }
    return allPresets;
}

/**
 * @brief Handles the automatic cycling of the "Last Time Departed" display through presets.
 * @details This function is called from the main loop. If preset cycling is enabled, it checks
 * if the configured interval has passed. If so, it fetches the full list of presets,
 * finds the current "Last Time Departed" in that list, and advances to the next preset,
 * wrapping around to the beginning if necessary. The `currentSettings` are then updated
 * with the new preset's date and time, which will be reflected on the display in the next
 * update cycle.
 */
void handlePresetCycling() {
    if (lastPresetCycleTime == 0 && bootState == BOOT_INACTIVE) {
        lastPresetCycleTime = millis();
    }
    // Return if cycling is disabled, an animation is playing, or the display is asleep
    if (currentSettings.presetCycleInterval == 0 || isAnimating || isDisplayAsleep || isStyledAnimating) {
        return;
    }

    // Check if the interval (in minutes) has elapsed
    if (millis() - lastPresetCycleTime > (unsigned long)currentSettings.presetCycleInterval * 60000) {
        lastPresetCycleTime = millis(); // Reset the timer

        Log_printf(LOG_LEVEL_INFO, "Preset cycle triggered.");

        // Get the combined list of movie and custom presets
        std::vector<Preset> allPresets = getFullPresetList();
        if (allPresets.empty()) {
            Log_printf(LOG_LEVEL_WARN, "No presets available to cycle.");
            return; // No presets to cycle
        }

        // Find the index of the current "Last Time Departed" in the list
        int currentIndex = -1;
        for (size_t i = 0; i < allPresets.size(); ++i) {
            if (allPresets[i] == currentSettings) {
                currentIndex = i;
                break;
            }
        }

        // Determine the index of the next preset, wrapping around if needed
        // If the current preset isn't found, start from the first one.
        int nextIndex = (currentIndex == -1) ? 0 : (currentIndex + 1) % allPresets.size();

        const Preset& nextPreset = allPresets[nextIndex];
        Log_printf(LOG_LEVEL_INFO, "Cycling to next preset: %s", nextPreset.name.c_str());

        // Update the global settings with the new "Last Time Departed"
        currentSettings.lastTimeDepartedYear = nextPreset.year;
        currentSettings.lastTimeDepartedMonth = nextPreset.month;
        currentSettings.lastTimeDepartedDay = nextPreset.day;
        currentSettings.lastTimeDepartedHour = nextPreset.hour;
        currentSettings.lastTimeDepartedMinute = nextPreset.minute;

        // The display will be updated automatically on the next loop iteration.
        // No need to call saveSettings() here, as this isn't a persistent change.
        broadcastPresetUpdate(nextPreset.name, nextPreset.year, nextPreset.month, nextPreset.day, nextPreset.hour, nextPreset.minute);
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
        // --- FIX: Turn off all AM/PM LEDs ---
        digitalWrite(DEST_AM_PIN, LOW);
        digitalWrite(DEST_PM_PIN, LOW);
        digitalWrite(PRES_AM_PIN, LOW);
        digitalWrite(PRES_PM_PIN, LOW);
        digitalWrite(LAST_AM_PIN, LOW);
        digitalWrite(LAST_PM_PIN, LOW);
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
