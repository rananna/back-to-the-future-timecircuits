#include "HardwareControl.h"
#include <HardwareSerial.h>
#include <LittleFS.h>
#include <time.h>
#include <WString.h>
#include <map>

// Create two TwoWire objects for the two I2C buses
TwoWire I2C_Bus_1(0);
TwoWire I2C_Bus_2(1);

// Definitions for global variables
const bool ENABLE_HARDWARE = true;
const bool ENABLE_I2C_HARDWARE = true;

// Initialize all displays
DisplayRow destRow = { Adafruit_7segment(), Adafruit_7segment(), Adafruit_7segment(), Adafruit_7segment(), 13, 14 };
DisplayRow presRow = { Adafruit_7segment(), Adafruit_7segment(), Adafruit_7segment(), Adafruit_7segment(), 32, 27 };
DisplayRow lastRow = { Adafruit_7segment(), Adafruit_7segment(), Adafruit_7segment(), Adafruit_7segment(), 2, 4 };

HardwareSerial dfpSerial(2);
DFRobotDFPlayerMini myDFPlayer;
std::map<String, int> soundFiles;
std::map<String, uint16_t> iconMap;

void setupIconLibrary() {
    iconMap["SUN"]      = 0b0010100101011010;
    iconMap["CLOUD"]    = 0b0000011111111100;
    iconMap["RAIN"]     = 0b0100100000010010;
    iconMap["STORM"]    = 0b0110000110001100;
    iconMap["SNOW"]     = 0b1001010101010010;
    iconMap["WIND"]     = 0b0001100011000110;
    iconMap["UP"]       = 0b0001111000110000;
    iconMap["DOWN"]     = 0b0011000000011110;
    iconMap["WIFI"]     = 0b1010101000000000;
    iconMap["HEART"]    = 0b0001101101101000;
    iconMap["MONEY"]    = 0b0110110110110110; // Dollar Sign
    iconMap["BTC"]      = 0b0110000111110001; // Bitcoin B
    iconMap["STOCK"]    = 0b1000010010010001; // Stock Chart
    iconMap["ALERT"]    = 0b0000000000110001; // Exclamation Mark
    iconMap["NONE"]     = 0x0000;
}

void drawIcon(Adafruit_7segment &disp, const char* iconName) {
#if ENABLE_HARDWARE
  String name = String(iconName);
  name.toUpperCase();
  uint16_t bitmap = iconMap.count(name) ? iconMap[name] : 0x0000;
  disp.clear();
  disp.displaybuffer[0] = bitmap;
  disp.writeDisplay();
#endif
}

void setupPhysicalDisplay() {
#if ENABLE_HARDWARE
  if (ENABLE_I2C_HARDWARE) {
    if (!I2C_Bus_1.begin(I2C_SDA_1, I2C_SCL_1, 100000)) { // 100kHz
        ESP_LOGE("Display", "CRITICAL ERROR: Failed to initialize I2C Bus 1.");
    }
    if (!I2C_Bus_2.begin(I2C_SDA_2, I2C_SCL_2, 100000)) { // 100kHz
        ESP_LOGE("Display", "CRITICAL ERROR: Failed to initialize I2C Bus 2.");
    }
    destRow.month.begin(ADDR_DEST_MONTH, &I2C_Bus_1);
    destRow.day.begin(ADDR_DEST_DAY, &I2C_Bus_1);
    destRow.year.begin(ADDR_DEST_YEAR, &I2C_Bus_1);
    destRow.time.begin(ADDR_DEST_TIME, &I2C_Bus_1);
    presRow.month.begin(ADDR_PRES_MONTH, &I2C_Bus_1);
    presRow.day.begin(ADDR_PRES_DAY, &I2C_Bus_1);
    presRow.year.begin(ADDR_PRES_YEAR, &I2C_Bus_1);
    presRow.time.begin(ADDR_PRES_TIME, &I2C_Bus_1);
    lastRow.month.begin(ADDR_LAST_MONTH, &I2C_Bus_2);
    lastRow.day.begin(ADDR_LAST_DAY, &I2C_Bus_2);
    lastRow.year.begin(ADDR_LAST_YEAR, &I2C_Bus_2);
    lastRow.time.begin(ADDR_LAST_TIME, &I2C_Bus_2);
  }
  DisplayRow *rows[] = { &destRow, &presRow, &lastRow };
  for (auto &row : rows) {
    pinMode(row->amPin, OUTPUT);
    pinMode(row->pmPin, OUTPUT);
  }
  setupIconLibrary();
#endif
}

void setDisplayBrightness(byte intensity) {
#if ENABLE_HARDWARE
  if (intensity > 15) intensity = 15;
  currentSettings.brightness = intensity;
  DisplayRow *rows[] = { &destRow, &presRow, &lastRow };
  for (auto &row : rows) {
    row->month.setBrightness(intensity);
    row->day.setBrightness(intensity);
    row->year.setBrightness(intensity);
    row->time.setBrightness(intensity);
  }
#endif
}

void clearDisplayRow(DisplayRow &row) {
#if ENABLE_HARDWARE
  row.month.clear(); row.month.writeDisplay();
  row.day.clear(); row.day.writeDisplay();
  row.year.clear(); row.year.writeDisplay();
  row.time.clear(); row.time.writeDisplay();
  digitalWrite(row.amPin, LOW);
  digitalWrite(row.pmPin, LOW);
#endif
}

void blankAllDisplays() {
#if ENABLE_HARDWARE
  clearDisplayRow(destRow);
  clearDisplayRow(presRow);
  clearDisplayRow(lastRow);
#endif
}

void updateDisplayRow(DisplayRow &row, struct tm &timeinfo, int year) {
#if ENABLE_HARDWARE
  char monthStr[4];
  strftime(monthStr, sizeof(monthStr), "%b", &timeinfo);
  for (int i = 0; i < 3; i++) monthStr[i] = toupper(monthStr[i]);
  
  row.month.clear();
  row.month.print(monthStr);
  row.month.writeDisplay();

  row.day.clear();
  row.day.print(timeinfo.tm_mday);
  row.day.writeDisplay();

  row.year.clear();
  row.year.print(year);
  row.year.writeDisplay();

  int hour = timeinfo.tm_hour;
  if (!currentSettings.displayFormat24h) {
    if (hour == 0) hour = 12;
    else if (hour > 12) hour -= 12;
  }
  row.time.clear();
  row.time.print(hour * 100 + timeinfo.tm_min);
  row.time.drawColon(true);
  row.time.writeDisplay();

  digitalWrite(row.amPin, (timeinfo.tm_hour < 12));
  digitalWrite(row.pmPin, (timeinfo.tm_hour >= 12));
#endif
}

void animateDisplayRowRandomly(DisplayRow &row) {
#if ENABLE_HARDWARE
  row.month.print(random(1000, 9999)); row.month.writeDisplay();
  row.day.print(random(10, 99)); row.day.writeDisplay();
  row.year.print(random(1000, 9999)); row.year.writeDisplay();
  row.time.print(random(100, 2359)); row.time.drawColon(random(0,2)); row.time.writeDisplay();
  digitalWrite(row.amPin, random(0, 2));
  digitalWrite(row.pmPin, random(0, 2));
#endif
}

void playSound(const char *soundName) {
#if ENABLE_HARDWARE
  String nameStr = String(soundName);
  nameStr.toUpperCase();

  if (soundFiles.count(nameStr)) {
    myDFPlayer.play(soundFiles[nameStr]);
    ESP_LOGI("Sound", "Playing sound: %s (File #%d)", nameStr.c_str(), soundFiles[nameStr]);
  } else {
    ESP_LOGW("Sound", "Sound file '%s.mp3' not found.", soundName);
    if (soundFiles.count("NOT_FOUND")) {
      myDFPlayer.play(soundFiles["NOT_FOUND"]);
    }
  }
#endif
}

void setupSoundFiles() {
#if ENABLE_HARDWARE
  soundFiles.clear();
  File root = LittleFS.open("/mp3");
  if (!root || !root.isDirectory()) {
    ESP_LOGE("Sound", "Failed to open /mp3 directory");
    return;
  }
  File file = root.openNextFile();
  int fileIndex = 1;
  while (file) {
    if (!file.isDirectory() && String(file.name()).endsWith(".mp3")) {
      String fileName = String(file.name());
      String descriptiveName = fileName.substring(0, fileName.lastIndexOf("."));
      descriptiveName.toUpperCase();
      soundFiles[descriptiveName] = fileIndex;
      ESP_LOGI("Sound", "Found sound: %s as file #%d", descriptiveName.c_str(), fileIndex);
      fileIndex++;
    }
    file = root.openNextFile();
  }
#endif
}

void animateMonthDisplay(DisplayRow &row) {
#if ENABLE_HARDWARE
  row.month.print("----");
  row.month.writeDisplay();
#endif
}

void animateDayDisplay(DisplayRow &row) {
#if ENABLE_HARDWARE
  row.day.print("----");
  row.day.writeDisplay();
#endif
}

void animateYearDisplay(DisplayRow &row) {
#if ENABLE_HARDWARE
  row.year.print("----");
  row.year.writeDisplay();
#endif
}

void animateTimeDisplay(DisplayRow &row) {
#if ENABLE_HARDWARE
  row.time.print("----");
  row.time.drawColon(false);
  row.time.writeDisplay();
#endif
}

void animateAmPmDisplay(DisplayRow &row) {
#if ENABLE_HARDWARE
  digitalWrite(row.amPin, LOW);
  digitalWrite(row.pmPin, LOW);
#endif
}

void display88MphSpeed(float currentSpeed) {
#if ENABLE_HARDWARE
  destRow.year.print("88");
  destRow.time.print("88");
  destRow.year.writeDisplay();
  destRow.time.writeDisplay();
#endif
}

void displayWindSpeed(float currentSpeed) {
#if ENABLE_HARDWARE
    lastRow.month.clear();
    lastRow.day.clear();
    lastRow.year.clear();
    lastRow.time.clear();

    lastRow.month.print("WIND");

    char speedStr[5];
    dtostrf(currentSpeed, 4, 1, speedStr);
    lastRow.day.print(speedStr);

    lastRow.year.print("MPH");

    lastRow.month.writeDisplay();
    lastRow.day.writeDisplay();
    lastRow.year.writeDisplay();
#endif
}