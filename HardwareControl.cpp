/**
 * @file HardwareControl.cpp
 * @brief Implements low-level control functions for displays, LEDs, and sound.
 * @details This file contains the concrete implementations for initializing and controlling
 * the hardware components. It directly interfaces with the Adafruit GFX and LED Backpack
 * libraries, as well as the ESP8266Audio library for I2S sound.
 */

#include "HardwareControl.h"
#include "EventManager.h"
#include "DisplayManager.h"
#include "LittleFS.h"

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

#endif

// --- HELPER FUNCTION ---
/**
 * @brief Writes a string to a 4-character alphanumeric display with justification.
 * @param display The Adafruit_AlphaNum4 object to write to.
 * @param text The C-string to display.
 * @param justification 0 for left, 1 for right, 2 for center.
 * @param width The physical character width of the display segment.
 */
void printToDisplay(Adafruit_AlphaNum4 &display, const char* text, int justification, int width) {
  display.clear();
  int len = strlen(text);
  int startPos = 0;

  // Calculate the starting position based on justification and logical width.
  if (justification == 1) { // Right Justify
    startPos = width - len;
  } else if (justification == 2) { // Center Justify
    startPos = (width - len) / 2;
  }

  // FIX: Apply a physical offset for the month and day displays.
  // This shifts the text one character to the right as requested.
  if (width == 3 || width == 2) {
      startPos += 1;
  }

  // Ensure start position is non-negative
  if (startPos < 0) {
      startPos = 0;
  }

  // FIX: The loop must iterate over all 4 physical characters of the display segment.
  for (int i = 0; i < 4; i++) {
    if (i >= startPos && i < (startPos + len)) {
      display.writeDigitAscii(i, text[i - startPos]);
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
 * with its correct I2C address and bus. Finally, it newpinModeures the AM/PM indicator
 * LED pins and the I2S amplifier shutdown pin as outputs.
 */
void setupPhysicalDisplay() {
  #if ENABLE_HARDWARE
  // Initialize both I2C buses with their respective SDA/SCL pins.
  I2C_1.begin(I2C_SDA_1, I2C_SCL_1, 50000);
  I2C_2.begin(I2C_SDA_2, I2C_SCL_2, 50000);

  // Initialize the Adafruit_AlphaNum4 objects.
  destRow = {Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4()};
  presRow = {Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4()};
  lastRow = {Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4()};

  // Associate each display module with its I2C address and bus.
  // Destination and Present rows are on I2C bus 1.
  destRow.month.begin(0x70, &I2C_1); destRow.day.begin(0x71, &I2C_1); destRow.year.begin(0x72, &I2C_1); destRow.time.begin(0x73, &I2C_1);
  presRow.month.begin(0x74, &I2C_1); presRow.day.begin(0x75, &I2C_1); presRow.year.begin(0x76, &I2C_1); presRow.time.begin(0x77, &I2C_1);
  // Last Time Departed row is on I2C bus 2.
  lastRow.month.begin(0x70, &I2C_2); lastRow.day.begin(0x71, &I2C_2); lastRow.year.begin(0x72, &I2C_2); lastRow.time.begin(0x73, &I2C_2);

  // Set LED indicator pins to output mode.
  pinMode(DEST_AM_PIN, OUTPUT); pinMode(DEST_PM_PIN, OUTPUT);
  pinMode(PRES_AM_PIN, OUTPUT); pinMode(PRES_PM_PIN, OUTPUT);
  pinMode(LAST_AM_PIN, OUTPUT); pinMode(LAST_PM_PIN, OUTPUT);
  
  // Set I2S Amplifier Shutdown pin to output mode and enable the amplifier.
  pinMode(I2S_SD_PIN, OUTPUT);
  digitalWrite(I2S_SD_PIN, HIGH);
  #endif
}
void updateDisplayRow(DisplayRow& row, const struct tm& timeinfo, int year, bool showDecimal) {
  #if ENABLE_HARDWARE
  char buffer[5];
  const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  
  // Month (3 chars, right justified)
  printToDisplay(row.month, months[timeinfo.tm_mon], 1, 3);

  // Day (2 chars, center justified)
  sprintf(buffer, "%02d", timeinfo.tm_mday);
  printToDisplay(row.day, buffer, 2, 2);

  // Year (4 chars)
  sprintf(buffer, "%04d", year);
  printToDisplay(row.year, buffer, 0, 4);

  // --- Time Display Logic (Corrected) ---
  int displayHour = timeinfo.tm_hour;

  // Identify which row we are updating
  int rowIndex = -1;
  if (&row == &destRow) rowIndex = 0;
  else if (&row == &presRow) rowIndex = 1;
  else if (&row == &lastRow) rowIndex = 2;

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

  // Display the Hour and Minute using the library's built-in dot parameter
  row.time.clear();
  char timeBuffer[5];
  sprintf(timeBuffer, "%02d%02d", displayHour, timeinfo.tm_min);

  row.time.writeDigitAscii(0, timeBuffer[0]);
  row.time.writeDigitAscii(1, timeBuffer[1], showDecimal); // Apply dot to the SECOND character
  row.time.writeDigitAscii(2, timeBuffer[2]);
  row.time.writeDigitAscii(3, timeBuffer[3]);
  
  // Write all changes to the hardware
  row.month.writeDisplay();
  row.day.writeDisplay();
  row.year.writeDisplay();
  row.time.writeDisplay();
  #endif
}

void animateTemporalLockOn(DisplayRow& row, const struct tm& timeinfo, int year, bool showDecimal) {
    #if ENABLE_HARDWARE
    // 50% chance to show the correct time, 50% chance to show random garbage.
    if (random(100) < 50) {
        updateDisplayRow(row, timeinfo, year, showDecimal);
    } else {
        animateDisplayRowRandomly(row);
    }
    #endif
}

// In HardwareControl.cpp

void animateDisplayRowRandomly(DisplayRow& row, int flickerProbability) {
  #if ENABLE_HARDWARE
    char buffer[5];
    // Animate year
    if(random(100) < flickerProbability) {
      sprintf(buffer, "%04d", random(1000, 9999));
      printToDisplay(row.year, buffer, 0, 4);
      row.year.writeDisplay();
      vTaskDelay(pdMS_TO_TICKS(5)); 
    }

    // Animate month
    if(random(100) < flickerProbability) {
      const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
      printToDisplay(row.month, months[random(0,12)], 1, 3); // Right justified
      row.month.writeDisplay();
      vTaskDelay(pdMS_TO_TICKS(5)); 
    }

    // Animate day
    if(random(100) < flickerProbability) {
      sprintf(buffer, "%02d", random(1, 32));
      printToDisplay(row.day, buffer, 2, 2); // Center justified
      row.day.writeDisplay();
      vTaskDelay(pdMS_TO_TICKS(5)); 
    }

    // Animate time
    if(random(100) < flickerProbability) {
      sprintf(buffer, "%02d%02d", random(0, 24), random(0, 60));
      printToDisplay(row.time, buffer, 0, 4);
      row.time.writeDisplay();
      vTaskDelay(pdMS_TO_TICKS(5)); 
    }
  #endif
}
void displaySpeed(int speed) {
  #if ENABLE_HARDWARE
  char speedBuffer[5];
  sprintf(speedBuffer, "%02d", speed);

  // Clear the first two displays of the bottom row.
  printToDisplay(lastRow.month, "", 0, 3);
  printToDisplay(lastRow.day, "", 0, 2);
  
  // Display speed on the "year" slot and "MPH" on the "time" slot.
  printToDisplay(lastRow.year, speedBuffer, 1, 4); // Right-justify speed.
  printToDisplay(lastRow.time, "MPH", 0, 4);

  lastRow.month.writeDisplay();
  lastRow.day.writeDisplay();
  lastRow.year.writeDisplay();
  lastRow.time.writeDisplay();
  #endif
}
void displaySpeedRamp(int speed) {
#if ENABLE_HARDWARE
    char speedBuffer[5];
    sprintf(speedBuffer, "%02d", speed);
    
    // Clear unused segments
    printToDisplay(lastRow.month, "", 0, 3);
    printToDisplay(lastRow.time, "", 0, 4);
    
    // Display speed on "day" and "MPH" on "year"
    printToDisplay(lastRow.day, speedBuffer, 2, 2);
    printToDisplay(lastRow.year, "MPH", 0, 4);

    lastRow.month.writeDisplay();
    lastRow.day.writeDisplay();
    lastRow.year.writeDisplay();
    lastRow.time.writeDisplay();
#endif
}

// In HardwareControl.cpp

void animateAllRowsTimelineSkim(unsigned long elapsed, int duration, int destinationYear, bool isCountingUp) {
    #if ENABLE_HARDWARE
    float progress = (float)elapsed / duration;
    progress = 1 - pow(1 - progress, 3); // Ease-out curve

    static int startYear = 0;
    if(elapsed < 100) startYear = isCountingUp ? (destinationYear - 100) : random(1, 2100);

    int currentYear = startYear + (destinationYear - startYear) * progress;
    char yearStr[5];
    sprintf(yearStr, "%04d", currentYear);

    DisplayRow* rows[] = {&destRow, &presRow, &lastRow};
    for (int i=0; i<3; ++i) {
        printToDisplay(rows[i]->year, yearStr, 0, 4);
         
        if (isCountingUp) {
            // For "Counting Up", keep other fields static to focus on the year
            printToDisplay(rows[i]->month, "---", 1, 3);
            printToDisplay(rows[i]->day, "--", 2, 2);
            printToDisplay(rows[i]->time, "----", 0, 4);
        } else {
            // For "Timeline Skim", use a high-speed random blur effect
            const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
            printToDisplay(rows[i]->month, months[(elapsed / 50) % 12], 1, 3);
            char buffer[5];
            sprintf(buffer, "%02d", (elapsed / 30) % 31 + 1);
            printToDisplay(rows[i]->day, buffer, 2, 2);
            sprintf(buffer, "%02d%02d", (elapsed / 20) % 24, (elapsed / 10) % 60);
            printToDisplay(rows[i]->time, buffer, 0, 4);
        }

        rows[i]->year.writeDisplay();
        rows[i]->month.writeDisplay();
        rows[i]->day.writeDisplay();
        rows[i]->time.writeDisplay();
        vTaskDelay(pdMS_TO_TICKS(4)); 
    }
    #endif
}
// In HardwareControl.cpp

void flashAllDisplays() {
    #if ENABLE_HARDWARE
    DisplayRow* rows[] = {&destRow, &presRow, &lastRow};
    for (int i=0; i<3; ++i) {
        // Directly manipulate the display buffer to turn on all 16 segments (16 bits).
        // 0xFFFF in hex is a 16-bit number with all bits set to 1.
        for(int j=0; j<8; ++j) { // MODIFIED: Changed loop from 16 to 8
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
    #endif
}

void animateTornadoFlicker() {
    #if ENABLE_HARDWARE
    animateDisplayRowRandomly(destRow, 100);
    animateDisplayRowRandomly(presRow, 100);
    animateDisplayRowRandomly(lastRow, 100);
    #endif
}
void animateRandomRealTimes() {
#if ENABLE_HARDWARE
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
#endif
}

void animateCapacitorChargeUp(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
    int phase = elapsed / (duration / 3);
    float progress = (float)(elapsed % (duration / 3)) / (duration / 3);
    int charsToShow = progress * 13; // Use 13 for total display width (3+2+4+4)

    auto fillRow = [&](DisplayRow& row, int numChars) {
        char buffer[14] = "#############";
        if (numChars < 13) buffer[numChars] = '\0';
        printToDisplay(row.month, String(buffer).substring(0, 3).c_str(), 1, 3);
        printToDisplay(row.day, String(buffer).substring(3, 5).c_str(), 2, 2);
        printToDisplay(row.year, String(buffer).substring(5, 9).c_str(), 0, 4);
        printToDisplay(row.time, String(buffer).substring(9, 13).c_str(), 0, 4);
        row.month.writeDisplay(); row.day.writeDisplay(); row.year.writeDisplay(); row.time.writeDisplay();
    };

    if (phase == 0) { 
        fillRow(lastRow, charsToShow);
    } else if (phase == 1) { 
        fillRow(lastRow, 13);
        fillRow(presRow, charsToShow);
    } else { 
        fillRow(lastRow, 13);
        fillRow(presRow, 13);
        fillRow(destRow, charsToShow);
    }
    #endif
}

void animateDigitalRain(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static char rain[3][13];
    if(elapsed < 100) { // Initialize on first run
        for(int r=0; r<3; ++r) {
            for(int c=0; c<13; ++c) {
                rain[r][c] = chars[random(strlen(chars))];
            }
        }
    }

    // Shift characters down
    for(int c=0; c<13; ++c) {
        rain[2][c] = rain[1][c];
        rain[1][c] = rain[0][c];
        rain[0][c] = chars[random(strlen(chars))];
    }

    DisplayRow* rows[] = {&destRow, &presRow, &lastRow};
    for(int r=0; r<3; ++r) {
        char monthStr[4], dayStr[3], yearStr[5], timeStr[5];
        strncpy(monthStr, &rain[r][0], 3); monthStr[3] = '\0';
        strncpy(dayStr, &rain[r][3], 2); dayStr[2] = '\0';
        strncpy(yearStr, &rain[r][5], 4); yearStr[4] = '\0';
        strncpy(timeStr, &rain[r][9], 4); timeStr[4] = '\0';
        
        printToDisplay(rows[r]->month, monthStr, 1, 3);
        printToDisplay(rows[r]->day, dayStr, 2, 2);
        printToDisplay(rows[r]->year, yearStr, 0, 4);
        printToDisplay(rows[r]->time, timeStr, 0, 4);

        rows[r]->month.writeDisplay();
        rows[r]->day.writeDisplay();
        rows[r]->year.writeDisplay();
        rows[r]->time.writeDisplay();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    #endif
}

void animateWaveformCollapse(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
    // 13-char patterns designed to fit the 3-2-4-4 display layout
    const char* waves[] = {
        "---  --  ----  ----",
        " -   --   --   -- ",
        "  -      -  -  -  "
    };
    int waveIndex = (elapsed / 200) % 3;
    const char* basePattern = waves[waveIndex];

    auto drawWave = [&](DisplayRow& row, bool inverse) {
        char finalPattern[14];
        if(inverse) { 
            for(int i=0; i<13; ++i) finalPattern[i] = (basePattern[i] == '-') ? ' ' : '-';
            finalPattern[13] = '\0';
        } else {
            strncpy(finalPattern, basePattern, 14);
        }

        // Extract substrings for each segment based on the 3-2-4-4 layout
        char monthStr[4], dayStr[3], yearStr[5], timeStr[5];
        strncpy(monthStr, finalPattern, 3); monthStr[3] = '\0';
        strncpy(dayStr, finalPattern + 5, 2); dayStr[2] = '\0';
        strncpy(yearStr, finalPattern + 9, 4); yearStr[4] = '\0';
        strncpy(timeStr, finalPattern + 14, 4); timeStr[4] = '\0';

        printToDisplay(row.month, monthStr, 1, 3);
        printToDisplay(row.day, dayStr, 2, 2);
        printToDisplay(row.year, yearStr, 0, 4);
        printToDisplay(row.time, timeStr, 0, 4);
        row.month.writeDisplay(); row.day.writeDisplay(); row.year.writeDisplay(); row.time.writeDisplay();
    };

    drawWave(destRow, false);
    drawWave(presRow, true);
    drawWave(lastRow, false);
    #endif
}

void animateTimelineSkim(unsigned long elapsed, int duration, int destinationYear) {
    #if ENABLE_HARDWARE
    float progress = (float)elapsed / duration;
    progress = 1 - pow(1 - progress, 3);
    
    static int startYear = 0;
    if(elapsed < 100) startYear = random(1, 2100);

    int currentYear = startYear + (destinationYear - startYear) * progress;

    char yearStr[5];
    sprintf(yearStr, "%04d", currentYear);

    printToDisplay(destRow.year, yearStr, 0, 4); destRow.year.writeDisplay();
    printToDisplay(presRow.year, yearStr, 0, 4); presRow.year.writeDisplay();
    printToDisplay(lastRow.year, yearStr, 0, 4); lastRow.year.writeDisplay();

    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    printToDisplay(destRow.month, months[random(0,12)], 1, 3); destRow.month.writeDisplay();
    
    char buffer[5];
    sprintf(buffer, "%02d", random(1, 32));
    printToDisplay(presRow.day, buffer, 2, 2); presRow.day.writeDisplay();
    
    sprintf(buffer, "%02d%02d", random(0, 24), random(0, 60));
    printToDisplay(lastRow.time, buffer, 0, 4); lastRow.time.writeDisplay();
    #endif
}
// In HardwareControl.cpp

void blankAllDisplays() {
  #if ENABLE_HARDWARE
  destRow.month.clear(); destRow.day.clear(); destRow.year.clear(); destRow.time.clear();
  presRow.month.clear(); presRow.day.clear(); presRow.year.clear(); presRow.time.clear();
  lastRow.month.clear(); lastRow.day.clear(); lastRow.year.clear(); lastRow.time.clear();

  destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();
  vTaskDelay(pdMS_TO_TICKS(2));
  presRow.month.writeDisplay(); presRow.day.writeDisplay(); presRow.year.writeDisplay(); presRow.time.writeDisplay();
  vTaskDelay(pdMS_TO_TICKS(2));
  lastRow.month.writeDisplay(); lastRow.day.writeDisplay(); lastRow.year.writeDisplay(); lastRow.time.writeDisplay();
  vTaskDelay(pdMS_TO_TICKS(2));
  #endif
}
void display88MphSpeed(float speed) {
  #if ENABLE_HARDWARE
  printToDisplay(lastRow.day, "88", 2, 2);
  printToDisplay(lastRow.year, "MPH", 0, 4);
  lastRow.day.writeDisplay();
  lastRow.year.writeDisplay();
  #endif
}

void playSound(const char* filepath) {
    #if ENABLE_HARDWARE
    if (!currentSettings.timeTravelSoundToggle) {
        return;
    }
    char fullPath[MAX_FILENAME_LENGTH];

    // Ensure the path starts with a single '/'
    if (filepath[0] == '/') {
        strncpy(fullPath, filepath, MAX_FILENAME_LENGTH);
    } else {
        snprintf(fullPath, MAX_FILENAME_LENGTH, "/%s", filepath);
    }
    // Ensure null-termination in case of overflow
    fullPath[MAX_FILENAME_LENGTH - 1] = '\0';

    Serial.printf("AUDIO_LOG: Request to play sound: %s\n", fullPath);

    if (audio.isRunning()) {
        audio.stopSong();
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
    
    if (!LittleFS.exists(fullPath)) {
        Serial.printf("AUDIO_LOG: File not found: %s\n", fullPath);
        return;
    }

    // The SD pin logic is harmless even if unwired.
    digitalWrite(I2S_SD_PIN, HIGH);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    audio.setVolume(currentSettings.notificationVolume);
    strncpy(currentSoundFile, fullPath, MAX_FILENAME_LENGTH - 1);
    currentSoundFile[MAX_FILENAME_LENGTH - 1] = '\0';
    
    if (audio.connecttoFS(LittleFS, fullPath)) {
        Serial.printf("AUDIO_LOG: Started playing: %s\n", fullPath);
    } else {
        Serial.printf("AUDIO_LOG: Failed to connect to file: %s\n", fullPath);
        currentSoundFile[0] = '\0';
        digitalWrite(I2S_SD_PIN, LOW);
    }
    #endif
}
void typeTextOnDisplay(DisplayRow& row, const char* text, int typeDelay, bool withCursor) {
  #if ENABLE_HARDWARE
  Adafruit_AlphaNum4* displays[] = {&row.month, &row.day, &row.year, &row.time};
  
  // Clear the entire row first
  for (int i = 0; i < 4; i++) {
    displays[i]->clear();
    displays[i]->writeDisplay();
  }
  vTaskDelay(pdMS_TO_TICKS(10));

  int len = strlen(text);
  const int total_visual_width = 13; 
  if (len > total_visual_width) len = total_visual_width;

  for (int i = 0; i < len; i++) {
    int displayIndex, digitIndex;
    if (i < 3) { 
        displayIndex = 0;
        digitIndex = i + 1; 
    } else if (i < 5) { 
        displayIndex = 1;
        digitIndex = (i - 3) + 1; 
    } else if (i < 9) { 
        displayIndex = 2;
        digitIndex = i - 5;
    } else { 
        displayIndex = 3;
        digitIndex = i - 9;
    }
    
    displays[displayIndex]->writeDigitAscii(digitIndex, text[i]);
    
    if (withCursor && (i + 1 < len)) {
        int next_i = i + 1;
        int nextDisplayIndex, nextDigitIndex;
        if (next_i < 3) {
            nextDisplayIndex = 0;
            nextDigitIndex = next_i + 1;
        } else if (next_i < 5) {
            nextDisplayIndex = 1;
            nextDigitIndex = (next_i - 3) + 1;
        } else if (next_i < 9) {
            nextDisplayIndex = 2;
            nextDigitIndex = next_i - 5;
        } else {
            nextDisplayIndex = 3;
            nextDigitIndex = next_i - 9;
        }
        displays[nextDisplayIndex]->writeDigitAscii(nextDigitIndex, '_');
        displays[nextDisplayIndex]->writeDisplay();
    }
    
    displays[displayIndex]->writeDisplay();
    vTaskDelay(pdMS_TO_TICKS(typeDelay));

    if (withCursor && (i + 1 < len)) {
        int next_i = i + 1;
        int nextDisplayIndex, nextDigitIndex;
        if (next_i < 3) {
            nextDisplayIndex = 0;
            nextDigitIndex = next_i + 1;
        } else if (next_i < 5) {
            nextDisplayIndex = 1;
            nextDigitIndex = (next_i - 3) + 1;
        } else if (next_i < 9) {
            nextDisplayIndex = 2;
            nextDigitIndex = next_i - 5;
        } else {
            nextDisplayIndex = 3;
            nextDigitIndex = next_i - 9;
        }
        displays[nextDisplayIndex]->writeDigitAscii(nextDigitIndex, ' ');
        displays[nextDisplayIndex]->writeDisplay();
    }
  }
  #endif
}
void animateFluxCapacitor() {
  #if ENABLE_HARDWARE
    static int frame = 0;
    // Simple 3-frame pulse effect
    if (frame == 0) {
        printToDisplay(presRow.month, " FLX", 0, 3);
        printToDisplay(presRow.day, "CP", 2, 2);
        printToDisplay(presRow.year, "ACTV", 0, 4);
    } else if (frame == 1) {
        printToDisplay(presRow.month, "FLX", 0, 3);
        printToDisplay(presRow.day, "CP", 2, 2);
        printToDisplay(presRow.year, "ACTV", 0, 4);
    } else {
        printToDisplay(presRow.month, " FLX", 0, 3);
        printToDisplay(presRow.day, "CP", 2, 2);
        printToDisplay(presRow.year, "ACTV", 0, 4);
    }
    frame = (frame + 1) % 3; // Cycle through frames

    presRow.month.writeDisplay();
    presRow.day.writeDisplay();
    presRow.year.writeDisplay();
  #endif
}
void displayStaticFluxText() {
    #if ENABLE_HARDWARE
    printToDisplay(presRow.month, "FLX", 1, 3);
    printToDisplay(presRow.day, "CP", 2, 2);
    printToDisplay(presRow.year, "ACTV", 0, 4);
    printToDisplay(presRow.time, "", 0, 4);
    presRow.month.writeDisplay();
    presRow.day.writeDisplay();
    presRow.year.writeDisplay();
    presRow.time.writeDisplay();
    #endif
}
void applyBrightness() {
  #if ENABLE_HARDWARE
  // Corrected: The UI provides a value from 0-7. We will use this value directly.
  // Although the hardware supports 0-15, the 0-7 range is what the UI and diagnostic test use.
  uint8_t brightnessValue = currentSettings.brightness;

  // The setBrightness function can accept values up to 15, but we are clamping it to the UI's max of 7.
  if (brightnessValue > 7) {
      brightnessValue = 7;
  }
  
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
  #endif
}

void animateSequentialFlicker(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
    // Create an array of all display segments for easy iteration
    Adafruit_AlphaNum4* displays[] = {
        &destRow.month, &destRow.day, &destRow.year, &destRow.time,
        &presRow.month, &presRow.day, &presRow.year, &presRow.time,
        &lastRow.month, &lastRow.day, &lastRow.year, &lastRow.time
    };
    const char* labels[] = {"CPU", "ME", "I2C1", "I2C2", "SND", "WF", "NTP", "MQTT", "SYS", "CO", "STAT", "PWR"};
    const int numDisplays = 12;
    const int phaseDuration = 800; 

    int activeIndex = (elapsed / phaseDuration) % numDisplays;
    int phaseTime = elapsed % phaseDuration;

    for (int i = 0; i < numDisplays; ++i) {
        int segmentType = i % 4; // 0=month, 1=day, 2=year, 3=time
        int justification = 0;
        int width = 4;
        switch(segmentType) {
            case 0: justification = 1; width = 3; break;
            case 1: justification = 2; width = 2; break;
            default: justification = 0; width = 4; break;
        }

        if (i < activeIndex) {
            // Segments that have already been "checked"
            printToDisplay(*displays[i], "OK--", justification, width);
        } else if (i == activeIndex) {
            // The currently active segment
            if (phaseTime < (phaseDuration / 2)) {
                printToDisplay(*displays[i], labels[i], justification, width);
            } else {
                if ((phaseTime / 100) % 2 == 0) {
                     printToDisplay(*displays[i], "OK--", justification, width);
                } else {
                     printToDisplay(*displays[i], "", justification, width);
                }
            }
        } else {
            // Segments waiting to be checked
            displays[i]->clear();
        }
        displays[i]->writeDisplay();
    }
    #endif
}

void animateCountingUp(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
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

        printToDisplay(rows[i]->month, months[timeinfo.tm_mon], 1, 3);
        
        sprintf(buffer, "%02d", timeinfo.tm_mday);
        printToDisplay(rows[i]->day, buffer, 2, 2);

        sprintf(buffer, "%04d", timeinfo.tm_year + 1900);
        printToDisplay(rows[i]->year, buffer, 0, 4);
        
        sprintf(buffer, "%02d%02d", timeinfo.tm_hour, timeinfo.tm_min);
        printToDisplay(rows[i]->time, buffer, 0, 4);
        
        rows[i]->month.writeDisplay();
        rows[i]->day.writeDisplay();
        rows[i]->year.writeDisplay();
        rows[i]->time.writeDisplay();
        vTaskDelay(pdMS_TO_TICKS(4)); 
    }
    #endif
}

void animateWaveFlicker(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
    Adafruit_AlphaNum4* displays[] = {
        &destRow.month, &destRow.day, &destRow.year, &destRow.time,
        &presRow.month, &presRow.day, &presRow.year, &presRow.time,
        &lastRow.month, &lastRow.day, &lastRow.year, &lastRow.time
    };
    int numDisplays = 12;
    float progress = (float)elapsed / duration;
    int wavePosition = progress * (numDisplays * 2);

    for (int i = 0; i < numDisplays; i++) {
        int distance = abs(i - wavePosition);
        int brightness = 15 - (distance * 3);
        if (brightness < 0) brightness = 0;
        if (brightness > 15) brightness = 15;
        displays[i]->setBrightness(brightness);
        displays[i]->writeDisplay();
    }
    #endif
}