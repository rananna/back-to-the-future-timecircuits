/**
 * @file HardwareControl.h
 * @brief Defines hardware pinouts, data structures, and function prototypes for hardware interaction.
 *
 * This file serves as a central hub for all hardware-related definitions. It includes:
 * - GPIO pin assignments for I2C, I2S, and LEDs.
 * - Core data structures that hold the device's state and settings (`ClockSettings`, `WeatherData`, etc.).
 * - Enumerations for various states and modes (`DisplayModeState`, `Theme`, etc.).
 * - Extern declarations for global hardware objects (displays, mutexes).
 * - Function prototypes for all public and internal functions that interact with the hardware,
 *   such as displays and the audio amplifier. It clearly separates thread-safe public functions
 *   from their non-locking internal counterparts.
 */
#ifndef HARDWARE_CONTROL_H
#define HARDWARE_CONTROL_H
#include "timezone.h"
#include <Wire.h>
#include "AnimationSequences.h"

/**
 * @brief Extern declarations for character arrays holding the previously displayed time strings.
 * @details These are used to detect changes and decide when a display update is necessary,
 * optimizing I2C traffic.
 */
extern char old_dest_str[17], old_pres_str[17], old_last_str[17];
#include <string>
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"
#include <time.h>

/**
 * @name GPIO Pin Definitions
 * @brief Defines the GPIO pins used for communication with various hardware components.
 * @{
 */
#define I2C_SDA_1 8             /**< SDA pin for the first I2C bus (Top and Middle Display Rows). */
#define I2C_SCL_1 9             /**< SCL pin for the first I2C bus. */
#define I2C_SDA_2 10            /**< SDA pin for the second I2C bus (Bottom Display Row). */
#define I2C_SCL_2 11            /**< SCL pin for the second I2C bus. */
#define I2S_LRC_PIN 15          /**< I2S Left/Right Clock (LRC) pin for the audio DAC. */
#define I2S_BCLK_PIN 16         /**< I2S Bit Clock (BCLK) pin for the audio DAC. */
#define I2S_DIN_PIN 17          /**< I2S Data In (DIN) pin for the audio DAC. */
#define I2S_SD_PIN 18           /**< I2S Shutdown (SD) pin for the audio amplifier. */
#define DEST_AM_PIN 13          /**< GPIO pin for the Destination Time AM indicator LED. */
#define DEST_PM_PIN 14          /**< GPIO pin for the Destination Time PM indicator LED. */
#define PRES_AM_PIN 38          /**< GPIO pin for the Present Time AM indicator LED. */
#define PRES_PM_PIN 39          /**< GPIO pin for the Present Time PM indicator LED. */
#define LAST_AM_PIN 4           /**< GPIO pin for the Last Time Departed AM indicator LED. */
#define LAST_PM_PIN 6           /**< GPIO pin for the Last Time Departed PM indicator LED. */
/** @} */

/**
 * @name Hardware Configuration Macros
 * @{
 */
#define ENABLE_HARDWARE 1       /**< Master switch to enable/disable actual hardware interaction. Set to 0 for simulation/debugging without hardware. */
#define MAX_FILENAME_LENGTH 256 /**< Maximum length for a filename path, used for sound effects on the filesystem. */
/** @} */


/**
 * @name Enumerations and Data Structures
 * @brief Core data types used throughout the firmware to manage state, settings, and data.
 * @{
 */

/** @brief Defines the possible states for the main display controller. */
enum DisplayModeState { NORMAL_CLOCK, STOCK_TICKER, WEATHER, MARQUEE, OVERRIDE_MESSAGE, MARQUEE_OVERRIDE };

/** @brief Holds the pre-formatted string components for a single row's marquee display. */
struct MarqueeData { std::string month; std::string day; std::string year; std::string time; };

/** @brief Defines the phases of the legacy time travel animation state machine. */
enum AnimationPhase { ANIM_INACTIVE, ANIM_START, ANIM_WAIT_FOR_KEYPAD_SOUND, ANIM_WAIT_FOR_SOUND, ANIM_POWER_UP, ANIM_FLICKER, ANIM_TIME_ACCELERATION, ANIM_ARRIVAL, ANIM_COOL_DOWN, ANIM_LANDING };

/** @brief Parameters for a data fetch operation, used for asynchronous fetching. */
struct FetchDataParams { int pointIndex; int totalRequests; };

/** @brief Defines the available color/style themes for the UI and device. */
enum Theme { THEME_TIME_CIRCUITS, THEME_OUTATIME, THEME_88MPH, THEME_PLUTONIUM_GLOW, THEME_MR_FUSION, THEME_CLOCK_TOWER };

/** @brief Defines the possible sources for a "Data Link" data point. */
enum DataSourceType { DATA_SOURCE_MQTT, DATA_SOURCE_HA, DATA_SOURCE_STATIC };

/** @brief Defines HTTP methods for API requests. */
enum HttpMethod { GET, POST };

/** @brief Defines the display format for a "Data Link" data point. */
enum DisplayMode { FOUR_COLUMN, SCROLLING_TEXT };

/** @brief Defines the primary display modes selectable by the user. DMS_MAX is used for validation. */
enum DisplayModeSetting { DMS_NORMAL_CLOCK, DMS_STOCK_TICKER, DMS_WEATHER, DMS_DATA_LINK, DMS_MAX };

/** @brief Represents a single configurable "Data Link" screen. */
struct DataPoint {
  bool enabled;                     /**< Whether this data point is active. */
  int scrollSpeed;                  /**< Marquee scroll speed in milliseconds per character. */
  DataSourceType dataSourceType;    /**< The source of the data (MQTT, Home Assistant, Static). */
  std::string mqttTopic;            /**< The MQTT topic to subscribe to if source is MQTT. */
  std::string scrollingText;        /**< The static text to display if source is Static. */
  std::string prefixText;           /**< Text to prepend to the data. */
  std::string suffixText;           /**< Text to append to the data. */
};

/**
 * @brief A comprehensive structure holding all user-configurable settings for the device.
 * @details This struct is serialized to and from JSON for saving to SPIFFS and for communication
 * with the web UI. It acts as the single source of truth for the device's configuration.
 */
struct ClockSettings {
    // Time & Location
    int destinationYear;                /**< The target year for the destination time display. */
    int destinationTimezoneIndex;       /**< Index in the `timezones` array for the destination time. */
    int lastTimeDepartedYear;           /**< The year of the last time departed display. */
    int lastTimeDepartedMonth;          /**< The month of the last time departed display. */
    int lastTimeDepartedDay;            /**< The day of the last time departed display. */
    int lastTimeDepartedHour;           /**< The hour of the last time departed display. */
    int lastTimeDepartedMinute;         /**< The minute of the last time departed display. */
    int presentTimezoneIndex;           /**< Index in the `timezones` array for the present time. */
    int departureHour;                  /**< The hour of the departure time (used for time travel calculations). */
    int departureMinute;                /**< The minute of the departure time. */
    int arrivalHour;                    /**< The hour of the arrival time (used for time travel calculations). */
    int arrivalMinute;                  /**< The minute of the arrival time. */
    std::string cityName;               /**< City name for weather data fetching. */
    bool useMetricUnits;                /**< True for Celsius/kmh, false for Fahrenheit/mph. */
    float latitude;                     /**< Latitude for weather data. */
    float longitude;                    /**< Longitude for weather data. */
    bool displayFormat24h;              /**< True for 24-hour format, false for 12-hour format with AM/PM LEDs. */

    // Display & Animation
    uint8_t brightness;                 /**< Global display brightness (0-15). */
    AnimationType animationSequence;    /**< The selected built-in animation sequence for time travel. */
    int presetCycleInterval;            /**< Interval in seconds for cycling through display presets (0 to disable). */
    int displayMode;                    /**< The primary display mode, corresponds to `DisplayModeSetting`. */
    int theme;                          /**< The selected color theme, corresponds to `Theme`. */
    int dataLinkTargetRow;              /**< The display row (0-2) to show Data Link info on. */

    // Audio
    uint8_t notificationVolume;         /**< Volume for sound effects (0-21). */
    bool timeTravelSoundToggle;         /**< Whether to play sounds during time travel sequences. */
    std::string favoriteRadioName;      /**< User-defined name for the favorite radio station. */
    std::string favoriteRadioUrl;       /**< URL of the favorite internet radio stream. */

    // Network & API
    std::string mqttBroker;             /**< MQTT broker address. */
    int mqttPort;                       /**< MQTT broker port. */
    std::string mqttUser;               /**< MQTT username. */
    std::string mqttPassword;           /**< MQTT password. */
    std::string financialModelingPrepApiKey; /**< API key for financialmodelingprep.com for stock data. */
    int stockRefreshInterval;           /**< Interval in minutes for refreshing stock data. */

    // Data Sources
    std::string stockRow1_symbol;       /**< Stock symbol for the top row in stock mode. */
    std::string stockRow2_symbol;       /**< Stock symbol for the middle row in stock mode. */
    std::string stockRow3_symbol;       /**< Stock symbol for the bottom row in stock mode. */
    DataPoint dataPoints[5];            /**< Array of 5 configurable Data Link screens. */
    int numDataPoints;                  /**< The number of enabled data points. */

    // Deprecated/Legacy
    AnimationType animationStyle;       /**< DEPRECATED. Legacy animation style. Replaced by `animationSequence`. */
    int timeTravelAnimationDuration;    /**< DEPRECATED. Legacy animation setting. */
    int timeTravelAnimationInterval;    /**< DEPRECATED. Legacy animation setting. */
};

/** @brief Holds the fetched data for a single stock symbol. */
struct StockData { std::string symbol; std::string price; std::string change_percent; bool dataValid = false; };

/** @brief Holds all fetched and calculated weather data for the configured location. */
struct WeatherData {
  float temperature; float apparentTemperature; float windSpeed; int humidity; int weatherCode; float dailyHigh;
  float dailyLow; float hourlyTemp[3]; int hourlyCode[3]; float tomorrowHigh; float tomorrowLow;
  int tomorrowWeatherCode; int precipitationProbability; float maxWindSpeed; time_t sunrise; time_t sunset;
  float latitude; float longitude; bool dataValid = false; std::string errorReason; std::string timezone;
};

/** @brief Represents a full display row, composed of four 7-segment alphanumeric displays. */
struct DisplayRow { Adafruit_AlphaNum4 month; Adafruit_AlphaNum4 day; Adafruit_AlphaNum4 year; Adafruit_AlphaNum4 time; };
/** @} */


/**
 * @name Digital Rain Effect
 * @brief Data structures and globals for the "Digital Rain" animation effect.
 * @{
 */
#define MAX_RAINDROPS 50    /**< Maximum number of raindrops to simulate concurrently. */
/** @brief Represents a single falling character in the digital rain effect. */
struct Raindrop {
    int column;             /**< The horizontal column (0-12) of the raindrop. */
    float y;                /**< The vertical position of the raindrop. */
    float speed;            /**< The falling speed of the raindrop. */
    bool active;            /**< Whether the raindrop is currently visible. */
};
extern Raindrop raindrops[MAX_RAINDROPS]; /**< Array holding the state of all raindrops. */
extern bool rain_initialized;             /**< Flag to ensure the rain effect is initialized only once. */
/** @} */


#if ENABLE_HARDWARE
/**
 * @name Global Hardware Objects and Mutexes
 * @brief Extern declarations for globally accessible hardware instances and synchronization primitives.
 * @details These objects are defined in HardwareControl.cpp. Using `extern` here makes them
 * accessible to other parts of the firmware while ensuring they are defined only once.
 * The mutexes are critical for preventing race conditions in the multi-threaded FreeRTOS environment.
 * @{
 */
#include <freertos/semphr.h>
extern TwoWire I2C_1;       /**< I2C bus instance for the top and middle display rows. */
extern TwoWire I2C_2;       /**< I2C bus instance for the bottom display row. */
extern DisplayRow destRow, presRow, lastRow; /**< The three main display row objects. */
extern bool ledStates[6];   /**< Stores the state of the 6 AM/PM LEDs before an animation. */

/**
 * @brief A FreeRTOS mutex to protect against concurrent access to I2C hardware.
 * @details All functions that write to the displays must acquire this mutex first to prevent
 * garbled I2C commands and potential crashes.
 */
extern SemaphoreHandle_t xDisplayHardwareMutex;

/**
 * @brief A FreeRTOS mutex to protect the underlying time library.
 * @details The standard C `time.h` library is not inherently thread-safe. This mutex ensures
 * that operations like getting or setting the time are atomic.
 */
extern SemaphoreHandle_t xTimeLibMutex;

/**
 * @brief A FreeRTOS mutex to protect the Serial output.
 * @details Ensures that log messages from different tasks are not interleaved, making
 * the debug output readable.
 */
extern SemaphoreHandle_t xSerialMutex;
/** @} */
#endif

/**
 * @name Public, Thread-Safe Function Prototypes
 * @brief Functions intended for general use from any task.
 * @details These functions are "thread-safe" because they internally acquire and release the
 * necessary mutexes (e.g., `xDisplayHardwareMutex`) before calling their `_internal` counterparts.
 * This prevents race conditions when, for example, an animation and a background data update
 * both try to write to the display at the same time.
 * @{
 */
void safe_printf(const char *format, ...);
void saveLedStates();
void turnOffAllLeds();
void restoreLedStates();
bool setupPhysicalDisplay();
void updateDisplayRow(DisplayRow& row, const struct tm& timeinfo, int year, bool showDecimal);
void animateDisplayRowRandomly(DisplayRow& row);
void animateAllRowsTimelineSkim(unsigned long elapsed, int duration, int destinationYear, bool isCountingUp);
void animateTornadoFlicker();
void animateCapacitorChargeUp(unsigned long elapsed, int duration);
void animateDigitalRain(unsigned long elapsed, int duration);
void animateWaveformCollapse(unsigned long elapsed, int duration);
void animateTimelineSkim(unsigned long elapsed, int duration, int destinationYear);
void blankAllDisplays();
void playSound(const char* filepath, bool fromMqtt = false, int volume = -1);
void printToDisplay(Adafruit_AlphaNum4 &display, const char* text, int justification = 0);
void displaySpeed(int speed);
void displaySpeedRamp(int speed);
void flashAllDisplays();
void animateTemporalLockOn(DisplayRow& row, const struct tm& timeinfo, int year, bool showDecimal = true);
void typeTextOnDisplay(DisplayRow& row, const char* text, int typeDelay, bool withCursor);
void animateFluxCapacitor();
void displayStaticFluxText();
void animateRandomRealTimes();
void applyBrightness();
void animateSequentialFlicker(unsigned long elapsed, int duration);
void animateCountingUp(unsigned long elapsed, int duration);
void animateCorruptedData();
void animateLockOnSequence(unsigned long elapsed, int duration);
void animateUnstableSkim(unsigned long elapsed, int duration, int destinationYear);
void animateTemporalDesync();
void animateGlitchyJumpCut(unsigned long elapsed, int duration);
void animatePlasmaWarmUp(unsigned long elapsed, int duration);
void animateTimeWarpStreaks(unsigned long elapsed, int duration, const char* final_dest, const char* final_pres, const char* final_last);
void animateCharacterScanline(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str);
void animateFocusIn(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str);
void animateCodeBreaker(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str);
void animateTemporalParadox(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str);
void animateDigitCascade(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str);
void animateElectricSurge(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str);
void animateFlipDiscDisplay(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str);
void animateInterferencePattern(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str);
void getFormattedTimeStrings(char* dest_str, char* pres_str, char* last_str);
void resetI2CBus(int i2c_num);
void display88MphSpeed(float speed);
/** @} */

/**
 * @name Internal, Non-Locking Function Prototypes
 * @brief Core logic functions that are NOT thread-safe.
 * @details These functions contain the actual implementation for hardware interaction but do
 * **not** handle mutexes themselves. They should **only** be called by their public, locking
 * wrapper counterparts. This design prevents deadlocks that could occur if a function that
 * already holds a mutex calls another function that tries to take the same mutex again.
 * @{
 */
void updateDisplayRow_internal(DisplayRow& row, const struct tm& timeinfo, int year, bool showDecimal);
void animateDisplayRowRandomly_internal(DisplayRow& row);
void displaySpeed_internal(int speed);
void displaySpeedRamp_internal(int speed);
void animateAllRowsTimelineSkim_internal(unsigned long elapsed, int duration, int destinationYear, bool isCountingUp);
void flashAllDisplays_internal();
void animateTornadoFlicker_internal();
void animateCorruptedData_internal();
void animateLockOnSequence_internal(unsigned long elapsed, int duration);
void blankDisplayRow_internal(DisplayRow& row);
void animateUnstableSkim_internal(unsigned long elapsed, int duration, int destinationYear);
void animateTemporalDesync_internal();
void animateRandomRealTimes_internal();
void animateCapacitorChargeUp_internal(unsigned long elapsed, int duration);
void animateDigitalRain_internal(unsigned long elapsed, int duration);
void animateWaveformCollapse_internal(unsigned long elapsed, int duration);
void animateTimelineSkim_internal(unsigned long elapsed, int duration, int destinationYear);
void display88MphSpeed_internal(float speed);
void typeTextOnDisplay_internal(DisplayRow& row, const char* text, int typeDelay, bool withCursor);
void animateFluxCapacitor_internal();
void displayStaticFluxText_internal();
void applyBrightness_internal();
void animateSequentialFlicker_internal(unsigned long elapsed, int duration);
void animateCountingUp_internal(unsigned long elapsed, int duration);
void animateGlitchyJumpCut_internal(unsigned long elapsed, int duration);
void animatePlasmaWarmUp_internal(unsigned long elapsed, int duration);
void animateTimeWarpStreaks_internal(unsigned long elapsed, int duration, const char* final_dest, const char* final_pres, const char* final_last);
void animateCharacterScanline_internal(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str);
void animateFocusIn_internal(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str);
void animateCodeBreaker_internal(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str);
void animateTemporalParadox_internal(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str);
void animateDigitCascade_internal(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str);
void animateElectricSurge_internal(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str);
void animateFlipDiscDisplay_internal(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str);
void animateInterferencePattern_internal(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str);
void animateTemporalLockOn_internal(DisplayRow& row, const struct tm& timeinfo, int year, bool showDecimal);
void blankAllDisplays_internal();
/** @} */

#endif // HARDWARE_CONTROL_H