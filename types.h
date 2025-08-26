#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>
#include <vector>
#include <Adafruit_LEDBackpack.h>
#include <string>

enum LogLevel { LOG_INFO, LOG_WARN, LOG_ERROR, LOG_CRITICAL };
enum DisplayMode {
  DM_NORMAL,
  DM_SCROLLING_TEXT,
  DM_STATIC_PREFIX_SCROLLING_SUFFIX,
  DM_STATIC_TEXT
};
enum DataSourceType { DST_HOME_ASSISTANT, DST_MQTT, DST_API };

struct DataPoint {
  String url;
  String monthPath;
  String dayPath;
  String yearPath;
  String timePath;
  String prefix;
  String suffix;
  String icon;
  int scrollSpeed;
  DataSourceType dataSourceType;
  String mqttTopic;
  String yearPrefix;
  String yearSuffix;
  DisplayMode displayMode;
  String scrollingText;
  String authHeaderKey;
  String authHeaderValue;
  String apiExampleKey;
};

struct ClockSettings {
  int destinationYear;
  int destinationTimezoneIndex;
  int departureHour;
  int departureMinute;
  int arrivalHour;
  int arrivalMinute;
  int lastTimeDepartedYear;
  int lastTimeDepartedMonth;
  int lastTimeDepartedDay;
  int lastTimeDepartedHour;
  int lastTimeDepartedMinute;
  uint8_t brightness;
  uint8_t notificationVolume;
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
  int dataLinkRefreshInterval;
  int numDataPoints;
  String mqttBroker;
  int mqttPort;
  String mqttUser;
  String mqttPassword;
  bool weatherModeEnabled;
  String cityName;
  bool useMetricUnits;
  float latitude;
  float longitude;
  bool stockTickerModeEnabled;
  String stockRow1_symbol;
  String stockRow2_symbol;
  String stockRow3_symbol;
  String alphaVantageApiKey;
  bool loggingEnabled;
  DataPoint dataPoints[5];
};

struct TimeZoneEntry {
  const char* name;
  const char* tzString;
  const char* ianaTzName;
  const char* region;
};

struct StockData {
  String symbol;
  String price;
  String change;
  String change_percent;
  bool dataValid;
};

struct WeatherData {
  float temp;
  float feels_like;
  float wind_speed;
  int humidity;
  int icon;
  float temp_max;
  float temp_min;
  time_t sunrise;
  time_t sunset;
  int precip_chance;
  float wind_gust;
  float tomorrow_temp_max;
  float tomorrow_temp_min;
  int tomorrow_icon;
  float hourly_temp[12];
  int hourly_icon[12];
  bool dataValid;
};

struct SequenceStep {
  int command;
  int targetRow;
  int targetSegment;
  String stringParam;
  int intParam;
};

struct MarqueeData {
  String month;
  String day;
  String year;
  String time;
  String scrollingText;
  int scrollSpeed;
};

struct ApiTestParams {
  String url;
  String authKey;
  String authValue;
  uint32_t clientId;
  String action;
  int rowIndex;
};

enum AnimationPhase {
  ANIM_INACTIVE,
  ANIM_POWER_UP,
  ANIM_TIME_ACCELERATION,
  ANIM_ARRIVAL,
  ANIM_LANDING
};

enum BootSequenceState {
  BOOT_INACTIVE,
  BOOT_START,
  BOOT_CHARGE_UP,
  BOOT_COMPLETE
};

enum MarqueeState { M_IDLE, M_SCROLLING, M_PAUSED };

enum MalfunctionPhase {
  MAL_INACTIVE,
  MAL_START,
  MAL_HAYWIRE,
  MAL_ERROR_MESSAGE,
  MAL_REBOOT
};

struct DisplayRow {
    Adafruit_AlphaNum4 month;
    Adafruit_AlphaNum4 day;
    Adafruit_AlphaNum4 year;
    Adafruit_AlphaNum4 time;
};

struct WeatherTaskParams {
    std::string cityName;
    bool forceGeocode;
};

struct FetchDataParams {
    int pointIndex;
    int unused;
};

#define MAX_FETCH_FAILURES 3

#endif  // TYPES_H