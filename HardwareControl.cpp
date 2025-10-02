/**
 * @file HardwareControl.cpp
 * @brief Implements low-level control functions for displays, LEDs, and sound.
 * @details This file contains the concrete implementations for initializing and controlling
 * the hardware components. It directly interfaces with the Adafruit GFX and LED Backpack
 * libraries, as well as the custom audio library for I2S sound.
 */

#include "DebugLog.h"
#include "HardwareControl.h"
#include "EventManager.h" 
#include "DisplayManager.h"
#include "LittleFS.h"
#include "AnimationManager.h"
#include "driver/i2c.h"

void getFormattedTimeStrings(char* dest_str, char* pres_str, char* last_str) {
    struct tm dest_timeinfo, present_timeinfo, last_time_departed_info;
    time_t now_t;
    if (xSemaphoreTake(xTimeLibMutex, portMAX_DELAY) == pdTRUE) {
        time(&now_t);
        setenv("TZ", TZ_DATA[currentSettings.destinationTimezoneIndex].tzString, 1);
        tzset();
        localtime_r(&now_t, &dest_timeinfo);
        dest_timeinfo.tm_year = currentSettings.destinationYear - 1900;

        setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
        tzset();
        localtime_r(&now_t, &present_timeinfo);

        xSemaphoreGive(xTimeLibMutex);
    }
    last_time_departed_info.tm_year = currentSettings.lastTimeDepartedYear - 1900;
    last_time_departed_info.tm_mon = currentSettings.lastTimeDepartedMonth - 1;
    last_time_departed_info.tm_mday = currentSettings.lastTimeDepartedDay;
    last_time_departed_info.tm_hour = currentSettings.lastTimeDepartedHour;
    last_time_departed_info.tm_min = currentSettings.lastTimeDepartedMinute;

    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

    snprintf(dest_str, 17, "%3s%02d%04d%02d%02d", months[dest_timeinfo.tm_mon], dest_timeinfo.tm_mday, currentSettings.destinationYear, dest_timeinfo.tm_hour, dest_timeinfo.tm_min);
    snprintf(pres_str, 17, "%3s%02d%04d%02d%02d", months[present_timeinfo.tm_mon], present_timeinfo.tm_mday, present_timeinfo.tm_year + 1900, present_timeinfo.tm_hour, present_timeinfo.tm_min);
    snprintf(last_str, 17, "%3s%02d%04d%02d%02d", months[last_time_departed_info.tm_mon], last_time_departed_info.tm_mday, currentSettings.lastTimeDepartedYear, last_time_departed_info.tm_hour, last_time_departed_info.tm_min);
}

// --- GLOBAL HARDWARE OBJECTS (DEFINITIONS) ---
#if ENABLE_HARDWARE
/** @brief The I2C bus instance for the Destination and Present time rows. */
TwoWire I2C_1 = TwoWire(0);
/** @brief The I2C bus instance for the Last Time Departed row. */
TwoWire I2C_2 = TwoWire(1);

/** @brief Struct containing the 4 display segments for the Destination row. */
DisplayRow destRow;
/** @brief Struct containing the 4 display segments for the Present row. */
DisplayRow presRow;
/** @brief Struct containing the 4 display segments for the Last Time Departed row. */
DisplayRow lastRow;

// --- Make global audio objects available ---
extern bool isPlayingSound;
extern bool isSoundFromMqtt;
extern BootSequenceState bootState;

// Definition for the serial printing mutex.
SemaphoreHandle_t xSerialMutex;

#endif

// These flags ensure that the I2C buses are only initialized once,
// preventing driver re-installation, which can cause a crash on retry.
static bool i2c_1_initialized = false;
static bool i2c_2_initialized = false;

// This flag ensures that the I2C buses are only initialized once,
// even if setupPhysicalDisplay() is called multiple times on retry attempts.
// static bool i2c_initialized = false; // This was causing a bug on hardware init retries

// --- HELPER FUNCTION ---
/**
 * @brief Writes a string to a 4-character alphanumeric display with justification.
 * @param display The Adafruit_AlphaNum4 object to write to.
 * @param text The C-string to display.
 * @param justification 0 for left, 1 for right, 2 for center.
 */
void printToDisplay(Adafruit_AlphaNum4 &display, const char* text, int justification) {
  display.clear();

  // --- FIX: Robustly handle blanking for flashing effects ---
  // If the text is null, empty, or just spaces, ensure the display is cleared and stop.
  // This avoids potential I2C issues with writing empty buffers on certain rows.
  if (text == nullptr) {
    return; // Already cleared
  }
  int text_len = strlen(text);
  if (text_len == 0) {
    return; // Already cleared
  }
  bool is_blank = true;
  for (int i = 0; i < text_len; i++) {
    if (text[i] != ' ') {
      is_blank = false;
      break;
    }
  }
  if (is_blank) {
    return; // Already cleared
  }

  // --- FIX: Safely handle strings to prevent overflow and ensure truncation ---
  char buffer[5];
  strncpy(buffer, text, 4); // Copy at most 4 characters
  buffer[4] = '\0';         // Ensure null-termination

  int len = strlen(buffer);
  int startPos = 0;

  // Calculate the starting position based on justification.
  if (justification == 1) { // Right Justify
    startPos = 4 - len;
  } else if (justification == 2) { // Center Justify
    startPos = (4 - len) / 2;
  }
  // justification 0 (or default) is left-justify, where startPos is already 0.

  // Write characters to the display buffer.
  for (int i = 0; i < 4; i++) {
    if (i >= startPos && i < (startPos + len)) {
      display.writeDigitAscii(i, buffer[i - startPos]);
    } else {
      display.writeDigitAscii(i, ' '); // Pad with spaces.
    }
  }
}

// --- FUNCTION IMPLEMENTATIONS ---
/**
 * @brief Initializes all physical display hardware.
 * @details This function sets up the two I2C buses with their designated GPIO pins.
 * It then initializes all 12 Adafruit_AlphaNum4 display objects, associating each
 * with its correct I2C address and bus. Finally, it configures the AM/PM indicator
 * LED pins and the I2S amplifier shutdown pin as outputs.
 */
bool setupPhysicalDisplay() {
  #if ENABLE_HARDWARE
    if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by setupPhysicalDisplay"); }

    // --- SAFER I2C INITIALIZATION ---
    // Only initialize the I2C buses if they haven't been already.
    // This prevents re-installing the driver on retries, which can cause a system crash.
    if (!i2c_1_initialized) {
        Log_printf(LOG_LEVEL_INFO, "Initializing I2C Bus 1 (SDA: %d, SCL: %d)", I2C_SDA_1, I2C_SCL_1);
        I2C_1.begin(I2C_SDA_1, I2C_SCL_1, 50000);
        I2C_1.setTimeout(250); // 250ms timeout
        i2c_1_initialized = true;
    }
    if (!i2c_2_initialized) {
        Log_printf(LOG_LEVEL_INFO, "Initializing I2C Bus 2 (SDA: %d, SCL: %d)", I2C_SDA_2, I2C_SCL_2);
        I2C_2.begin(I2C_SDA_2, I2C_SCL_2, 50000);
        I2C_2.setTimeout(250); // 250ms timeout
        i2c_2_initialized = true;
    }

    // Initialize the Adafruit_AlphaNum4 objects.
    destRow = {Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4()};
    presRow = {Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4()};
    lastRow = {Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4()};

    // --- Destination Row ---
    if (!destRow.month.begin(0x70, &I2C_1)) { Log_printf(LOG_LEVEL_ERROR, "Dest Row Month display failed to init."); return false; }
    if (!destRow.day.begin(0x71, &I2C_1)) { Log_printf(LOG_LEVEL_ERROR, "Dest Row Day display failed to init."); return false; }
    if (!destRow.year.begin(0x72, &I2C_1)) { Log_printf(LOG_LEVEL_ERROR, "Dest Row Year display failed to init."); return false; }
    if (!destRow.time.begin(0x73, &I2C_1)) { Log_printf(LOG_LEVEL_ERROR, "Dest Row Time display failed to init."); return false; }

    // --- Present Row ---
    if (!presRow.month.begin(0x74, &I2C_1)) { Log_printf(LOG_LEVEL_ERROR, "Pres Row Month display failed to init."); return false; }
    if (!presRow.day.begin(0x75, &I2C_1)) { Log_printf(LOG_LEVEL_ERROR, "Pres Row Day display failed to init."); return false; }
    if (!presRow.year.begin(0x76, &I2C_1)) { Log_printf(LOG_LEVEL_ERROR, "Pres Row Year display failed to init."); return false; }
    if (!presRow.time.begin(0x77, &I2C_1)) { Log_printf(LOG_LEVEL_ERROR, "Pres Row Time display failed to init."); return false; }

    // --- Last Time Departed Row ---
    if (!lastRow.month.begin(0x70, &I2C_2)) { Log_printf(LOG_LEVEL_ERROR, "LTD Row Month display failed to init."); return false; }
    if (!lastRow.day.begin(0x71, &I2C_2)) { Log_printf(LOG_LEVEL_ERROR, "LTD Row Day display failed to init."); return false; }
    if (!lastRow.year.begin(0x72, &I2C_2)) { Log_printf(LOG_LEVEL_ERROR, "LTD Row Year display failed to init."); return false; }
    if (!lastRow.time.begin(0x73, &I2C_2)) { Log_printf(LOG_LEVEL_ERROR, "LTD Row Time display failed to init."); return false; }

    // Set LED indicator pins to output mode.
    pinMode(DEST_AM_PIN, OUTPUT); pinMode(DEST_PM_PIN, OUTPUT);
    pinMode(PRES_AM_PIN, OUTPUT); pinMode(PRES_PM_PIN, OUTPUT);
    pinMode(LAST_AM_PIN, OUTPUT); pinMode(LAST_PM_PIN, OUTPUT);

    // Set I2S Amplifier Shutdown pin to output mode and enable the amplifier.
    pinMode(I2S_SD_PIN, OUTPUT);
    digitalWrite(I2S_SD_PIN, HIGH);

    return true; // All displays initialized successfully
  #else
    return true; // If hardware is disabled, we consider it "initialized" successfully.
  #endif
}
/**
 * @brief Updates a full row of 4 displays to show a specific date and time.
 * @param row A reference to the DisplayRow object to be updated.
 * @param timeinfo A tm struct containing the month, day, hour, and minute.
 * @param year The four-digit year to display.
 * @param showDecimal A boolean flag to control the blinking colon on the time display.
 */
void updateDisplayRow(DisplayRow& row, const struct tm& timeinfo, int year, bool showDecimal) {
  #if ENABLE_HARDWARE
    if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by updateDisplayRow"); }
    char buffer[5];
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

    // Identify which row we are updating to check for sequencer effects.
    int rowIndex = -1;
    if (&row == &destRow) { rowIndex = 0; }
    else if (&row == &presRow) { rowIndex = 1; }
    else if (&row == &lastRow) { rowIndex = 2; }

    // Month (segment 0)
    if (rowIndex != -1 && ((sequencerTracks[rowIndex].isPulsing[0] && !sequencerTracks[rowIndex].pulseStates[0]) || (sequencerTracks[rowIndex].isFlashing[0] && !sequencerTracks[rowIndex].flashStates[0]))) {
        printToDisplay(row.month, "   ", 1);
    } else {
        printToDisplay(row.month, months[timeinfo.tm_mon], 1);
    }

    // Day (segment 1)
    if (rowIndex != -1 && ((sequencerTracks[rowIndex].isPulsing[1] && !sequencerTracks[rowIndex].pulseStates[1]) || (sequencerTracks[rowIndex].isFlashing[1] && !sequencerTracks[rowIndex].flashStates[1]))) {
        printToDisplay(row.day, "  ", 2);
    } else {
        sprintf(buffer, "%02d", timeinfo.tm_mday);
        printToDisplay(row.day, buffer, 2);
    }

    // Year (segment 2)
    if (rowIndex != -1 && ((sequencerTracks[rowIndex].isPulsing[2] && !sequencerTracks[rowIndex].pulseStates[2]) || (sequencerTracks[rowIndex].isFlashing[2] && !sequencerTracks[rowIndex].flashStates[2]))) {
        printToDisplay(row.year, "    ");
    } else {
        sprintf(buffer, "%04d", year);
        printToDisplay(row.year, buffer);
    }

    // --- Time Display Logic (Corrected) ---
    int displayHour = timeinfo.tm_hour;

    // Handle AM/PM and 12/24 hour logic
    if (!currentSettings.displayFormat24h) {
      bool is_pm = displayHour >= 12;
      if (rowIndex == 0) {
        digitalWrite(DEST_AM_PIN, !is_pm);
        digitalWrite(DEST_PM_PIN, is_pm);
      } else if (rowIndex == 1) {
        digitalWrite(PRES_AM_PIN, !is_pm);
        digitalWrite(PRES_PM_PIN, is_pm);
      } else if (rowIndex == 2) {
        digitalWrite(LAST_AM_PIN, !is_pm);
        digitalWrite(LAST_PM_PIN, is_pm);
      }
      if (displayHour >= 13) {
        displayHour -= 12;
      } else if (displayHour == 0) {
        displayHour = 12;
      }
    } else {
      // Turn off AM/PM lights in 24-hour mode
      if (rowIndex == 0) {
          digitalWrite(DEST_AM_PIN, LOW);
          digitalWrite(DEST_PM_PIN, LOW);
      } else if (rowIndex == 1) {
          digitalWrite(PRES_AM_PIN, LOW);
          digitalWrite(PRES_PM_PIN, LOW);
      } else if (rowIndex == 2) {
          digitalWrite(LAST_AM_PIN, LOW);
          digitalWrite(LAST_PM_PIN, LOW);
      }
    }

    // Time (segment 3)
    if (rowIndex != -1 && ((sequencerTracks[rowIndex].isPulsing[3] && !sequencerTracks[rowIndex].pulseStates[3]) || (sequencerTracks[rowIndex].isFlashing[3] && !sequencerTracks[rowIndex].flashStates[3]))) {
        printToDisplay(row.time, "    ");
    } else {
        // Display the Hour and Minute using the library's built-in dot parameter
        row.time.clear();
        char timeBuffer[5];
        sprintf(timeBuffer, "%02d%02d", displayHour, timeinfo.tm_min);

        row.time.writeDigitAscii(0, timeBuffer[0]);
        row.time.writeDigitAscii(1, timeBuffer[1], showDecimal); // Apply dot to the SECOND character
        row.time.writeDigitAscii(2, timeBuffer[2]);
        row.time.writeDigitAscii(3, timeBuffer[3]);
    }

    // Write all changes to the hardware
    row.month.writeDisplay();
    row.day.writeDisplay();
    row.year.writeDisplay();
    row.time.writeDisplay();
    if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by updateDisplayRow"); }
  #endif
}

/**
 * @brief Animates a single display row with a "locking on" effect.
 * @details The display rapidly flickers between the correct target time and random
 * "garbage" data, creating a sense of the time circuits homing in on a target.
 * @param row The DisplayRow to animate.
 * @param timeinfo The target time information.
 * @param year The target year.
 * @param showDecimal Controls the blinking colon for the time.
 */
void animateTemporalLockOn(DisplayRow& row, const struct tm& timeinfo, int year, bool showDecimal) {
    #if ENABLE_HARDWARE
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by animateTemporalLockOn"); }
        // 50% chance to show the correct time, 50% chance to show random "garbage" data.
        if (random(100) < 50) {
            updateDisplayRow(row, timeinfo, year, showDecimal);
        } else {
            animateDisplayRowRandomly(row);
        }
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by animateTemporalLockOn"); }
    #endif
}

// In HardwareControl.cpp

void animateDisplayRowRandomly(DisplayRow& row) {
  #if ENABLE_HARDWARE
    if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by animateDisplayRowRandomly"); }
    char buffer[5];

    // Animate year
    sprintf(buffer, "%04d", random(1000, 9999));
    printToDisplay(row.year, buffer);

    // Animate month
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    printToDisplay(row.month, months[random(0,12)], 1); // Right justified

    // Animate day
    sprintf(buffer, "%02d", random(1, 32));
    printToDisplay(row.day, buffer, 2); // Center justified

    // Animate time
    sprintf(buffer, "%02d%02d", random(0, 24), random(0, 60));
    printToDisplay(row.time, buffer);

    // Write all changes to the hardware at once
    row.year.writeDisplay();
    row.month.writeDisplay();
    row.day.writeDisplay();
    row.time.writeDisplay();
    vTaskDelay(pdMS_TO_TICKS(1));
    if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by animateDisplayRowRandomly"); }
  #endif
}
/**
 * @brief Displays the speedometer reading on the bottom display row.
 * @details This is used during the time travel animation sequence to show the
 * DeLorean's speed. It clears the first two segments and shows the speed
 * right-justified in the "year" segment and "MPH" in the "time" segment.
 * @param speed The speed to display (0-99).
 */
void displaySpeed(int speed) {
  #if ENABLE_HARDWARE
    if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by displaySpeed"); }
    char speedBuffer[5];
    sprintf(speedBuffer, "%02d", speed);

    // Clear the first two displays of the bottom row.
    printToDisplay(lastRow.month, "");
    printToDisplay(lastRow.day, "");
    
    // Display speed on the "year" slot and "MPH" on the "time" slot.
    printToDisplay(lastRow.year, speedBuffer, 1); // Right-justify speed.
    printToDisplay(lastRow.time, "MPH");

    lastRow.month.writeDisplay();
    lastRow.day.writeDisplay();
    lastRow.year.writeDisplay();
    lastRow.time.writeDisplay();
    if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by displaySpeed"); }
  #endif
}
void displaySpeedRamp(int speed) {
#if ENABLE_HARDWARE
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by displaySpeedRamp"); }
        char speedBuffer[5];
        sprintf(speedBuffer, "%02d", speed);

        // Clear unused segments
        printToDisplay(lastRow.month, "");
        printToDisplay(lastRow.time, "");

        // Display speed on "day" and "MPH" on "year"
        printToDisplay(lastRow.day, speedBuffer, 2);
        printToDisplay(lastRow.year, "MPH");

        lastRow.month.writeDisplay();
        lastRow.day.writeDisplay();
        lastRow.year.writeDisplay();
        lastRow.time.writeDisplay();
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by displaySpeedRamp"); }
#endif
}

void animateAllRowsTimelineSkim(unsigned long elapsed, int duration, int destinationYear, bool isCountingUp) {
    #if ENABLE_HARDWARE
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by animateAllRowsTimelineSkim"); }
        float progress = (float)elapsed / duration;
        progress = 1 - pow(1 - progress, 3); // Ease-out curve

        static int startYear = 0;
        if(elapsed < 100) startYear = isCountingUp ? (destinationYear - 100) : random(1, 2100);

        int currentYear = startYear + (destinationYear - startYear) * progress;
        char yearStr[5];
        sprintf(yearStr, "%04d", currentYear);

        DisplayRow* rows[] = {&destRow, &presRow, &lastRow};
        for (int i=0; i<3; ++i) {
            printToDisplay(rows[i]->year, yearStr);

            if (isCountingUp) {
                // For "Counting Up", keep other fields static to focus on the year
                printToDisplay(rows[i]->month, "---");
                printToDisplay(rows[i]->day, "--", 2);
                printToDisplay(rows[i]->time, "----");
            } else {
                // For "Timeline Skim", use a high-speed random blur effect
                const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
                printToDisplay(rows[i]->month, months[(elapsed / 50) % 12], 1);
                char buffer[5];
                sprintf(buffer, "%02d", (elapsed / 30) % 31 + 1);
                printToDisplay(rows[i]->day, buffer, 2);
                sprintf(buffer, "%02d%02d", (elapsed / 20) % 24, (elapsed / 10) % 60);
                printToDisplay(rows[i]->time, buffer);
            }

            rows[i]->year.writeDisplay();
            rows[i]->month.writeDisplay();
            rows[i]->day.writeDisplay();
            rows[i]->time.writeDisplay();
            vTaskDelay(pdMS_TO_TICKS(4));
        }
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by animateAllRowsTimelineSkim"); }
    #endif
}

void flashAllDisplays() {
    #if ENABLE_HARDWARE
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by flashAllDisplays"); }
        DisplayRow* rows[] = {&destRow, &presRow, &lastRow};
        for (int i=0; i<3; ++i) {
            // Directly manipulate the display buffer to turn on all 16 segments (16 bits).
            // 0xFFFF in hex is a 16-bit number with all bits set to 1.
            for(int j=0; j<8; ++j) {
                rows[i]->month.displaybuffer[j] = 0xFFFF;
                rows[i]->day.displaybuffer[j] = 0xFFFF;
                rows[i]->year.displaybuffer[j] = 0xFFFF;
                rows[i]->time.displaybuffer[j] = 0xFFFF;
            }
            // Write the modified buffer to the hardware.
            rows[i]->month.writeDisplay();
            rows[i]->day.writeDisplay();
            rows[i]->year.writeDisplay();
            rows[i]->time.writeDisplay();
        }
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by flashAllDisplays"); }
    #endif
}

void animateTornadoFlicker() {
  #if ENABLE_HARDWARE
    if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by animateTornadoFlicker"); }
    char buffer[5];
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

    // --- Destination Row ---
    sprintf(buffer, "%04d", random(1000, 9999));
    printToDisplay(destRow.year, buffer);
    printToDisplay(destRow.month, months[random(0,12)], 1);
    sprintf(buffer, "%02d", random(1, 32));
    printToDisplay(destRow.day, buffer, 2);
    sprintf(buffer, "%02d%02d", random(0, 24), random(0, 60));
    printToDisplay(destRow.time, buffer);

    // --- Present Row ---
    sprintf(buffer, "%04d", random(1000, 9999));
    printToDisplay(presRow.year, buffer);
    printToDisplay(presRow.month, months[random(0,12)], 1);
    sprintf(buffer, "%02d", random(1, 32));
    printToDisplay(presRow.day, buffer, 2);
    sprintf(buffer, "%02d%02d", random(0, 24), random(0, 60));
    printToDisplay(presRow.time, buffer);

    // --- Last Departed Row ---
    sprintf(buffer, "%04d", random(1000, 9999));
    printToDisplay(lastRow.year, buffer);
    printToDisplay(lastRow.month, months[random(0,12)], 1);
    sprintf(buffer, "%02d", random(1, 32));
    printToDisplay(lastRow.day, buffer, 2);
    sprintf(buffer, "%02d%02d", random(0, 24), random(0, 60));
    printToDisplay(lastRow.time, buffer);

    // --- Write all 12 segments at once ---
    destRow.year.writeDisplay();
    destRow.month.writeDisplay();
    destRow.day.writeDisplay();
    destRow.time.writeDisplay();

    presRow.year.writeDisplay();
    presRow.month.writeDisplay();
    presRow.day.writeDisplay();
    presRow.time.writeDisplay();

    lastRow.year.writeDisplay();
    lastRow.month.writeDisplay();
    lastRow.day.writeDisplay();
    lastRow.time.writeDisplay();
    vTaskDelay(pdMS_TO_TICKS(1));
    if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by animateTornadoFlicker"); }
  #endif
}

/**
 * @brief Fills all displays with random alphanumeric characters without blinking.
 * @details This creates a "corrupted data" effect, where the displays are stable
 * but show rapidly changing, nonsensical information.
 */
void animateCorruptedData() {
    #if ENABLE_HARDWARE
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by animateCorruptedData"); }
        const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        const int numChars = strlen(chars);

        auto rand_str = [&](char* buf, int len) {
            for(int i=0; i<len; ++i) {
                buf[i] = chars[random(numChars)];
            }
            buf[len] = '\0';
        };

        char m_buf[4], d_buf[3], y_buf[5], t_buf[5];
        DisplayRow* rows[] = {&destRow, &presRow, &lastRow};

        for (int i = 0; i < 3; i++) {
            // --- FIX: Generate random strings of the correct length and use printToDisplay for proper justification ---
            rand_str(m_buf, 3); printToDisplay(rows[i]->month, m_buf, 1);
            rand_str(d_buf, 2); printToDisplay(rows[i]->day, d_buf, 2);
            rand_str(y_buf, 4); printToDisplay(rows[i]->year, y_buf);
            rand_str(t_buf, 4); printToDisplay(rows[i]->time, t_buf);

            rows[i]->month.writeDisplay();
            rows[i]->day.writeDisplay();
            rows[i]->year.writeDisplay();
            rows[i]->time.writeDisplay();
            vTaskDelay(pdMS_TO_TICKS(2));
        }
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by animateCorruptedData"); }
    #endif
}

void animateLockOnSequence(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by animateLockOnSequence"); }
        // Define the phases for the lock-on sequence
        unsigned long yearPhaseDuration = duration * 0.45;
        unsigned long monthPhaseDuration = duration * 0.35;

        // Correctly determine the destination timeinfo
        time_t now_t;
        struct tm dest_timeinfo;

        if (xSemaphoreTake(xTimeLibMutex, portMAX_DELAY) == pdTRUE) {
            time(&now_t);
            setenv("TZ", TZ_DATA[currentSettings.destinationTimezoneIndex].tzString, 1);
            tzset();
            localtime_r(&now_t, &dest_timeinfo);
            xSemaphoreGive(xTimeLibMutex);
        }

        char buffer[5];
        const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

        DisplayRow* rows[] = {&destRow, &presRow, &lastRow};

        for (int i = 0; i < 3; ++i) {
            // --- YEAR LOGIC ---
            if (elapsed < yearPhaseDuration) {
                // Phase 1: Year is animating
                sprintf(buffer, "%04d", random(1885, 2085));
                printToDisplay(rows[i]->year, buffer);
                printToDisplay(rows[i]->month, "---", 1);
                printToDisplay(rows[i]->day, "--", 2);
            } else {
                // Year is locked
                sprintf(buffer, "%04d", currentSettings.destinationYear);
                printToDisplay(rows[i]->year, buffer);
            }

            // --- MONTH LOGIC ---
            if (elapsed >= yearPhaseDuration && elapsed < yearPhaseDuration + monthPhaseDuration) {
                // Phase 2: Month is animating
                printToDisplay(rows[i]->month, months[random(0, 12)], 1);
                printToDisplay(rows[i]->day, "--", 2);
            } else if (elapsed >= yearPhaseDuration + monthPhaseDuration) {
                // Month is locked to the correct destination month
                printToDisplay(rows[i]->month, months[dest_timeinfo.tm_mon], 1);
            }

            // --- DAY LOGIC ---
            if (elapsed >= yearPhaseDuration + monthPhaseDuration) {
                // Phase 3: Day is animating (it will lock on the final frame)
                sprintf(buffer, "%02d", random(1, dest_timeinfo.tm_mday + 1));
                printToDisplay(rows[i]->day, buffer, 2);
            }

            // Time is always animating to keep the energy up
            sprintf(buffer, "%02d%02d", random(0, 24), random(0, 60));
            printToDisplay(rows[i]->time, buffer);

            // Write all updates to the hardware
            rows[i]->month.writeDisplay();
            rows[i]->day.writeDisplay();
            rows[i]->year.writeDisplay();
            rows[i]->time.writeDisplay();
            vTaskDelay(pdMS_TO_TICKS(2));
        }

        // Restore the original timezone
        if (xSemaphoreTake(xTimeLibMutex, portMAX_DELAY) == pdTRUE) {
            setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
            tzset();
            xSemaphoreGive(xTimeLibMutex);
        }
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by animateLockOnSequence"); }
    #endif
}

/**
 * @brief Blanks all four segments of a single display row.
 */
void blankDisplayRow(DisplayRow& row) {
    #if ENABLE_HARDWARE
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by blankDisplayRow"); }
        row.month.clear(); row.day.clear(); row.year.clear(); row.time.clear();
        row.month.writeDisplay(); row.day.writeDisplay(); row.year.writeDisplay(); row.time.writeDisplay();
        vTaskDelay(pdMS_TO_TICKS(2));
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by blankDisplayRow"); }
    #endif
}

void animateUnstableSkim(unsigned long elapsed, int duration, int destinationYear) {
    #if ENABLE_HARDWARE
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by animateUnstableSkim"); }
        static int blankingRow = -1;
        static unsigned long blankingStartTime = 0;

        // Check if a blanking period is over
        if (blankingRow != -1 && millis() - blankingStartTime > 150) {
            blankingRow = -1;
        }

        // Check if we should trigger a new blank
        if (blankingRow == -1 && random(100) < 5) {
            blankingRow = random(3);
            blankingStartTime = millis();
        }

        // The rest of this is based on animateAllRowsTimelineSkim
        float progress = (float)elapsed / duration;
        progress = 1 - pow(1 - progress, 3); // Ease-out curve

        static int startYear = 0;
        if(elapsed < 100) startYear = random(1, 2100);

        int currentYear = startYear + (destinationYear - startYear) * progress;
        char yearStr[5];
        sprintf(yearStr, "%04d", currentYear);

        DisplayRow* rows[] = {&destRow, &presRow, &lastRow};
        for (int i=0; i<3; ++i) {
            if (i == blankingRow) {
                blankDisplayRow(*rows[i]);
                continue; // Skip the rest of the drawing for this row
            }

            printToDisplay(rows[i]->year, yearStr);

            const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
            printToDisplay(rows[i]->month, months[(elapsed / 50) % 12], 1);
            char buffer[5];
            sprintf(buffer, "%02d", (elapsed / 30) % 31 + 1);
            printToDisplay(rows[i]->day, buffer, 2);
            sprintf(buffer, "%02d%02d", (elapsed / 20) % 24, (elapsed / 10) % 60);
            printToDisplay(rows[i]->time, buffer);

            rows[i]->year.writeDisplay();
            rows[i]->month.writeDisplay();
            rows[i]->day.writeDisplay();
            rows[i]->time.writeDisplay();
            vTaskDelay(pdMS_TO_TICKS(4));
        }
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by animateUnstableSkim"); }
    #endif
}

void animateTemporalDesync() {
    #if ENABLE_HARDWARE
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by animateTemporalDesync"); }

        // Helper lambda to update a row with a time skimming effect
        auto updateRowWithSkimmingTime = [&](DisplayRow& row, time_t baseTime, long timeMultiplier) {
            char buffer[5];
            time_t fastForwardTime = baseTime + (millis() * timeMultiplier);
            struct tm timeinfo;
            if (xSemaphoreTake(xTimeLibMutex, portMAX_DELAY) == pdTRUE) {
                gmtime_r(&fastForwardTime, &timeinfo);
                xSemaphoreGive(xTimeLibMutex);
            }
            const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

            printToDisplay(row.month, months[timeinfo.tm_mon], 1);
            sprintf(buffer, "%02d", timeinfo.tm_mday);
            printToDisplay(row.day, buffer, 2);
            sprintf(buffer, "%04d", timeinfo.tm_year + 1900);
            printToDisplay(row.year, buffer);
            sprintf(buffer, "%02d%02d", timeinfo.tm_hour, timeinfo.tm_min);
            printToDisplay(row.time, buffer);

            row.month.writeDisplay();
            row.day.writeDisplay();
            row.year.writeDisplay();
            row.time.writeDisplay();
            vTaskDelay(pdMS_TO_TICKS(1));
        };

        time_t baseTime = 1445433600; // Approx Oct 21, 2015

        // Row 1 (Top): Skimming forward very fast
        updateRowWithSkimmingTime(destRow, baseTime, 120); // 2 minutes per ms

        // Row 2 (Middle): Skimming forward at a medium pace
        updateRowWithSkimmingTime(presRow, baseTime, 30); // 30 seconds per ms

        // Row 3 (Bottom): Skimming backwards
        updateRowWithSkimmingTime(lastRow, baseTime, -60); // 1 minute backwards per ms

        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by animateTemporalDesync"); }
    #endif
}

void animateRandomRealTimes() {
#if ENABLE_HARDWARE
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by animateRandomRealTimes"); }
        DisplayRow* rows[] = {&destRow, &presRow, &lastRow};
        for (int i = 0; i < 3; ++i) {
            struct tm timeinfo;
            timeinfo.tm_mon = random(0, 12);
            timeinfo.tm_mday = random(1, 29);
            int year = random(1885, 2085);
            timeinfo.tm_hour = random(0, 24);
            timeinfo.tm_min = random(0, 60);
            updateDisplayRow(*rows[i], timeinfo, year, false);
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by animateRandomRealTimes"); }
#endif
}

void animateCapacitorChargeUp(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by animateCapacitorChargeUp"); }
        int phase = elapsed / (duration / 3);
        float progress = (float)(elapsed % (duration / 3)) / (duration / 3);
        int charsToShow = progress * 16;

        auto fillRow = [&](DisplayRow& row, int numChars) {
            char buffer[17] = "################";
            if (numChars < 16) buffer[numChars] = '\0';
            String s_buffer(buffer);

            // --- FIX: Use correct substring lengths and justification for each segment ---
            // Month: 3 chars, right-justified
            printToDisplay(row.month, s_buffer.substring(0, 3).c_str(), 1);

            // Day: 2 chars, center-justified
            printToDisplay(row.day, s_buffer.substring(3, 5).c_str(), 2);

            // Year: 4 chars, left-justified (default)
            printToDisplay(row.year, s_buffer.substring(5, 9).c_str());

            // Time: 4 chars, left-justified (default)
            printToDisplay(row.time, s_buffer.substring(9, 13).c_str());

            row.month.writeDisplay(); row.day.writeDisplay(); row.year.writeDisplay(); row.time.writeDisplay();
        };

        if (phase == 0) {
            fillRow(lastRow, charsToShow);
        } else if (phase == 1) {
            fillRow(lastRow, 16);
            fillRow(presRow, charsToShow);
        } else {
            fillRow(lastRow, 16);
            fillRow(presRow, 16);
            fillRow(destRow, charsToShow);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by animateCapacitorChargeUp"); }
    #endif
}

// --- State for the Digital Rain Animation ---
#define MAX_RAINDROPS 25
struct Raindrop {
    int column; // 0-12, for each character position
    float y;    // Vertical position (0.0-2.99 for rows)
    float speed;
    bool active;
};
static Raindrop raindrops[MAX_RAINDROPS];
static bool rain_initialized = false;

void animateDigitalRain(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by animateDigitalRain"); }

        if (!rain_initialized) {
            for (int i = 0; i < MAX_RAINDROPS; i++) {
                raindrops[i].active = false;
            }
            rain_initialized = true;
        }

        // Clear all display buffers before drawing the new frame
        destRow.month.clear(); destRow.day.clear(); destRow.year.clear(); destRow.time.clear();
        presRow.month.clear(); presRow.day.clear(); presRow.year.clear(); presRow.time.clear();
        lastRow.month.clear(); lastRow.day.clear(); lastRow.year.clear(); lastRow.time.clear();

        const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        const int numChars = strlen(chars);

        // Update positions and draw active raindrops
        for (int i = 0; i < MAX_RAINDROPS; i++) {
            if (raindrops[i].active) {
                raindrops[i].y += raindrops[i].speed;

                if (raindrops[i].y >= 3.0) {
                    raindrops[i].active = false;
                } else {
                    int row_idx = (int)raindrops[i].y;
                    int col_idx = raindrops[i].column;

                    // Helper lambda to draw a character at a specific row and logical column
                    auto draw_char_at = [&](int r, int c, char ch) {
                        if (r < 0 || r > 2) return;
                        DisplayRow* p_row = (r == 0) ? &destRow : (r == 1) ? &presRow : &lastRow;

                        if (c < 3) { // Month segment (3 chars, right-justified on a 4-char display)
                            p_row->month.writeDigitAscii(c + 1, ch);
                        } else if (c < 5) { // Day segment (2 chars, center-justified)
                            p_row->day.writeDigitAscii(c - 3 + 1, ch);
                        } else if (c < 9) { // Year segment (4 chars, left-justified)
                            p_row->year.writeDigitAscii(c - 5, ch);
                        } else if (c < 13) { // Time segment (4 chars, left-justified)
                            p_row->time.writeDigitAscii(c - 9, ch);
                        }
                    };

                    // Draw the lead character
                    draw_char_at(row_idx, col_idx, chars[random(numChars)]);

                    // Draw a tail character one step behind
                    int tail_row_idx = row_idx - 1;
                    if (tail_row_idx >= 0) {
                        draw_char_at(tail_row_idx, col_idx, '.'); // Use a dot for a dimmer tail effect
                    }
                }
            }
        }

        // Spawn new raindrops periodically
        if (random(100) < 45) { // 45% chance to spawn a new one each frame
            for (int i = 0; i < MAX_RAINDROPS; i++) {
                if (!raindrops[i].active) {
                    raindrops[i].active = true;
                    raindrops[i].column = random(13); // Logical columns 0-12
                    raindrops[i].y = 0.0f;
                    raindrops[i].speed = (random(8, 20)) / 100.0f; // Random speed for variation
                    break;
                }
            }
        }

        // Write all 12 display buffers to the hardware
        destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();
        presRow.month.writeDisplay(); presRow.day.writeDisplay(); presRow.year.writeDisplay(); presRow.time.writeDisplay();
        lastRow.month.writeDisplay(); lastRow.day.writeDisplay(); lastRow.year.writeDisplay(); lastRow.time.writeDisplay();

        vTaskDelay(pdMS_TO_TICKS(40)); // Control animation speed to ~25 FPS

        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by animateDigitalRain"); }
    #endif
}

void animateWaveformCollapse(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by animateWaveformCollapse"); }
        // Define clean, 13-character patterns for a "collapsing" and "expanding" wave.
        const char* waves[] = {
            "-------------",
            " ---     --- ",
            "  ---   ---  ",
            "   -------   ",
            "  ---   ---  ",
            " ---     --- "
        };
        // Cycle through the 6 patterns over the animation's duration. Increased speed by 10x.
        int waveIndex = (elapsed * 60 / duration) % 6;
        const char* pattern = waves[waveIndex];

        auto drawWave = [&](DisplayRow& row, bool inverse) {
            char p_month[4], p_day[3], p_year[5], p_time[5];

            // Create the final pattern for this row (either normal or inverted)
            char finalPattern[14];
            for(int i=0; i<13; ++i) {
                // Use the pattern character if it's not a space, otherwise use a space.
                // This allows patterns to have spaces inside them.
                char baseChar = (pattern[i] == '-') ? '-' : ' ';
                finalPattern[i] = inverse ? ((baseChar == '-') ? ' ' : '-') : baseChar;
            }
            finalPattern[13] = '\0';

            // Safely extract substrings for each segment from the 13-char string.
            // Layout is Month(3), Day(2), Year(4), Time(4).
            strncpy(p_month, finalPattern + 0, 3); p_month[3] = '\0';
            strncpy(p_day,   finalPattern + 3, 2); p_day[2]   = '\0';
            strncpy(p_year,  finalPattern + 5, 4); p_year[4]  = '\0';
            strncpy(p_time,  finalPattern + 9, 4); p_time[4]  = '\0';

            // Write to the displays with correct justification
            printToDisplay(row.month, p_month, 1); // Right justified
            printToDisplay(row.day,   p_day,   2); // Center justified
            printToDisplay(row.year,  p_year);
            printToDisplay(row.time,  p_time);
            row.month.writeDisplay(); row.day.writeDisplay(); row.year.writeDisplay(); row.time.writeDisplay();
        };

        // Draw the wave on all three rows, with the middle one inverted for a nice effect.
        drawWave(destRow, false);
        drawWave(presRow, true);
        drawWave(lastRow, false);
        vTaskDelay(pdMS_TO_TICKS(1));
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by animateWaveformCollapse"); }
    #endif
}

void animateTimelineSkim(unsigned long elapsed, int duration, int destinationYear) {
    #if ENABLE_HARDWARE
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by animateTimelineSkim"); }
        float progress = (float)elapsed / duration;
        progress = 1 - pow(1 - progress, 3);

        static int startYear = 0;
        if(elapsed < 100) startYear = random(1, 2100);

        int currentYear = startYear + (destinationYear - startYear) * progress;

        char yearStr[5];
        sprintf(yearStr, "%04d", currentYear);

        printToDisplay(destRow.year, yearStr); destRow.year.writeDisplay();
        printToDisplay(presRow.year, yearStr); presRow.year.writeDisplay();
        printToDisplay(lastRow.year, yearStr); lastRow.year.writeDisplay();

        const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
        printToDisplay(destRow.month, months[random(0,12)], 1); destRow.month.writeDisplay();

        char buffer[5];
        sprintf(buffer, "%02d", random(1, 32));
        printToDisplay(presRow.day, buffer, 2); presRow.day.writeDisplay();

        sprintf(buffer, "%02d%02d", random(0, 24), random(0, 60));
        printToDisplay(lastRow.time, buffer); lastRow.time.writeDisplay();
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by animateTimelineSkim"); }
    #endif
}
// In HardwareControl.cpp

void blankAllDisplays() {
  #if ENABLE_HARDWARE
    if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by blankAllDisplays"); }
    destRow.month.clear(); destRow.day.clear(); destRow.year.clear(); destRow.time.clear();
    presRow.month.clear(); presRow.day.clear(); presRow.year.clear(); presRow.time.clear();
    lastRow.month.clear(); lastRow.day.clear(); lastRow.year.clear(); lastRow.time.clear();

    destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();
    vTaskDelay(pdMS_TO_TICKS(2));
    presRow.month.writeDisplay(); presRow.day.writeDisplay(); presRow.year.writeDisplay(); presRow.time.writeDisplay();
    vTaskDelay(pdMS_TO_TICKS(2));
    lastRow.month.writeDisplay(); lastRow.day.writeDisplay(); lastRow.year.writeDisplay(); lastRow.time.writeDisplay();
    vTaskDelay(pdMS_TO_TICKS(2));
    if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by blankAllDisplays"); }
  #endif
}
void display88MphSpeed(float speed) {
  #if ENABLE_HARDWARE
    if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by display88MphSpeed"); }
    printToDisplay(lastRow.day, "88", 2);
    printToDisplay(lastRow.year, "MPH");
    lastRow.day.writeDisplay();
    lastRow.year.writeDisplay();
    if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by display88MphSpeed"); }
  #endif
}

void playSound(const char* filepath, bool fromMqtt) {
    #if ENABLE_HARDWARE
        char fullPath[MAX_FILENAME_LENGTH];

        // Ensure the path starts with a single '/'
        if (filepath[0] == '/') {
            strncpy(fullPath, filepath, MAX_FILENAME_LENGTH);
        } else {
            snprintf(fullPath, MAX_FILENAME_LENGTH, "/%s", filepath);
        }
        // Ensure null-termination in case of overflow
        fullPath[MAX_FILENAME_LENGTH - 1] = '\0';

        Log_printf(LOG_LEVEL_INFO, "Request to play sound: %s (fromMqtt: %s)", fullPath, fromMqtt ? "true" : "false");

        if (audio.isRunning()) {
            Log_printf(LOG_LEVEL_DEBUG, "Audio is already running. Stopping current sound.");
            audio.stopSong();
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        if (!LittleFS.exists(fullPath)) {
            Log_printf(LOG_LEVEL_WARN, "Audio file not found: %s", fullPath);
            return;
        }

        isSoundFromMqtt = fromMqtt;
        isPlayingSound = true;

        // The SD pin logic is harmless even if unwired.
        digitalWrite(I2S_SD_PIN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(10));

        audio.setVolume(currentSettings.notificationVolume);
        strncpy(currentSoundFile, fullPath, MAX_FILENAME_LENGTH - 1);
        currentSoundFile[MAX_FILENAME_LENGTH - 1] = '\0';

        // --- FIX: Pass the persistent global buffer, not the temporary 'fullPath' pointer ---
        if (audio.connecttoFS(LittleFS, currentSoundFile)) {
            Log_printf(LOG_LEVEL_INFO, "Started playing: %s", currentSoundFile);
        } else {
            Log_printf(LOG_LEVEL_ERROR, "Failed to connect to audio file: %s", currentSoundFile);
            currentSoundFile[0] = '\0';
            isPlayingSound = false;
            isSoundFromMqtt = false;
            digitalWrite(I2S_SD_PIN, LOW);
        }
    #endif
}
void typeTextOnDisplay(DisplayRow& row, const char* text, int typeDelay, bool withCursor) {
  #if ENABLE_HARDWARE
    if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by typeTextOnDisplay"); }

    int len = strlen(text);
    // The total visual width of a row is 13 characters (3 for month, 2 for day, 4 for year, 4 for time)
    if (len > 13) len = 13;

    // This buffer will hold the text to be displayed at each step of the animation
    char currentText[14];

    for (int i = 1; i <= len; i++) {
        // Build the substring for the current step (from 1 to len)
        strncpy(currentText, text, i);
        currentText[i] = '\0';

        // --- Break down the current text into segments ---
        char monthBuf[4] = "";
        char dayBuf[3] = "";
        char yearBuf[5] = "";
        char timeBuf[5] = "";

        // Copy the relevant part of the substring for the month
        strncpy(monthBuf, currentText, 3);
        monthBuf[3] = '\0'; // Ensure null termination

        // Copy for the day, only if the current text is long enough
        if (i > 3) {
            strncpy(dayBuf, currentText + 3, 2);
            dayBuf[2] = '\0';
        }
        // Copy for the year
        if (i > 5) {
            strncpy(yearBuf, currentText + 5, 4);
            yearBuf[4] = '\0';
        }
        // Copy for the time
        if (i > 9) {
            strncpy(timeBuf, currentText + 9, 4);
            timeBuf[4] = '\0';
        }

        // --- Print segments with correct justification using the reliable helper function ---
        printToDisplay(row.month, monthBuf, 1); // 1 = Right justify
        printToDisplay(row.day, dayBuf, 2);     // 2 = Center justify
        printToDisplay(row.year, yearBuf, 0);   // 0 = Left justify (default)
        printToDisplay(row.time, timeBuf, 0);   // 0 = Left justify (default)

        // Write all segments to the hardware displays
        row.month.writeDisplay();
        row.day.writeDisplay();
        row.year.writeDisplay();
        row.time.writeDisplay();

        // Delay to create the typing effect
        vTaskDelay(pdMS_TO_TICKS(typeDelay));
    }
    if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by typeTextOnDisplay"); }
  #endif
}
void animateFluxCapacitor() {
  #if ENABLE_HARDWARE
    if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by animateFluxCapacitor"); }
    static int frame = 0;
    // Simple 3-frame pulse effect
    if (frame == 0) {
        printToDisplay(presRow.month, " FLX");
        printToDisplay(presRow.day, "CP", 2);
        printToDisplay(presRow.year, "ACTV");
    } else if (frame == 1) {
        printToDisplay(presRow.month, "FLX");
        printToDisplay(presRow.day, "CP", 2);
        printToDisplay(presRow.year, "ACTV");
    } else {
        printToDisplay(presRow.month, " FLX");
        printToDisplay(presRow.day, "CP", 2);
        printToDisplay(presRow.year, "ACTV");
    }
    frame = (frame + 1) % 3; // Cycle through frames

    presRow.month.writeDisplay();
    presRow.day.writeDisplay();
    presRow.year.writeDisplay();
    if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by animateFluxCapacitor"); }
  #endif
}
void displayStaticFluxText() {
    #if ENABLE_HARDWARE
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by displayStaticFluxText"); }
        printToDisplay(presRow.month, "FLX", 1);
        printToDisplay(presRow.day, "CP", 2);
        printToDisplay(presRow.year, "ACTV");
        printToDisplay(presRow.time, "");
        presRow.month.writeDisplay();
        presRow.day.writeDisplay();
        presRow.year.writeDisplay();
        presRow.time.writeDisplay();
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by displayStaticFluxText"); }
    #endif
}
void applyBrightness() {
  #if ENABLE_HARDWARE
    if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by applyBrightness"); }
    uint8_t brightnessValue = currentSettings.brightness;

    // Apply the new brightness level to all 12 display segments
    destRow.month.setBrightness(brightnessValue);
    destRow.day.setBrightness(brightnessValue);
    destRow.year.setBrightness(brightnessValue);
    destRow.time.setBrightness(brightnessValue);

    presRow.month.setBrightness(brightnessValue);
    presRow.day.setBrightness(brightnessValue);
    presRow.year.setBrightness(brightnessValue);
    presRow.time.setBrightness(brightnessValue);

    lastRow.month.setBrightness(brightnessValue);
    lastRow.day.setBrightness(brightnessValue);
    lastRow.year.setBrightness(brightnessValue);
    lastRow.time.setBrightness(brightnessValue);

    // Write the changes to the hardware to make them take effect
    destRow.month.writeDisplay();
    destRow.day.writeDisplay();
    destRow.year.writeDisplay();
    destRow.time.writeDisplay();

    presRow.month.writeDisplay();
    presRow.day.writeDisplay();
    presRow.year.writeDisplay();
    presRow.time.writeDisplay();

    lastRow.month.writeDisplay();
    lastRow.day.writeDisplay();
    lastRow.year.writeDisplay();
    lastRow.time.writeDisplay();
    if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by applyBrightness"); }
  #endif
}

void animateSequentialFlicker(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by animateSequentialFlicker"); }
        float progress = (float)elapsed / duration;
        int segmentsToShow = (int)(progress * 12);

        Adafruit_AlphaNum4* all_displays[] = {
            &destRow.month, &destRow.day, &destRow.year, &destRow.time,
            &presRow.month, &presRow.day, &presRow.year, &presRow.time,
            &lastRow.month, &lastRow.day, &lastRow.year, &lastRow.time
        };

        // Get current times
        time_t now_t;
        struct tm dest_timeinfo;
        if (xSemaphoreTake(xTimeLibMutex, portMAX_DELAY) == pdTRUE) {
            time(&now_t);
            // --- Destination Time ---
            setenv("TZ", TZ_DATA[currentSettings.destinationTimezoneIndex].tzString, 1);
            tzset();
            localtime_r(&now_t, &dest_timeinfo);
            dest_timeinfo.tm_year = currentSettings.destinationYear - 1900;
            xSemaphoreGive(xTimeLibMutex);
        }

        // --- Present Time ---
        struct tm present_timeinfo;
        bool showDecimalForPresent = (millis() / 1000) % 2 == 0;
        if (xSemaphoreTake(xTimeLibMutex, portMAX_DELAY) == pdTRUE) {
            setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
            tzset();
            localtime_r(&now_t, &present_timeinfo);
            xSemaphoreGive(xTimeLibMutex);
        }

        // --- Last Time Departed ---
        struct tm lastTimeDepartedInfo = {0};
        lastTimeDepartedInfo.tm_year = currentSettings.lastTimeDepartedYear - 1900;
        lastTimeDepartedInfo.tm_mon = currentSettings.lastTimeDepartedMonth - 1;
        lastTimeDepartedInfo.tm_mday = currentSettings.lastTimeDepartedDay;
        lastTimeDepartedInfo.tm_hour = currentSettings.lastTimeDepartedHour;
        lastTimeDepartedInfo.tm_min = currentSettings.lastTimeDepartedMinute;

        char buffer[5];
        const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

        // Helper lambda to update a single segment
        auto updateSegment = [&](int index, Adafruit_AlphaNum4* display, const struct tm& time, int year, bool showDecimal = false) {
            if (index >= segmentsToShow) {
                display->clear();
                return;
            }

            int segmentIndex = index % 4;
            switch(segmentIndex) {
                case 0: // Month
                    printToDisplay(*display, months[time.tm_mon], 1);
                    break;
                case 1: // Day
                    sprintf(buffer, "%02d", time.tm_mday);
                    printToDisplay(*display, buffer, 2);
                    break;
                case 2: // Year
                    sprintf(buffer, "%04d", year);
                    printToDisplay(*display, buffer);
                    break;
                case 3: // Time
                    int displayHour = time.tm_hour;
                    if (!currentSettings.displayFormat24h) {
                        if (displayHour >= 13) displayHour -= 12;
                        else if (displayHour == 0) displayHour = 12;
                    }
                    sprintf(buffer, "%02d%02d", displayHour, time.tm_min);
                    display->clear();
                    display->writeDigitAscii(0, buffer[0]);
                    display->writeDigitAscii(1, buffer[1], showDecimal);
                    display->writeDigitAscii(2, buffer[2]);
                    display->writeDigitAscii(3, buffer[3]);
                    break;
            }
        };

        // Update all segments based on segmentsToShow
        updateSegment(0, &destRow.month, dest_timeinfo, currentSettings.destinationYear);
        updateSegment(1, &destRow.day, dest_timeinfo, currentSettings.destinationYear);
        updateSegment(2, &destRow.year, dest_timeinfo, currentSettings.destinationYear);
        updateSegment(3, &destRow.time, dest_timeinfo, currentSettings.destinationYear);

        updateSegment(4, &presRow.month, present_timeinfo, present_timeinfo.tm_year + 1900);
        updateSegment(5, &presRow.day, present_timeinfo, present_timeinfo.tm_year + 1900);
        updateSegment(6, &presRow.year, present_timeinfo, present_timeinfo.tm_year + 1900);
        updateSegment(7, &presRow.time, present_timeinfo, present_timeinfo.tm_year + 1900, showDecimalForPresent);

        updateSegment(8, &lastRow.month, lastTimeDepartedInfo, currentSettings.lastTimeDepartedYear);
        updateSegment(9, &lastRow.day, lastTimeDepartedInfo, currentSettings.lastTimeDepartedYear);
        updateSegment(10, &lastRow.year, lastTimeDepartedInfo, currentSettings.lastTimeDepartedYear);
        updateSegment(11, &lastRow.time, lastTimeDepartedInfo, currentSettings.lastTimeDepartedYear, true);


        // AM/PM LEDs - update them based on row visibility
        if (segmentsToShow > 0 && !currentSettings.displayFormat24h) {
            bool is_pm = dest_timeinfo.tm_hour >= 12;
            digitalWrite(DEST_AM_PIN, !is_pm);
            digitalWrite(DEST_PM_PIN, is_pm);
        } else {
            digitalWrite(DEST_AM_PIN, LOW);
            digitalWrite(DEST_PM_PIN, LOW);
        }
        if (segmentsToShow > 4 && !currentSettings.displayFormat24h) {
            bool is_pm = present_timeinfo.tm_hour >= 12;
            digitalWrite(PRES_AM_PIN, !is_pm);
            digitalWrite(PRES_PM_PIN, is_pm);
        } else {
            digitalWrite(PRES_AM_PIN, LOW);
            digitalWrite(PRES_PM_PIN, LOW);
        }
         if (segmentsToShow > 8 && !currentSettings.displayFormat24h) {
            bool is_pm = lastTimeDepartedInfo.tm_hour >= 12;
            digitalWrite(LAST_AM_PIN, !is_pm);
            digitalWrite(LAST_PM_PIN, is_pm);
        } else {
            digitalWrite(LAST_AM_PIN, LOW);
            digitalWrite(LAST_PM_PIN, LOW);
        }


        // Write all changes to hardware
        for (int i = 0; i < 12; ++i) {
            all_displays[i]->writeDisplay();
        }

        // Reset timezone
        if (xSemaphoreTake(xTimeLibMutex, portMAX_DELAY) == pdTRUE) {
            setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
	        tzset();
            xSemaphoreGive(xTimeLibMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by animateSequentialFlicker"); }
    #endif
}

void animateCountingUp(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Acquired by animateCountingUp"); }
        DisplayRow* rows[] = {&destRow, &presRow, &lastRow};
        char buffer[5];

        // Create a time structure and advance it based on elapsed time
        // This creates a much more realistic "fast forward" effect
        time_t startTime = 1445433600; // Approx Oct 21, 2015
        time_t fastForwardTime = startTime + (elapsed * 60); // Each ms represents one minute

        struct tm timeinfo;
        gmtime_r(&fastForwardTime, &timeinfo);

        const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

        for (int i = 0; i < 3; ++i) {
            // Offset each row slightly for more visual interest
            time_t rowTime = fastForwardTime + (i * 3600 * 24 * 157); // Offset by ~5 months
            gmtime_r(&rowTime, &timeinfo);

            printToDisplay(rows[i]->month, months[timeinfo.tm_mon], 1);

            sprintf(buffer, "%02d", timeinfo.tm_mday);
            printToDisplay(rows[i]->day, buffer, 2);

            sprintf(buffer, "%04d", timeinfo.tm_year + 1900);
            printToDisplay(rows[i]->year, buffer);

            sprintf(buffer, "%02d%02d", timeinfo.tm_hour, timeinfo.tm_min);
            printToDisplay(rows[i]->time, buffer);

            rows[i]->month.writeDisplay();
            rows[i]->day.writeDisplay();
            rows[i]->year.writeDisplay();
            rows[i]->time.writeDisplay();
            vTaskDelay(pdMS_TO_TICKS(4));
        }
        if (bootState != BOOT_INACTIVE) { Serial.println("MUTEX_LOG: Released by animateCountingUp"); }
    #endif
}

void animateGlitchyJumpCut(unsigned long elapsed, int duration) {
  #if ENABLE_HARDWARE
    float progress = (float)elapsed / duration;
    if (progress > 1.0) progress = 1.0;

    DisplayRow* rows[] = {&destRow, &presRow, &lastRow};

    // The main glitch effect runs for the first 90% of the duration.
    if (progress < 0.9) {
        for (int i = 0; i < 3; ++i) {
            // More chaotic: always show random data, don't fall back to stable clock.
            animateDisplayRowRandomly(*rows[i]);

            // Increased chance of a "jump cut" on a single segment to make it more frantic.
            if (random(100) < 50) { // Was 25
                int segmentToGlitch = random(4);
                Adafruit_AlphaNum4* segment;
                switch(segmentToGlitch) {
                    case 0: segment = &rows[i]->month; break;
                    case 1: segment = &rows[i]->day; break;
                    case 2: segment = &rows[i]->year; break;
                    default: segment = &rows[i]->time; break;
                }

                // The glitch can either be a blank or a full flash
                if (random(100) < 50) {
                    segment->clear();
                } else {
                    for(int j=0; j<8; ++j) {
                        segment->displaybuffer[j] = 0xFFFF;
                    }
                }
                segment->writeDisplay();
            }
        }
    } else {
        // Settle on the correct time at the very end.
        updateNormalClockDisplay(true, true, true);
    }
    // Add a small delay to control the frame rate of the glitches.
    vTaskDelay(pdMS_TO_TICKS(20));
  #endif
}

void animatePlasmaWarmUp(unsigned long elapsed, int duration) {
  #if ENABLE_HARDWARE
    float progress = (float)elapsed / duration;
    if (progress > 1.0) progress = 1.0;

    // The main animation runs for 90% of the duration
    if (progress < 0.9) {
        // 1. Brightness warm-up
        uint8_t targetBrightness = progress * 1.1 * currentSettings.brightness;
        if (targetBrightness > currentSettings.brightness) targetBrightness = currentSettings.brightness;
        uint8_t currentBrightness = random(100) < 40 ? targetBrightness * 0.6 : targetBrightness;

        // 2. Plasma field effect
        const char* plasmaChars = "-*~ ";
        int numPlasmaChars = strlen(plasmaChars);
        DisplayRow* all_rows[] = {&destRow, &presRow, &lastRow};

        for(auto& row : all_rows) {
            row->month.setBrightness(currentBrightness);
            row->day.setBrightness(currentBrightness);
            row->year.setBrightness(currentBrightness);
            row->time.setBrightness(currentBrightness);

            Adafruit_AlphaNum4* segments[] = {&row->month, &row->day, &row->year, &row->time};
            for (auto& segment : segments) {
                segment->clear();
                for (int c = 0; c < 4; c++) {
                    // Chance of a character appearing increases with progress
                    if (random(100) < progress * 110) {
                        segment->writeDigitAscii(c, plasmaChars[random(numPlasmaChars)]);
                    }
                }
                segment->writeDisplay();
            }
        }
    } else {
        // Settle on the correct time for the last 10%
        updateNormalClockDisplay(true, true, true);
        applyBrightness(); // Restore full brightness
    }
    vTaskDelay(pdMS_TO_TICKS(30));
  #endif
}

void animateTimeWarpStreaks(unsigned long elapsed, int duration, const char* final_dest, const char* final_pres, const char* final_last) {
  #if ENABLE_HARDWARE
    float progress = (float)elapsed / duration;
    if (progress > 1.0) progress = 1.0;

    int slideInPos = 16 * progress;

    auto streakRow = [&](DisplayRow& row, const char* final_str) {
        char display_str[17] = "                "; // 16 spaces
        int start_pos = 16 - slideInPos;
        if (start_pos < 0) start_pos = 0;

        for (int i = 0; i < slideInPos; ++i) {
            display_str[start_pos + i] = final_str[i];
        }

        printToDisplay(row.month, String(display_str).substring(0, 3).c_str(), 1);
        printToDisplay(row.day, String(display_str).substring(3, 5).c_str(), 2);
        printToDisplay(row.year, String(display_str).substring(5, 9).c_str());
        printToDisplay(row.time, String(display_str).substring(9, 13).c_str());
        row.month.writeDisplay(); row.day.writeDisplay(); row.year.writeDisplay(); row.time.writeDisplay();
    };

    streakRow(destRow, final_dest);
    streakRow(presRow, final_pres);
    streakRow(lastRow, final_last);

  #endif
}

void animateCharacterScanline(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
  #if ENABLE_HARDWARE
    float progress = (float)elapsed / duration;
    if (progress > 1.0) progress = 1.0;

    int charsToShow = progress * 16;

    auto scanlineRow = [&](DisplayRow& row, const char* final_str, int count) {
        char month_buf[4] = "   ";
        char day_buf[3] = "  ";
        char year_buf[5] = "    ";
        char time_buf[5] = "    ";

        if (count > 0) strncpy(month_buf, final_str, min(count, 3));
        if (count > 3) strncpy(day_buf, final_str + 3, min(count - 3, 2));
        if (count > 5) strncpy(year_buf, final_str + 5, min(count - 5, 4));
        if (count > 9) strncpy(time_buf, final_str + 9, min(count - 9, 4));

        printToDisplay(row.month, month_buf, 1);
        printToDisplay(row.day, day_buf, 2);
        printToDisplay(row.year, year_buf);
        printToDisplay(row.time, time_buf);
        row.month.writeDisplay(); row.day.writeDisplay(); row.year.writeDisplay(); row.time.writeDisplay();
    };

    scanlineRow(destRow, dest_str, charsToShow);
    scanlineRow(presRow, pres_str, charsToShow);
    scanlineRow(lastRow, last_str, charsToShow);
  #endif
}

void animateFocusIn(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
  #if ENABLE_HARDWARE
    float progress = (float)elapsed / duration;
    if (progress > 1.0) progress = 1.0;

    int charsToFocus = progress * 16;

    auto focusRow = [&](DisplayRow& row, const char* final_str, int count) {
        char buffer[17];
        for (int i = 0; i < 16; ++i) {
            if (i < count) {
                buffer[i] = final_str[i];
            } else {
                const char* chars = "!@#$%^&*()";
                buffer[i] = chars[random(strlen(chars))];
            }
        }
        buffer[16] = '\0';

        char month_buf[4];
        char day_buf[3];
        char year_buf[5];
        char time_buf[5];

        strncpy(month_buf, buffer, 3);
        month_buf[3] = '\0';
        strncpy(day_buf, buffer + 3, 2);
        day_buf[2] = '\0';
        strncpy(year_buf, buffer + 5, 4);
        year_buf[4] = '\0';
        strncpy(time_buf, buffer + 9, 4);
        time_buf[4] = '\0';

        printToDisplay(row.month, month_buf, 1);
        printToDisplay(row.day, day_buf, 2);
        printToDisplay(row.year, year_buf);
        printToDisplay(row.time, time_buf);
        row.month.writeDisplay(); row.day.writeDisplay(); row.year.writeDisplay(); row.time.writeDisplay();
    };

    focusRow(destRow, dest_str, charsToFocus);
    focusRow(presRow, pres_str, charsToFocus);
    focusRow(lastRow, last_str, charsToFocus);
  #endif
}

void animateCodeBreaker(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
  #if ENABLE_HARDWARE
    float progress = (float)elapsed / duration;
    if (progress > 1.0) progress = 1.0;

    int charsToLock = progress * 16;

    auto codeBreakerRow = [&](DisplayRow& row, const char* final_str, int count) {
        char buffer[17];
        for (int i = 0; i < 16; ++i) {
            if (i < count) {
                buffer[i] = final_str[i];
            } else {
                const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
                buffer[i] = chars[random(strlen(chars))];
            }
        }
        buffer[16] = '\0';

        char month_buf[4];
        char day_buf[3];
        char year_buf[5];
        char time_buf[5];

        strncpy(month_buf, buffer, 3);
        month_buf[3] = '\0';
        strncpy(day_buf, buffer + 3, 2);
        day_buf[2] = '\0';
        strncpy(year_buf, buffer + 5, 4);
        year_buf[4] = '\0';
        strncpy(time_buf, buffer + 9, 4);
        time_buf[4] = '\0';

        printToDisplay(row.month, month_buf, 1);
        printToDisplay(row.day, day_buf, 2);
        printToDisplay(row.year, year_buf);
        printToDisplay(row.time, time_buf);
        row.month.writeDisplay(); row.day.writeDisplay(); row.year.writeDisplay(); row.time.writeDisplay();
    };

    codeBreakerRow(destRow, dest_str, charsToLock);
    codeBreakerRow(presRow, pres_str, charsToLock);
    codeBreakerRow(lastRow, last_str, charsToLock);
  #endif
}

void animateTemporalParadox(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
  #if ENABLE_HARDWARE
    // The animation will flicker between the two states every 200ms
    bool show_swapped = (elapsed / 200) % 2 == 0;

    const char* top_row_str = show_swapped ? pres_str : dest_str;
    const char* middle_row_str = show_swapped ? dest_str : pres_str;

    // Helper to print a 13-char string to a row.
    auto print_row = [&](DisplayRow& row, const char* str) {
        char segment_buf[5];
        strncpy(segment_buf, str + 0, 3); segment_buf[3] = '\0';
        printToDisplay(row.month, segment_buf, 1);
        strncpy(segment_buf, str + 3, 2); segment_buf[2] = '\0';
        printToDisplay(row.day, segment_buf, 2);
        strncpy(segment_buf, str + 5, 4); segment_buf[4] = '\0';
        printToDisplay(row.year, segment_buf, 0);
        strncpy(segment_buf, str + 9, 4); segment_buf[4] = '\0';
        printToDisplay(row.time, segment_buf, 0);
        row.month.writeDisplay();
        row.day.writeDisplay();
        row.year.writeDisplay();
        row.time.writeDisplay();
    };

    print_row(destRow, top_row_str);
    print_row(presRow, middle_row_str);

    // Glitch the last row to add to the unstable feeling
    animateDisplayRowRandomly(lastRow);

    vTaskDelay(pdMS_TO_TICKS(50));
  #endif
}

void animateDigitCascade(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
  #if ENABLE_HARDWARE
    float progress = (float)elapsed / duration;
    if (progress > 1.0) progress = 1.0;

    int segmentsToLock = progress * 12; // 3 rows * 4 segments

    auto cascadeRow = [&](DisplayRow& row, const char* final_str, int start_index) {
        char month_buf[4], day_buf[3], year_buf[5], time_buf[5];

        // Month
        if (segmentsToLock > start_index) {
            strncpy(month_buf, final_str, 3);
            month_buf[3] = '\0';
        } else {
            // Rapidly cycle through random chars
            for (int i = 0; i < 3; i++) {
                month_buf[i] = random(256);
            }
            month_buf[3] = '\0';
        }

        // Day
        if (segmentsToLock > start_index + 1) {
            strncpy(day_buf, final_str + 3, 2);
            day_buf[2] = '\0';
        } else {
            sprintf(day_buf, "%02d", random(0, 99));
        }

        // Year
        if (segmentsToLock > start_index + 2) {
            strncpy(year_buf, final_str + 5, 4);
            year_buf[4] = '\0';
        } else {
            sprintf(year_buf, "%04d", random(0, 9999));
        }

        // Time
        if (segmentsToLock > start_index + 3) {
            strncpy(time_buf, final_str + 9, 4);
            time_buf[4] = '\0';
        } else {
            sprintf(time_buf, "%04d", random(0, 9999));
        }

        // Simulate falling from above by clearing and re-printing
        row.month.clear();
        row.day.clear();
        row.year.clear();
        row.time.clear();

        printToDisplay(row.month, month_buf, 1);
        printToDisplay(row.day, day_buf, 2);
        printToDisplay(row.year, year_buf);
        printToDisplay(row.time, time_buf);

        row.month.writeDisplay(); row.day.writeDisplay(); row.year.writeDisplay(); row.time.writeDisplay();
    };

    cascadeRow(destRow, dest_str, 0);
    cascadeRow(presRow, pres_str, 4);
    cascadeRow(lastRow, last_str, 8);
  #endif
}

void animateElectricSurge(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
  #if ENABLE_HARDWARE
    float progress = (float)elapsed / duration;
    if (progress > 1.0) progress = 1.0;

    int surgePosition = progress * 16;

    auto surgeRow = [&](DisplayRow& row, const char* final_str, const char* initial_str) {
        char buffer[17];
        for (int i = 0; i < 16; ++i) {
            if (i < surgePosition) {
                buffer[i] = final_str[i];
            } else if (i == surgePosition) {
                const char* chars = "1234567890";
                buffer[i] = chars[random(strlen(chars))];
            } else {
                buffer[i] = initial_str[i];
            }
        }
        buffer[16] = '\0';

        char month_buf[4], day_buf[3], year_buf[5], time_buf[5];

        strncpy(month_buf, buffer, 3); month_buf[3] = '\0';
        strncpy(day_buf, buffer + 3, 2); day_buf[2] = '\0';
        strncpy(year_buf, buffer + 5, 4); year_buf[4] = '\0';
        strncpy(time_buf, buffer + 9, 4); time_buf[4] = '\0';

        printToDisplay(row.month, month_buf, 1);
        printToDisplay(row.day, day_buf, 2);
        printToDisplay(row.year, year_buf);
        printToDisplay(row.time, time_buf);
        row.month.writeDisplay(); row.day.writeDisplay(); row.year.writeDisplay(); row.time.writeDisplay();
    };

    surgeRow(destRow, dest_str, old_dest_str);
    surgeRow(presRow, pres_str, old_pres_str);
    surgeRow(lastRow, last_str, old_last_str);
  #endif
}

void animateFlipDiscDisplay(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
  #if ENABLE_HARDWARE
    float progress = (float)elapsed / duration;
    if (progress > 1.0) progress = 1.0;

    int chars_to_show = progress * 13; // Total characters across one row is 13

    auto flipRow = [&](DisplayRow& row, const char* final_str) {
        char current_str[14];
        const char* flip_chars = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        int flip_chars_len = strlen(flip_chars);

        for (int i = 0; i < 13; i++) {
            if (i < chars_to_show) {
                // This character is "locked in"
                current_str[i] = final_str[i];
            } else if (i == chars_to_show) {
                // This is the character currently "flipping"
                current_str[i] = flip_chars[random(flip_chars_len)];
            } else {
                // This character hasn't started flipping yet
                current_str[i] = ' ';
            }
        }
        current_str[13] = '\0';

        char month_buf[4], day_buf[3], year_buf[5], time_buf[5];

        strncpy(month_buf, current_str, 3); month_buf[3] = '\0';
        strncpy(day_buf, current_str + 3, 2); day_buf[2] = '\0';
        strncpy(year_buf, current_str + 5, 4); year_buf[4] = '\0';
        strncpy(time_buf, current_str + 9, 4); time_buf[4] = '\0';

        printToDisplay(row.month, month_buf, 1);
        printToDisplay(row.day, day_buf, 2);
        printToDisplay(row.year, year_buf);
        printToDisplay(row.time, time_buf);
        row.month.writeDisplay(); row.day.writeDisplay(); row.year.writeDisplay(); row.time.writeDisplay();
    };

    flipRow(destRow, dest_str);
    flipRow(presRow, pres_str);
    flipRow(lastRow, last_str);
  #endif
}

void animateInterferencePattern(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
  #if ENABLE_HARDWARE
    float progress = (float)elapsed / duration;
    if (progress > 1.0) progress = 1.0;

    auto interferenceRow = [&](DisplayRow& row, const char* final_str) {
        char buffer[17];
        for (int i = 0; i < 16; ++i) {
            if (random(100) < progress * 100) {
                buffer[i] = final_str[i];
            } else {
                const char* chars = "1234567890!@#$%^&*()_+-=[]{}|;':,./<>?";
                buffer[i] = chars[random(strlen(chars))];
            }
        }
        buffer[16] = '\0';

        char month_buf[4], day_buf[3], year_buf[5], time_buf[5];

        strncpy(month_buf, buffer, 3); month_buf[3] = '\0';
        strncpy(day_buf, buffer + 3, 2); day_buf[2] = '\0';
        strncpy(year_buf, buffer + 5, 4); year_buf[4] = '\0';
        strncpy(time_buf, buffer + 9, 4); time_buf[4] = '\0';

        printToDisplay(row.month, month_buf, 1);
        printToDisplay(row.day, day_buf, 2);
        printToDisplay(row.year, year_buf);
        printToDisplay(row.time, time_buf);
        row.month.writeDisplay(); row.day.writeDisplay(); row.year.writeDisplay(); row.time.writeDisplay();
    };

    interferenceRow(destRow, dest_str);
    interferenceRow(presRow, pres_str);
    interferenceRow(lastRow, last_str);
  #endif
}

/**
 * @brief A thread-safe wrapper for Serial.printf.
 * @details This function acquires a mutex before printing to the serial port,
 * ensuring that log messages from different FreeRTOS tasks are not interleaved
 * and garbled. It supports standard printf format strings.
 * @param format The format string.
 * @param ... The variable arguments for the format string.
 */
void safe_printf(const char *format, ...) {
    // Define a static buffer to hold the formatted string. This avoids placing a large
    // buffer on the stack, which could cause an overflow. The mutex ensures that this
    // static buffer is not accessed by multiple tasks simultaneously.
    // Increased size to 2500 to safely accommodate the largest HA discovery payloads.
    static char buf[2500];

    va_list args;
    va_start(args, format);

    if (xSerialMutex == NULL) {
        // Fallback to vprintf if mutex is not available (e.g., before scheduler starts).
        // This path is less likely to handle very long strings but is a safe fallback.
        Serial.vprintf(format, args);
    } else {
        if (xSemaphoreTake(xSerialMutex, portMAX_DELAY) == pdTRUE) {
            // Format the complete message into our local buffer.
            // vsnprintf is safe and will not write more than sizeof(buf) bytes.
            int len = vsnprintf(buf, sizeof(buf), format, args);

            if (len >= 0) {
                // Write the buffer to the Serial port in chunks. This avoids overflowing
                // the UART's internal buffer, which is the root cause of the truncation.
                const size_t chunkSize = 64;
                for (int i = 0; i < len; i += chunkSize) {
                    Serial.write(&buf[i], min((size_t)len - i, chunkSize));
                }
            }
            // If vsnprintf fails (returns < 0), there's not much we can do, so we just release the mutex.

            xSemaphoreGive(xSerialMutex);
        } else {
            // If we fail to take the mutex, something is critically wrong.
            // As a last resort, try to print an error and then the message without protection.
            Serial.println("FATAL: Could not take serial mutex!");
            vsnprintf(buf, sizeof(buf), format, args);
            Serial.print(buf); // Use print() as a final attempt.
        }
    }

    va_end(args);
}

/**
 * @brief Resets an I2C bus that may have locked up.
 * @details This function de-initializes the I2C peripheral. It is called before
 * attempting to initialize the displays to clear any bus lock-ups from a previous
 * run. The subsequent call to `I2C_X.begin()` will re-initialize it.
 * @param i2c_num The I2C port number (0 or 1) to reset.
 */
void resetI2CBus(int i2c_num) {
    #if ENABLE_HARDWARE
    Log_printf(LOG_LEVEL_WARN, "I2C bus reset for bus #%d requested, but this action is now disabled to prevent system instability.", i2c_num);
    #endif
}
