#ifndef HARDWARE_CONTROL_H
#define HARDWARE_CONTROL_H

#include "esp_log.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>
#include <DFRobotDFPlayerMini.h>
#include <LittleFS.h>
#include <vector>
#include <algorithm>
#include <time.h>
#include <map>

// --- ADDED CONSTANT DEFINITIONS TO FIX COMPILE ERRORS ---
#define THEME_TIME_CIRCUITS 0
#define ANIMATION_SEQUENTIAL_FLICKER 0

#define SOUND_TIME_TRAVEL "TIME_TRAVEL"
#define SOUND_EASTER_EGG "EASTER_EGG"
#define SOUND_SLEEP_ON "SLEEP_ON"
#define SOUND_CONFIRM_ON "CONFIRM_ON"
#define SOUND_ACCELERATION "ACCELERATION"
#define SOUND_WARP_WHOOSH "WARP_WHOOSH"
#define SOUND_ARRIVAL_THUD "ARRIVAL_THUD"
#define SOUND_NOT_FOUND "NOT_FOUND" // New macro for the fallback sound
// --------------------------------------------------------

// --- UPDATED: Marquee Data Point Structure ---
struct DataPoint {
  // Core Data
  char url[256];
  char label[5];
  char jsonPath[128];
  char format[32];

  // Visuals
  char icon[16];
  int transitionEffect;
  int scrollSpeed;
  int textAlign;

  // Live Data
  bool isLiveData;
  char liveDataTag[16];
};

// Structs are fully defined here so both files can access them
struct ClockSettings {
  int destinationYear;
  int destinationTimezoneIndex;
  int departureHour, departureMinute;
  int arrivalHour, arrivalMinute;
  int lastTimeDepartedHour, lastTimeDepartedMinute, lastTimeDepartedYear, lastTimeDepartedMonth, lastTimeDepartedDay;
  byte brightness;
  int notificationVolume;
  bool timeTravelSoundToggle;
  int timeTravelAnimationInterval;
  int presetCycleInterval;
  bool displayFormat24h;
  int theme;
  int presentTimezoneIndex;
  unsigned long timeTravelAnimationDuration;
  int animationStyle;
  int glitchEffectFrequency;
  int malfunctionFrequency; // <-- ADDED
  bool timeTravelVolumeFade;
  bool windSpeedModeEnabled;
  float longitude;
  float latitude;

  // --- MODIFIED: Added fields for API keys ---
  char openWeatherMapApiKey[64];
  char alphaVantageApiKey[64];
  // ------------------------------------------

  // Global Marquee Settings
  bool dataLinkEnabled;
  int dataLinkTargetRow;
  int dataLinkRefreshInterval;

  // Data Points
  int numDataPoints;
  DataPoint dataPoints[5];
};

struct DisplayRow {
  Adafruit_7segment month;
  Adafruit_7segment day;
  Adafruit_7segment year;
  Adafruit_7segment time;
  const uint8_t amPin;
  const uint8_t pmPin;
};
struct SoundFile {
  String name;
};
struct TimeZoneEntry {
  const char *tzString;
  const char *displayName;
  const char *ianaTzName;
  const char *country;
};

// Global declarations using 'extern'
extern ClockSettings currentSettings;
extern DisplayRow destRow, presRow, lastRow;
extern DFRobotDFPlayerMini myDFPlayer;
extern HardwareSerial dfpSerial;
extern std::map<String, int> soundFiles;
extern const bool ENABLE_HARDWARE;
extern const bool ENABLE_I2C_HARDWARE;
extern const TimeZoneEntry TZ_DATA[];
extern const int NUM_TIMEZONE_OPTIONS;
extern float currentWindSpeed;

// Hardware Pin Definitions for I2C
#define I2C_SDA_1 21
#define I2C_SCL_1 22
#define I2C_SDA_2 25
#define I2C_SCL_2 26

// I2C Addresses for all 12 displays (you must configure these with solder jumpers)
#define ADDR_DEST_MONTH 0x70
#define ADDR_DEST_DAY   0x71
#define ADDR_DEST_YEAR  0x72
#define ADDR_DEST_TIME  0x73

#define ADDR_PRES_MONTH 0x74
#define ADDR_PRES_DAY   0x75
#define ADDR_PRES_YEAR  0x76
#define ADDR_PRES_TIME  0x77

#define ADDR_LAST_MONTH 0x70
#define ADDR_LAST_DAY   0x71
#define ADDR_LAST_YEAR  0x72
#define ADDR_LAST_TIME  0x73

// Corrected pin assignments for DFP
#define DFP_TX_PIN 17
#define DFP_RX_PIN 16

// Function prototypes
void setupPhysicalDisplay();
void setDisplayBrightness(byte intensity);
void clearDisplayRow(DisplayRow &row);
void blankAllDisplays();
void updateDisplayRow(DisplayRow &row, struct tm &timeinfo, int year);
void animateMonthDisplay(DisplayRow &row);
void animateDayDisplay(DisplayRow &row);
void animateYearDisplay(DisplayRow &row);
void animateTimeDisplay(DisplayRow &row);
void animateAmPmDisplay(DisplayRow &row);
void display88MphSpeed(float currentSpeed);
void displayWindSpeed(float currentSpeed);
void playSound(const char *soundName);
void setupSoundFiles();
void drawIcon(Adafruit_7segment &disp, const char* iconName);

// *** FIX: Added prototype for function defined in the .ino file ***
void runBootSequence();

#endif // HARDWARE_CONTROL_H