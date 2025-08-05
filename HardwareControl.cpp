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