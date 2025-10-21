/**
 * @file DisplayManager.h
 * @brief Public interface for managing the content and state of the physical displays.
 * @details This file declares the state machines, global state variables, and function prototypes
 * responsible for controlling what is shown on the displays. It manages the different display
 * modes (Clock, Stocks, Weather, Data Link), handles scrolling text, and allows for manual
 * overrides by the animation sequencer.
 */
#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <string>

/**
 * @name Display Mode State Machines
 * @brief Enums defining the states for each of the complex, multi-stage display modes.
 * @{
 */

/** @brief Defines the states for the 'Data Link' scrolling marquee display mode. */
enum MarqueeState {
    M_IDLE,         /**< The marquee is not active. */
    M_PAUSED,       /**< The marquee is paused, showing a full page of data. */
    M_SCROLLING,    /**< The marquee text is currently scrolling. */
    M_START_PAGE    /**< The initial state when entering marquee mode, prepares the first page. */
};

/** @brief Defines the states for the Weather display mode. */
enum WeatherDisplayState {
    WD_START_PAGE,  /**< Preparing the initial, non-scrolling weather summary. */
    WD_SCROLLING,   /**< Scrolling the detailed weather forecast text. */
    WD_PAUSING,     /**< Paused at the end of a scroll. */
    WD_ERROR        /**< An error occurred while fetching weather data. */
};

/** @brief Defines the states for the Stock Ticker display mode. */
enum StockDisplayState {
    SD_START_PAGE,      /**< Preparing the initial, non-scrolling stock summary. */
    SD_SCROLLING,       /**< Scrolling the detailed stock information. */
    SD_PAUSING,         /**< Paused at the end of a scroll. */
    SD_ERROR,           /**< An error occurred while fetching stock data. */
    SD_CONNECTING,      /**< Currently attempting to fetch data. */
    SD_MARKET_CLOSED    /**< The stock market is currently closed. */
};
/** @} */


/**
 * @name Global Display State Variables
 * @brief Extern declarations for global variables that track the current state of the displays.
 * @{
 */
extern MarqueeState marqueeState;           /**< The current state of the Data Link marquee state machine. */
extern int marqueeScrollPosition;           /**< The current horizontal scroll position for the marquee text. */
extern StockDisplayState stockState;        /**< The current state of the stock ticker state machine. */
extern bool weatherDataUpdated;             /**< Flag indicating that new weather data has been fetched. */

/**
 * @brief A flag for each row (0-2) indicating if it's under manual control by the sequencer.
 * @details When true, the normal display handlers (clock, weather, etc.) will not update this row,
 * allowing an animation to have exclusive control.
 */
extern bool isRowInManualMode[3];

/**
 * @brief Buffers to hold the text set by the sequencer when a row is in manual mode.
 * @details Indexed by [row][segment]. This allows an animation to set static text that persists
 * until the row is released from manual mode.
 */
extern std::string manualDisplayText[3][4];

/**
 * @brief Buffers to hold the display text from before an animation started.
 * @details Indexed by [row][segment]. This is populated before a sequence begins
 * and is used by `RESTORE_SEGMENT` and `RESTORE_ROW` to correctly restore
 * the display to its previous state.
 */
extern std::string preAnimationDisplayText[3][4];

/**
 * @brief A flag that is set to true when the Data Link configuration changes.
 * @details This signals `updateMarqueeDisplay` to rebuild the `marqueeBuffer` with the new content.
 */
extern bool isMarqueeBufferDirty;

/**
 * @brief A flag that is set to true when new weather data is available.
 * @details This signals `handleWeatherDisplay` to rebuild the `weatherBuffer` with the new forecast.
 */
extern bool isWeatherBufferDirty;

extern std::string marqueeBuffer;       /**< The complete, concatenated string for the Data Link marquee scroll. */
extern char weatherBuffer[512];         /**< The complete, concatenated string for the weather scroll. */
extern std::string marqueeOverrideBuffer; /**< A buffer for displaying a high-priority, temporary scrolling message. */
/** @} */

// Forward declaration to allow using SequencerTrack in function signatures without a circular include.
struct SequencerTrack;

/**
 * @name Display Update Functions
 * @brief Core functions for updating the display content based on the current mode.
 * @{
 */
void updateNormalClockDisplay(bool updateDest = true, bool updatePres = true, bool updateLast = true);
void updateNormalClockDisplay_internal(bool updateDest = true, bool updatePres = true, bool updateLast = true);
void updateMarqueeDisplay();
void handleWeatherDisplay();
void displayOverrideMessage();
void displayMarqueeOverride();
void updateStockTickerDisplay();
void showTemporaryMessage(const char* month, const char* day, const char* year, const char* time, int duration);
const char* getIconForWeatherCode(int code);
void resetWeatherFetchState();
/** @} */


/**
 * @name Sequencer Display Control Functions
 * @brief Functions used by the animation sequencer to interact with the display.
 * @{
 */

/**
 * @brief Sets the text of a specific display segment or a whole row.
 * @details This is a primary interface for the sequencer to write text to the display.
 * It respects the `isRowInManualMode` flag.
 * @param row The target row (0-2).
 * @param segment The target segment (0-3), or -1 to update the entire row with a 13-char string.
 * @param text The text to display.
 */
void updateDisplaySegment(int row, int segment, const std::string& text);

/**
 * @brief Restores a display row to its normal, non-manual state.
 * @details This function is called by the sequencer (`SEQ_CMD_RESTORE_ROW`) to release control of a
 * row, allowing the standard display handlers to take over again.
 * @param row The row to restore (0-2).
 */
void restoreDisplayRow(int row);

/**
 * @brief Calculates and caches the UTC offsets for the present and destination timezones.
 * @details This function is called infrequently (on startup, settings change, or hourly)
 * to perform the expensive, memory-unsafe `setenv` calls. The calculated offsets
 * are then used by the high-frequency display loop for safe time calculations.
 */
void updateTimezoneOffsets();
/** @} */

#endif // DISPLAY_MANAGER_H