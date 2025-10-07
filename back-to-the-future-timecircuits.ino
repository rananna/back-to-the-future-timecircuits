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
#include "MqttManager.h"
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_mac.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <string>
#include <cctype>
#include <LCBUrl.h>
#include <ArduinoOTA.h>
#include "esp_task_wdt.h"

#include "MqttManager.h"
#include "HardwareControl.h"
#include "web_server.h"
#include "api_templates.h"
#include "EventManager.h"
#include "AnimationManager.h"
#include "DisplayManager.h"
#include "DataManager.h"
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

#define WDT_TIMEOUT 10 // Watchdog timeout in seconds

// --- SYSTEM CONSTANTS ---
const unsigned long WIFI_CONNECT_TIMEOUT = 15000;       // Time in ms to wait for WiFi before starting the captive portal.
const unsigned int MQTT_INITIAL_RETRY_INTERVAL = 5000;  // Initial time in ms to wait before first MQTT reconnect attempt.
const unsigned int MQTT_MAX_RETRY_INTERVAL = 60000;     // Maximum time in ms for the exponential backoff for MQTT reconnects.
const unsigned int MQTT_MAX_FAILS = 5;                  // Number of consecutive MQTT fails before tripping the circuit breaker.
const unsigned long MQTT_HOLDOFF_DURATION = 300000;     // Time in ms (5 minutes) to wait before retrying after a circuit break.
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
bool initialMqttConnectionAttempted = false; // Tracks if the first connection attempt has been made.
unsigned int mqttConsecutiveFails = 0;       // Counter for consecutive MQTT connection failures.
unsigned long mqttHoldoffUntil = 0;           // Timestamp (millis) until which MQTT reconnections are paused.
bool mDnsIsActive = false;                   // Tracks whether the mDNS service is currently running.

// --- STATE VARIABLES ---
BootSequenceState bootState = BOOT_INACTIVE; // Current phase of the cinematic boot sequence.
DisplayModeState currentDisplayMode = NORMAL_CLOCK; // Current primary mode of the display (e.g., clock, weather).

// --- AUDIO GLOBALS ---
Audio audio; // The global audio object from the ESP32-audioI2S library.
bool isPlayingSound = false;
bool isSoundFromMqtt = false;
StockManager stockManager;
char currentSoundFile[MAX_FILENAME_LENGTH] = ""; // Filename of the audio file currently being played.

// --- DEVICE IDENTIFIERS ---
char MQTT_UNIQUE_ID[21]; // The unique identifier for this device, derived from its MAC address.

// --- FUNCTION PROTOTYPES ---
// Forward declarations for functions defined later in this file.
void handlePresetCycling();
void handleSleepSchedule();
bool attemptHardwareInit();
void onHardwareInitialized();
void checkDataFetchStatusTask(void* p);
void startAudioStream(const char* url, bool is_tts);
void stopAudioStream();
void wifiManagerTask(void *pvParameters);
void updateDisplaySegment(int row, int segment, const std::string& text);

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
// Settings are now applied and saved directly in the web server request handler
// to avoid using global variables and intermediate string buffers, which can
// cause memory issues on the ESP32.

volatile bool webServerRestartRequired = false;
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

int dataPointFetchFailures[5] = {0, 0, 0, 0, 0};
const int MAX_FETCH_FAILURES = 3;
volatile int requestsCompleted = 0;
int currentPageIndex = 0;
bool isMessageOverrideActive = false;
String overrideMessageLine1 = "";
String overrideMessageLine2 = "";
String overrideMessageLine3 = "";
SemaphoreHandle_t xDisplayDataMutex;
SemaphoreHandle_t xAnimationStartMutex;
SemaphoreHandle_t xTimeLibMutex;
SemaphoreHandle_t xDisplayHardwareMutex;

SequencerTrack sequencerTracks[3];

// A more detailed state machine for the main display logic. This helps to cleanly
// separate the logic for each display mode and ensures only one mode is active at a time,
// preventing conflicts between different features trying to control the display.
enum DisplayState {
    STATE_NORMAL_CLOCK,       // Default state, showing the time.
    STATE_MESSAGE_OVERRIDE,
    STATE_ANIMATING,
    STATE_STOCK_TICKER,
    STATE_DATA_LINK,
    STATE_WEATHER
};
DisplayState currentDisplayState = STATE_NORMAL_CLOCK;



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

/**
 * @brief Converts a string name to its corresponding AnimationType enum.
 * @param name The string name of the animation (e.g., "Intruder Alert").
 * @return The matching AnimationType enum, or ANIMATION_RANDOMIZE_ALL if not found.
 */
AnimationType animationTypeFromString(const std::string& name) {
    if (name == "Intruder Alert") return ANIMATION_INTRUDER_ALERT;
    if (name == "Time Travel") return ANIMATION_TIME_TRAVEL;
    if (name == "Party Mode") return ANIMATION_PARTY_MODE;
    if (name == "Countdown") return ANIMATION_COUNTDOWN;
    if (name == "Knight Rider") return ANIMATION_KNIGHT_RIDER;
    if (name == "Lightning") return ANIMATION_LIGHTNING;
    if (name == "Loading") return ANIMATION_LOADING;
    if (name == "Error") return ANIMATION_ERROR;
    if (name == "Flux Capacitor Charge-Up") return ANIMATION_FLUX_CHARGE;
    if (name == "Tachyons Detected") return ANIMATION_TACHYONS;
    if (name == "Data Stream") return ANIMATION_DATA_STREAM;
    if (name == "Wormhole Collapse") return ANIMATION_WORMHOLE_COLLAPSE;
    if (name == "All Displays Random") return ANIMATION_ALL_DISPLAYS_RANDOM;
    if (name == "Time Travel Tunnel") return ANIMATION_TIME_TRAVEL_TUNNEL;
    if (name == "Fire Trails") return ANIMATION_FIRE_TRAILS;
    if (name == "Sparkle Reveal") return ANIMATION_SPARKLE_REVEAL;
    if (name == "Sequential Flicker") return ANIMATION_SEQUENTIAL_FLICKER;
    if (name == "Random Flicker") return ANIMATION_RANDOM_FLICKER;
    if (name == "Counting Up") return ANIMATION_COUNTING_UP;
    if (name == "Wave Flicker") return ANIMATION_WAVE_FLICKER;
    if (name == "Tornado Flicker") return ANIMATION_TORNADO_FLICKER;
    if (name == "Capacitor Charge-Up") return ANIMATION_CAPACITOR_CHARGE_UP;
    if (name == "Digital Rain") return ANIMATION_DIGITAL_RAIN;
    if (name == "Waveform Collapse") return ANIMATION_WAVEFORM_COLLAPSE;
    if (name == "Timeline Skim") return ANIMATION_TIMELINE_SKIM;
    if (name == "Temporal Desync") return ANIMATION_TEMPORAL_DESYNC;
    if (name == "Glitchy Jump-Cut") return ANIMATION_GLITCHY_JUMP_CUT;
    if (name == "Plasma Warm-Up") return ANIMATION_PLASMA_WARM_UP;
    if (name == "Time Warp Streaks") return ANIMATION_TIME_WARP_STREAKS;
    if (name == "Character Scanline") return ANIMATION_CHARACTER_SCANLINE;
    if (name == "Focus In") return ANIMATION_FOCUS_IN;
    if (name == "Code Breaker") return ANIMATION_CODE_BREAKER;
    if (name == "Temporal Paradox") return ANIMATION_TEMPORAL_PARADOX;
    if (name == "Digit Cascade") return ANIMATION_DIGIT_CASCADE;
    if (name == "Electric Surge") return ANIMATION_ELECTRIC_SURGE;
    if (name == "Flip-Disc Display") return ANIMATION_FLIP_DISC_DISPLAY;
    if (name == "Interference Pattern") return ANIMATION_INTERFERENCE_PATTERN;
    if (name == "Randomize All") return ANIMATION_RANDOMIZE_ALL;

    // Default fallback if the name is not recognized
    return ANIMATION_RANDOMIZE_ALL;
}

/**
 * @brief Converts an AnimationType enum to its string representation.
 * @param type The AnimationType enum value.
 * @return The string name of the animation, or "Randomize All" if not found.
 */
const char* animationTypeToString(AnimationType type) {
    switch (type) {
        case ANIMATION_INTRUDER_ALERT: return "Intruder Alert";
        case ANIMATION_TIME_TRAVEL: return "Time Travel";
        case ANIMATION_PARTY_MODE: return "Party Mode";
        case ANIMATION_COUNTDOWN: return "Countdown";
        case ANIMATION_KNIGHT_RIDER: return "Knight Rider";
        case ANIMATION_LIGHTNING: return "Lightning";
        case ANIMATION_LOADING: return "Loading";
        case ANIMATION_ERROR: return "Error";
        case ANIMATION_FLUX_CHARGE: return "Flux Capacitor Charge-Up";
        case ANIMATION_TACHYONS: return "Tachyons Detected";
        case ANIMATION_DATA_STREAM: return "Data Stream";
        case ANIMATION_WORMHOLE_COLLAPSE: return "Wormhole Collapse";
        case ANIMATION_ALL_DISPLAYS_RANDOM: return "All Displays Random";
        case ANIMATION_TIME_TRAVEL_TUNNEL: return "Time Travel Tunnel";
        case ANIMATION_FIRE_TRAILS: return "Fire Trails";
        case ANIMATION_SPARKLE_REVEAL: return "Sparkle Reveal";
        case ANIMATION_SEQUENTIAL_FLICKER: return "Sequential Flicker";
        case ANIMATION_RANDOM_FLICKER: return "Random Flicker";
        case ANIMATION_COUNTING_UP: return "Counting Up";
        case ANIMATION_WAVE_FLICKER: return "Wave Flicker";
        case ANIMATION_TORNADO_FLICKER: return "Tornado Flicker";
        case ANIMATION_CAPACITOR_CHARGE_UP: return "Capacitor Charge-Up";
        case ANIMATION_DIGITAL_RAIN: return "Digital Rain";
        case ANIMATION_WAVEFORM_COLLAPSE: return "Waveform Collapse";
        case ANIMATION_TIMELINE_SKIM: return "Timeline Skim";
        case ANIMATION_TEMPORAL_DESYNC: return "Temporal Desync";
        case ANIMATION_GLITCHY_JUMP_CUT: return "Glitchy Jump-Cut";
        case ANIMATION_PLASMA_WARM_UP: return "Plasma Warm-Up";
        case ANIMATION_TIME_WARP_STREAKS: return "Time Warp Streaks";
        case ANIMATION_CHARACTER_SCANLINE: return "Character Scanline";
        case ANIMATION_FOCUS_IN: return "Focus In";
        case ANIMATION_CODE_BREAKER: return "Code Breaker";
        case ANIMATION_TEMPORAL_PARADOX: return "Temporal Paradox";
        case ANIMATION_DIGIT_CASCADE: return "Digit Cascade";
        case ANIMATION_ELECTRIC_SURGE: return "Electric Surge";
        case ANIMATION_FLIP_DISC_DISPLAY: return "Flip-Disc Display";
        case ANIMATION_INTERFERENCE_PATTERN: return "Interference Pattern";
        case ANIMATION_RANDOMIZE_ALL: return "Randomize All";
        default: return "Randomize All";
    }
}

// Forward declaration for the function that applies settings from a JSON object.
void applySettingsFromJson(const JsonObject& obj);

// This function applies settings from a JSON object and saves them.
// It is called directly from the web server handler.
void applyAndSaveSettings(JsonVariant& json) {
    JsonObject obj = json.as<JsonObject>();

    // Apply the new settings from the JSON object.
    applySettingsFromJson(obj);

    // Save the newly applied settings to NVS.
    saveSettings();

    // Set the volume (might have changed).
    audio.setVolume(currentSettings.notificationVolume);
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
    currentSettings.displayMode = DMS_NORMAL_CLOCK;
    saveSettings(); // Persist the change

    // Broadcast the change to the web UI via MQTT
    if (mqttClient.connected()) {
        // This topic will be created in a later step
        // mqttClient.publish((String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/display_mode/state").c_str(), "Normal Clock", true);
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
    Log_printf(LOG_LEVEL_INFO, "DIAG: Entered applySettingsFromJson.");

    // --- START: FIX - Removed risky JSON serialization to prevent heap allocation failure ---
    // The following lines were removed as they could cause a crash on the ESP32
    // when saving complex settings due to large memory allocation on the heap.
    // String jsonData;
    // serializeJson(obj, jsonData);
    // Log_printf(LOG_LEVEL_INFO, "DIAG: JSON object to apply: %s", jsonData.c_str());
    // --- END: FIX ---

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
    int oldDisplayMode = currentSettings.displayMode;
    bool oldSoundToggle = currentSettings.timeTravelSoundToggle;
    bool old24hFormat = currentSettings.displayFormat24h;
    uint8_t oldBrightness = currentSettings.brightness;
    int oldStockRefresh = currentSettings.stockRefreshInterval;

    // --- Apply All Settings from JSON ---
    validateAndSet("displayMode", currentSettings.displayMode, 0, DMS_MAX - 1);

    // --- FIX: Handle display mode changes from the "Data Link" page ---
    // The Data Link UI sends boolean flags instead of a single displayMode integer.
    // This logic correctly interprets those flags to set the right mode.
    if (!obj["weatherModeEnabled"].isNull() || !obj["stockTickerModeEnabled"].isNull() || !obj["dataLinkEnabled"].isNull()) {
        if (obj["stockTickerModeEnabled"] | false) {
            currentSettings.displayMode = DMS_STOCK_TICKER;
        } else if (obj["weatherModeEnabled"] | false) {
            currentSettings.displayMode = DMS_WEATHER;
        } else if (obj["dataLinkEnabled"] | false) {
            currentSettings.displayMode = DMS_DATA_LINK;
        } else {
            currentSettings.displayMode = DMS_NORMAL_CLOCK;
        }
    }

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
    validateAndSetUChar("brightness", currentSettings.brightness, 0, 7);
    if (hardwareInitialized) {
        applyBrightness();
    }
    int tempAnimationStyle = currentSettings.animationStyle;
    validateAndSet("animationStyle", tempAnimationStyle, 0, 27);
    currentSettings.animationStyle = (AnimationType)tempAnimationStyle;

    if (!obj["animationSequence"].isNull()) {
        // --- START: MODIFICATION - Handle string-based animation sequence ---
        // The UI now sends the animation name as a string. We need to convert it
        // to the corresponding enum value before saving.
        if (obj["animationSequence"].is<const char*>()) {
            std::string sequenceName = obj["animationSequence"].as<std::string>();
            currentSettings.animationSequence = animationTypeFromString(sequenceName);
        } else {
            // Fallback for the old integer-based value, ensuring backward compatibility
            // with older clients or direct API calls.
            int tempAnimationSequence = obj["animationSequence"].as<int>();
            if (tempAnimationSequence >= 0 && tempAnimationSequence < ANIMATION_TYPE_MAX) {
                currentSettings.animationSequence = (AnimationType)tempAnimationSequence;
            }
        }
        // --- END: MODIFICATION ---
    }

    validateAndSetUChar("notificationVolume", currentSettings.notificationVolume, 0, 21);
    if (!obj["timeTravelSoundToggle"].isNull()) currentSettings.timeTravelSoundToggle = obj["timeTravelSoundToggle"];
    validateAndSet("presentTimezoneIndex", currentSettings.presentTimezoneIndex, 0, NUM_TIMEZONE_OPTIONS - 1);
    if (!obj["displayFormat24h"].isNull()) currentSettings.displayFormat24h = obj["displayFormat24h"];
    if (!obj["mqttBroker"].isNull()) currentSettings.mqttBroker = obj["mqttBroker"].as<std::string>();
    currentSettings.mqttPort = obj["mqttPort"] | 1883;
    if (!obj["mqttUser"].isNull()) currentSettings.mqttUser = obj["mqttUser"].as<std::string>();
    if (!obj["mqttPassword"].isNull()) currentSettings.mqttPassword = obj["mqttPassword"].as<std::string>();
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
    stockManager.setEnabled(currentSettings.displayMode == DMS_STOCK_TICKER);
    if (!obj["stockRefreshInterval"].isNull()) {
        int newInterval = obj["stockRefreshInterval"].as<int>();
        if (newInterval > 0) { // Basic validation
            Log_printf(LOG_LEVEL_DEBUG, "TRACE: Applying stockRefreshInterval from UI: %d", newInterval);
            currentSettings.stockRefreshInterval = newInterval;
            stockManager.setRefreshInterval(newInterval);
        }
    }
    if (!obj["financialModelingPrepApiKey"].isNull()) {
        currentSettings.financialModelingPrepApiKey = obj["financialModelingPrepApiKey"].as<std::string>();
        stockManager.setApiKey(currentSettings.financialModelingPrepApiKey.c_str());
    }
    if (!obj["stockRow1_symbol"].isNull()) {
        currentSettings.stockRow1_symbol = obj["stockRow1_symbol"].as<std::string>();
    }
    if (!obj["stockRow2_symbol"].isNull()) {
        currentSettings.stockRow2_symbol = obj["stockRow2_symbol"].as<std::string>();
    }
    if (!obj["stockRow3_symbol"].isNull()) {
        currentSettings.stockRow3_symbol = obj["stockRow3_symbol"].as<std::string>();
    }
    if (!obj["stockAssets"].isNull()) {
        JsonArray arr = obj["stockAssets"].as<JsonArray>();
        std::vector<String> symbols;
        for (JsonVariant v : arr) {
            JsonObject assetObj = v.as<JsonObject>();
            String symbol;
            if (assetObj["symbol"].is<JsonObject>()) {
                // Handle {"symbol": {"symbol": "MSFT", ...}}
                symbol = assetObj["symbol"]["symbol"].as<String>();
            } else {
                // Handle {"symbol": "MSFT"}
                symbol = assetObj["symbol"].as<String>();
            }

            if (!symbol.isEmpty()) {
                symbols.push_back(symbol);
            }
        }
        // This single call now handles adding, removing, reordering, and saving.
        stockManager.updateAndSaveAssets(symbols);
    }

    int numPoints = obj["numDataPoints"] | 0;
    currentSettings.numDataPoints = (numPoints < 0) ? 0 : (numPoints > 5 ? 5 : numPoints);
    if (!obj["dataPoints"].isNull()) {
        JsonArray dataPoints = obj["dataPoints"];
        for (int i = 0; i < 5; i++) {
            if (i < currentSettings.numDataPoints && i < dataPoints.size()) {
                JsonObject dp = dataPoints[i];
                if (!dp["enabled"].isNull()) currentSettings.dataPoints[i].enabled = dp["enabled"];
                if (!dp["dataSourceType"].isNull()) currentSettings.dataPoints[i].dataSourceType = (DataSourceType)(dp["dataSourceType"].as<int>());
                currentSettings.dataPoints[i].scrollSpeed = dp["scrollSpeed"] | 150;
                if (!dp["mqttTopic"].isNull()) currentSettings.dataPoints[i].mqttTopic = dp["mqttTopic"].as<std::string>();
                if (!dp["scrollingText"].isNull()) {
                    std::string newText = dp["scrollingText"].as<std::string>();
                    if (currentSettings.dataPoints[i].scrollingText != newText) {
                        currentSettings.dataPoints[i].scrollingText = newText;
                        isMarqueeBufferDirty = true;
                    }
                }
                if (!dp["prefixText"].isNull()) {
                    std::string newPrefix = dp["prefixText"].as<std::string>();
                    if (currentSettings.dataPoints[i].prefixText != newPrefix) {
                        currentSettings.dataPoints[i].prefixText = newPrefix;
                        isMarqueeBufferDirty = true;
                    }
                }
                if (!dp["suffixText"].isNull()) {
                    std::string newSuffix = dp["suffixText"].as<std::string>();
                    if (currentSettings.dataPoints[i].suffixText != newSuffix) {
                        currentSettings.dataPoints[i].suffixText = newSuffix;
                        isMarqueeBufferDirty = true;
                    }
                }
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
    if (oldDisplayMode == DMS_WEATHER && currentSettings.displayMode != DMS_WEATHER) {
        resetWeatherFetchState();
    }

    if (oldDisplayMode != currentSettings.displayMode) {
        publishDisplayMode(currentSettings.displayMode);
    }

    if (oldSoundToggle != currentSettings.timeTravelSoundToggle) {
        publishSoundToggle(currentSettings.timeTravelSoundToggle);
    }

    if (old24hFormat != currentSettings.displayFormat24h) {
        publishDisplayFormat(currentSettings.displayFormat24h);
    }

    if (oldBrightness != currentSettings.brightness) {
        publishBrightness(currentSettings.brightness);
    }

    if (oldStockRefresh != currentSettings.stockRefreshInterval) {
        publishStockRefresh(currentSettings.stockRefreshInterval);
    }

    if (oldCityName != currentSettings.cityName) {
        publishWeatherCity(currentSettings.cityName);
    }
}


/**
 * @brief Saves the current settings to non-volatile storage (NVS).
 * @details This function uses the Preferences library to persist the `currentSettings`
 * object. To minimize unnecessary writes to the flash memory, it checks each setting
 * against its previously saved value and only writes if the value has actually changed.
 */
void saveSettings() {
    preferences.begin(PREFERENCES_NAMESPACE, false);

    preferences.putInt("destYear", currentSettings.destinationYear);
    preferences.putInt("destTzIndex", currentSettings.destinationTimezoneIndex);
    preferences.putInt("lastYear", currentSettings.lastTimeDepartedYear);
    preferences.putInt("lastMonth", currentSettings.lastTimeDepartedMonth);
    preferences.putInt("lastDay", currentSettings.lastTimeDepartedDay);
    preferences.putInt("lastHour", currentSettings.lastTimeDepartedHour);
    preferences.putInt("lastMinute", currentSettings.lastTimeDepartedMinute);
    preferences.putUChar("brightness", currentSettings.brightness);
    preferences.putUChar("volume", currentSettings.notificationVolume);
    preferences.putBool("soundToggle", currentSettings.timeTravelSoundToggle);
    preferences.putInt("presTzIndex", currentSettings.presentTimezoneIndex);
    preferences.putBool("format24h", currentSettings.displayFormat24h);
    preferences.putString("mqttBroker", currentSettings.mqttBroker.c_str());
    preferences.putInt("mqttPort", currentSettings.mqttPort);
    preferences.putString("mqttUser", currentSettings.mqttUser.c_str());
    preferences.putString("mqttPass", currentSettings.mqttPassword.c_str());
    preferences.putInt("displayMode", currentSettings.displayMode);
    preferences.putString("cityName", currentSettings.cityName.c_str());
    preferences.putBool("useMetric", currentSettings.useMetricUnits);
    preferences.putFloat("latitude", currentSettings.latitude);
    preferences.putFloat("longitude", currentSettings.longitude);
    preferences.putInt("depHour", currentSettings.departureHour);
    preferences.putInt("depMinute", currentSettings.departureMinute);
    preferences.putInt("arrHour", currentSettings.arrivalHour);
    preferences.putInt("arrMinute", currentSettings.arrivalMinute);
    preferences.putInt("presetCycle", currentSettings.presetCycleInterval);
    preferences.putInt("theme", currentSettings.theme);
    preferences.putInt("animStyle", currentSettings.animationStyle);
    preferences.putInt("animSequence", currentSettings.animationSequence);
    preferences.putInt("dlTargetRow", currentSettings.dataLinkTargetRow);
    preferences.putString("stockRow1Symbol", currentSettings.stockRow1_symbol.c_str());
    preferences.putString("stockRow2Symbol", currentSettings.stockRow2_symbol.c_str());
    preferences.putString("stockRow3Symbol", currentSettings.stockRow3_symbol.c_str());
    preferences.putInt("numDataPoints", currentSettings.numDataPoints);

    char key_buffer[20];
    for (int i = 0; i < 5; i++) {
        snprintf(key_buffer, sizeof(key_buffer), "dp%d_en", i);
        preferences.putBool(key_buffer, currentSettings.dataPoints[i].enabled);

        snprintf(key_buffer, sizeof(key_buffer), "dp%d_srcType", i);
        preferences.putInt(key_buffer, currentSettings.dataPoints[i].dataSourceType);

        snprintf(key_buffer, sizeof(key_buffer), "dp%d_topic", i);
        preferences.putString(key_buffer, currentSettings.dataPoints[i].mqttTopic.c_str());

        snprintf(key_buffer, sizeof(key_buffer), "dp%d_scrollTxt", i);
        preferences.putString(key_buffer, currentSettings.dataPoints[i].scrollingText.c_str());

        snprintf(key_buffer, sizeof(key_buffer), "dp%d_scroll", i);
        preferences.putInt(key_buffer, currentSettings.dataPoints[i].scrollSpeed);

        snprintf(key_buffer, sizeof(key_buffer), "dp%d_prefix", i);
        preferences.putString(key_buffer, currentSettings.dataPoints[i].prefixText.c_str());

        snprintf(key_buffer, sizeof(key_buffer), "dp%d_suffix", i);
        preferences.putString(key_buffer, currentSettings.dataPoints[i].suffixText.c_str());
    }

	preferences.end();

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
    preferences.begin(PREFERENCES_NAMESPACE, true); // Open preferences in read-only mode to check for existence.

    bool needsInit = !preferences.isKey("destYear");
    if (needsInit) {
        preferences.end(); // End the read-only session before writing.
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
        currentSettings.timeTravelAnimationDuration = 4000;
        currentSettings.animationStyle = ANIMATION_SEQUENTIAL_FLICKER;
        currentSettings.animationSequence = ANIMATION_RANDOMIZE_ALL;
        currentSettings.displayMode = DMS_NORMAL_CLOCK;
        currentSettings.dataLinkTargetRow = 2;
        currentSettings.stockRefreshInterval = 10;
        currentSettings.numDataPoints = 0;
        currentSettings.mqttBroker = "broker.emqx.io";
        currentSettings.mqttPort = 1883;
        currentSettings.mqttUser = "";
        currentSettings.mqttPassword = "";
        currentSettings.cityName = "New York";
        currentSettings.useMetricUnits = false;
        currentSettings.latitude = 40.7128;
        currentSettings.longitude = -74.0060;
        currentSettings.stockRefreshInterval = 20; // Default to 20 minutes
        currentSettings.financialModelingPrepApiKey = "";
        stockManager.clearAssets();
        for (int i = 0; i < 5; i++) {
            currentSettings.dataPoints[i] = {};
            currentSettings.dataPoints[i].enabled = false;
        }
        // Now that defaults are populated in currentSettings, save them.
        saveSettings();
    } else {
        Log_printf(LOG_LEVEL_INFO, "Loading settings from NVS.");
        currentSettings.destinationYear = preferences.getInt("destYear", 1955);
        Log_printf(LOG_LEVEL_INFO, "Loaded destYear: %d", currentSettings.destinationYear);

        currentSettings.destinationTimezoneIndex = preferences.getInt("destTzIndex", 4);
        Log_printf(LOG_LEVEL_INFO, "Loaded destTzIndex: %d", currentSettings.destinationTimezoneIndex);

        currentSettings.lastTimeDepartedYear = preferences.getInt("lastYear", 1985);
        Log_printf(LOG_LEVEL_INFO, "Loaded lastYear: %d", currentSettings.lastTimeDepartedYear);

        currentSettings.lastTimeDepartedMonth = preferences.getInt("lastMonth", 10);
        Log_printf(LOG_LEVEL_INFO, "Loaded lastMonth: %d", currentSettings.lastTimeDepartedMonth);

        currentSettings.lastTimeDepartedDay = preferences.getInt("lastDay", 26);
        Log_printf(LOG_LEVEL_INFO, "Loaded lastDay: %d", currentSettings.lastTimeDepartedDay);

        currentSettings.lastTimeDepartedHour = preferences.getInt("lastHour", 1);
        Log_printf(LOG_LEVEL_INFO, "Loaded lastHour: %d", currentSettings.lastTimeDepartedHour);

        currentSettings.lastTimeDepartedMinute = preferences.getInt("lastMinute", 21);
        Log_printf(LOG_LEVEL_INFO, "Loaded lastMinute: %d", currentSettings.lastTimeDepartedMinute);

        currentSettings.brightness = preferences.getUChar("brightness", 5);
        Log_printf(LOG_LEVEL_INFO, "Loaded brightness: %d", currentSettings.brightness);

        currentSettings.notificationVolume = preferences.getUChar("volume", 15);
        Log_printf(LOG_LEVEL_INFO, "Loaded volume: %d", currentSettings.notificationVolume);

        currentSettings.timeTravelSoundToggle = preferences.getBool("soundToggle", true);
        Log_printf(LOG_LEVEL_INFO, "Loaded soundToggle: %s", currentSettings.timeTravelSoundToggle ? "true" : "false");

        currentSettings.presentTimezoneIndex = preferences.getInt("presTzIndex", 1);
        Log_printf(LOG_LEVEL_INFO, "Loaded presTzIndex: %d", currentSettings.presentTimezoneIndex);

        currentSettings.displayFormat24h = preferences.getBool("format24h", false);
        Log_printf(LOG_LEVEL_INFO, "Loaded format24h: %s", currentSettings.displayFormat24h ? "true" : "false");

        currentSettings.timeTravelAnimationInterval = preferences.getInt("animInterval", 15);
        Log_printf(LOG_LEVEL_INFO, "Loaded animInterval: %d", currentSettings.timeTravelAnimationInterval);

        String brokerStr = preferences.getString("mqttBroker", "broker.emqx.io");
        currentSettings.mqttBroker = brokerStr.c_str();
        Log_printf(LOG_LEVEL_INFO, "Loaded mqttBroker: %s", currentSettings.mqttBroker.c_str());

        currentSettings.mqttPort = preferences.getInt("mqttPort", 1883);
        Log_printf(LOG_LEVEL_INFO, "Loaded mqttPort: %d", currentSettings.mqttPort);

        String userStr = preferences.getString("mqttUser", "");
        currentSettings.mqttUser = userStr.c_str();
        Log_printf(LOG_LEVEL_INFO, "Loaded mqttUser: %s", currentSettings.mqttUser.c_str());

        String passStr = preferences.getString("mqttPass", "");
        currentSettings.mqttPassword = passStr.c_str();
        Log_printf(LOG_LEVEL_INFO, "Loaded mqttPass: (hidden)");

        currentSettings.displayMode = preferences.getInt("displayMode", DMS_NORMAL_CLOCK);
        Log_printf(LOG_LEVEL_INFO, "Loaded displayMode: %d", currentSettings.displayMode);

        String tempString = preferences.getString("cityName", "New York");
        currentSettings.cityName = tempString.c_str();
        Log_printf(LOG_LEVEL_INFO, "Loaded cityName: %s", currentSettings.cityName.c_str());

        currentSettings.useMetricUnits = preferences.getBool("useMetric", false);
        Log_printf(LOG_LEVEL_INFO, "Loaded useMetric: %s", currentSettings.useMetricUnits ? "true" : "false");

        currentSettings.latitude = preferences.getFloat("latitude", 40.7128);
        Log_printf(LOG_LEVEL_INFO, "Loaded latitude: %f", currentSettings.latitude);

        currentSettings.longitude = preferences.getFloat("longitude", -74.0060);
        Log_printf(LOG_LEVEL_INFO, "Loaded longitude: %f", currentSettings.longitude);

        currentSettings.stockRefreshInterval = preferences.getInt("stockRefresh", 20);
        Log_printf(LOG_LEVEL_INFO, "Loaded stockRefresh: %d", currentSettings.stockRefreshInterval);

        if (preferences.isKey("fmpApiKey")) {
            tempString = preferences.getString("fmpApiKey", "");
            currentSettings.financialModelingPrepApiKey = tempString.c_str();
        } else {
            currentSettings.financialModelingPrepApiKey = "";
        }
        Log_printf(LOG_LEVEL_INFO, "Loaded fmpApiKey: %s", currentSettings.financialModelingPrepApiKey.c_str());

        currentSettings.departureHour = preferences.getInt("depHour", 22);
        Log_printf(LOG_LEVEL_INFO, "Loaded depHour: %d", currentSettings.departureHour);

        currentSettings.departureMinute = preferences.getInt("depMinute", 0);
        Log_printf(LOG_LEVEL_INFO, "Loaded depMinute: %d", currentSettings.departureMinute);

        currentSettings.arrivalHour = preferences.getInt("arrHour", 7);
        Log_printf(LOG_LEVEL_INFO, "Loaded arrHour: %d", currentSettings.arrivalHour);

        currentSettings.arrivalMinute = preferences.getInt("arrMinute", 0);
        Log_printf(LOG_LEVEL_INFO, "Loaded arrMinute: %d", currentSettings.arrivalMinute);

        currentSettings.presetCycleInterval = preferences.getInt("presetCycle", 10);
        Log_printf(LOG_LEVEL_INFO, "Loaded presetCycle: %d", currentSettings.presetCycleInterval);

        currentSettings.theme = preferences.getInt("theme", THEME_TIME_CIRCUITS);
        Log_printf(LOG_LEVEL_INFO, "Loaded theme: %d", currentSettings.theme);

        currentSettings.timeTravelAnimationDuration = 4000; // Hardcode to default
        currentSettings.animationStyle = (AnimationType)preferences.getInt("animStyle", ANIMATION_SEQUENTIAL_FLICKER);
        Log_printf(LOG_LEVEL_INFO, "Loaded animStyle: %d", currentSettings.animationStyle);

        currentSettings.animationSequence = (AnimationType)preferences.getInt("animSequence", ANIMATION_RANDOMIZE_ALL);
        Log_printf(LOG_LEVEL_INFO, "Loaded animSequence: %d", currentSettings.animationSequence);

        currentSettings.dataLinkTargetRow = preferences.getInt("dlTargetRow", 2);
        Log_printf(LOG_LEVEL_INFO, "Loaded dlTargetRow: %d", currentSettings.dataLinkTargetRow);

        tempString = preferences.getString("stockRow1Symbol", "");
        currentSettings.stockRow1_symbol = tempString.c_str();
        Log_printf(LOG_LEVEL_INFO, "Loaded stockRow1Symbol: %s", currentSettings.stockRow1_symbol.c_str());

        tempString = preferences.getString("stockRow2Symbol", "");
        currentSettings.stockRow2_symbol = tempString.c_str();
        Log_printf(LOG_LEVEL_INFO, "Loaded stockRow2Symbol: %s", currentSettings.stockRow2_symbol.c_str());

        tempString = preferences.getString("stockRow3Symbol", "");
        currentSettings.stockRow3_symbol = tempString.c_str();
        Log_printf(LOG_LEVEL_INFO, "Loaded stockRow3Symbol: %s", currentSettings.stockRow3_symbol.c_str());

        // Restore loading data points
        currentSettings.numDataPoints = preferences.getInt("numDataPoints", 0);
        Log_printf(LOG_LEVEL_INFO, "Loaded numDataPoints: %d", currentSettings.numDataPoints);
        for (int i = 0; i < 5; i++) {
            String prefix = "dp" + String(i) + "_";
            currentSettings.dataPoints[i].enabled = preferences.getBool((prefix + "en").c_str(), false);
            currentSettings.dataPoints[i].dataSourceType = (DataSourceType)preferences.getInt((prefix + "srcType").c_str(), 0);
            currentSettings.dataPoints[i].mqttTopic = preferences.getString((prefix + "topic").c_str(), "").c_str();
            currentSettings.dataPoints[i].scrollingText = preferences.getString((prefix + "scrollTxt").c_str(), "").c_str();
            currentSettings.dataPoints[i].scrollSpeed = preferences.getInt((prefix + "scroll").c_str(), 150);
            currentSettings.dataPoints[i].prefixText = preferences.getString((prefix + "prefix").c_str(), "").c_str();
            currentSettings.dataPoints[i].suffixText = preferences.getString((prefix + "suffix").c_str(), "").c_str();
        }

        preferences.end(); // End the read-only session.
    }

    // Initialize the StockManager with the loaded/default settings
    stockManager.setApiKey(currentSettings.financialModelingPrepApiKey.c_str());
    stockManager.setEnabled(currentSettings.displayMode == DMS_STOCK_TICKER);
    stockManager.setRefreshInterval(currentSettings.stockRefreshInterval);
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
    // This is the generic event callback, which we use for the primary EOF event.
    Audio::audio_info_callback = audio_info;
    // The setEofCallback is deprecated and its functionality is now handled
    // by the main audio_info callback.
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
  wifiManager->autoConnect("TimeCircuits-Setup");
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

    // --- FIX: Seed the random number generator at boot ---
    // Use an unconnected analog pin to get a source of entropy.
    randomSeed(analogRead(0));

    Log_printf(LOG_LEVEL_INFO, "Initializing watchdog with %d second timeout...", WDT_TIMEOUT);
    // De-initialize the watchdog timer first to prevent an error if it's already running.
    // This can happen if the ESP-IDF default watchdog is enabled.
    esp_task_wdt_deinit();
    // New configuration struct for the watchdog timer
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WDT_TIMEOUT * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,    // Watch idle tasks on all cores
        .trigger_panic = true,
    };
    ESP_ERROR_CHECK(esp_task_wdt_init(&wdt_config)); // Initialize WDT with the new config struct
    esp_task_wdt_add(NULL);         // Subscribe the current task (the main loop) to the watchdog
    Log_printf(LOG_LEVEL_INFO, "Watchdog initialized.");

    // Get MAC address early for MQTT Unique ID. This is more reliable than WiFi.macAddress().
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(MQTT_UNIQUE_ID, sizeof(MQTT_UNIQUE_ID), "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    for (int i = 0; MQTT_UNIQUE_ID[i]; i++) {
        MQTT_UNIQUE_ID[i] = tolower(MQTT_UNIQUE_ID[i]);
    }

    xSerialMutex = xSemaphoreCreateMutex(); // For thread-safe logging
    Log_printf(LOG_LEVEL_INFO, "--- BOOTING ---");
    Log_printf(LOG_LEVEL_INFO, "Device ID: %s", MQTT_UNIQUE_ID);
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
        // --- NEW: Immediately blank displays on successful init to prevent showing stale data ---
        blankAllDisplays();
    }

    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
    tzset();
    Log_printf(LOG_LEVEL_INFO, "Timezone configured.");

    setupMqtt();
    Log_printf(LOG_LEVEL_INFO, "MQTT setup initiated.");

    stockManager.begin();
    Log_printf(LOG_LEVEL_INFO, "StockManager setup initiated.");

    Log_printf(LOG_LEVEL_INFO, "Free heap after setup: %u bytes", ESP.getFreeHeap());

    ArduinoOTA.setHostname("BTTF_TC");
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
   // for (int i = 0; i < 3; ++i) {
   //     for (int j = 0; j < 4; ++j) {
   //         updateDisplaySegment(i, j, "");
 //       }
 //  }

    Log_printf(LOG_LEVEL_INFO, "--- BOOT COMPLETE ---");
    bootTimestamp = millis();

    // --- NEW: Run the sequencer test on startup to verify parallel animations ---
   // runSequencerTest();
}

// --- NEW STATE DETERMINATION FUNCTION ---
void updateDisplayState() {
    static DisplayState previousDisplayState = STATE_NORMAL_CLOCK;
    DisplayState newDisplayState;

    if (isMessageOverrideActive) {
        newDisplayState = STATE_MESSAGE_OVERRIDE;
    } else if (isAnimating) {
        newDisplayState = STATE_ANIMATING;
    } else {
        // The main display logic is now driven by the displayMode setting
        switch (currentSettings.displayMode) {
            case DMS_STOCK_TICKER:
                newDisplayState = STATE_STOCK_TICKER;
                break;
            case DMS_DATA_LINK:
                newDisplayState = STATE_DATA_LINK;
                break;
            case DMS_WEATHER:
                newDisplayState = STATE_WEATHER;
                break;
            case DMS_NORMAL_CLOCK:
            default:
                newDisplayState = STATE_NORMAL_CLOCK;
                break;
        }
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
        case STATE_STOCK_TICKER:
            updateStockTickerDisplay();
            break;
        case STATE_DATA_LINK:
            updateMarqueeDisplay();
            break;
        case STATE_WEATHER:
            // Weather mode is now handled by the handleWeatherDisplay function
            handleWeatherDisplay();
            break;
        case STATE_NORMAL_CLOCK:
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

/**
 * @brief Checks if any sequencer track is currently active.
 * @return True if at least one track's `isActive` flag is true, false otherwise.
 */
bool isAnySequenceActive() {
    for (int i = 0; i < 3; i++) {
        if (sequencerTracks[i].isActive) {
            return true;
        }
    }
    return false;
}

void loop() {
    // Feed the watchdog to prevent a reset. This must be the first action in the loop.
    esp_task_wdt_reset();

    vTaskDelay(1); // Yield to other tasks, making the system responsive.

    // Clean up disconnected WebSocket clients and send pings
    ws.cleanupClients();

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

                ntpSyncRequested = true;
                runBootSequence();
            }
            if (!currentSettings.mqttBroker.empty()) {
                unsigned long now = millis();
                if (!mqttClient.connected()) {
                    // --- mDNS Management: Stop mDNS when MQTT is disconnected ---
                    if (mDnsIsActive) {
                        MDNS.end();
                        mDnsIsActive = false;
                        Log_printf(LOG_LEVEL_INFO, "mDNS service stopped to conserve memory during MQTT reconnect.");
                    }

                    // Check if we are in a hold-off period (circuit breaker is tripped)
                    if (mqttHoldoffUntil > 0 && now < mqttHoldoffUntil) {
                        // We are in a hold-off period, do nothing.
                    } else {
                        // If hold-off is over, reset the timestamp to allow a single new attempt.
                        if (mqttHoldoffUntil > 0 && now >= mqttHoldoffUntil) {
                            Log_printf(LOG_LEVEL_INFO, "MQTT: Hold-off period is over. Attempting to reconnect.");
                            mqttHoldoffUntil = 0;
                            nextMqttReconnectAttempt = 0; // Force immediate attempt
                        }

                        // Check if it's time for a reconnect attempt
                        if (!initialMqttConnectionAttempted || now > nextMqttReconnectAttempt) {
                            reconnectMqtt();
                            initialMqttConnectionAttempted = true;

                            if (!mqttClient.connected()) {
                                // --- CIRCUIT BREAKER: FAILED CONNECTION ---
                                mqttConsecutiveFails++;
                                Log_printf(LOG_LEVEL_WARN, "MQTT: Connection failed. Consecutive failures: %d", mqttConsecutiveFails);

                                if (mqttConsecutiveFails >= MQTT_MAX_FAILS) {
                                    // Trip the circuit breaker
                                    Log_printf(LOG_LEVEL_ERROR, "MQTT: Too many consecutive failures. Tripping circuit breaker for 5 minutes.");
                                    mqttHoldoffUntil = now + MQTT_HOLDOFF_DURATION;
                                    mqttConsecutiveFails = 0; // Reset for after the hold-off
                                    mqttReconnectInterval = MQTT_INITIAL_RETRY_INTERVAL; // Reset for after the hold-off
                                } else {
                                    // If breaker not tripped, use exponential backoff for next attempt
                                    nextMqttReconnectAttempt = now + mqttReconnectInterval;
                                    mqttReconnectInterval *= 2;
                                    if (mqttReconnectInterval > MQTT_MAX_RETRY_INTERVAL) {
                                        mqttReconnectInterval = MQTT_MAX_RETRY_INTERVAL;
                                    }
                                }
                            } else {
                                // --- CIRCUIT BREAKER: SUCCESSFUL CONNECTION ---
                                Log_printf(LOG_LEVEL_INFO, "MQTT: Connection successful. Resetting failure counter.");
                                mqttConsecutiveFails = 0;
                                mqttReconnectInterval = MQTT_INITIAL_RETRY_INTERVAL;
                            }
                        }
                    }
                } else {
                    // --- mDNS Management: Start mDNS only after HA discovery is complete ---
                    // This prevents a memory allocation race condition on the ESP32.
                    // We start mDNS after HA discovery is complete.
                    if (!mDnsIsActive && isHaDiscoveryComplete()) {
                        Log_printf(LOG_LEVEL_INFO, "HA Discovery complete. Starting mDNS...");
                        if (MDNS.begin("BTTF_TC")) {
                            MDNS.addService("http", "tcp", 80);
                            mDnsIsActive = true;
                            Log_printf(LOG_LEVEL_INFO, "mDNS service started successfully.");
                        } else {
                            Log_printf(LOG_LEVEL_ERROR, "mDNS failed to start.");
                        }
                    }

                    // If we are connected, ensure the failure counter is reset.
                    if (mqttConsecutiveFails > 0) {
                        Log_printf(LOG_LEVEL_INFO, "MQTT: Re-established connection. Resetting failure counter.");
                        mqttConsecutiveFails = 0;
                        mqttReconnectInterval = MQTT_INITIAL_RETRY_INTERVAL;
                    }
                    mqttClient.loop();
                }
            }
            
            // Handle the non-blocking Home Assistant discovery process
            handleHaDiscovery();

            stockManager.loop();

            // --- START: MODIFICATION - Run sequencer on every loop ---
            // This ensures that sequences can run in parallel with any display mode.
            handleSequencer();
            handleAllSequencerMarquees();
            handleTemporalEcho();
            handlePresetCycling();
            handleSleepSchedule();
            // --- END: MODIFICATION ---

            // --- NEW: Audio State Synchronization Safety Net ---
            // Periodically check if the application's radio state is out of sync with the audio library's state.
            // This can happen if a stream drops unexpectedly and the EOF callback doesn't fire.
            static unsigned long lastAudioSyncCheck = 0;
            if (millis() - lastAudioSyncCheck > 1000) { // Check every second
                if (isRadioStreaming && !audio.isRunning()) {
                    Log_printf(LOG_LEVEL_WARN, "SAFETY NET: Radio state desync detected! Forcing cleanup.");
                    cleanupAudio(true); // Force a permanent cleanup
                }
                lastAudioSyncCheck = millis();
            }

            // --- START: MODIFICATION - Periodic Stock Manager Reset ---
            static unsigned long lastStockManagerReset = 0;
            if (currentSettings.displayMode == DMS_STOCK_TICKER) {
                if (lastStockManagerReset == 0) {
                    lastStockManagerReset = millis();
                }
                unsigned long now = millis();
                if (now - lastStockManagerReset > STOCK_MANAGER_RESET_INTERVAL) {
                    Log_printf(LOG_LEVEL_INFO, "Performing periodic reset of StockManager to prevent heap fragmentation.");
                    // Re-initialize the stock manager from the master settings object.
                    stockManager.setApiKey(currentSettings.financialModelingPrepApiKey.c_str());
                    stockManager.setRefreshInterval(currentSettings.stockRefreshInterval);
                    Log_printf(LOG_LEVEL_DEBUG, "TRACE: Re-applying stockRefreshInterval during periodic reset: %d", currentSettings.stockRefreshInterval);
                    stockManager.loadAssets();
                    stockManager.setEnabled(true); // Re-enable the manager after reset
                    lastStockManagerReset = now;
                }
            } else {
                lastStockManagerReset = 0; // Reset the timer if stock ticker mode is disabled
            }
            // --- END: MODIFICATION ---

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
                    // --- NEW: Only run normal display logic after boot is complete ---
                    if (bootSequenceCompleted) {
                        if (isAnimating) {
                            if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                                handleDisplayAnimation();
                                xSemaphoreGive(xDisplayDataMutex);
                            }
                        } else if (isStyledAnimating) {
                            if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                                handleDisplayAnimation();
                                xSemaphoreGive(xDisplayDataMutex);
                            }
                        } else {
                            if (millis() - lastDisplayUpdateTime > DISPLAY_UPDATE_INTERVAL) {
                                lastDisplayUpdateTime = millis();
                                // --- START: MODIFICATION - Prioritize Sequencer ---
                                // If any sequence is active, we bypass the normal display mode logic
                                // to prevent interference with the sequence.
                                if (isAnySequenceActive()) {
                                    // A sequence is running. Do nothing here to allow the sequence
                                    // to have full control of the display. The handleSequencer()
                                    // function will handle the necessary display updates.
                                } else {
                                    // No sequence is running. Proceed with the normal display logic.
                                    updateDisplayState();
                                    handleDisplay();
                                }
                                // --- END: MODIFICATION ---
                            }
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

    // In ArduinoJson v7, JsonDocument manages its own memory on the heap.
    // The constructor no longer takes a size, so a default instance is created.
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
    // --- FIX: Prevent preset cycling from running during the boot animation ---
    // The bootSequenceCompleted flag is set to true only after the entire boot
    // sequence has finished.
    if (!bootSequenceCompleted) {
        return;
    }
    // --- START: MODIFICATION ---
    // Only cycle presets when in normal clock mode. This prevents interference with
    // other modes that use the last time departed row for their own display.
    if (currentDisplayState != STATE_NORMAL_CLOCK) {
        return;
    }
    // --- END: MODIFICATION ---

    if (lastPresetCycleTime == 0 && bootState == BOOT_INACTIVE) {
        lastPresetCycleTime = millis();
    }
    // Return if cycling is disabled or the display is asleep
    if (currentSettings.presetCycleInterval == 0 || isDisplayAsleep) {
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

        // --- START: MODIFICATION - Trigger animation on preset cycle ---
        triggerAnimation(currentSettings.animationSequence);
        // --- END: MODIFICATION ---
    }
}


void handleSleepSchedule() {
  if (!timeSynchronized || isAnySequenceActive()) return;
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
        playSound("/CONFIRM_ON.mp3");
    }
    updateHaStatus("Idle");
  }
}