#ifndef HARDWARE_CONTROL_H
#define HARDWARE_CONTROL_H

#include <Wire.h>
#include <string>
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"
#include <time.h>

// --- PIN DEFINITIONS ---
#define I2C_SDA_1 21
#define I2C_SCL_1 22
#define I2S_LRC_PIN 16
#define I2S_BCLK_PIN 17
#define I2S_DIN_PIN 5
#define I2S_SD_PIN 18
#define DEST_AM_PIN 13
#define DEST_PM_PIN 14
#define PRES_AM_PIN 32
#define PRES_PM_PIN 27
#define LAST_AM_PIN 2
#define LAST_PM_PIN 4
#define I2C_SDA_2 25
#define I2C_SCL_2 26

// --- HARDWARE CONFIG ---
#define ENABLE_HARDWARE 1

// --- ENUMS & DATA STRUCTURES ---

struct MarqueeData {
  std::string month;
  std::string day;
  std::string year;
  std::string time;
};

enum AnimationPhase {
  ANIM_INACTIVE,
  ANIM_POWER_UP,
  ANIM_FLICKER,
  ANIM_TIME_ACCELERATION,
  ANIM_ARRIVAL,
  ANIM_LANDING
};

// --- GLOBAL ENUMS & STRUCTS ---
// Defines the possible states of the boot-up animation
// In HardwareControl.h

// In HardwareControl.h

enum BootSequenceState {
  BOOT_INACTIVE,
  BOOT_START,
  BOOT_CHARGE_UP,
  BOOT_SPARKLE,
  BOOT_SCROLL_TEXT,
  BOOT_SPEEDOMETER,
  BOOT_FLASH,
  BOOT_REVEAL_INFO,
  BOOT_FINISHED,
  BOOT_COMPLETE
};
enum MarqueeState { M_IDLE, M_PAUSED, M_SCROLLING };
enum MalfunctionPhase { MAL_INACTIVE, MAL_HAYWIRE, MAL_ERROR_MESSAGE, MAL_REBOOT };

struct FetchDataParams {
    int pointIndex;
    int totalRequests;
};

enum Theme {
  THEME_TIME_CIRCUITS, THEME_OUTATIME, THEME_88MPH,
  THEME_PLUTONIUM_GLOW, THEME_MR_FUSION, THEME_CLOCK_TOWER
};

enum DataSourceType { DATA_SOURCE_API, DATA_SOURCE_MQTT, DATA_SOURCE_HA };
enum DisplayMode { FOUR_COLUMN, SCROLLING_TEXT };
enum HttpMethod { METHOD_GET, METHOD_POST };

struct DataPoint {
  std::string url;
  std::string monthPath;
  std::string dayPath;
  std::string yearPath;
  std::string timePath;
  std::string prefix;
  std::string suffix;
  std::string icon;
  int scrollSpeed;
  DataSourceType dataSourceType;
  std::string mqttTopic;
  std::string yearPrefix;
  std::string yearSuffix;
  DisplayMode displayMode;
  std::string scrollingText;
  std::string authHeaderKey;
  std::string authHeaderValue;
  HttpMethod httpMethod;
  std::string requestBody;
  std::string apiExampleKey;
};

struct ClockSettings {
  int destinationYear;
  int destinationTimezoneIndex;
  int departureHour, departureMinute, arrivalHour, arrivalMinute;
  int lastTimeDepartedYear, lastTimeDepartedMonth, lastTimeDepartedDay;
  int lastTimeDepartedHour, lastTimeDepartedMinute;
  byte brightness;
  byte notificationVolume;
  bool timeTravelSoundToggle;
  int presentTimezoneIndex;
  int presetCycleInterval;
  bool displayFormat24h;
  int theme;
  int timeTravelAnimationInterval;
  int timeTravelAnimationDuration;
  int animationStyle;
  int glitchEffectFrequency;
  int malfunctionFrequency;
  bool timeTravelVolumeFade;
  bool dataLinkEnabled;
  int dataLinkTargetRow;
  int dataLinkRefreshInterval;
  int numDataPoints;
  DataPoint dataPoints[5];
  std::string mqttBroker;
  int mqttPort;
  std::string mqttUser;
  std::string mqttPassword;
  bool weatherModeEnabled;
  float latitude;
  float longitude;
  std::string cityName;
  bool useMetricUnits;
  bool stockTickerModeEnabled;
  std::string stockRow1_symbol;
  std::string stockRow2_symbol;
  std::string stockRow3_symbol;
  std::string alphaVantageApiKey;
};

struct StockData {
  std::string symbol;
  std::string price;
  std::string change_percent;
  bool dataValid = false;
};

struct WeatherData {
  float temperature;
  float apparentTemperature;
  float windSpeed;
  int humidity;
  int weatherCode;
  float dailyHigh;
  float dailyLow;
  float hourlyTemp[3];
  int hourlyCode[3];
  float tomorrowHigh;
  float tomorrowLow;
  int tomorrowWeatherCode;
  int precipitationProbability;
  float maxWindSpeed;
  time_t sunrise;
  time_t sunset;
  bool dataValid = false;
};

struct DisplayRow {
    Adafruit_AlphaNum4 month; Adafruit_AlphaNum4 day;
    Adafruit_AlphaNum4 year; Adafruit_AlphaNum4 time;
};

struct TimeZoneEntry {
  const char* tzString; const char* displayName;
  const char* ianaTzName; const char* region;
};

enum AnimationStyle {
  ANIMATION_SEQUENTIAL_FLICKER, ANIMATION_RANDOM_FLICKER,
  ANIMATION_ALL_DISPLAYS_RANDOM, ANIMATION_COUNTING_UP, ANIMATION_WAVE_FLICKER,
  ANIMATION_TORNADO_FLICKER, ANIMATION_CAPACITOR_CHARGE_UP, ANIMATION_DIGITAL_RAIN,
  ANIMATION_WAVEFORM_COLLAPSE, ANIMATION_TIMELINE_SKIM
};

// --- START: SEQUENCER DEFINITIONS ---
enum SequenceCommandType {
    SEQ_CMD_TEXT,
    SEQ_CMD_FLASH,
    SEQ_CMD_WAIT,
    SEQ_CMD_SOUND,
    SEQ_CMD_END
};

struct SequenceStep {
    SequenceCommandType command;
    int targetRow;
    int targetSegment;
    int intParam;
    std::string stringParam;
};
// --- END: SEQUENCER DEFINITIONS ---


#if ENABLE_HARDWARE
extern TwoWire I2C_1; extern TwoWire I2C_2;
extern DisplayRow destRow, presRow, lastRow;
#endif

void setupPhysicalDisplay();
void updateDisplayRow(DisplayRow& row, const struct tm& timeinfo, int year);
void updateDisplaySegment(Adafruit_AlphaNum4& display, const struct tm& timeinfo, int year, int segment);
void animateDisplayRowRandomly(DisplayRow& row);
void animateAllRowsTimelineSkim(unsigned long elapsed, int duration, int destinationYear);
void animateTornadoFlicker();
void animateCapacitorChargeUp(unsigned long elapsed, int duration);
void animateDigitalRain(unsigned long elapsed, int duration);
void animateWaveformCollapse(unsigned long elapsed, int duration);
void animateTimelineSkim(unsigned long elapsed, int duration, int destinationYear);
void blankAllDisplays();
void drawIcon(Adafruit_AlphaNum4& display, const char* iconName);
void playSound(const char* filepath);
void printToDisplay(Adafruit_AlphaNum4 &display, const char* text, int justification = 0);
void displaySpeed(int speed);
void flashAllDisplays();
void animateTemporalLockOn(DisplayRow& row, const struct tm& timeinfo, int year);


#endif // HARDWARE_CONTROL_H