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

// Initialize all displays using the new I2C bus and addresses
DisplayRow destRow = {
    Adafruit_7segment(), Adafruit_7segment(), Adafruit_7segment(), Adafruit_7segment(), 13, 14
};
DisplayRow presRow = {
    Adafruit_7segment(), Adafruit_7segment(), Adafruit_7segment(), Adafruit_7segment(), 32, 27
};
DisplayRow lastRow = {
    Adafruit_7segment(), Adafruit_7segment(), Adafruit_7segment(), Adafruit_7segment(), 2, 4
};

HardwareSerial dfpSerial(2); 
DFRobotDFPlayerMini myDFPlayer;
std::map<String, int> soundFiles;

// Function implementations
void setupPhysicalDisplay() {
  if (!ENABLE_HARDWARE) {
    ESP_LOGI("Display (Disabled)", "Physical displays setup skipped.");
    return;
  }

  if (ENABLE_I2C_HARDWARE) {
    // Initialize both I2C buses with their respective pins and check for success
    if (!I2C_Bus_1.begin(I2C_SDA_1, I2C_SCL_1)) {
        ESP_LOGE("Display", "CRITICAL ERROR: Failed to initialize I2C Bus 1. Check wiring for GPIO %d (SDA) and %d (SCL).", I2C_SDA_1, I2C_SCL_1);
    } else {
        ESP_LOGI("Display", "I2C Bus 1 initialized successfully.");
    }

    if (!I2C_Bus_2.begin(I2C_SDA_2, I2C_SCL_2)) {
        ESP_LOGE("Display", "CRITICAL ERROR: Failed to initialize I2C Bus 2. Check wiring for GPIO %d (SDA) and %d (SCL).", I2C_SDA_2, I2C_SCL_2);
    } else {
        ESP_LOGI("Display", "I2C Bus 2 initialized successfully.");
    }
    
    // Begin all displays on the two buses with their unique addresses
    destRow.month.begin(ADDR_DEST_MONTH, &I2C_Bus_1);
    destRow.day.begin(ADDR_DEST_DAY, &I2C_Bus_1);
    destRow.year.begin(ADDR_DEST_YEAR, &I2C_Bus_1);
    destRow.time.begin(ADDR_DEST_TIME, &I2C_Bus_1);

    presRow.month.begin(ADDR_PRES_MONTH, &I2C_Bus_1);
    presRow.day.begin(ADDR_PRES_DAY, &I2C_Bus_1);
    presRow.year.begin(ADDR_PRES_YEAR, &I2C_Bus_1);
    presRow.time.begin(ADDR_PRES_TIME, &I2C_Bus_1);

    // *** CORRECTED I2C BUS INITIALIZATION ***
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

// *** UPDATED FUNCTION ***
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
    // *** FIX: Ensure colon is drawn ***
    row.time.drawColon(true);
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
    row.time.drawColon(random(0,2));
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

void displayWindSpeed(float currentSpeed) {
    if (!ENABLE_HARDWARE || !ENABLE_I2C_HARDWARE) {
      ESP_LOGI("Display (Disabled)", "displayWindSpeed skipped.");
      return;
    }
    
    // Convert km/h to mph for the display
    float speedMph = currentSpeed * 0.621371;

    lastRow.month.clear();
    lastRow.month.print("MPH");
    lastRow.month.writeDisplay();

    lastRow.day.clear();
    lastRow.day.writeDisplay();
    lastRow.year.clear();
    lastRow.year.writeDisplay();
    
    lastRow.time.clear();
    lastRow.time.print((int)speedMph);
    lastRow.time.writeDisplay();
    
    digitalWrite(lastRow.amPin, LOW);
    digitalWrite(lastRow.pmPin, LOW);
}

// *** CORRECTED FUNCTION: Added fallback for missing sounds ***
void playSound(const char *soundName) {
  if (!ENABLE_HARDWARE) {
    ESP_LOGI("Sound (Disabled)", "Attempted to play sound: %s", soundName);
    return;
  }
  
  auto it = soundFiles.find(soundName);
  if (it != soundFiles.end()) {
    myDFPlayer.play(it->second);
    ESP_LOGI("Sound", "Playing sound: %s (Index: %d)", it->first.c_str(), it->second);
  } else {
    ESP_LOGW("Sound", "WARNING: Sound file '%s' not found. Playing fallback sound.", soundName);
    auto fallbackIt = soundFiles.find(SOUND_NOT_FOUND); // Check for the dedicated fallback sound
    if (fallbackIt != soundFiles.end()) {
      myDFPlayer.play(fallbackIt->second);
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