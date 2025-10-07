#ifndef HARDWARE_CONTROL_H
#define HARDWARE_CONTROL_H
#include "timezone.h"
#include <Wire.h>
#include "AnimationSequences.h"

extern char old_dest_str[17], old_pres_str[17], old_last_str[17];
#include <string>
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"
#include <time.h>

// --- PIN DEFINITIONS (MINIMAL CHANGE ESP32-S3 SAFE VERSION) ---
#define I2C_SDA_1 8
#define I2C_SCL_1 9
#define I2C_SDA_2 10
#define I2C_SCL_2 11
#define I2S_LRC_PIN 15
#define I2S_BCLK_PIN 16
#define I2S_DIN_PIN 17
#define I2S_SD_PIN 18
#define DEST_AM_PIN 13
#define DEST_PM_PIN 14
#define PRES_AM_PIN 38
#define PRES_PM_PIN 39
#define LAST_AM_PIN 4
#define LAST_PM_PIN 6

// --- HARDWARE CONFIG ---
#define ENABLE_HARDWARE 1
#define MAX_FILENAME_LENGTH 256

// --- ENUMS & DATA STRUCTURES ---
enum DisplayModeState { NORMAL_CLOCK, STOCK_TICKER, WEATHER, MARQUEE, OVERRIDE_MESSAGE, MARQUEE_OVERRIDE };
struct MarqueeData { std::string month; std::string day; std::string year; std::string time; };
enum AnimationPhase { ANIM_INACTIVE, ANIM_START, ANIM_WAIT_FOR_KEYPAD_SOUND, ANIM_WAIT_FOR_SOUND, ANIM_POWER_UP, ANIM_FLICKER, ANIM_TIME_ACCELERATION, ANIM_ARRIVAL, ANIM_COOL_DOWN, ANIM_LANDING };
struct FetchDataParams { int pointIndex; int totalRequests; };
enum Theme { THEME_TIME_CIRCUITS, THEME_OUTATIME, THEME_88MPH, THEME_PLUTONIUM_GLOW, THEME_MR_FUSION, THEME_CLOCK_TOWER };
enum DataSourceType { DATA_SOURCE_MQTT, DATA_SOURCE_HA, DATA_SOURCE_STATIC };
enum HttpMethod { GET, POST };
enum DisplayMode { FOUR_COLUMN, SCROLLING_TEXT };
enum DisplayModeSetting { DMS_NORMAL_CLOCK, DMS_STOCK_TICKER, DMS_WEATHER, DMS_DATA_LINK, DMS_MAX };

struct DataPoint {
  bool enabled; int scrollSpeed; DataSourceType dataSourceType; std::string mqttTopic;
  std::string scrollingText; std::string prefixText; std::string suffixText;
};

struct ClockSettings {
    int destinationYear; int destinationTimezoneIndex; int lastTimeDepartedYear; int lastTimeDepartedMonth;
    int lastTimeDepartedDay; int lastTimeDepartedHour; int lastTimeDepartedMinute; int presentTimezoneIndex;
    int departureHour; int departureMinute; int arrivalHour; int arrivalMinute; uint8_t brightness;
    uint8_t notificationVolume; int timeTravelAnimationDuration; int timeTravelAnimationInterval;
    AnimationType animationStyle; AnimationType animationSequence; bool timeTravelSoundToggle;
    int presetCycleInterval; bool displayFormat24h; int numDataPoints; std::string mqttBroker;
    int mqttPort; std::string mqttUser; std::string mqttPassword; std::string cityName; bool useMetricUnits;
    float latitude; float longitude; int stockRefreshInterval; std::string financialModelingPrepApiKey;
    std::string stockRow1_symbol; std::string stockRow2_symbol; std::string stockRow3_symbol;
    DataPoint dataPoints[5]; int theme; int dataLinkTargetRow; int displayMode;
};

struct StockData { std::string symbol; std::string price; std::string change_percent; bool dataValid = false; };

struct WeatherData {
  float temperature; float apparentTemperature; float windSpeed; int humidity; int weatherCode; float dailyHigh;
  float dailyLow; float hourlyTemp[3]; int hourlyCode[3]; float tomorrowHigh; float tomorrowLow;
  int tomorrowWeatherCode; int precipitationProbability; float maxWindSpeed; time_t sunrise; time_t sunset;
  float latitude; float longitude; bool dataValid = false; std::string errorReason; std::string timezone;
};

struct DisplayRow { Adafruit_AlphaNum4 month; Adafruit_AlphaNum4 day; Adafruit_AlphaNum4 year; Adafruit_AlphaNum4 time; };

#if ENABLE_HARDWARE
#include <freertos/semphr.h>
extern TwoWire I2C_1; extern TwoWire I2C_2;
extern DisplayRow destRow, presRow, lastRow;
extern SemaphoreHandle_t xDisplayHardwareMutex;
extern SemaphoreHandle_t xTimeLibMutex;
extern SemaphoreHandle_t xSerialMutex;
#endif

// --- PUBLIC, LOCKING FUNCTION PROTOTYPES ---
void safe_printf(const char *format, ...);
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
void playSound(const char* filepath, bool fromMqtt = false);
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

// --- INTERNAL, NON-LOCKING FUNCTION PROTOTYPES ---
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

#endif // HARDWARE_CONTROL_H