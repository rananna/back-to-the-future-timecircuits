#ifndef HARDWARE_CONTROL_H
#define HARDWARE_CONTROL_H

#include "esp_log.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>
#include <TM1637Display.h>
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
#define SOUND_CONFIRM_OFF "CONFIRM_OFF"
#define SOUND_ACCELERATION "ACCELERATION"
#define SOUND_WARP_WHOOSH "WARP_WHOOSH"
#define SOUND_ARRIVAL_THUD "ARRIVAL_THUD"
#define SOUND_NOT_FOUND "NOT_FOUND" // New macro for the fallback sound
// --------------------------------------------------------

// Structs are fully defined here so both files can access them
struct ClockSettings {
  int destinationYear;
  int destinationTimezoneIndex;
  int departureHour, departureMinute;
  int arrivalHour, arrivalMinute;
  int lastTimeDepartedHour, lastTimeDepartedMinute, lastTimeDepartedYear, lastTimeDepartedMonth, lastTimeDepartedDay;
  int brightness;
  int notificationVolume;
  bool timeTravelSoundToggle;
  bool greatScottSoundToggle;
  int timeTravelAnimationInterval;
  int presetCycleInterval;
  bool displayFormat24h;
  int theme;
  int presentTimezoneIndex;
  int timeTravelAnimationDuration;
  int animationStyle;
  bool timeTravelVolumeFade; // NEW SETTING
};
struct DisplayRow {
  Adafruit_7segment ht16k33;
  TM1637Display day;
  TM1637Display year;
  TM1637Display time;
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
extern DFRobotDFPlayerMini myDFPlayer; extern HardwareSerial dfpSerial;
extern std::map<String, int> soundFiles; // Changed from vector to map
extern const bool ENABLE_HARDWARE;
extern const bool ENABLE_I2C_HARDWARE;
extern const TimeZoneEntry TZ_DATA[];
extern const int NUM_TIMEZONE_OPTIONS;

// Hardware Pin Definitions
#define CLK_PIN 23
#define I2C_ADDR_DEST 0x70
#define DIO_DEST_DAY 19
#define DIO_DEST_YEAR 18
#define DIO_DEST_TIME 5
#define LED_DEST_AM 17
#define LED_DEST_PM 16
#define I2C_ADDR_PRES 0x71
#define DIO_PRES_DAY 26
#define DIO_PRES_YEAR 25
#define DIO_PRES_TIME 33
#define LED_PRES_AM 32
#define LED_PRES_PM 27
#define I2C_ADDR_LAST 0x72
#define DIO_LAST_DAY 14
#define DIO_LAST_YEAR 12
#define DIO_LAST_TIME 13
#define LED_LAST_AM 2
#define LED_LAST_PM 4

// Corrected pin assignments for DFP
#define DFP_TX_PIN 21
#define DFP_RX_PIN 22
//

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
void playSound(const char *soundName);
void setupSoundFiles();
void validateSoundFiles();

#endif // HARDWARE_CONTROL_H