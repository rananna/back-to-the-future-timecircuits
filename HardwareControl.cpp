#include "HardwareControl.h"

// --- GLOBAL HARDWARE OBJECTS (DEFINITIONS) ---
#if ENABLE_HARDWARE
TwoWire I2C_1 = TwoWire(0);
TwoWire I2C_2 = TwoWire(1);

DisplayRow destRow, presRow, lastRow;

HardwareSerial dfpSerial(2);
DFRobotDFPlayerMini myDFPlayer;
#endif

// --- HELPER FUNCTION ---
// Correctly writes a string to a 4-character alphanumeric display with justification.
// Justification: 0 = left, 1 = right, 2 = center
void printToDisplay(Adafruit_AlphaNum4 &display, const char* text, int justification = 0) {
  display.clear();
  int len = strlen(text);
  int startPos = 0;

  if (justification == 1) { // Right Justify
    startPos = 4 - len;
  } else if (justification == 2) { // Center Justify
    startPos = (4 - len) / 2;
  }

  for (int i = 0; i < 4; i++) {
    if (i >= startPos && i < (startPos + len)) {
      display.writeDigitAscii(i, text[i - startPos]);
    } else {
      display.writeDigitAscii(i, ' '); // Clear remaining characters
    }
  }
}

// --- FUNCTION IMPLEMENTATIONS ---

void setupPhysicalDisplay() {
  #if ENABLE_HARDWARE
  I2C_1.begin(I2C_SDA_1, I2C_SCL_1, 100000);
  I2C_2.begin(I2C_SDA_2, I2C_SCL_2, 100000);

  destRow = {Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4()};
  presRow = {Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4()};
  lastRow = {Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4()};

  // Initialize all 12 displays with their correct I2C addresses and buses
  destRow.month.begin(0x70, &I2C_1); destRow.day.begin(0x71, &I2C_1); destRow.year.begin(0x72, &I2C_1); destRow.time.begin(0x73, &I2C_1);
  presRow.month.begin(0x74, &I2C_1); presRow.day.begin(0x75, &I2C_1); presRow.year.begin(0x76, &I2C_1); presRow.time.begin(0x77, &I2C_1);
  lastRow.month.begin(0x70, &I2C_2); lastRow.day.begin(0x71, &I2C_2); lastRow.year.begin(0x72, &I2C_2); lastRow.time.begin(0x73, &I2C_2);

  // Set LED indicator pins to output
  pinMode(DEST_AM_PIN, OUTPUT); pinMode(DEST_PM_PIN, OUTPUT);
  pinMode(PRES_AM_PIN, OUTPUT); pinMode(PRES_PM_PIN, OUTPUT);
  pinMode(LAST_AM_PIN, OUTPUT); pinMode(LAST_PM_PIN, OUTPUT);
  #endif
}

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

  // Time (4 chars with decimal point, e.g., 01.21)
  char timeBuffer[5];
  sprintf(timeBuffer, "%02d%02d", timeinfo.tm_hour, timeinfo.tm_min);
  row.time.clear();
  row.time.writeDigitAscii(0, timeBuffer[0]);
  // Set the decimal point on the second digit by ORing the ASCII char with 0x80
  row.time.writeDigitAscii(1, timeBuffer[1] | 0x80);
  row.time.writeDigitAscii(2, timeBuffer[2]);
  row.time.writeDigitAscii(3, timeBuffer[3]);


  // Write the buffer to all displays in the row
  row.month.writeDisplay();
  row.day.writeDisplay();
  row.year.writeDisplay();
  row.time.writeDisplay();
  #endif
}

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

void display88MphSpeed(float speed) {
  #if ENABLE_HARDWARE
  printToDisplay(lastRow.day, "88", 2);
  printToDisplay(lastRow.year, "MPH");
  lastRow.day.writeDisplay();
  lastRow.year.writeDisplay();
  #endif
}

void drawIcon(Adafruit_AlphaNum4& display, const char* iconName) {
  #if ENABLE_HARDWARE
  display.clear();
  if (strcmp(iconName, "SUN") == 0) {
    // Custom character for a sun icon
    display.writeDigitRaw(1, 0b0000101010001000);
    display.writeDigitRaw(2, 0b0000101001001000);
  } else if (strcmp(iconName, "CLOUD") == 0) {
    // Custom character for a cloud icon
    display.writeDigitRaw(0, 0b0000000011100011);
    display.writeDigitRaw(1, 0b0000000011111111);
    display.writeDigitRaw(2, 0b0000000011111111);
    display.writeDigitRaw(3, 0b0000000011100011);
  } else {
    printToDisplay(display, iconName);
  }
  display.writeDisplay();
  #endif
}

void playSound(const char* soundName) {
  #if ENABLE_HARDWARE
  if (strcmp(soundName, "TIME_TRAVEL") == 0) myDFPlayer.playMp3Folder(1);
  else if (strcmp(soundName, "ACCELERATION") == 0) myDFPlayer.playMp3Folder(2);
  else if (strcmp(soundName, "ARRIVAL_THUD") == 0) myDFPlayer.playMp3Folder(4);
  else if (strcmp(soundName, "CONFIRM_ON") == 0) myDFPlayer.playMp3Folder(5);
  else if (strcmp(soundName, "SLEEP_ON") == 0) myDFPlayer.playMp3Folder(6);
  else if (strcmp(soundName, "EASTER_EGG") == 0) myDFPlayer.playMp3Folder(7);
  #endif
}

void setupSoundFiles() {
  // This function is a placeholder for any future SD card validation logic.
}