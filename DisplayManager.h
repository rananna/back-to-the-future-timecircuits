#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <string>

// Function Declarations for display updates
void updateNormalClockDisplay();
void updateMarqueeDisplay();
void handleWeatherDisplay();
void displayOverrideMessage();
void displayMarqueeOverride();
void showTemporaryMessage(const char* month, const char* day, const char* year, const char* time, int duration);
const char* getIconForWeatherCode(int code);

#endif // DISPLAY_MANAGER_H