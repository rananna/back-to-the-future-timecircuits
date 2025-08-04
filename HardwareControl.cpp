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
const bool ENABLE_HARDWARE = false;
const bool ENABLE_I2C_HARDWARE = false;

// Initialize all displays using the new I2C bus and addresses
DisplayRow destRow = {
    Adafruit_7segment(), Adafruit_7segment(), Adafruit_7segment(), Adafruit_7segment(), LED_DEST_AM, LED_DEST_PM
};
DisplayRow presRow = {
    Adafruit_7segment(), Adafruit_7segment(), Adafruit_7segment(), Adafruit_7segment(), LED_PRES_AM, LED_PRES_PM
};
DisplayRow lastRow = {
    Adafruit_7segment(), Adafruit_7segment(), Adafruit_7segment(), Adafruit_7segment(), LED_LAST_AM, LED_LAST_PM
};

HardwareSerial dfpSerial(2); DFRobotDFPlayerMini myDFPlayer;
std::map<String, int> soundFiles;
// Function implementations
void setupPhysicalDisplay() {
  if (!ENABLE_HARDWARE) {
    ESP_LOGI("Display (Disabled)", "Physical displays setup skipped.");
    return;
  }

  if (ENABLE_I2C_HARDWARE) {
    // Initialize both I2C buses with their respective pins
    I2C_Bus_1.begin(I2C_SDA_1, I2C_SCL_1);
    I2C_Bus_2.begin(I2C_SDA_2, I2C_SCL_2);
    
    // Begin all displays on the two buses with their unique addresses
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
    
    ESP_LOGI("Display", "I2C displays initialized on two buses.");
  } else {
    ESP_LOGI("Display (Disabled)", "I2C displays initialization skipped.");
  }

  // Rest of the setup for brightness and LEDs
  DisplayRow *rows[] = { &destRow, &presRow, &lastRow };
  for (auto &row : rows) {
    if (ENABLE_I2C_HARDWARE) {
      row->month.setBrightness(currentSettings.brightness);
      row->day.setBrightness(currentSettings.brightness);
      row->year.setBrightness(currentSettings.brightness);
      row->time.setBrightness(currentSettings.brightness);
    }
    pinMode(row->amPin, OUTPUT);
    pinMode(row->pmPin, OUTPUT);
  }
  ESP_LOGI("Display", "All I2C displays and LEDs initialized.");
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
      row->month.setBrightness(intensity);
      row->day.setBrightness(intensity);
      row->year.setBrightness(intensity);
      row->time.setBrightness(intensity);
    }
  }
}
void clearDisplayRow(DisplayRow &row) {
  if (!ENABLE_HARDWARE) {
    ESP_LOGD("Display (Disabled)", "clearDisplayRow skipped for row (I2C)");
    return;
  }
  if (ENABLE_I2C_HARDWARE) {
    row.month.clear();
    row.month.writeDisplay();
    row.day.clear();
    row.day.writeDisplay();
    row.year.clear();
    row.year.writeDisplay();
    row.time.clear();
    row.time.writeDisplay();
  }
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
    ESP_LOGD("Display (Disabled)", "updateDisplayRow skipped for row (I2C)");
    return;
  }
  char monthStr[4];
  strftime(monthStr, sizeof(monthStr), "%b", &timeinfo);
  for (int i = 0; i < 3; i++) monthStr[i] = toupper(monthStr[i]);
  if (ENABLE_I2C_HARDWARE) {
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
    row.time.writeDisplay();
  }
  digitalWrite(row.amPin, (timeinfo.tm_hour < 12));
  digitalWrite(row.pmPin, (timeinfo.tm_hour >= 12));
}
void animateMonthDisplay(DisplayRow &row) {
  if (!ENABLE_HARDWARE) {
    ESP_LOGD("Display (Disabled)", "animateMonthDisplay skipped (I2C)");
    return;
  }
  if (ENABLE_I2C_HARDWARE) {
    row.month.clear();
    row.month.print("---");
    row.month.writeDisplay();
  }
}
void animateDayDisplay(DisplayRow &row) {
  if (!ENABLE_HARDWARE) {
    ESP_LOGD("Display (Disabled)", "animateDayDisplay skipped (I2C)");
    return;
  }
  if (ENABLE_I2C_HARDWARE) {
    row.day.clear();
    row.day.print(random(1, 32));
    row.day.writeDisplay();
  }
}
void animateYearDisplay(DisplayRow &row) {
  if (!ENABLE_HARDWARE) {
    ESP_LOGD("Display (Disabled)", "animateYearDisplay skipped (I2C)");
    return;
  }
  if (ENABLE_I2C_HARDWARE) {
    row.year.clear();
    row.year.print(random(1000, 10000));
    row.year.writeDisplay();
  }
}
void animateTimeDisplay(DisplayRow &row) {
  if (!ENABLE_HARDWARE) {
    ESP_LOGD("Display (Disabled)", "animateTimeDisplay skipped (I2C)");
    return;
  }
  if (ENABLE_I2C_HARDWARE) {
    row.time.clear();
    row.time.print(random(0, 2400));
    row.time.writeDisplay();
  }
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
      lastRow.month.clear();
      lastRow.month.print("MPH");
      lastRow.month.writeDisplay();

      lastRow.day.clear();
      lastRow.day.writeDisplay();
      lastRow.year.clear();
      lastRow.year.writeDisplay();
      
      lastRow.time.clear();
      lastRow.time.print((int)currentSpeed);
      lastRow.time.writeDisplay();
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