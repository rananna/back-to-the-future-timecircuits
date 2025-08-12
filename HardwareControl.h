#ifndef HARDWARE_CONTROL_H
#define HARDWARE_CONTROL_H

#include <Wire.h>
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"
#include "DFRobotDFPlayerMini.h"
#include <time.h>

// --- PIN DEFINITIONS ---
#define I2C_SDA_1 21
#define I2C_SCL_1 22
#define I2C_SDA_2 25
#define I2C_SCL_2 26

#define DFP_RX_PIN 16
#define DFP_TX_PIN 17

#define DEST_AM_PIN 13
#define DEST_PM_PIN 14
#define PRES_AM_PIN 32
#define PRES_PM_PIN 27
#define LAST_AM_PIN 2
#define LAST_PM_PIN 4

// --- HARDWARE CONFIG ---
#define ENABLE_HARDWARE 1

// --- ENUMS & DATA STRUCTURES ---
enum Theme {
  THEME_TIME_CIRCUITS,
  THEME_OUTATIME,
  THEME_88MPH,
  THEME_PLUTONIUM_GLOW,
  THEME_MR_FUSION,
  THEME_CLOCK_TOWER
};

struct DataPoint {
  char url[256];
  char label[5];
  char jsonPath[128];
  char prefix[16];
  char suffix[16];
  char format[64];
  char icon[16];
  int scrollSpeed;
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
  float latitude;
  float longitude;
  bool dataLinkEnabled;
  int dataLinkTargetRow;
  int dataLinkRefreshInterval;
  int numDataPoints;
  DataPoint dataPoints[5];
};

struct DisplayRow {
    Adafruit_AlphaNum4 month;
    Adafruit_AlphaNum4 day;
    Adafruit_AlphaNum4 year;
    Adafruit_AlphaNum4 time;
};

struct TimeZoneEntry {
  const char* tzString;
  const char* displayName;
  const char* ianaTzName;
  const char* region;
};

enum AnimationStyle {
  ANIMATION_SEQUENTIAL_FLICKER,
  ANIMATION_RANDOM_FLICKER,
  ANIMATION_ALL_DISPLAYS_RANDOM,
  ANIMATION_COUNTING_UP,
  ANIMATION_WAVE_FLICKER
};

// --- GLOBAL HARDWARE OBJECTS (DECLARED EXTERN) ---
#if ENABLE_HARDWARE
extern TwoWire I2C_1;
extern TwoWire I2C_2;
extern DisplayRow destRow, presRow, lastRow;
extern HardwareSerial dfpSerial;
extern DFRobotDFPlayerMini myDFPlayer;
#endif

// --- FUNCTION PROTOTYPES ---
void setupPhysicalDisplay();
void updateDisplayRow(DisplayRow& row, const struct tm& timeinfo, int year);
void animateDisplayRowRandomly(DisplayRow& row);
void blankAllDisplays();
void display88MphSpeed(float speed);
void drawIcon(Adafruit_AlphaNum4& display, const char* iconName);
void playSound(const char* soundName);
void setupSoundFiles();
void printToDisplay(Adafruit_AlphaNum4 &display, const char* text);

#endif // HARDWARE_CONTROL_H