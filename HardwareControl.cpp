/**
 * @file HardwareControl.cpp
 * @brief Implements low-level control functions for displays, LEDs, and sound.
 * @details This file contains the concrete implementations for initializing and controlling
 * the hardware components. It directly interfaces with the Adafruit GFX and LED Backpack
 * libraries, as well as the DFPlayer Mini library. The ENABLE_HARDWARE macro allows
 * for compiling the code without the hardware for testing purposes.
 */

#include "HardwareControl.h"

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

/** @brief HardwareSerial instance for communication with the DFPlayer Mini MP3 module. */
HardwareSerial dfpSerial(2);
/** @brief The DFPlayer Mini MP3 module object. */
DFRobotDFPlayerMini myDFPlayer;
#endif

// --- HELPER FUNCTION ---
/**
 * @brief Writes a string to a 4-character alphanumeric display with justification.
 * @param display The Adafruit_AlphaNum4 object to write to.
 * @param text The C-string to display.
 * @param justification 0 for left, 1 for right, 2 for center.
 */
void printToDisplay(Adafruit_AlphaNum4 &display, const char* text, int justification) {
  display.clear();
  int len = strlen(text);
  int startPos = 0;

  // Calculate the starting position based on justification.
  if (justification == 1) { // Right Justify
    startPos = 4 - len;
  } else if (justification == 2) { // Center Justify
    startPos = (4 - len) / 2;
  }

  // Write characters to the display buffer.
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
 * with its correct I2C address and bus. Finally, it configures the AM/PM indicator
 * LED pins as outputs.
 */
void setupPhysicalDisplay() {
  #if ENABLE_HARDWARE
  // Initialize both I2C buses with their respective SDA/SCL pins.
  I2C_1.begin(I2C_SDA_1, I2C_SCL_1, 100000);
  I2C_2.begin(I2C_SDA_2, I2C_SCL_2, 100000);

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
  #endif
}

/**
 * @brief Updates a full row of 4 displays with a specific time.
 * @param row A reference to the DisplayRow struct to be updated.
 * @param timeinfo A `tm` struct containing the time to display.
 * @param year The four-digit year to display.
 */
void updateDisplayRow(DisplayRow& row, const struct tm& timeinfo, int year) {
  #if ENABLE_HARDWARE
  char buffer[5];
  const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

  // Month (3 chars, right justified)
  printToDisplay(row.month, months[timeinfo.tm_mon], 1);

  // Day (2 chars, center justified)
  sprintf(buffer, "%02d", timeinfo.tm_mday);
  printToDisplay(row.day, buffer, 2);

  // Year (4 chars)
  sprintf(buffer, "%04d", year);
  printToDisplay(row.year, buffer);

  // Time (4 chars with a decimal point, e.g., 01.21)
  char timeBuffer[5];
  sprintf(timeBuffer, "%02d%02d", timeinfo.tm_hour, timeinfo.tm_min);
  row.time.clear();
  row.time.writeDigitAscii(0, timeBuffer[0]);
  // Set the decimal point on the second digit by ORing the ASCII char with 0x80.
  // This is a feature of the Adafruit LED Backpack library.
  row.time.writeDigitAscii(1, timeBuffer[1] | 0x80);
  row.time.writeDigitAscii(2, timeBuffer[2]);
  row.time.writeDigitAscii(3, timeBuffer[3]);


  // Write the buffer to all displays in the row to make the changes visible.
  row.month.writeDisplay();
  row.day.writeDisplay();
  row.year.writeDisplay();
  row.time.writeDisplay();
  #endif
}

/**
 * @brief Fills a display row with random characters for a flicker effect.
 * @param row A reference to the DisplayRow struct to be animated.
 */
void animateDisplayRowRandomly(DisplayRow& row) {
  #if ENABLE_HARDWARE
    char buffer[5];
    // Animate year
    sprintf(buffer, "%04d", random(1000, 9999));
    printToDisplay(row.year, buffer);
    row.year.writeDisplay();

    // Animate month
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    printToDisplay(row.month, months[random(0,12)], 1); // Right justified
    row.month.writeDisplay();

    // Animate day
    sprintf(buffer, "%02d", random(1, 32));
    printToDisplay(row.day, buffer, 2); // Center justified
    row.day.writeDisplay();

    // Animate time
    sprintf(buffer, "%02d%02d", random(0, 24), random(0, 60));
    printToDisplay(row.time, buffer);
    row.time.writeDisplay();
  #endif
}

/**
 * @brief Displays the current speed on the bottom row during the 88 MPH acceleration animation.
 * @param speed The speed to display (0-88).
 */
void displaySpeed(int speed) {
  #if ENABLE_HARDWARE
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
  #endif
}

/**
 * @brief Animates a coordinated "time blur" effect across all three display rows.
 * @param elapsed The time in milliseconds since the animation phase started.
 * @param duration The total duration of the animation phase in milliseconds.
 * @param destinationYear The target year for the animation.
 */
void animateAllRowsTimelineSkim(unsigned long elapsed, int duration, int destinationYear) {
    #if ENABLE_HARDWARE
    // Calculate the animation progress with an easing function for a smoother effect.
    float progress = (float)elapsed / duration;
    progress = 1 - pow(1 - progress, 3); // Ease-out curve.

    // Calculate the current year to display based on progress.
    static int startYear = 0;
    if(elapsed < 100) startYear = random(1, 2100); // Pick a new random start year each time.
    int currentYear = startYear + (destinationYear - startYear) * progress;
    char yearStr[5];
    sprintf(yearStr, "%04d", currentYear);

    // Rapidly cycle other date/time fields based on elapsed time.
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    int monthIndex = (elapsed / 50) % 12;
    int day = (elapsed / 30) % 31 + 1;
    int hour = (elapsed / 20) % 24;
    int minute = (elapsed / 10) % 60;
    
    char buffer[5];

    // Update all three rows with the blurred time.
    DisplayRow* rows[] = {&destRow, &presRow, &lastRow};
    for (int i=0; i<3; ++i) {
        printToDisplay(rows[i]->year, yearStr);
        printToDisplay(rows[i]->month, months[monthIndex], 1);
        sprintf(buffer, "%02d", day);
        printToDisplay(rows[i]->day, buffer, 2);
        sprintf(buffer, "%02d%02d", hour, minute);
        printToDisplay(rows[i]->time, buffer);

        // Write the changes to the physical hardware.
        rows[i]->year.writeDisplay();
        rows[i]->month.writeDisplay();
        rows[i]->day.writeDisplay();
        rows[i]->time.writeDisplay();
    }
    #endif
}

/**
 * @brief Implements the "Temporal Lock-On" effect where a display flickers between random data and the correct time.
 * @param row A reference to the DisplayRow struct to animate.
 * @param timeinfo A `tm` struct containing the correct time to lock on to.
 * @param year The correct four-digit year.
 */
void animateTemporalLockOn(DisplayRow& row, const struct tm& timeinfo, int year) {
    #if ENABLE_HARDWARE
    // 50% chance to show the correct time, 50% chance to show random garbage.
    if (random(100) < 50) {
        updateDisplayRow(row, timeinfo, year);
    } else {
        animateDisplayRowRandomly(row);
    }
    #endif
}

/**
 * @brief Turns on all segments of all 12 displays to create a bright white flash effect.
 * @details This is achieved by directly manipulating the display buffer of each segment,
 * setting all 16 bits of each character position to 1 (0xFFFF), and then writing
 * the buffer to the hardware.
 */
void flashAllDisplays() {
    #if ENABLE_HARDWARE
    DisplayRow* rows[] = {&destRow, &presRow, &lastRow};
    for (int i=0; i<3; ++i) {
        // Directly manipulate the display buffer to turn on all 16 segments (16 bits).
        // 0xFFFF in hex is a 16-bit number with all bits set to 1.
        for(int j=0; j<16; ++j) {
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

/**
 * @brief Animation style: a chaotic flicker on all displays.
 */
void animateTornadoFlicker() {
    #if ENABLE_HARDWARE
    animateDisplayRowRandomly(destRow);
    animateDisplayRowRandomly(presRow);
    animateDisplayRowRandomly(lastRow);
    #endif
}

/**
 * @brief Animation style: fills the displays from bottom to top like a charging capacitor.
 * @param elapsed The time in milliseconds since the animation phase started.
 * @param duration The total duration of the animation phase in milliseconds.
 */
void animateCapacitorChargeUp(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
    // Divide the animation into three phases, one for each row.
    int phase = elapsed / (duration / 3);
    float progress = (float)(elapsed % (duration / 3)) / (duration / 3);
    int charsToShow = progress * 16; // 16 total characters across a row.

    // Helper lambda to fill a row with a certain number of characters.
    auto fillRow = [&](DisplayRow& row, int numChars) {
        char buffer[17] = "################";
        if (numChars < 16) buffer[numChars] = '\0';
        printToDisplay(row.month, String(buffer).substring(0, 4).c_str());
        printToDisplay(row.day, String(buffer).substring(4, 8).c_str());
        printToDisplay(row.year, String(buffer).substring(8, 12).c_str());
        printToDisplay(row.time, String(buffer).substring(12, 16).c_str());
        row.month.writeDisplay(); row.day.writeDisplay(); row.year.writeDisplay(); row.time.writeDisplay();
    };

    if (phase == 0) { // Phase 1: Fill bottom row.
        fillRow(lastRow, charsToShow);
    } else if (phase == 1) { // Phase 2: Fill middle row.
        fillRow(lastRow, 16);
        fillRow(presRow, charsToShow);
    } else { // Phase 3: Fill top row.
        fillRow(lastRow, 16);
        fillRow(presRow, 16);
        fillRow(destRow, charsToShow);
    }
    #endif
}

/**
 * @brief Animation style: fills displays with random falling characters like a digital rain effect.
 * @param elapsed The time in milliseconds since the animation phase started.
 * @param duration The total duration of the animation phase in milliseconds.
 */
void animateDigitalRain(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    // Helper lambda to apply the rain effect to one column of displays.
    auto rainColumn = [&](Adafruit_AlphaNum4& d, Adafruit_AlphaNum4& p, Adafruit_AlphaNum4& l) {
        char d_c[5], p_c[5], l_c[5];
        for(int i=0; i<4; ++i) {
            d_c[i] = chars[random(strlen(chars))];
            p_c[i] = chars[random(strlen(chars))];
            l_c[i] = chars[random(strlen(chars))];
        }
        d_c[4] = p_c[4] = l_c[4] = '\0';
        printToDisplay(d, d_c); d.writeDisplay();
        printToDisplay(p, p_c); p.writeDisplay();
        printToDisplay(l, l_c); l.writeDisplay();
    };
    rainColumn(destRow.month, presRow.month, lastRow.month);
    rainColumn(destRow.day, presRow.day, lastRow.day);
    rainColumn(destRow.year, presRow.year, lastRow.year);
    rainColumn(destRow.time, presRow.time, lastRow.time);
    #endif
}

/**
 * @brief Animation style: shows a scrolling sine-wave pattern on the displays.
 * @param elapsed The time in milliseconds since the animation phase started.
 * @param duration The total duration of the animation phase in milliseconds.
 */
void animateWaveformCollapse(unsigned long elapsed, int duration) {
    #if ENABLE_HARDWARE
    const char* wave[] = {"-___-", "_--_-", "__-__"};
    int waveIndex = (elapsed / 200) % 3;
    int scrollOffset = (elapsed / 100) % 5;

    // Helper lambda to draw the wave pattern on a row.
    auto drawWave = [&](DisplayRow& row, bool inverse) {
        char pattern[6];
        const char* basePattern = wave[waveIndex];
        
        char scrolledPattern[6];
        for(int i=0; i<5; ++i) {
            scrolledPattern[i] = basePattern[(i + scrollOffset) % 5];
        }
        scrolledPattern[5] = '\0';
        
        if(inverse) { // Invert the pattern for the middle row.
            for(int i=0; i<5; ++i) pattern[i] = (scrolledPattern[i] == '-') ? '_' : '-';
            pattern[5] = '\0';
        } else {
            strcpy(pattern, scrolledPattern);
        }

        printToDisplay(row.month, String(pattern).substring(0, 4).c_str());
        printToDisplay(row.day, String(pattern).substring(1, 3).c_str(), 2);
        printToDisplay(row.year, String(pattern).substring(0, 4).c_str());
        printToDisplay(row.time, String(pattern).substring(1, 5).c_str());
        row.month.writeDisplay(); row.day.writeDisplay(); row.year.writeDisplay(); row.time.writeDisplay();
    };

    drawWave(destRow, false);
    drawWave(presRow, true);
    drawWave(lastRow, false);
    #endif
}

/**
 * @brief Animation style: rapidly cycles the year while other fields flicker randomly.
 * @param elapsed The time in milliseconds since the animation phase started.
 * @param duration The total duration of the animation phase in milliseconds.
 * @param destinationYear The target year for the animation.
 */
void animateTimelineSkim(unsigned long elapsed, int duration, int destinationYear) {
    #if ENABLE_HARDWARE
    float progress = (float)elapsed / duration;
    progress = 1 - pow(1 - progress, 3); // Ease-out curve
    
    static int startYear = 0;
    if(elapsed < 100) startYear = random(1, 2100);

    int currentYear = startYear + (destinationYear - startYear) * progress;

    char yearStr[5];
    sprintf(yearStr, "%04d", currentYear);

    // Update only the year on all rows.
    printToDisplay(destRow.year, yearStr); destRow.year.writeDisplay();
    printToDisplay(presRow.year, yearStr); presRow.year.writeDisplay();
    printToDisplay(lastRow.year, yearStr); lastRow.year.writeDisplay();

    // Flicker other random fields.
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
 * @brief Clears all 12 displays.
 */
void blankAllDisplays() {
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
 * @brief Special display function for the boot sequence to show "88 MPH".
 * @param speed Not currently used, but could be used for animation.
 */
void display88MphSpeed(float speed) {
  #if ENABLE_HARDWARE
  printToDisplay(lastRow.day, "88", 2);
  printToDisplay(lastRow.year, "MPH");
  lastRow.day.writeDisplay();
  lastRow.year.writeDisplay();
  #endif
}

/**
 * @brief Plays a sound file from the SD card via the DFPlayer Mini.
 * @param soundName A friendly name for the sound to play (e.g., "TIME_TRAVEL").
 * @details This function maps the friendly name to the corresponding file index in the
 * 'mp3' folder on the SD card required by the DFPlayer library.
 */
void playSound(const char* soundName) {
  #if ENABLE_HARDWARE
  // The DFPlayer library can play files by their index in a specific folder.
  // We map friendly names to these indices.
  if (strcmp(soundName, "TIME_TRAVEL") == 0) myDFPlayer.playMp3Folder(1);
  else if (strcmp(soundName, "ACCELERATION") == 0) myDFPlayer.playMp3Folder(2);
  else if (strcmp(soundName, "FLUX_CAPACITOR_CHARGE") == 0) myDFPlayer.playMp3Folder(3);
  else if (strcmp(soundName, "ARRIVAL_THUD") == 0) myDFPlayer.playMp3Folder(4);
  else if (strcmp(soundName, "CONFIRM_ON") == 0) myDFPlayer.playMp3Folder(5);
  else if (strcmp(soundName, "SLEEP_ON") == 0) myDFPlayer.playMp3Folder(6);
  else if (strcmp(soundName, "EASTER_EGG") == 0) myDFPlayer.playMp3Folder(7);
  #endif
}

/**
 * @brief Placeholder function for any future sound file validation or setup logic.
 */
void setupSoundFiles() {
  // This function is currently empty but can be used for SD card checks.
}