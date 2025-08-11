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

#define THEME_TIME_CIRCUITS 0
#define ANIMATION_SEQUENTIAL_FLICKER 0

#define SOUND_TIME_TRAVEL "TIME_TRAVEL"
#define SOUND_EASTER_EGG "EASTER_EGG"
#define SOUND_SLEEP_ON "SLEEP_ON"
#define SOUND_CONFIRM_ON "CONFIRM_ON"
#define SOUND_ACCELERATION "ACCELERATION"
#define SOUND_WARP_WHOOSH "WARP_WHOOSH"
#define SOUND_ARRIVAL_THUD "ARRIVAL_THUD"
#define SOUND_NOT_FOUND "NOT_FOUND"

struct DataPoint {
  char url[256];
  char label[5];
  char jsonPath[128];
  char format[32];
  char icon[16];
  int transitionEffect;
  int scrollSpeed;
  int textAlign;
  bool isLiveData;
  char liveDataTag[16];
};

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
  int malfunctionFrequency;
  bool timeTravelVolumeFade;
  bool windSpeedModeEnabled;
  float longitude;
  float latitude;
  char openWeatherMapApiKey[64];
  char alphaVantageApiKey[64];
  char youtubeApiKey[64];
  bool dataLinkEnabled;
  int dataLinkTargetRow;
  int dataLinkRefreshInterval;
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

#define I2C_SDA_1 21
#define I2C_SCL_1 22
#define I2C_SDA_2 25
#define I2C_SCL_2 26

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

#define DFP_TX_PIN 17
#define DFP_RX_PIN 16

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
void runBootSequence();

#endif // HARDWARE_CONTROL_H