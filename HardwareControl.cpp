/**
 * @file HardwareControl.cpp
 * @brief Implements low-level control functions for displays, LEDs, and sound.
 * @details This file contains the concrete implementations for initializing and controlling
 * the hardware components. It directly interfaces with the Adafruit GFX and LED Backpack
 * libraries, as well as the custom audio library for I2S sound.
 */

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
#include "MqttManager.h"
#include "driver/i2c.h"

// --- NEW: Make the hardware mutex available in this file ---
extern SemaphoreHandle_t xDisplayHardwareMutex;

// --- FORWARD DECLARATIONS for internal, non-locking functions ---
void updateDisplayRow_internal(DisplayRow& row, const struct tm& timeinfo, int year, bool showDecimal);
void blankDisplayRow_internal(DisplayRow& row);


/**
 * @brief Gets the fully formatted time strings for all three display rows.
 * @details This function calculates the current time for the destination and present timezones,
 * retrieves the "last time departed" from settings, and formats all three into the
 * `MONDDYYYYHHMM` format used by some animations and external integrations.
 * @param dest_str A character buffer to store the destination time string (must be at least 17 chars).
 * @param pres_str A character buffer to store the present time string (must be at least 17 chars).
 * @param last_str A character buffer to store the last time departed string (must be at least 17 chars).
 */
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
Raindrop raindrops[MAX_RAINDROPS];
bool rain_initialized = false;
TwoWire I2C_1 = TwoWire(0);
TwoWire I2C_2 = TwoWire(1);
DisplayRow destRow;
DisplayRow presRow;
DisplayRow lastRow;
extern bool isPlayingSound;
extern bool isSoundFromMqtt;
extern BootSequenceState bootState;
SemaphoreHandle_t xSerialMutex;
#endif

static bool i2c_1_initialized = false;
static bool i2c_2_initialized = false;

/**
 * @brief Prints text to a 4-character alphanumeric display segment with justification.
 * @details This is a utility function to simplify writing text to the Adafruit displays.
 * It handles clearing the display, padding, and justification (left, right, center).
 * It is optimized to use a static buffer to prevent heap fragmentation from frequent calls.
 * @param display A reference to the `Adafruit_AlphaNum4` object to write to.
 * @param text The text to display.
 * @param justification 0 for left, 1 for right, 2 for center.
 */
void printToDisplay(Adafruit_AlphaNum4 &display, const char* text, int justification) {
    display.clear();
    if (text == nullptr) {
        return;
    }

    // Use a static buffer to prevent heap fragmentation on frequent calls.
    static char buffer[5]; 
    int len = 0;

    // Copy and sanitize the input text directly into the static buffer.
    // This avoids creating intermediate String objects.
    for (int i = 0; i < 4 && text[i] != '\0'; ++i) {
        // Replace unsupported characters.
        if (text[i] == '!') {
            buffer[len++] = ' ';
        } else {
            buffer[len++] = text[i];
        }
    }
    buffer[len] = '\0';

    // If the buffer is empty after sanitization, there's nothing to display.
    if (len == 0) {
        return;
    }

    // Calculate the starting position for justification.
    int startPos = 0;
    if (justification == 1) { // Right justification
        startPos = 4 - len;
    } else if (justification == 2) { // Center justification
        startPos = (4 - len) / 2;
    }

    // Write the sanitized and justified text to the display's internal buffer.
    for (int i = 0; i < 4; i++) {
        if (i >= startPos && i < (startPos + len)) {
            display.writeDigitAscii(i, buffer[i - startPos]);
        } else {
            // It's important to fill unused spaces, otherwise previous characters may remain.
            display.writeDigitAscii(i, ' ');
        }
    }
}

/**
 * @brief Initializes the physical hardware components.
 * @details This function initializes the two I2C buses and probes for all 12 alphanumeric
 * display segments. It also sets up the GPIO pins for the AM/PM LEDs.
 * @return `true` if all hardware components are found and initialized successfully, `false` otherwise.
 */
bool setupPhysicalDisplay() {
  #if ENABLE_HARDWARE
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) != pdTRUE) { return false; }
    if (!i2c_1_initialized) {
        Log_printf(LOG_LEVEL_INFO, "Initializing I2C Bus 1 (SDA: %d, SCL: %d)", I2C_SDA_1, I2C_SCL_1);
        I2C_1.begin(I2C_SDA_1, I2C_SCL_1, 50000);
        I2C_1.setTimeout(250);
        i2c_1_initialized = true;
    }
    if (!i2c_2_initialized) {
        Log_printf(LOG_LEVEL_INFO, "Initializing I2C Bus 2 (SDA: %d, SCL: %d)", I2C_SDA_2, I2C_SCL_2);
        I2C_2.begin(I2C_SDA_2, I2C_SCL_2, 50000);
        I2C_2.setTimeout(250);
        i2c_2_initialized = true;
    }
    destRow = {Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4()};
    presRow = {Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4()};
    lastRow = {Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4()};
    if (!destRow.month.begin(0x70, &I2C_1) || !destRow.day.begin(0x71, &I2C_1) || !destRow.year.begin(0x72, &I2C_1) || !destRow.time.begin(0x73, &I2C_1) ||
        !presRow.month.begin(0x74, &I2C_1) || !presRow.day.begin(0x75, &I2C_1) || !presRow.year.begin(0x76, &I2C_1) || !presRow.time.begin(0x77, &I2C_1) ||
        !lastRow.month.begin(0x70, &I2C_2) || !lastRow.day.begin(0x71, &I2C_2) || !lastRow.year.begin(0x72, &I2C_2) || !lastRow.time.begin(0x73, &I2C_2)) {
        Log_printf(LOG_LEVEL_ERROR, "Display failed to init.");
        xSemaphoreGive(xDisplayHardwareMutex);
        return false;
    }
    pinMode(DEST_AM_PIN, OUTPUT); pinMode(DEST_PM_PIN, OUTPUT);
    pinMode(PRES_AM_PIN, OUTPUT); pinMode(PRES_PM_PIN, OUTPUT);
    pinMode(LAST_AM_PIN, OUTPUT); pinMode(LAST_PM_PIN, OUTPUT);
    pinMode(I2S_SD_PIN, OUTPUT);
    digitalWrite(I2S_SD_PIN, HIGH);
    xSemaphoreGive(xDisplayHardwareMutex);
    return true;
  #else
    return true;
  #endif
}

/**
 * @brief Internal, non-locking function to update a single display row with a specific time.
 * @details This function contains the core logic for rendering a time on one of the three
 * display rows. It handles formatting, AM/PM LEDs, and the blinking colon. It does NOT
 * acquire the hardware mutex, assuming the calling function has already done so.
 * @param row The `DisplayRow` object to update.
 * @param timeinfo A `tm` struct containing the time to display.
 * @param year The four-digit year to display.
 * @param showDecimal `true` to show the blinking colon (decimal point) on the time segment.
 */
void updateDisplayRow_internal(DisplayRow& row, const struct tm& timeinfo, int year, bool showDecimal) {
  #if ENABLE_HARDWARE
    char buffer[5];
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    int rowIndex = -1;
    if (&row == &destRow) { rowIndex = 0; }
    else if (&row == &presRow) { rowIndex = 1; }
    else if (&row == &lastRow) { rowIndex = 2; }
    if (rowIndex != -1 && ((sequencerTracks[rowIndex].isPulsing[0] && !sequencerTracks[rowIndex].pulseStates[0]) || (sequencerTracks[rowIndex].isFlashing[0] && !sequencerTracks[rowIndex].flashStates[0]))) { printToDisplay(row.month, "   ", 1); } else { printToDisplay(row.month, months[timeinfo.tm_mon], 1); }
    if (rowIndex != -1 && ((sequencerTracks[rowIndex].isPulsing[1] && !sequencerTracks[rowIndex].pulseStates[1]) || (sequencerTracks[rowIndex].isFlashing[1] && !sequencerTracks[rowIndex].flashStates[1]))) { printToDisplay(row.day, "  ", 2); } else { sprintf(buffer, "%02d", timeinfo.tm_mday); printToDisplay(row.day, buffer, 2); }
    if (rowIndex != -1 && ((sequencerTracks[rowIndex].isPulsing[2] && !sequencerTracks[rowIndex].pulseStates[2]) || (sequencerTracks[rowIndex].isFlashing[2] && !sequencerTracks[rowIndex].flashStates[2]))) { printToDisplay(row.year, "    "); } else { sprintf(buffer, "%04d", year); printToDisplay(row.year, buffer); }
    int displayHour = timeinfo.tm_hour;
    if (!currentSettings.displayFormat24h) {
      bool is_pm = displayHour >= 12;
      if (rowIndex == 0) { digitalWrite(DEST_AM_PIN, !is_pm); digitalWrite(DEST_PM_PIN, is_pm); }
      else if (rowIndex == 1) { digitalWrite(PRES_AM_PIN, !is_pm); digitalWrite(PRES_PM_PIN, is_pm); }
      else if (rowIndex == 2) { digitalWrite(LAST_AM_PIN, !is_pm); digitalWrite(LAST_PM_PIN, is_pm); }
      if (displayHour >= 13) { displayHour -= 12; } else if (displayHour == 0) { displayHour = 12; }
    } else {
      if (rowIndex == 0) { digitalWrite(DEST_AM_PIN, LOW); digitalWrite(DEST_PM_PIN, LOW); }
      else if (rowIndex == 1) { digitalWrite(PRES_AM_PIN, LOW); digitalWrite(PRES_PM_PIN, LOW); }
      else if (rowIndex == 2) { digitalWrite(LAST_AM_PIN, LOW); digitalWrite(LAST_PM_PIN, LOW); }
    }
    if (rowIndex != -1 && ((sequencerTracks[rowIndex].isPulsing[3] && !sequencerTracks[rowIndex].pulseStates[3]) || (sequencerTracks[rowIndex].isFlashing[3] && !sequencerTracks[rowIndex].flashStates[3]))) { printToDisplay(row.time, "    ");
    } else {
        row.time.clear();
        char timeBuffer[5];
        sprintf(timeBuffer, "%02d%02d", displayHour, timeinfo.tm_min);
        row.time.writeDigitAscii(0, timeBuffer[0]);
        row.time.writeDigitAscii(1, timeBuffer[1], showDecimal);
        row.time.writeDigitAscii(2, timeBuffer[2]);
        row.time.writeDigitAscii(3, timeBuffer[3]);
    }
    row.month.writeDisplay();
    row.day.writeDisplay();
    row.year.writeDisplay();
    row.time.writeDisplay();
  #endif
}

/**
 * @brief A thread-safe wrapper to update a single display row.
 * @details This function acquires the hardware mutex before calling the internal `updateDisplayRow_internal`
 * function, ensuring that the I2C writes are atomic and protected from other tasks.
 * @param row The `DisplayRow` object to update.
 * @param timeinfo A `tm` struct containing the time to display.
 * @param year The four-digit year to display.
 * @param showDecimal `true` to show the blinking colon on the time segment.
 */
void updateDisplayRow(DisplayRow& row, const struct tm& timeinfo, int year, bool showDecimal) {
  #if ENABLE_HARDWARE
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) != pdTRUE) { return; }
    updateDisplayRow_internal(row, timeinfo, year, showDecimal);
    xSemaphoreGive(xDisplayHardwareMutex);
  #endif
}

/**
 * @brief Internal, non-locking function for the "temporal lock-on" animation frame.
 * @details This function either displays the correct time or a random time on a row,
 * creating a flickering "locking-on" effect.
 * @param row The `DisplayRow` object to animate.
 * @param timeinfo The final `tm` struct to lock onto.
 * @param year The final four-digit year to lock onto.
 * @param showDecimal `true` to show the blinking colon.
 */
void animateTemporalLockOn_internal(DisplayRow& row, const struct tm& timeinfo, int year, bool showDecimal) {
    #if ENABLE_HARDWARE
        if (random(100) < 50) {
            updateDisplayRow_internal(row, timeinfo, year, showDecimal);
        } else {
            animateDisplayRowRandomly_internal(row);
        }
    #endif
}

/**
 * @brief A thread-safe wrapper for the "temporal lock-on" animation.
 * @param row The `DisplayRow` object to animate.
 * @param timeinfo The final `tm` struct to lock onto.
 * @param year The final four-digit year to lock onto.
 * @param showDecimal `true` to show the blinking colon.
 */
void animateTemporalLockOn(DisplayRow& row, const struct tm& timeinfo, int year, bool showDecimal) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateTemporalLockOn_internal(row, timeinfo, year, showDecimal);
        xSemaphoreGive(xDisplayHardwareMutex);
    }
}

/**
 * @brief Internal, non-locking function to display random characters on a row.
 * @param row The `DisplayRow` object to animate.
 */
void animateDisplayRowRandomly_internal(DisplayRow& row) {
  #if ENABLE_HARDWARE
    char buffer[5];
    sprintf(buffer, "%04d", random(1000, 9999));
    printToDisplay(row.year, buffer);
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    printToDisplay(row.month, months[random(0,12)], 1);
    sprintf(buffer, "%02d", random(1, 32));
    printToDisplay(row.day, buffer, 2);
    sprintf(buffer, "%02d%02d", random(0, 24), random(0, 60));
    printToDisplay(row.time, buffer);
    row.year.writeDisplay();
    row.month.writeDisplay();
    row.day.writeDisplay();
    row.time.writeDisplay();
  #endif
}

/**
 * @brief A thread-safe wrapper to display random characters on a row.
 * @param row The `DisplayRow` object to animate.
 */
void animateDisplayRowRandomly(DisplayRow& row) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateDisplayRowRandomly_internal(row);
        xSemaphoreGive(xDisplayHardwareMutex);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/**
 * @brief Internal, non-locking function to display the speedometer.
 * @param speed The speed (0-99) to display.
 */
void displaySpeed_internal(int speed) {
  #if ENABLE_HARDWARE
    char speedBuffer[5];
    sprintf(speedBuffer, "%02d", speed);
    printToDisplay(lastRow.month, "");
    printToDisplay(lastRow.day, "");
    printToDisplay(lastRow.year, speedBuffer, 1);
    printToDisplay(lastRow.time, "MPH");
    lastRow.month.writeDisplay();
    lastRow.day.writeDisplay();
    lastRow.year.writeDisplay();
    lastRow.time.writeDisplay();
  #endif
}

/**
 * @brief A thread-safe wrapper to display the speedometer.
 * @param speed The speed (0-99) to display.
 */
void displaySpeed(int speed) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        displaySpeed_internal(speed);
        xSemaphoreGive(xDisplayHardwareMutex);
    }
}

/**
 * @brief Internal, non-locking function for the speedometer ramp-up effect.
 * @param speed The speed to display.
 */
void displaySpeedRamp_internal(int speed) {
#if ENABLE_HARDWARE
        char speedBuffer[5];
        sprintf(speedBuffer, "%02d", speed);
        printToDisplay(lastRow.month, "");
        printToDisplay(lastRow.time, "");
        printToDisplay(lastRow.day, speedBuffer, 2);
        printToDisplay(lastRow.year, "MPH");
        lastRow.month.writeDisplay();
        lastRow.day.writeDisplay();
        lastRow.year.writeDisplay();
        lastRow.time.writeDisplay();
#endif
}

/**
 * @brief A thread-safe wrapper for the speedometer ramp-up effect.
 * @param speed The speed to display.
 */
void displaySpeedRamp(int speed) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        displaySpeedRamp_internal(speed);
        xSemaphoreGive(xDisplayHardwareMutex);
    }
}

/**
 * @brief Internal, non-locking function for the "timeline skim" animation.
 * @details Creates an effect where the year rapidly changes while other date/time
 * components flicker randomly, simulating a fast skim through time.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param destinationYear The target year to approach.
 * @param isCountingUp If true, the year counts up; otherwise, it's random.
 */
void animateAllRowsTimelineSkim_internal(unsigned long elapsed, int duration, int destinationYear, bool isCountingUp) {
    #if ENABLE_HARDWARE
        float progress = (float)elapsed / duration;
        progress = 1 - pow(1 - progress, 3);
        static int startYear = 0;
        if(elapsed < 100) startYear = isCountingUp ? (destinationYear - 100) : random(1, 2100);
        int currentYear = startYear + (destinationYear - startYear) * progress;
        char yearStr[5];
        sprintf(yearStr, "%04d", currentYear);
        DisplayRow* rows[] = {&destRow, &presRow, &lastRow};
        for (int i=0; i<3; ++i) {
            printToDisplay(rows[i]->year, yearStr);
            if (isCountingUp) {
                printToDisplay(rows[i]->month, "---");
                printToDisplay(rows[i]->day, "--", 2);
                printToDisplay(rows[i]->time, "----");
            } else {
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
        }
    #endif
}

/**
 * @brief A thread-safe wrapper for the "timeline skim" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param destinationYear The target year to approach.
 * @param isCountingUp If true, the year counts up; otherwise, it's random.
 */
void animateAllRowsTimelineSkim(unsigned long elapsed, int duration, int destinationYear, bool isCountingUp) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateAllRowsTimelineSkim_internal(elapsed, duration, destinationYear, isCountingUp);
        xSemaphoreGive(xDisplayHardwareMutex);
        vTaskDelay(pdMS_TO_TICKS(4));
    }
}

/**
 * @brief Internal, non-locking function to light up all segments of all displays.
 */
void flashAllDisplays_internal() {
    #if ENABLE_HARDWARE
        DisplayRow* rows[] = {&destRow, &presRow, &lastRow};
        for (int i=0; i<3; ++i) {
            for(int j=0; j<8; ++j) {
                rows[i]->month.displaybuffer[j] = 0xFFFF;
                rows[i]->day.displaybuffer[j] = 0xFFFF;
                rows[i]->year.displaybuffer[j] = 0xFFFF;
                rows[i]->time.displaybuffer[j] = 0xFFFF;
            }
            rows[i]->month.writeDisplay();
            rows[i]->day.writeDisplay();
            rows[i]->year.writeDisplay();
            rows[i]->time.writeDisplay();
        }
    #endif
}

/**
 * @brief A thread-safe wrapper to light up all segments of all displays.
 */
void flashAllDisplays() {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        flashAllDisplays_internal();
        xSemaphoreGive(xDisplayHardwareMutex);
    }
}

/**
 * @brief Internal, non-locking function for the "tornado flicker" animation.
 * @details Displays random characters across all display rows.
 */
void animateTornadoFlicker_internal() {
  #if ENABLE_HARDWARE
    char buffer[5];
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    sprintf(buffer, "%04d", random(1000, 9999));
    printToDisplay(destRow.year, buffer);
    printToDisplay(destRow.month, months[random(0,12)], 1);
    sprintf(buffer, "%02d", random(1, 32));
    printToDisplay(destRow.day, buffer, 2);
    sprintf(buffer, "%02d%02d", random(0, 24), random(0, 60));
    printToDisplay(destRow.time, buffer);
    sprintf(buffer, "%04d", random(1000, 9999));
    printToDisplay(presRow.year, buffer);
    printToDisplay(presRow.month, months[random(0,12)], 1);
    sprintf(buffer, "%02d", random(1, 32));
    printToDisplay(presRow.day, buffer, 2);
    sprintf(buffer, "%02d%02d", random(0, 24), random(0, 60));
    printToDisplay(presRow.time, buffer);
    sprintf(buffer, "%04d", random(1000, 9999));
    printToDisplay(lastRow.year, buffer);
    printToDisplay(lastRow.month, months[random(0,12)], 1);
    sprintf(buffer, "%02d", random(1, 32));
    printToDisplay(lastRow.day, buffer, 2);
    sprintf(buffer, "%02d%02d", random(0, 24), random(0, 60));
    printToDisplay(lastRow.time, buffer);
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
  #endif
}

/**
 * @brief A thread-safe wrapper for the "tornado flicker" animation.
 */
void animateTornadoFlicker() {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateTornadoFlicker_internal();
        xSemaphoreGive(xDisplayHardwareMutex);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/**
 * @brief Internal, non-locking function to display random garbage data on all rows.
 */
void animateCorruptedData_internal() {
    #if ENABLE_HARDWARE
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
            rand_str(m_buf, 3); printToDisplay(rows[i]->month, m_buf, 1);
            rand_str(d_buf, 2); printToDisplay(rows[i]->day, d_buf, 2);
            rand_str(y_buf, 4); printToDisplay(rows[i]->year, y_buf);
            rand_str(t_buf, 4); printToDisplay(rows[i]->time, t_buf);
            rows[i]->month.writeDisplay();
            rows[i]->day.writeDisplay();
            rows[i]->year.writeDisplay();
            rows[i]->time.writeDisplay();
        }
    #endif
}

/**
 * @brief A thread-safe wrapper to display random garbage data on all rows.
 */
void animateCorruptedData() {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateCorruptedData_internal();
        xSemaphoreGive(xDisplayHardwareMutex);
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

/**
 * @brief Internal, non-locking function for a multi-phase "lock-on" animation.
 * @details This animation sequentially locks on the year, then the month, then the day,
 * creating a dramatic reveal effect.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 */
void animateLockOnSequence_internal(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
        unsigned long yearPhaseDuration = duration * 0.45;
        unsigned long monthPhaseDuration = duration * 0.35;
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
            if (elapsed < yearPhaseDuration) {
                sprintf(buffer, "%04d", random(1885, 2085));
                printToDisplay(rows[i]->year, buffer);
                printToDisplay(rows[i]->month, "---", 1);
                printToDisplay(rows[i]->day, "--", 2);
            } else {
                sprintf(buffer, "%04d", currentSettings.destinationYear);
                printToDisplay(rows[i]->year, buffer);
            }
            if (elapsed >= yearPhaseDuration && elapsed < yearPhaseDuration + monthPhaseDuration) {
                printToDisplay(rows[i]->month, months[random(0, 12)], 1);
                printToDisplay(rows[i]->day, "--", 2);
            } else if (elapsed >= yearPhaseDuration + monthPhaseDuration) {
                printToDisplay(rows[i]->month, months[dest_timeinfo.tm_mon], 1);
            }
            if (elapsed >= yearPhaseDuration + monthPhaseDuration) {
                sprintf(buffer, "%02d", random(1, dest_timeinfo.tm_mday + 1));
                printToDisplay(rows[i]->day, buffer, 2);
            }
            sprintf(buffer, "%02d%02d", random(0, 24), random(0, 60));
            printToDisplay(rows[i]->time, buffer);
            rows[i]->month.writeDisplay();
            rows[i]->day.writeDisplay();
            rows[i]->year.writeDisplay();
            rows[i]->time.writeDisplay();
        }
        if (xSemaphoreTake(xTimeLibMutex, portMAX_DELAY) == pdTRUE) {
            setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
            tzset();
            xSemaphoreGive(xTimeLibMutex);
        }
    #endif
}

/**
 * @brief A thread-safe wrapper for the multi-phase "lock-on" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 */
void animateLockOnSequence(unsigned long elapsed, int duration) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateLockOnSequence_internal(elapsed, duration);
        xSemaphoreGive(xDisplayHardwareMutex);
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

/**
 * @brief Internal, non-locking function to clear a single display row.
 * @param row The `DisplayRow` object to clear.
 */
void blankDisplayRow_internal(DisplayRow& row) {
    #if ENABLE_HARDWARE
        row.month.clear(); row.day.clear(); row.year.clear(); row.time.clear();
        row.month.writeDisplay(); row.day.writeDisplay(); row.year.writeDisplay(); row.time.writeDisplay();
    #endif
}

/**
 * @brief A thread-safe wrapper to clear a single display row.
 * @param row The `DisplayRow` object to clear.
 */
void blankDisplayRow(DisplayRow& row) {
    #if ENABLE_HARDWARE
        if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) != pdTRUE) { return; }
        blankDisplayRow_internal(row);
        xSemaphoreGive(xDisplayHardwareMutex);
    #endif
}

/**
 * @brief Internal, non-locking function for an "unstable skim" animation.
 * @details Similar to the timeline skim, but with the added effect of entire rows
 * randomly blanking out, creating a more unstable, glitchy appearance.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param destinationYear The target year to approach.
 */
void animateUnstableSkim_internal(unsigned long elapsed, int duration, int destinationYear) {
    #if ENABLE_HARDWARE
        static int blankingRow = -1;
        static unsigned long blankingStartTime = 0;
        if (blankingRow != -1 && millis() - blankingStartTime > 150) {
            blankingRow = -1;
        }
        if (blankingRow == -1 && random(100) < 5) {
            blankingRow = random(3);
            blankingStartTime = millis();
        }
        float progress = (float)elapsed / duration;
        progress = 1 - pow(1 - progress, 3);
        static int startYear = 0;
        if(elapsed < 100) startYear = random(1, 2100);
        int currentYear = startYear + (destinationYear - startYear) * progress;
        char yearStr[5];
        sprintf(yearStr, "%04d", currentYear);
        DisplayRow* rows[] = {&destRow, &presRow, &lastRow};
        for (int i=0; i<3; ++i) {
            if (i == blankingRow) {
                blankDisplayRow_internal(*rows[i]);
                continue;
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
        }
    #endif
}

/**
 * @brief A thread-safe wrapper for the "unstable skim" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param destinationYear The target year to approach.
 */
void animateUnstableSkim(unsigned long elapsed, int duration, int destinationYear) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateUnstableSkim_internal(elapsed, duration, destinationYear);
        xSemaphoreGive(xDisplayHardwareMutex);
        vTaskDelay(pdMS_TO_TICKS(4));
    }
}

/**
 * @brief Internal, non-locking function for the "temporal desync" animation.
 * @details Creates an effect where each row skims through time at a different rate,
 * suggesting a desynchronization of the time circuits.
 */
void animateTemporalDesync_internal() {
    #if ENABLE_HARDWARE
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
        };
        time_t baseTime = 1445433600;
        updateRowWithSkimmingTime(destRow, baseTime, 120);
        updateRowWithSkimmingTime(presRow, baseTime, 30);
        updateRowWithSkimmingTime(lastRow, baseTime, -60);
    #endif
}

/**
 * @brief A thread-safe wrapper for the "temporal desync" animation.
 */
void animateTemporalDesync() {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateTemporalDesync_internal();
        xSemaphoreGive(xDisplayHardwareMutex);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/**
 * @brief Internal, non-locking function to display random, valid dates and times.
 */
void animateRandomRealTimes_internal() {
#if ENABLE_HARDWARE
        DisplayRow* rows[] = {&destRow, &presRow, &lastRow};
        for (int i = 0; i < 3; ++i) {
            struct tm timeinfo;
            timeinfo.tm_mon = random(0, 12);
            timeinfo.tm_mday = random(1, 29);
            int year = random(1885, 2085);
            timeinfo.tm_hour = random(0, 24);
            timeinfo.tm_min = random(0, 60);
            updateDisplayRow_internal(*rows[i], timeinfo, year, false);
        }
#endif
}

/**
 * @brief A thread-safe wrapper to display random, valid dates and times.
 */
void animateRandomRealTimes() {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateRandomRealTimes_internal();
        xSemaphoreGive(xDisplayHardwareMutex);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/**
 * @brief Internal, non-locking function for a "capacitor charge-up" animation.
 * @details Fills each row sequentially with block characters, simulating a charging effect.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 */
void animateCapacitorChargeUp_internal(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
        int phase = elapsed / (duration / 3);
        float progress = (float)(elapsed % (duration / 3)) / (duration / 3);
        int charsToShow = progress * 16;
        auto fillRow = [&](DisplayRow& row, int numChars) {
            char buffer[17] = "################";
            if (numChars < 16) buffer[numChars] = '\0';
            String s_buffer(buffer);
            printToDisplay(row.month, s_buffer.substring(0, 3).c_str(), 1);
            printToDisplay(row.day, s_buffer.substring(3, 5).c_str(), 2);
            printToDisplay(row.year, s_buffer.substring(5, 9).c_str());
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
    #endif
}

/**
 * @brief A thread-safe wrapper for the "capacitor charge-up" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 */
void animateCapacitorChargeUp(unsigned long elapsed, int duration) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateCapacitorChargeUp_internal(elapsed, duration);
        xSemaphoreGive(xDisplayHardwareMutex);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/**
 * @brief Internal, non-locking function for a "digital rain" animation.
 * @details Creates a "Matrix"-style digital rain effect with characters falling down the displays.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 */
void animateDigitalRain_internal(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
        if (!rain_initialized) {
            for (int i = 0; i < MAX_RAINDROPS; i++) {
                raindrops[i].active = false;
            }
            rain_initialized = true;
        }
        destRow.month.clear(); destRow.day.clear(); destRow.year.clear(); destRow.time.clear();
        presRow.month.clear(); presRow.day.clear(); presRow.year.clear(); presRow.time.clear();
        lastRow.month.clear(); lastRow.day.clear(); lastRow.year.clear(); lastRow.time.clear();
        const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        const int numChars = strlen(chars);
        for (int i = 0; i < MAX_RAINDROPS; i++) {
            if (raindrops[i].active) {
                raindrops[i].y += raindrops[i].speed;
                if (raindrops[i].y >= 3.0) {
                    raindrops[i].active = false;
                } else {
                    int row_idx = (int)raindrops[i].y;
                    int col_idx = raindrops[i].column;
                    auto draw_char_at = [&](int r, int c, char ch) {
                        if (r < 0 || r > 2) return;
                        DisplayRow* p_row = (r == 0) ? &destRow : (r == 1) ? &presRow : &lastRow;
                        if (c < 3) {
                            p_row->month.writeDigitAscii(c + 1, ch);
                        } else if (c < 5) {
                            p_row->day.writeDigitAscii(c - 3 + 1, ch);
                        } else if (c < 9) {
                            p_row->year.writeDigitAscii(c - 5, ch);
                        } else if (c < 13) {
                            p_row->time.writeDigitAscii(c - 9, ch);
                        }
                    };
                    draw_char_at(row_idx, col_idx, chars[random(numChars)]);
                    int tail_row_idx = row_idx - 1;
                    if (tail_row_idx >= 0) {
                        draw_char_at(tail_row_idx, col_idx, '.');
                    }
                }
            }
        }
        if (random(100) < 45) {
            for (int i = 0; i < MAX_RAINDROPS; i++) {
                if (!raindrops[i].active) {
                    raindrops[i].active = true;
                    raindrops[i].column = random(13);
                    raindrops[i].y = 0.0f;
                    raindrops[i].speed = (random(8, 20)) / 100.0f;
                    break;
                }
            }
        }
        destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();
        presRow.month.writeDisplay(); presRow.day.writeDisplay(); presRow.year.writeDisplay(); presRow.time.writeDisplay();
        lastRow.month.writeDisplay(); lastRow.day.writeDisplay(); lastRow.year.writeDisplay(); lastRow.time.writeDisplay();
    #endif
}

/**
 * @brief A thread-safe wrapper for the "digital rain" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 */
void animateDigitalRain(unsigned long elapsed, int duration) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateDigitalRain_internal(elapsed, duration);
        xSemaphoreGive(xDisplayHardwareMutex);
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

/**
 * @brief Internal, non-locking function for a "waveform collapse" animation.
 * @details Displays an animated waveform that collapses and expands.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 */
void animateWaveformCollapse_internal(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
        const char* waves[] = { "-------------", " ---     --- ", "  ---   ---  ", "   -------   ", "  ---   ---  ", " ---     --- " };
        int waveIndex = (elapsed * 60 / duration) % 6;
        const char* pattern = waves[waveIndex];
        auto drawWave = [&](DisplayRow& row, bool inverse) {
            char p_month[4], p_day[3], p_year[5], p_time[5];
            char finalPattern[14];
            for(int i=0; i<13; ++i) {
                char baseChar = (pattern[i] == '-') ? '-' : ' ';
                finalPattern[i] = inverse ? ((baseChar == '-') ? ' ' : '-') : baseChar;
            }
            finalPattern[13] = '\0';
            strncpy(p_month, finalPattern + 0, 3); p_month[3] = '\0';
            strncpy(p_day,   finalPattern + 3, 2); p_day[2]   = '\0';
            strncpy(p_year,  finalPattern + 5, 4); p_year[4]  = '\0';
            strncpy(p_time,  finalPattern + 9, 4); p_time[4]  = '\0';
            printToDisplay(row.month, p_month, 1);
            printToDisplay(row.day,   p_day,   2);
            printToDisplay(row.year,  p_year);
            printToDisplay(row.time,  p_time);
            row.month.writeDisplay(); row.day.writeDisplay(); row.year.writeDisplay(); row.time.writeDisplay();
        };
        drawWave(destRow, false);
        drawWave(presRow, true);
        drawWave(lastRow, false);
    #endif
}

/**
 * @brief A thread-safe wrapper for the "waveform collapse" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 */
void animateWaveformCollapse(unsigned long elapsed, int duration) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateWaveformCollapse_internal(elapsed, duration);
        xSemaphoreGive(xDisplayHardwareMutex);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/**
 * @brief DEPRECATED. Another internal, non-locking function for a "timeline skim" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param destinationYear The target year to approach.
 */
void animateTimelineSkim_internal(unsigned long elapsed, int duration, int destinationYear) {
    #if ENABLE_HARDWARE
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
    #endif
}

/**
 * @brief DEPRECATED. A thread-safe wrapper for the second "timeline skim" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param destinationYear The target year to approach.
 */
void animateTimelineSkim(unsigned long elapsed, int duration, int destinationYear) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateTimelineSkim_internal(elapsed, duration, destinationYear);
        xSemaphoreGive(xDisplayHardwareMutex);
    }
}

/**
 * @brief Internal, non-locking function to clear all display segments on all rows.
 */
void blankAllDisplays_internal() {
  #if ENABLE_HARDWARE
    destRow.month.clear(); destRow.day.clear(); destRow.year.clear(); destRow.time.clear();
    presRow.month.clear(); presRow.day.clear(); presRow.year.clear(); presRow.time.clear();
    lastRow.month.clear(); lastRow.day.clear(); lastRow.year.clear(); lastRow.time.clear();
    destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();
    presRow.month.writeDisplay(); presRow.day.writeDisplay(); presRow.year.writeDisplay(); presRow.time.writeDisplay();
    lastRow.month.writeDisplay(); lastRow.day.writeDisplay(); lastRow.year.writeDisplay(); lastRow.time.writeDisplay();
  #endif
}

/**
 * @brief A thread-safe wrapper to clear all display segments on all rows.
 */
void blankAllDisplays() {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        blankAllDisplays_internal();
        xSemaphoreGive(xDisplayHardwareMutex);
    }
}

/**
 * @brief Internal, non-locking function to display "88 MPH".
 * @param speed The speed value (unused, kept for signature compatibility).
 */
void display88MphSpeed_internal(float speed) {
  #if ENABLE_HARDWARE
    printToDisplay(lastRow.day, "88", 2);
    printToDisplay(lastRow.year, "MPH");
    lastRow.day.writeDisplay();
    lastRow.year.writeDisplay();
  #endif
}

/**
 * @brief A thread-safe wrapper to display "88 MPH".
 * @param speed The speed value (unused, kept for signature compatibility).
 */
void display88MphSpeed(float speed) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        display88MphSpeed_internal(speed);
        xSemaphoreGive(xDisplayHardwareMutex);
    }
}

/**
 * @brief Plays a sound effect from LittleFS.
 * @details This function handles playing audio files. It includes a guard to prevent
 * sound effects from interrupting an active internet radio stream. It also supports
 * a volume override.
 * @param filepath The path to the audio file in the LittleFS filesystem (e.g., "/hum.mp3").
 * @param fromMqtt `true` if the sound was triggered by an MQTT message.
 * @param volume The volume to play the sound at (0-21), or -1 to use the default setting.
 */
void playSound(const char* filepath, bool fromMqtt, int volume) {
    #if ENABLE_HARDWARE
        if (isRadioStreaming) {
            Log_printf(LOG_LEVEL_INFO, "Radio is streaming, skipping sound effect: %s", filepath);
            return;
        }
        char fullPath[MAX_FILENAME_LENGTH];
        if (filepath[0] == '/') {
            strncpy(fullPath, filepath, MAX_FILENAME_LENGTH);
        } else {
            snprintf(fullPath, MAX_FILENAME_LENGTH, "/%s", filepath);
        }
        fullPath[MAX_FILENAME_LENGTH - 1] = '\0';
        Log_printf(LOG_LEVEL_INFO, "Request to play sound: %s (fromMqtt: %s, volume: %d)", fullPath, fromMqtt ? "true" : "false", volume);
        if (!LittleFS.exists(fullPath)) {
            Log_printf(LOG_LEVEL_WARN, "Audio file not found: %s", fullPath);
            return;
        }
        isSoundFromMqtt = fromMqtt;
        isPlayingSound = true;
        digitalWrite(I2S_SD_PIN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(10));

        // --- NEW: Volume override logic ---
        int final_volume = (volume == -1) ? currentSettings.notificationVolume : volume;
        if (final_volume > 21) {
            Log_printf(LOG_LEVEL_WARN, "Requested volume %d exceeds max of 21. Capping.", final_volume);
            final_volume = 21;
        }
        audio.setVolume(final_volume);

        strncpy(currentSoundFile, fullPath, MAX_FILENAME_LENGTH - 1);
        currentSoundFile[MAX_FILENAME_LENGTH - 1] = '\0';
        if (audio.connecttoFS(LittleFS, currentSoundFile)) {
            Log_printf(LOG_LEVEL_INFO, "Started playing: %s with volume %d", currentSoundFile, final_volume);
        } else {
            Log_printf(LOG_LEVEL_ERROR, "Failed to connect to audio file: %s", currentSoundFile);
            currentSoundFile[0] = '\0';
            isPlayingSound = false;
            isSoundFromMqtt = false;
            digitalWrite(I2S_SD_PIN, LOW);
        }
    #endif
}

/**
 * @brief Internal, non-locking function to display text with a typewriter effect.
 * @param row The `DisplayRow` to type on.
 * @param text The text to display.
 * @param typeDelay The delay in milliseconds between each character.
 * @param withCursor (Unused) A placeholder for adding a cursor effect.
 */
void typeTextOnDisplay_internal(DisplayRow& row, const char* text, int typeDelay, bool withCursor) {
  #if ENABLE_HARDWARE
    int len = strlen(text);
    if (len > 13) len = 13;
    char currentText[14];
    for (int i = 1; i <= len; i++) {
        strncpy(currentText, text, i);
        currentText[i] = '\0';
        char monthBuf[4] = "";
        char dayBuf[3] = "";
        char yearBuf[5] = "";
        char timeBuf[5] = "";
        strncpy(monthBuf, currentText, 3);
        monthBuf[3] = '\0';
        if (i > 3) {
            strncpy(dayBuf, currentText + 3, 2);
            dayBuf[2] = '\0';
        }
        if (i > 5) {
            strncpy(yearBuf, currentText + 5, 4);
            yearBuf[4] = '\0';
        }
        if (i > 9) {
            strncpy(timeBuf, currentText + 9, 4);
            timeBuf[4] = '\0';
        }
        printToDisplay(row.month, monthBuf, 1);
        printToDisplay(row.day, dayBuf, 2);
        printToDisplay(row.year, yearBuf, 0);
        printToDisplay(row.time, timeBuf, 0);
        row.month.writeDisplay();
        row.day.writeDisplay();
        row.year.writeDisplay();
        row.time.writeDisplay();
        vTaskDelay(pdMS_TO_TICKS(typeDelay));
    }
  #endif
}

/**
 * @brief A thread-safe wrapper to display text with a typewriter effect.
 * @param row The `DisplayRow` to type on.
 * @param text The text to display.
 * @param typeDelay The delay in milliseconds between each character.
 * @param withCursor (Unused) A placeholder for adding a cursor effect.
 */
void typeTextOnDisplay(DisplayRow& row, const char* text, int typeDelay, bool withCursor) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        typeTextOnDisplay_internal(row, text, typeDelay, withCursor);
        xSemaphoreGive(xDisplayHardwareMutex);
    }
}

/**
 * @brief Internal, non-locking function for a simple "flux capacitor" animation.
 * @details Displays "FLX CP ACTV" with a simple frame-based animation.
 */
void animateFluxCapacitor_internal() {
  #if ENABLE_HARDWARE
    static int frame = 0;
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
    frame = (frame + 1) % 3;
    presRow.month.writeDisplay();
    presRow.day.writeDisplay();
    presRow.year.writeDisplay();
  #endif
}

/**
 * @brief A thread-safe wrapper for the "flux capacitor" animation.
 */
void animateFluxCapacitor() {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateFluxCapacitor_internal();
        xSemaphoreGive(xDisplayHardwareMutex);
    }
}

/**
 * @brief Internal, non-locking function to display static "FLX CP ACTV" text.
 */
void displayStaticFluxText_internal() {
    #if ENABLE_HARDWARE
        printToDisplay(presRow.month, "FLX", 1);
        printToDisplay(presRow.day, "CP", 2);
        printToDisplay(presRow.year, "ACTV");
        printToDisplay(presRow.time, "");
        presRow.month.writeDisplay();
        presRow.day.writeDisplay();
        presRow.year.writeDisplay();
        presRow.time.writeDisplay();
    #endif
}

/**
 * @brief A thread-safe wrapper to display static "FLX CP ACTV" text.
 */
void displayStaticFluxText() {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        displayStaticFluxText_internal();
        xSemaphoreGive(xDisplayHardwareMutex);
    }
}

/**
 * @brief Internal, non-locking function to apply the global brightness setting to all displays.
 */
void applyBrightness_internal() {
  #if ENABLE_HARDWARE
    uint8_t brightnessValue = currentSettings.brightness;
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
  #endif
}

/**
 * @brief A thread-safe wrapper to apply the global brightness setting to all displays.
 */
void applyBrightness() {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        applyBrightness_internal();
        xSemaphoreGive(xDisplayHardwareMutex);
    }
}

/**
 * @brief Internal, non-locking function for a "sequential flicker" animation.
 * @details Reveals the display segments one by one in a sequence.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 */
void animateSequentialFlicker_internal(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
        float progress = (float)elapsed / duration;
        int segmentsToShow = (int)(progress * 12);
        Adafruit_AlphaNum4* all_displays[] = {
            &destRow.month, &destRow.day, &destRow.year, &destRow.time,
            &presRow.month, &presRow.day, &presRow.year, &presRow.time,
            &lastRow.month, &lastRow.day, &lastRow.year, &lastRow.time
        };
        time_t now_t;
        struct tm dest_timeinfo;
        if (xSemaphoreTake(xTimeLibMutex, portMAX_DELAY) == pdTRUE) {
            time(&now_t);
            setenv("TZ", TZ_DATA[currentSettings.destinationTimezoneIndex].tzString, 1);
            tzset();
            localtime_r(&now_t, &dest_timeinfo);
            dest_timeinfo.tm_year = currentSettings.destinationYear - 1900;
            xSemaphoreGive(xTimeLibMutex);
        }
        struct tm present_timeinfo;
        bool showDecimalForPresent = (millis() / 1000) % 2 == 0;
        if (xSemaphoreTake(xTimeLibMutex, portMAX_DELAY) == pdTRUE) {
            setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
            tzset();
            localtime_r(&now_t, &present_timeinfo);
            xSemaphoreGive(xTimeLibMutex);
        }
        struct tm lastTimeDepartedInfo = {0};
        lastTimeDepartedInfo.tm_year = currentSettings.lastTimeDepartedYear - 1900;
        lastTimeDepartedInfo.tm_mon = currentSettings.lastTimeDepartedMonth - 1;
        lastTimeDepartedInfo.tm_mday = currentSettings.lastTimeDepartedDay;
        lastTimeDepartedInfo.tm_hour = currentSettings.lastTimeDepartedHour;
        lastTimeDepartedInfo.tm_min = currentSettings.lastTimeDepartedMinute;
        char buffer[5];
        const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
        auto updateSegment = [&](int index, Adafruit_AlphaNum4* display, const struct tm& time, int year, bool showDecimal = false) {
            if (index >= segmentsToShow) {
                display->clear();
                return;
            }
            int segmentIndex = index % 4;
            switch(segmentIndex) {
                case 0:
                    printToDisplay(*display, months[time.tm_mon], 1);
                    break;
                case 1:
                    sprintf(buffer, "%02d", time.tm_mday);
                    printToDisplay(*display, buffer, 2);
                    break;
                case 2:
                    sprintf(buffer, "%04d", year);
                    printToDisplay(*display, buffer);
                    break;
                case 3:
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
        for (int i = 0; i < 12; ++i) {
            all_displays[i]->writeDisplay();
        }
        if (xSemaphoreTake(xTimeLibMutex, portMAX_DELAY) == pdTRUE) {
            setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
	        tzset();
            xSemaphoreGive(xTimeLibMutex);
        }
    #endif
}

/**
 * @brief A thread-safe wrapper for the "sequential flicker" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 */
void animateSequentialFlicker(unsigned long elapsed, int duration) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateSequentialFlicker_internal(elapsed, duration);
        xSemaphoreGive(xDisplayHardwareMutex);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/**
 * @brief Internal, non-locking function for a "counting up" animation.
 * @details Rapidly cycles through dates and times on all rows.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 */
void animateCountingUp_internal(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
        DisplayRow* rows[] = {&destRow, &presRow, &lastRow};
        char buffer[5];
        time_t startTime = 1445433600;
        time_t fastForwardTime = startTime + (elapsed * 60);
        struct tm timeinfo;
        gmtime_r(&fastForwardTime, &timeinfo);
        const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
        for (int i = 0; i < 3; ++i) {
            time_t rowTime = fastForwardTime + (i * 3600 * 24 * 157);
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
        }
    #endif
}

/**
 * @brief A thread-safe wrapper for the "counting up" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 */
void animateCountingUp(unsigned long elapsed, int duration) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateCountingUp_internal(elapsed, duration);
        xSemaphoreGive(xDisplayHardwareMutex);
        vTaskDelay(pdMS_TO_TICKS(4));
    }
}

/**
 * @brief Internal, non-locking function for a "glitchy jump-cut" animation.
 * @details Creates a glitch effect by randomly flickering and blanking segments.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 */
void animateGlitchyJumpCut_internal(unsigned long elapsed, int duration) {
  #if ENABLE_HARDWARE
    float progress = (float)elapsed / duration;
    if (progress > 1.0) progress = 1.0;
    DisplayRow* rows[] = {&destRow, &presRow, &lastRow};
    if (progress < 0.9) {
        for (int i = 0; i < 3; ++i) {
            animateDisplayRowRandomly(*rows[i]);
            if (random(100) < 50) {
                int segmentToGlitch = random(4);
                Adafruit_AlphaNum4* segment;
                switch(segmentToGlitch) {
                    case 0: segment = &rows[i]->month; break;
                    case 1: segment = &rows[i]->day; break;
                    case 2: segment = &rows[i]->year; break;
                    default: segment = &rows[i]->time; break;
                }
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
        updateNormalClockDisplay(true, true, true);
    }
  #endif
}

/**
 * @brief A thread-safe wrapper for the "glitchy jump-cut" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 */
void animateGlitchyJumpCut(unsigned long elapsed, int duration) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateGlitchyJumpCut_internal(elapsed, duration);
        xSemaphoreGive(xDisplayHardwareMutex);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/**
 * @brief Internal, non-locking function for a "plasma warm-up" animation.
 * @details Simulates a plasma effect by flickering special characters with increasing brightness.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 */
void animatePlasmaWarmUp_internal(unsigned long elapsed, int duration) {
  #if ENABLE_HARDWARE
    float progress = (float)elapsed / duration;
    if (progress > 1.0) progress = 1.0;
    if (progress < 0.9) {
        uint8_t targetBrightness = progress * 1.1 * currentSettings.brightness;
        if (targetBrightness > currentSettings.brightness) targetBrightness = currentSettings.brightness;
        uint8_t currentBrightness = random(100) < 40 ? targetBrightness * 0.6 : targetBrightness;
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
                    if (random(100) < progress * 110) {
                        segment->writeDigitAscii(c, plasmaChars[random(numPlasmaChars)]);
                    }
                }
                segment->writeDisplay();
            }
        }
    } else {
        updateNormalClockDisplay(true, true, true);
        applyBrightness();
    }
  #endif
}

/**
 * @brief A thread-safe wrapper for the "plasma warm-up" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 */
void animatePlasmaWarmUp(unsigned long elapsed, int duration) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animatePlasmaWarmUp_internal(elapsed, duration);
        xSemaphoreGive(xDisplayHardwareMutex);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

/**
 * @brief Internal, non-locking function for a "time warp streaks" animation.
 * @details Slides in the final text from the right, creating a streaking effect.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param final_dest The final destination time string to display.
 * @param final_pres The final present time string to display.
 * @param final_last The final last time departed string to display.
 */
void animateTimeWarpStreaks_internal(unsigned long elapsed, int duration, const char* final_dest, const char* final_pres, const char* final_last) {
  #if ENABLE_HARDWARE
    float progress = (float)elapsed / duration;
    if (progress > 1.0) progress = 1.0;
    int slideInPos = 16 * progress;
    auto streakRow = [&](DisplayRow& row, const char* final_str) {
        char display_str[17] = "                ";
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

/**
 * @brief A thread-safe wrapper for the "time warp streaks" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param final_dest The final destination time string to display.
 * @param final_pres The final present time string to display.
 * @param final_last The final last time departed string to display.
 */
void animateTimeWarpStreaks(unsigned long elapsed, int duration, const char* final_dest, const char* final_pres, const char* final_last) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateTimeWarpStreaks_internal(elapsed, duration, final_dest, final_pres, last_str);
        xSemaphoreGive(xDisplayHardwareMutex);
    }
}

/**
 * @brief Internal, non-locking function for a "character scanline" animation.
 * @details Reveals the final text one character at a time, like a scanline.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param dest_str The final destination time string to display.
 * @param pres_str The final present time string to display.
 * @param last_str The final last time departed string to display.
 */
void animateCharacterScanline_internal(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
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

/**
 * @brief A thread-safe wrapper for the "character scanline" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param dest_str The final destination time string to display.
 * @param pres_str The final present time string to display.
 * @param last_str The final last time departed string to display.
 */
void animateCharacterScanline(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateCharacterScanline_internal(elapsed, duration, dest_str, pres_str, last_str);
        xSemaphoreGive(xDisplayHardwareMutex);
    }
}

/**
 * @brief Internal, non-locking function for a "focus in" animation.
 * @details Reveals the final text by replacing random characters with the correct ones.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param dest_str The final destination time string to display.
 * @param pres_str The final present time string to display.
 * @param last_str The final last time departed string to display.
 */
void animateFocusIn_internal(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
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

/**
 * @brief A thread-safe wrapper for the "focus in" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param dest_str The final destination time string to display.
 * @param pres_str The final present time string to display.
 * @param last_str The final last time departed string to display.
 */
void animateFocusIn(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateFocusIn_internal(elapsed, duration, dest_str, pres_str, last_str);
        xSemaphoreGive(xDisplayHardwareMutex);
    }
}

/**
 * @brief Internal, non-locking function for a "code breaker" animation.
 * @details Similar to "focus in," but uses a different character set for the scramble.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param dest_str The final destination time string to display.
 * @param pres_str The final present time string to display.
 * @param last_str The final last time departed string to display.
 */
void animateCodeBreaker_internal(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
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

/**
 * @brief A thread-safe wrapper for the "code breaker" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param dest_str The final destination time string to display.
 * @param pres_str The final present time string to display.
 * @param last_str The final last time departed string to display.
 */
void animateCodeBreaker(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateCodeBreaker_internal(elapsed, duration, dest_str, pres_str, last_str);
        xSemaphoreGive(xDisplayHardwareMutex);
    }
}

/**
 * @brief Internal, non-locking function for a "temporal paradox" animation.
 * @details Swaps the top and middle rows back and forth while the bottom row glitches.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param dest_str The final destination time string to display.
 * @param pres_str The final present time string to display.
 * @param last_str The final last time departed string to display.
 */
void animateTemporalParadox_internal(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
  #if ENABLE_HARDWARE
    bool show_swapped = (elapsed / 200) % 2 == 0;
    const char* top_row_str = show_swapped ? pres_str : dest_str;
    const char* middle_row_str = show_swapped ? dest_str : pres_str;
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
    animateDisplayRowRandomly(lastRow);
  #endif
}

/**
 * @brief A thread-safe wrapper for the "temporal paradox" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param dest_str The final destination time string to display.
 * @param pres_str The final present time string to display.
 * @param last_str The final last time departed string to display.
 */
void animateTemporalParadox(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateTemporalParadox_internal(elapsed, duration, dest_str, pres_str, last_str);
        xSemaphoreGive(xDisplayHardwareMutex);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/**
 * @brief Internal, non-locking function for a "digit cascade" animation.
 * @details Reveals the final text by "cascading" in the correct segments from random garbage.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param dest_str The final destination time string to display.
 * @param pres_str The final present time string to display.
 * @param last_str The final last time departed string to display.
 */
void animateDigitCascade_internal(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
  #if ENABLE_HARDWARE
    float progress = (float)elapsed / duration;
    if (progress > 1.0) progress = 1.0;
    int segmentsToLock = progress * 12;
    auto cascadeRow = [&](DisplayRow& row, const char* final_str, int start_index) {
        char month_buf[4], day_buf[3], year_buf[5], time_buf[5];
        if (segmentsToLock > start_index) {
            strncpy(month_buf, final_str, 3);
            month_buf[3] = '\0';
        } else {
            for (int i = 0; i < 3; i++) {
                month_buf[i] = random(256);
            }
            month_buf[3] = '\0';
        }
        if (segmentsToLock > start_index + 1) {
            strncpy(day_buf, final_str + 3, 2);
            day_buf[2] = '\0';
        } else {
            sprintf(day_buf, "%02d", random(0, 99));
        }
        if (segmentsToLock > start_index + 2) {
            strncpy(year_buf, final_str + 5, 4);
            year_buf[4] = '\0';
        } else {
            sprintf(year_buf, "%04d", random(0, 9999));
        }
        if (segmentsToLock > start_index + 3) {
            strncpy(time_buf, final_str + 9, 4);
            time_buf[4] = '\0';
        } else {
            sprintf(time_buf, "%04d", random(0, 9999));
        }
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

/**
 * @brief A thread-safe wrapper for the "digit cascade" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param dest_str The final destination time string to display.
 * @param pres_str The final present time string to display.
 * @param last_str The final last time departed string to display.
 */
void animateDigitCascade(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateDigitCascade_internal(elapsed, duration, dest_str, pres_str, last_str);
        xSemaphoreGive(xDisplayHardwareMutex);
    }
}

/**
 * @brief Internal, non-locking function for an "electric surge" animation.
 * @details Simulates a surge of electricity that reveals the final text.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param dest_str The final destination time string to display.
 * @param pres_str The final present time string to display.
 * @param last_str The final last time departed string to display.
 */
void animateElectricSurge_internal(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
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

/**
 * @brief A thread-safe wrapper for the "electric surge" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param dest_str The final destination time string to display.
 * @param pres_str The final present time string to display.
 * @param last_str The final last time departed string to display.
 */
void animateElectricSurge(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateElectricSurge_internal(elapsed, duration, dest_str, pres_str, last_str);
        xSemaphoreGive(xDisplayHardwareMutex);
    }
}

/**
 * @brief Internal, non-locking function for a "flip-disc" animation.
 * @details Simulates a mechanical flip-disc display revealing the text.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param dest_str The final destination time string to display.
 * @param pres_str The final present time string to display.
 * @param last_str The final last time departed string to display.
 */
void animateFlipDiscDisplay_internal(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
  #if ENABLE_HARDWARE
    float progress = (float)elapsed / duration;
    if (progress > 1.0) progress = 1.0;
    int chars_to_show = progress * 13;
    auto flipRow = [&](DisplayRow& row, const char* final_str) {
        char current_str[14];
        const char* flip_chars = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        int flip_chars_len = strlen(flip_chars);
        for (int i = 0; i < 13; i++) {
            if (i < chars_to_show) {
                current_str[i] = final_str[i];
            } else if (i == chars_to_show) {
                current_str[i] = flip_chars[random(flip_chars_len)];
            } else {
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

/**
 * @brief A thread-safe wrapper for the "flip-disc" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param dest_str The final destination time string to display.
 * @param pres_str The final present time string to display.
 * @param last_str The final last time departed string to display.
 */
void animateFlipDiscDisplay(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateFlipDiscDisplay_internal(elapsed, duration, dest_str, pres_str, last_str);
        xSemaphoreGive(xDisplayHardwareMutex);
    }
}

/**
 * @brief Internal, non-locking function for an "interference pattern" animation.
 * @details Creates a visual static/interference effect that resolves to the final text.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param dest_str The final destination time string to display.
 * @param pres_str The final present time string to display.
 * @param last_str The final last time departed string to display.
 */
void animateInterferencePattern_internal(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
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
 * @brief A thread-safe wrapper for the "interference pattern" animation.
 * @param elapsed The time in milliseconds since the animation started.
 * @param duration The total duration of the animation in milliseconds.
 * @param dest_str The final destination time string to display.
 * @param pres_str The final present time string to display.
 * @param last_str The final last time departed string to display.
 */
void animateInterferencePattern(unsigned long elapsed, int duration, const char* dest_str, const char* pres_str, const char* last_str) {
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        animateInterferencePattern_internal(elapsed, duration, dest_str, pres_str, last_str);
        xSemaphoreGive(xDisplayHardwareMutex);
    }
}

/**
 * @brief A thread-safe wrapper for `printf` to the Serial port.
 * @details This function uses a mutex to prevent garbled output when multiple FreeRTOS
 * tasks are logging to the Serial port simultaneously.
 * @param format The format string, as in `printf`.
 * @param ... The variable arguments for the format string.
 */
void safe_printf(const char *format, ...) {
    static char buf[2500];
    va_list args;
    va_start(args, format);
    if (xSerialMutex == NULL) {
        Serial.vprintf(format, args);
    } else {
        if (xSemaphoreTake(xSerialMutex, portMAX_DELAY) == pdTRUE) {
            int len = vsnprintf(buf, sizeof(buf), format, args);
            if (len >= 0) {
                const size_t chunkSize = 64;
                for (int i = 0; i < len; i += chunkSize) {
                    Serial.write(&buf[i], min((size_t)len - i, chunkSize));
                }
            }
            xSemaphoreGive(xSerialMutex);
        } else {
            Serial.println("FATAL: Could not take serial mutex!");
            vsnprintf(buf, sizeof(buf), format, args);
            Serial.print(buf);
        }
    }
    va_end(args);
}

/**
 * @brief DEPRECATED. Resets a specified I2C bus.
 * @details This function was originally intended to recover a stuck I2C bus by
 * re-initializing it. However, this has been found to cause system instability
 * and is now disabled. It remains as a no-op to avoid breaking any code that might
 * still call it.
 * @param i2c_num The I2C bus number to reset (0 or 1).
 */
void resetI2CBus(int i2c_num) {
    #if ENABLE_HARDWARE
    Log_printf(LOG_LEVEL_WARN, "I2C bus reset for bus #%d requested, but this action is now disabled to prevent system instability.", i2c_num);
    #endif
}