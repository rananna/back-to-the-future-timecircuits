#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>
#include "types.h"
#include <time.h>

// Note: The DisplayManager was a class with no implementations.
// The functions have been moved to be standalone functions,
// which is how they are used in the other code files.

void showTemporaryMessage(const char* month, const char* day, const char* year, const char* time, int duration);
void updateStockTickerDisplay();
void displayOverrideMessage();
void displayMarqueeOverride();
void updateMarqueeDisplay();
void updateNormalClockDisplay();
void handleWeatherDisplay();
void updateDisplayRow(DisplayRow& row, struct tm& timeinfo, int year, int rowIndex);
void updateDisplaySegment(int row, int segment, const char* text);
const char* getIconForWeatherCode(int code);

#endif // DISPLAY_MANAGER_H