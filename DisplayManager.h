#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <string>

extern bool weatherDataUpdated;
extern bool isRowInManualMode[3];
extern std::string manualDisplayText[3][4];

// MODIFIED: Function now accepts flags to control which rows are updated
void updateNormalClockDisplay(bool updateDest = true, bool updatePres = true, bool updateLast = true);
void updateMarqueeDisplay();
void handleWeatherDisplay();
void displayOverrideMessage();
void displayMarqueeOverride();
void updateStockTickerDisplay();
void showTemporaryMessage(const char* month, const char* day, const char* year, const char* time, int duration);
const char* getIconForWeatherCode(int code);
void updateDisplaySegment(int row, int segment, const std::string& text);
void resetWeatherFetchState();

#endif // DISPLAY_MANAGER_H