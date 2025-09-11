#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <string>
#include <Preferences.h>
#include "HardwareControl.h" // For TimeZoneEntry, remove this later if we move it

// --- SETTINGS-RELATED ENUMS & STRUCTS ---
// Moved from HardwareControl.h to centralize settings definitions.

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
  std::string financialModelingPrepApiKey;
};

enum AnimationStyle {
  ANIMATION_SEQUENTIAL_FLICKER, ANIMATION_RANDOM_FLICKER,
  ANIMATION_ALL_DISPLAYS_RANDOM, ANIMATION_COUNTING_UP, ANIMATION_WAVE_FLICKER,
  ANIMATION_TORNADO_FLICKER, ANIMATION_CAPACITOR_CHARGE_UP, ANIMATION_DIGITAL_RAIN,
  ANIMATION_WAVEFORM_COLLAPSE, ANIMATION_TIMELINE_SKIM, ANIMATION_TEMPORAL_DESYNC, ANIMATION_RANDOM_ALL
};


// --- SettingsManager Class Definition ---

class SettingsManager {
public:
    SettingsManager();

    // The main settings object, accessible publicly.
    ClockSettings settings;

    // Loads settings from NVS, populates the settings object, and applies the timezone.
    void load();

    // Saves the current settings object to NVS.
    void save();

private:
    // A handle to the Preferences library.
    Preferences preferences;

    // Initializes the settings object with default values.
    void initializeDefaultSettings();
};

#endif // SETTINGS_MANAGER_H
