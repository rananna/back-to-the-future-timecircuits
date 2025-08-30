#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <string>

extern std::string manualDisplayText[3][4];
extern bool isRowInManualMode[3];
// --- ADD THESE TWO LINES ---
extern bool isRowInManualMode[3];
extern std::string manualDisplayText[3][4];
// --- END OF ADDITION ---
void updateNormalClockDisplay();
void updateMarqueeDisplay();
void handleWeatherDisplay();
void displayOverrideMessage();
void displayMarqueeOverride();
void updateStockTickerDisplay();
void showTemporaryMessage(const char* month, const char* day, const char* year, const char* time, int duration);
const char* getIconForWeatherCode(int code);
void updateDisplaySegment(int row, int segment, const std::string& text);

#endif // DISPLAY_MANAGER_H