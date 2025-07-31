#include "HardwareControl.h"
#include <HardwareSerial.h>
#include <LittleFS.h>
#include <time.h>
#include <WString.h>
#include <map>

// Definitions for global variables
const bool ENABLE_HARDWARE = false;
const bool ENABLE_I2C_HARDWARE = false;
DisplayRow destRow = { Adafruit_7segment(), TM1637Display(CLK_PIN, DIO_DEST_DAY), TM1637Display(CLK_PIN, DIO_DEST_YEAR), TM1637Display(CLK_PIN, DIO_DEST_TIME), LED_DEST_AM, LED_DEST_PM };
DisplayRow presRow = { Adafruit_7segment(), TM1637Display(CLK_PIN, DIO_PRES_DAY), TM1637Display(CLK_PIN, DIO_PRES_YEAR), TM1637Display(CLK_PIN, DIO_PRES_TIME), LED_PRES_AM, LED_PRES_PM };
DisplayRow lastRow = { Adafruit_7segment(), TM1637Display(CLK_PIN, DIO_LAST_DAY), TM1637Display(CLK_PIN, DIO_LAST_YEAR), TM1637Display(CLK_PIN, DIO_LAST_TIME), LED_LAST_AM, LED_LAST_PM };
HardwareSerial dfpSerial(2); DFRobotDFPlayerMini myDFPlayer;
std::map<String, int> soundFiles; // Changed from vector to map

// Function implementations
void setupPhysicalDisplay() {
  if (!ENABLE_HARDWARE) {
    ESP_LOGI("Display (Disabled)", "Physical displays setup skipped.");
    return;
  }
  if (ENABLE_I2C_HARDWARE) {
    Wire.begin();
    destRow.ht16k33.begin(I2C_ADDR_DEST);
    presRow.ht16k33.begin(I2C_ADDR_PRES);
    lastRow.ht16k33.begin(I2C_ADDR_LAST);
    ESP_LOGI("Display", "I2C displays initialized.");
  } else {
    ESP_LOGI("Display (Disabled)", "I2C displays initialization skipped.");
  }
  DisplayRow *rows[] = { &destRow, &presRow, &lastRow };
  for (auto &row : rows) {
    if (ENABLE_I2C_HARDWARE) {
      row->ht16k33.setBrightness(currentSettings.brightness);
    }
    row->day.setBrightness(currentSettings.brightness);
    row->year.setBrightness(currentSettings.brightness);
    row->time.setBrightness(currentSettings.brightness);
    pinMode(row->amPin, OUTPUT);
    pinMode(row->pmPin, OUTPUT);
  }
  ESP_LOGI("Display", "TM1637 displays and LEDs initialized.");
}
void setDisplayBrightness(byte intensity) {
  if (!ENABLE_HARDWARE) {
    ESP_LOGD("Display (Disabled)", "setDisplayBrightness skipped. Desired: %d", intensity);
    return;
  }
  if (intensity > 7) intensity = 7;
  currentSettings.brightness = intensity;
  DisplayRow *rows[] = { &destRow, &presRow, &lastRow };
  for (auto &row : rows) {
    if (ENABLE_I2C_HARDWARE) {
      row->ht16k33.setBrightness(intensity);
    }
    row->day.setBrightness(intensity);
    row->year.setBrightness(intensity);
    row->time.setBrightness(intensity);
  }
}
void clearDisplayRow(DisplayRow &row) {
  if (!ENABLE_HARDWARE) {
    ESP_LOGD("Display (Disabled)", "clearDisplayRow skipped for row (HT16K33 ID: [unknown])");
    return;
  }
  if (ENABLE_I2C_HARDWARE) {
    row.ht16k33.clear();
    row.ht16k33.writeDisplay();
  }
  row.day.clear();
  row.year.clear();
  row.time.clear();
  digitalWrite(row.amPin, LOW);
  digitalWrite(row.pmPin, LOW);
}
void blankAllDisplays() {
  if (!ENABLE_HARDWARE) {
    ESP_LOGI("Display (Disabled)", "blankAllDisplays skipped.");
    return;
  }
  clearDisplayRow(destRow);
  clearDisplayRow(presRow);
  clearDisplayRow(lastRow);
}
void updateDisplayRow(DisplayRow &row, struct tm &timeinfo, int year) {
  if (!ENABLE_HARDWARE) {
    ESP_LOGD("Display (Disabled)", "updateDisplayRow skipped for row (HT16K33 ID: [unknown])");
    return;
  }
  char monthStr[4];
  strftime(monthStr, sizeof(monthStr), "%b", &timeinfo);
  for (int i = 0; i < 3; i++) monthStr[i] = toupper(monthStr[i]);
  if (ENABLE_I2C_HARDWARE) {
    row.ht16k33.clear();
    row.ht16k33.print(" ");
    row.ht16k33.writeDisplay();
  }
  row.day.showNumberDec(timeinfo.tm_mday, false, 2, 0);
  row.year.showNumberDec(year, false, 4, 0);
  int hour = timeinfo.tm_hour;
  if (!currentSettings.displayFormat24h) {
    if (hour == 0) hour = 12;
    else if (hour > 12) hour -= 12;
  }
  uint16_t timeData = (hour * 100) + timeinfo.tm_min;
  row.time.showNumberDecEx(timeData, 0b01000000, true);
  digitalWrite(row.amPin, (timeinfo.tm_hour < 12));
  digitalWrite(row.pmPin, (timeinfo.tm_hour >= 12));
}
void animateMonthDisplay(DisplayRow &row) {
  if (!ENABLE_HARDWARE) {
    ESP_LOGD("Display (Disabled)", "animateMonthDisplay skipped (HT16K33 ID: [unknown])");
    return;
  }
  if (ENABLE_I2C_HARDWARE) {
    row.ht16k33.clear();
    row.ht16k33.print(" ---");
    row.ht16k33.writeDisplay();
  }
}
void animateDayDisplay(DisplayRow &row) {
  if (!ENABLE_HARDWARE) {
    ESP_LOGD("Display (Disabled)", "animateDayDisplay skipped (TM1637 DIO: [unknown])");
    return;
  }
  row.day.showNumberDec(random(1, 32));
}
void animateYearDisplay(DisplayRow &row) {
  if (!ENABLE_HARDWARE) {
    ESP_LOGD("Display (Disabled)", "animateYearDisplay skipped (TM1637 DIO: [unknown])");
    return;
  }
  row.year.showNumberDec(random(1000, 10000));
}
void animateTimeDisplay(DisplayRow &row) {
  if (!ENABLE_HARDWARE) {
    ESP_LOGD("Display (Disabled)", "animateTimeDisplay skipped (TM1637 DIO: [unknown])");
    return;
  }
  row.time.showNumberDec(random(0, 2400), true);
}
void animateAmPmDisplay(DisplayRow &row) {
  if (!ENABLE_HARDWARE) {
    ESP_LOGD("Display (Disabled)", "animateAmPmDisplay skipped (AM Pin: %d)", row.amPin);
    return;
  }
  digitalWrite(row.amPin, random(0, 2));
  digitalWrite(row.pmPin, random(0, 2));
}
void display88MphSpeed(float currentSpeed) {
    if (!ENABLE_HARDWARE) {
      ESP_LOGI("Display (Disabled)", "display88MphSpeed skipped.");
      return;
    }
    clearDisplayRow(destRow);
    clearDisplayRow(presRow);
    if (ENABLE_I2C_HARDWARE) {
      lastRow.ht16k33.clear();
      lastRow.ht16k33.print("MPH");
      lastRow.ht16k33.writeDisplay();
    }
    int speedInt = (int)currentSpeed;
    if (speedInt < 10) {
        lastRow.time.showNumberDec(speedInt, false, 1, 3);
    } else if (speedInt <= 99) {
        lastRow.time.showNumberDec(speedInt, false, 2, 2);
    } else {
        lastRow.time.showNumberDec(speedInt, false, 4, 0);
    }
    digitalWrite(lastRow.amPin, LOW);
    digitalWrite(lastRow.pmPin, LOW);
}
void playSound(const char *soundName) {
  if (!ENABLE_HARDWARE) {
    ESP_LOGI("Sound (Disabled)", "Attempted to play sound: %s", soundName);
    return;
  }
  
  auto it = soundFiles.find(soundName);
  if (it != soundFiles.end()) {
    myDFPlayer.playMp3Folder(it->second);
    ESP_LOGI("Sound", "Playing sound: %s (Index: %d)", soundName, it->second);
  } else {
    ESP_LOGW("Sound", "WARNING: Sound file '%s' not found. Playing fallback sound.", soundName);
    auto fallbackIt = soundFiles.find(SOUND_NOT_FOUND); // Check for the dedicated fallback sound
    if (fallbackIt != soundFiles.end()) {
      myDFPlayer.playMp3Folder(fallbackIt->second);
      ESP_LOGW("Sound", "Playing fallback sound: %s", SOUND_NOT_FOUND);
    } else {
      ESP_LOGE("Sound", "Fallback sound '%s' not found either. No sound played.", SOUND_NOT_FOUND);
    }
  }
}

// New function to dynamically load sound files from LittleFS
void setupSoundFiles() {
  soundFiles.clear();
  ESP_LOGI("Sound", "Scanning for sound files in /mp3...");
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
      ESP_LOGI("Sound", "Found sound file: %s (Mapped to index: %d)", descriptiveName.c_str(), fileIndex);
      fileIndex++;
    }
    file = root.openNextFile();
  }
}

void validateSoundFiles() {
    // This function is no longer needed with the new map-based approach
    // as sound lookup and validation happens directly in playSound
    // and missing files are handled by the fallback.
}