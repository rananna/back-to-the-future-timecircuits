/**
 * @file DisplayManager.h
 * @brief Manages the content displayed on the time circuits during normal operation.
 * @details This module is responsible for rendering the standard clock display, the
 * Data Link marquee, the live weather display, and any override messages. It acts as the
 * primary interface for what should be shown when no special animations are active.
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <string>

// --- Global State ---

/**
 * @var manualDisplayText
 * @brief A 3x4 array holding text for manual display segment overrides, typically from Home Assistant.
 * @details `manualDisplayText[row][segment]` stores the string to display. An empty string means no override.
 */
extern std::string manualDisplayText[3][4];

/**
 * @var isRowInManualMode
 * @brief A boolean array indicating if a display row is currently under manual control.
 * @details If `isRowInManualMode[row]` is true, the normal clock/data display for that row is suspended.
 */
extern bool isRowInManualMode[3];

// --- Function Declarations ---

/**
 * @brief Updates all three display rows with the standard time information.
 * @details This is the default display function. It renders the Destination, Present,
 * and Last Time Departed rows based on the current time and user settings. It also
 * handles any active manual overrides from `manualDisplayText`.
 */
void updateNormalClockDisplay();

/**
 * @brief Updates the bottom display row with scrolling data from the Data Link feature.
 * @details Manages the scrolling position and content for the marquee display, cycling through
 * the various configured data points.
 */
void updateMarqueeDisplay();

/**
 * @brief Updates the bottom display row with the live weather information screen.
 * @details Cycles through the different pages of weather data (current, forecast, etc.)
 * on the Last Time Departed display row.
 */
void handleWeatherDisplay();

/**
 * @brief Renders a full-screen override message across all three rows.
 * @details Used for critical alerts or notifications from Home Assistant.
 */
void displayOverrideMessage();

/**
 * @brief Renders a temporary, scrolling marquee message on the bottom row.
 * @details Used for temporary notifications from Home Assistant that override the normal Data Link.
 */
void displayMarqueeOverride();

/**
 * @brief Displays a static, temporary message on the bottom row for a fixed duration.
 * @param month The text for the 'Month' segment.
 * @param day The text for the 'Day' segment.
 * @param year The text for the 'Year' segment.
 * @param time The text for the 'Time' segment.
 * @param duration The duration to display the message in milliseconds.
 */
void showTemporaryMessage(const char* month, const char* day, const char* year, const char* time, int duration);

/**
 * @brief Converts a weather code from the Open-Meteo API into a 2-character display icon.
 * @param code The integer weather code.
 * @return A 2-character string representing the weather condition (e.g., "SU" for sunny).
 */
const char* getIconForWeatherCode(int code);

/**
 * @brief Sets the text for a single display segment, putting that row into manual override mode.
 * @param row The target display row (0: Destination, 1: Present, 2: Last Departed).
 * @param segment The target display segment (0: Month, 1: Day, 2: Year, 3: Time).
 * @param text The text to display. An empty string can be used to clear the override for that segment.
 */
void updateDisplaySegment(int row, int segment, const std::string& text);

#endif // DISPLAY_MANAGER_H