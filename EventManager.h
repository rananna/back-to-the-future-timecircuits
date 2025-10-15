/**
 * @file EventManager.h
 * @brief Central repository of `extern` declarations for global state variables.
 * @details This file serves as a way to make global variables, primarily defined in the main
 * `back-to-the-future-timecircuits.ino` file, accessible to other modules across the project.
 * This is a common pattern in Arduino projects but centralizes the declaration of shared state.
 * It includes state flags for animations, display modes, data fetching, and more.
 *
 * @note Much of the state declared here is legacy. Newer features tend to encapsulate state
 * within their respective manager classes (e.g., `StockManager`).
 */
#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include <ArduinoJson.h>
#include <string>
#include <PubSubClient.h>
#include "HardwareControl.h"
#include "DisplayManager.h"
#include "AnimationManager.h"
#include "Audio.h"


/**
 * @name Global State Variable Declarations
 * @brief Extern declarations for global variables defined in the main .ino file.
 * @{
 */

// --- Core Settings and Data ---
extern ClockSettings currentSettings;           /**< Holds all user-configurable settings. The single source of truth for configuration. */
extern MarqueeData displayPages[5];             /**< An array holding the parsed data for each of the 5 "Data Link" pages. */
extern MarqueeData lastGoodDisplayPages[5];     /**< A backup of the last known good data for the "Data Link" pages. */
extern WeatherData currentWeatherData;          /**< Holds the latest fetched weather data. */

// --- Legacy Animation State ---
extern bool isAnimating;                        /**< DEPRECATED. Flag indicating if a legacy animation is active. */
extern unsigned long animationStartTime;        /**< DEPRECATED. `millis()` timestamp when the legacy animation started. */
extern unsigned long lastAnimationFrameTime;    /**< DEPRECATED. `millis()` timestamp of the last frame update for a legacy animation. */
extern AnimationPhase currentPhase;             /**< DEPRECATED. The current phase of the legacy time travel animation state machine. */

// --- System & Display State ---
extern bool isDisplayAsleep;                    /**< Flag indicating if the display is currently in a sleep state. */
extern BootSequenceState bootState;             /**< The current state of the cinematic boot sequence state machine. */
extern unsigned long bootStateStartTime;        /**< `millis()` timestamp when the current boot state began. */
extern unsigned long lastPresetCycleTime;       /**< `millis()` timestamp of the last time the display mode preset was cycled. */
extern bool isMessageOverrideActive;            /**< Flag indicating if a high-priority override message is being shown. */
extern char overrideMessageLine1[128];          /**< The text for the top row of an override message. */
extern char overrideMessageLine2[128];          /**< The text for the middle row of an override message. */
extern char overrideMessageLine3[128];          /**< The text for the bottom row of an override message. */

// --- Temporal Echo Effect State ---
extern bool isEchoEffectActive;                 /**< Flag indicating if the post-time-travel "temporal echo" effect is active. */
extern unsigned long echoEffectStartTime;       /**< `millis()` timestamp when the echo effect started. */
extern unsigned long lastEchoCheckTime;         /**< `millis()` timestamp of the last echo effect update. */

// --- Legacy Flicker Effect State ---
extern bool isFlickeringNow;                    /**< DEPRECATED. Flag indicating if the legacy flicker effect is active. */
extern unsigned long flickerStartTime;          /**< DEPRECATED. `millis()` timestamp when the flicker effect started. */
extern int flickerDisplayIndex;                 /**< DEPRECATED. Index of the display being flickered. */

// --- Data Link / Marquee State ---
extern MarqueeState marqueeState;               /**< The current state of the "Data Link" marquee state machine. */
extern unsigned long lastDataLinkFetch;         /**< `millis()` timestamp of the last data fetch for the Data Link mode. */
extern unsigned long lastMarqueeStateChange;    /**< `millis()` timestamp when the marquee state last changed. */
extern int marqueeScrollPosition;               /**< The current horizontal scroll position for the main marquee. */
extern int marqueeScrollPositionYear;           /**< The scroll position for the year segment in the marquee. */
extern int currentPageIndex;                    /**< The index of the "Data Link" page currently being displayed. */

// --- Data Fetching State ---
extern volatile bool isFetchingData;            /**< Volatile flag indicating a generic data fetch is in progress. */
extern volatile bool isFetchingWeather;         /**< Volatile flag indicating a weather data fetch is in progress. */
extern int dataPointFetchFailures[5];           /**< Array to track the number of consecutive fetch failures for each data point. */
extern const int MAX_FETCH_FAILURES;            /**< The maximum number of fetch failures before disabling a data point. */
extern volatile int requestsCompleted;          /**< A counter for completed asynchronous requests. */
extern std::string lastCityName;                /**< The last known city name, used to detect changes and trigger geocoding. */

// --- Network & Time State ---
extern PubSubClient mqttClient;                 /**< The global instance of the MQTT client library. */
extern bool timeSynchronized;                   /**< Flag indicating if the device's internal clock has been synced with an NTP server. */

// --- Audio State ---
extern char currentSoundFile[MAX_FILENAME_LENGTH]; /**< The filename of the sound effect currently being played. */
extern Audio audio;                             /**< The global instance of the audio player library. */

// --- System State ---
extern bool hardwareInitialized;                /**< Flag indicating if the core hardware (displays, etc.) has been initialized. */
extern SemaphoreHandle_t xDisplayDataMutex;     /**< A mutex to protect shared data structures related to display content. */

// --- Sequencer Globals (some are legacy/redundant) ---
extern SequencerTrack sequencerTracks[3];       /**< The three tracks for the animation sequencer. */
extern bool isFading;                           /**< DEPRECATED. A global fade flag, largely replaced by per-track state. */
extern bool isSequencerMarqueeActive;           /**< DEPRECATED. A global marquee flag, replaced by per-track state. */

/** @} */



#endif // EVENT_MANAGER_H