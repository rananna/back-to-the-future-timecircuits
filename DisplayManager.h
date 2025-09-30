#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <string>

enum MarqueeState {
    M_IDLE,
    M_PAUSED,
    M_SCROLLING,
    M_START_PAGE
};

enum WeatherDisplayState {
    WD_START_PAGE,
    WD_SCROLLING,
    WD_PAUSING,
    WD_ERROR
};

enum StockDisplayState {
    SD_START_PAGE,
    SD_SCROLLING,
    SD_PAUSING,
    SD_ERROR,
    SD_CONNECTING,
    SD_MARKET_CLOSED
};

extern MarqueeState marqueeState;
extern int marqueeScrollPosition;
extern StockDisplayState stockState;
extern bool weatherDataUpdated;
extern bool isRowInManualMode[3];
extern std::string manualDisplayText[3][4];

// Dirty flags and buffers for scrolling text
extern bool isMarqueeBufferDirty;
extern bool isWeatherBufferDirty;

extern std::string marqueeBuffer;
extern char weatherBuffer[512];
extern std::string marqueeOverrideBuffer;

// Forward declaration to use SequencerTrack in function signatures
struct SequencerTrack;

// MODIFIED: Function now accepts flags to control which rows are updated
void updateNormalClockDisplay(bool updateDest = true, bool updatePres = true, bool updateLast = true);
void updateNormalClockDisplay_internal(bool updateDest = true, bool updatePres = true, bool updateLast = true);
void updateMarqueeDisplay();
void handleWeatherDisplay();
void displayOverrideMessage();
void displayMarqueeOverride();
void updateStockTickerDisplay();
void setOverrideMessage(const char* line1, const char* line2, const char* line3);
void startSequencerMarquee(SequencerTrack& track, const std::string& text);
void handleAllSequencerMarquees();
void showTemporaryMessage(const char* month, const char* day, const char* year, const char* time, int duration);
const char* getIconForWeatherCode(int code);
void updateDisplaySegment(int row, int segment, const std::string& text);
void restoreDisplayRow(int row);
void resetWeatherFetchState();

#endif // DISPLAY_MANAGER_H