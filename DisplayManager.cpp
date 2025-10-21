/**
 * @file DisplayManager.cpp
 * @brief Manages the content displayed on the time circuits during normal operation.
 * @details This module is responsible for rendering the standard clock display, as well
 * as handling the logic for alternative display modes like the stock ticker, weather
 * forecast, and data-driven marquee. It acts as a high-level controller for what
 * should be shown, calling the lower-level functions in HardwareControl.cpp to
 * actually write to the displays.
 */

/**
 * @file DisplayManager.cpp
 * @brief Manages the content displayed on the time circuits during normal operation.
 * @details This module is responsible for rendering the standard clock display, as well
 * as handling the logic for alternative display modes like the stock ticker, weather
 * forecast, and data-driven marquee. It acts as a high-level controller for what
 * should be shown, calling the lower-level functions in HardwareControl.cpp to
 * actually write to the displays.
 */

#include "DebugLog.h"
#include "DisplayManager.h"
#include "DataManager.h"
#include "timezone.h"
#include "EventManager.h"
#include "StockManager.h"
#include <string>
#include <algorithm>
#include <cctype>

extern StockManager stockManager;
extern String overrideMessageLine1;
extern String overrideMessageLine2;
extern String overrideMessageLine3;

// State management for override message scrolling
static int overrideScrollPosition[3] = {0, 0, 0};
static unsigned long lastOverrideScrollTime[3] = {0, 0, 0};
static std::string previousOverrideMessage[3] = {"", "", ""};

// Define and initialize the dirty flags and buffers for scrolling text
bool isMarqueeBufferDirty = true;
bool isWeatherBufferDirty = true;

std::string marqueeBuffer;
char weatherBuffer[512]; // Increased size for safety, changed to char array
std::string marqueeOverrideBuffer;

#include "HardwareControl.h"
#include "AnimationManager.h" // For SequencerTrack struct
#include <cmath> // For std::isnan and std::isinf

// --- Timezone Offset Caching ---
// These variables store the calculated UTC offsets to avoid calling the memory-unsafe
// `setenv` function in the high-frequency display loop.
static long present_tz_offset_seconds = 0;
static long dest_tz_offset_seconds = 0;

// Forward declaration for the timeout handler in the main .ino file
void handleWeatherTimeout();

/**
 * @brief Performs a sanity check on the contents of the weather data structure.
 * @details This function acts as a final line of defense to prevent displaying
 * nonsensical or potentially crashing data that might have been parsed correctly
 * but is logically invalid (e.g., extreme temperatures, invalid humidity).
 * @param data A const reference to the WeatherData object to be checked.
 * @return `true` if the data is plausible, `false` otherwise.
 */
bool isWeatherDataSane(const WeatherData& data) {
    // Check for NaN or infinity in float values, which can cause crashes or weird display artifacts.
    if (std::isnan(data.temperature) || std::isinf(data.temperature) ||
        std::isnan(data.apparentTemperature) || std::isinf(data.apparentTemperature) ||
        std::isnan(data.windSpeed) || std::isinf(data.windSpeed) ||
        std::isnan(data.dailyHigh) || std::isinf(data.dailyHigh) ||
        std::isnan(data.dailyLow) || std::isinf(data.dailyLow) ||
        std::isnan(data.tomorrowHigh) || std::isinf(data.tomorrowHigh) ||
        std::isnan(data.tomorrowLow) || std::isinf(data.tomorrowLow) ||
        std::isnan(data.maxWindSpeed) || std::isinf(data.maxWindSpeed)) {
        Log_printf(LOG_LEVEL_WARN, "Weather data sanity check failed: NaN or Inf value detected.");
        return false;
    }
    for (int i = 0; i < 3; ++i) {
        if (std::isnan(data.hourlyTemp[i]) || std::isinf(data.hourlyTemp[i])) {
            Log_printf(LOG_LEVEL_WARN, "Weather data sanity check failed: NaN or Inf in hourly temp.");
            return false;
        }
    }

    // Check for plausible temperature ranges. Using a wide range to be safe.
    // Assuming units are either Celsius or Fahrenheit, -200 to 200 should cover all realistic scenarios.
    if (data.temperature < -200 || data.temperature > 200 ||
        data.apparentTemperature < -200 || data.apparentTemperature > 200 ||
        data.dailyHigh < -200 || data.dailyHigh > 200 ||
        data.dailyLow < -200 || data.dailyLow > 200 ||
        data.tomorrowHigh < -200 || data.tomorrowHigh > 200 ||
        data.tomorrowLow < -200 || data.tomorrowLow > 200) {
        Log_printf(LOG_LEVEL_WARN, "Weather data sanity check failed: Unrealistic temperature value.");
        return false;
    }

    // Check humidity range
    if (data.humidity < 0 || data.humidity > 100) {
        Log_printf(LOG_LEVEL_WARN, "Weather data sanity check failed: Humidity out of range (0-100).");
        return false;
    }

    // Check for negative wind speed
    if (data.windSpeed < 0 || data.maxWindSpeed < 0) {
        Log_printf(LOG_LEVEL_WARN, "Weather data sanity check failed: Negative wind speed.");
        return false;
    }

    // Check for valid (non-zero) timestamps for sunrise/sunset
    if (data.sunrise <= 0 || data.sunset <= 0) {
        Log_printf(LOG_LEVEL_WARN, "Weather data sanity check failed: Invalid sunrise/sunset timestamp.");
        return false;
    }

    return true; // All checks passed
}


// File-scoped state variables for the weather fetch process
static bool initialFetchTriggered = false;
static unsigned long initialFetchStartTime = 0;
static TaskHandle_t weatherTaskHandle = NULL;
static bool initialFetchTimedOut = false;
static unsigned long lastWeatherFetchTime = 0;
const unsigned long WEATHER_REFRESH_INTERVAL = 300000; // 5 minutes
const int WEATHER_TASK_STACK_SIZE = 8192;

// File-scoped state variables for the weather display state machine
static WeatherDisplayState weatherState = WD_START_PAGE;
static int weatherPage = 0;
static int weatherScrollPosition = 0;
static unsigned long lastWeatherUpdate = 0;

// File-scoped state variables for the stock ticker display state machine
StockDisplayState stockState = SD_START_PAGE;
static int stockScrollPosition = 0;
static unsigned long lastStockUpdate = 0;

/**
 * @brief Resets the state flags used for the initial weather data fetch.
 * @details This function is called to clear any timeout or error states,
 * allowing a fresh attempt to fetch weather data.
 */
void resetWeatherFetchState() {
    Log_printf(LOG_LEVEL_INFO, "Resetting weather fetch timeout state.");
    initialFetchTimedOut = false;
    initialFetchTriggered = false;
    initialFetchStartTime = 0;
}

// A struct to hold the state of a scrolling text segment.
struct ScrollState {
    int position = 0;
    unsigned long lastScrollTime = 0;
};

// An array to hold the scroll state for each of the 4 segments of the weather display row.
static ScrollState weatherScrollStates[4];

/**
 * @brief Manages the scrolling of a string within a fixed-width viewport.
 * @param fullText The complete string to be scrolled.
 * @param width The width of the display segment (viewport).
 * @param state A reference to the ScrollState object for this segment.
 * @param scrollSpeed The delay in milliseconds between scroll steps.
 * @return A substring of the fullText representing the current viewport.
 */
String getScrolledViewport(const String& fullText, int width, ScrollState& state, unsigned long scrollSpeed) {
    if (fullText.length() <= width) {
        // If the text fits, reset the scroll position for the next long text and return.
        state.position = 0;
        return fullText;
    }

    // Pad the text with spaces for a smoother looping effect.
    String paddedText = "  " + fullText + "  ";

    // Update the scroll position based on the scroll speed.
    if (millis() - state.lastScrollTime > scrollSpeed) {
        state.lastScrollTime = millis();
        state.position++;
        // If we've scrolled past the end, loop back to the beginning.
        if (state.position > paddedText.length() - width) {
            state.position = 0;
        }
    }

    return paddedText.substring(state.position, state.position + width);
}

// External declaration for the global stock data array.
extern StockData stockData[3];

// Global arrays to support manual text override via MQTT or API.
bool weatherDataUpdated = false;
std::string manualDisplayText[3][4];
std::string preAnimationDisplayText[3][4];
bool isRowInManualMode[3] = {false, false, false};

/**
 * @brief Displays a temporary, static message on the bottom display row.
 * @details This is a utility function for showing a short-lived message that will be
 * automatically cleared after the specified duration.
 * @param month Text for the month segment (3 chars).
 * @param day Text for the day segment (2 chars).
 * @param year Text for the year segment (4 chars).
 * @param time Text for the time segment (4 chars).
 * @param duration The time in milliseconds to display the message.
 */
void showTemporaryMessage(const char* month, const char* day, const char* year, const char* time, int duration) {
    if (!hardwareInitialized) return;
#if ENABLE_HARDWARE
    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        printToDisplay(lastRow.month, month, 1);
        printToDisplay(lastRow.day, day, 2);
        printToDisplay(lastRow.year, year);
        printToDisplay(lastRow.time, time);
        lastRow.month.writeDisplay();
        lastRow.day.writeDisplay();
        lastRow.year.writeDisplay();
        lastRow.time.writeDisplay();
        xSemaphoreGive(xDisplayHardwareMutex);
    }
    delay(duration);
#endif
}

/**
 * @brief Converts an Open-Meteo weather code into a human-readable string.
 * @param code The integer weather code from the API.
 * @return A C-string with the weather description (e.g., "PARTLY CLOUDY").
 */
const char* getWeatherDescriptionForCode(int code) {
    switch (code) {
        case 0: return "CLEAR SKY";
        case 1: return "MAINLY CLEAR";
        case 2: return "PARTLY CLOUDY";
        case 3: return "OVERCAST";
        case 45: case 48: return "FOG";
        case 51: case 53: case 55: return "DRIZZLE";
        case 61: case 63: case 65: return "RAIN";
        case 66: case 67: return "FREEZING RAIN";
        case 71: case 73: case 75: return "SNOW";
        case 77: return "SNOW GRAINS";
        case 80: case 81: case 82: return "RAIN SHOWERS";
        case 85: case 86: return "SNOW SHOWERS";
        case 95: return "THUNDERSTORM";
        case 96: case 99: return "T-STORM W/ HAIL";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Converts an Open-Meteo weather code into a 2-character icon for display.
 * @param code The integer weather code from the API.
 * @return A 2-character C-string icon (e.g., "CL" for cloudy).
 */
const char* getIconForWeatherCode(int code) {
    // ... function content remains the same ...
    switch (code) {
        case 0: case 1: return "SU";
        case 2: return "CL";
        case 3: return "CL";
        case 45: case 48: return "CL";
        case 51: case 53: case 55: return "RN";
        case 61: case 63: case 65: return "RN";
        case 66: case 67: return "RN";
        case 71: case 73: case 75: return "SN";
        case 77: return "SN";
        case 80: case 81: case 82: return "RN";
        case 85: case 86: return "SN";
        case 95: case 96: case 99: return "ST";
        default: return "--";
    }
}

/**
 * @brief Manages the display logic for the stock ticker mode.
 * @details This function implements the state machine for the stock ticker. It keeps the
 * top two rows as a normal clock and uses the bottom row to scroll through stock data
 * fetched by the `StockManager`. It handles states for connecting, scrolling data, and
 * pausing between tickers.
 */
void updateStockTickerDisplay() {
    if (isDisplayAsleep || isAnimating || !hardwareInitialized) return;
#if ENABLE_HARDWARE
    // In stock mode, top two rows show the clock
    updateNormalClockDisplay_internal(true, true, false);

    // Turn off AM/PM LEDs for the last row
    digitalWrite(LAST_AM_PIN, LOW);
    digitalWrite(LAST_PM_PIN, LOW);

    if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
        const unsigned long scrollSpeed = 250;
        const unsigned long pauseDuration = 250; // 0.25-second pause between tickers
        static char stockMarqueeBuffer[256]; // Buffer for the full text

        static bool isStatusMessage = false;
        // State machine for stock ticker display
        switch (stockState) {
            case SD_CONNECTING: {
                std::string statusMessage;
                if (stockManager.getAssets().empty()) {
                    statusMessage = "ADD STOCKS IN UI";
                } else if (!stockManager.isTimeSynchronized()) {
                    statusMessage = "CONNECTING...";
                } else if (stockManager.isFetching()) {
                    statusMessage = "LOADING STOCKS";
                } else if (stockManager.hasDataBeenUpdated() || stockManager.hasAnyValidData()) {
                    if (stockManager.hasDataBeenUpdated()) {
                        stockManager.clearDataUpdatedFlag();
                    }
                    stockState = SD_START_PAGE;
                    xSemaphoreGive(xDisplayDataMutex);
                    return;
                } else {
                    statusMessage = "WAIT...";
                }

                if (stockState == SD_CONNECTING) {
                    isStatusMessage = true;
                    snprintf(stockMarqueeBuffer, sizeof(stockMarqueeBuffer), "             %s", statusMessage.c_str());
                    stockScrollPosition = 0;
                    stockState = SD_SCROLLING;
                    lastStockUpdate = millis();
                }
                break;
            }
            case SD_START_PAGE: {
                isStatusMessage = false;
                if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
                    printToDisplay(lastRow.month, "   ", 0); printToDisplay(lastRow.day, "  ", 0);
                    printToDisplay(lastRow.year, "    ", 0); printToDisplay(lastRow.time, "    ", 0);
                    lastRow.month.writeDisplay(); lastRow.day.writeDisplay(); lastRow.year.writeDisplay(); lastRow.time.writeDisplay();
                    vTaskDelay(pdMS_TO_TICKS(2));
                    xSemaphoreGive(xDisplayHardwareMutex);
                }

                String marqueeLine = stockManager.getMarqueeLine();
                marqueeLine.toUpperCase();

                snprintf(stockMarqueeBuffer, sizeof(stockMarqueeBuffer), "             %s", marqueeLine.c_str());
                stockScrollPosition = 0;
                stockState = SD_SCROLLING;
                lastStockUpdate = millis();
                break;
            }

            case SD_SCROLLING: {
                // --- FIX: Add a task delay to prevent watchdog timeouts ---
                // This tight loop can starve other tasks. A small delay allows the
                // scheduler to run other essential tasks, like the watchdog feed.
                vTaskDelay(pdMS_TO_TICKS(1));
                if (millis() - lastStockUpdate > scrollSpeed) {
                    lastStockUpdate = millis();

                    if (stockScrollPosition > strlen(stockMarqueeBuffer)) {
                        stockState = SD_PAUSING;
                        lastStockUpdate = millis();
                    } else {
                        char viewport[14];
                        int text_len = strlen(stockMarqueeBuffer);
                        for (int i = 0; i < 13; i++) {
                            int source_idx = stockScrollPosition + i;
                            if (source_idx < text_len) viewport[i] = stockMarqueeBuffer[source_idx];
                            else viewport[i] = ' ';
                        }
                        viewport[13] = '\0';

                        char segment_month[4], segment_day[3], segment_year[5], segment_time[5];
                        strncpy(segment_month, viewport, 3); segment_month[3] = '\0';
                        strncpy(segment_day, viewport + 3, 2); segment_day[2] = '\0';
                        strncpy(segment_year, viewport + 5, 4); segment_year[4] = '\0';
                        strncpy(segment_time, viewport + 9, 4); segment_time[4] = '\0';

                        if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
                            printToDisplay(lastRow.month, segment_month, 1);
                            printToDisplay(lastRow.day, segment_day, 2);
                            printToDisplay(lastRow.year, segment_year, 0);
                            printToDisplay(lastRow.time, segment_time, 0);
                            lastRow.month.writeDisplay(); lastRow.day.writeDisplay(); lastRow.year.writeDisplay(); lastRow.time.writeDisplay();
                            vTaskDelay(pdMS_TO_TICKS(2));
                            xSemaphoreGive(xDisplayHardwareMutex);
                        }
                        stockScrollPosition++;
                    }
                }
                break;
            }

            case SD_PAUSING: {
                if (millis() - lastStockUpdate > pauseDuration) {
                    if (isStatusMessage) {
                        stockState = SD_CONNECTING;
                    } else {
                        stockManager.nextPage();
                        stockState = SD_START_PAGE;
                    }
                }
                break;
            }

            // The SD_MARKET_CLOSED and SD_ERROR states are now removed.
            // All messages are handled by the scrolling logic above.
        }
        xSemaphoreGive(xDisplayDataMutex);
    }
#endif
}

/**
 * @brief Safely calculates and caches the UTC offsets for the configured timezones.
 * @details This is the key function for the memory-safe timezone fix. It is called
 * very infrequently. It temporarily sets the system timezone to calculate the offset
 * from UTC for both the 'Present' and 'Destination' timezones, storing the results
 * in static variables. This avoids calling the problematic `setenv` function in any
 * high-frequency loops.
 */
void updateTimezoneOffsets() {
    if (!timeSynchronized) {
        Log_printf(LOG_LEVEL_WARN, "Cannot update timezone offsets: NTP time not synchronized yet.");
        return;
    }

    Log_printf(LOG_LEVEL_INFO, "Recalculating and caching timezone offsets...");

    if (xSemaphoreTake(xTimeLibMutex, portMAX_DELAY) == pdTRUE) {
        time_t now_utc;
        time(&now_utc);
        struct tm timeinfo_utc;
        gmtime_r(&now_utc, &timeinfo_utc);

        // --- Calculate Present Timezone Offset ---
        setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
        tzset();
        struct tm timeinfo_local;
        localtime_r(&now_utc, &timeinfo_local);
        
        // Temporarily set TZ to UTC to get the UTC time in a tm struct
        setenv("TZ", "UTC0", 1);
        tzset();
        struct tm timeinfo_utc_as_local;
        localtime_r(&now_utc, &timeinfo_utc_as_local);
        
        present_tz_offset_seconds = mktime(&timeinfo_local) - mktime(&timeinfo_utc_as_local);

        // --- Calculate Destination Timezone Offset ---
        setenv("TZ", TZ_DATA[currentSettings.destinationTimezoneIndex].tzString, 1);
        tzset();
        localtime_r(&now_utc, &timeinfo_local); // Recalculate with the new destination TZ
        
        // Temporarily set TZ back to UTC to get the correct UTC time in a tm struct for the subtraction
        setenv("TZ", "UTC0", 1);
        tzset();
        localtime_r(&now_utc, &timeinfo_utc_as_local);

        dest_tz_offset_seconds = mktime(&timeinfo_local) - mktime(&timeinfo_utc_as_local);

        // --- IMPORTANT: Restore original timezone ---
        setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
        tzset();

        xSemaphoreGive(xTimeLibMutex);
    }

    Log_printf(LOG_LEVEL_INFO, "Timezone offsets cached: Present=%ld, Destination=%ld", present_tz_offset_seconds, dest_tz_offset_seconds);
}

/**
 * @brief Displays a high-priority, persistent override message on all three rows.
 * @details This function is used for critical alerts or messages sent via MQTT/API.
 * It takes precedence over all other display modes. It supports both static, centered
 * text for short messages and a scrolling marquee for longer messages.
 */
void displayOverrideMessage() {
    if (!hardwareInitialized) return;
#if ENABLE_HARDWARE
    if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
        if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {

            // Turn off all AM/PM LEDs as they are not used in this mode
            digitalWrite(DEST_AM_PIN, LOW);
            digitalWrite(DEST_PM_PIN, LOW);
            digitalWrite(PRES_AM_PIN, LOW);
            digitalWrite(PRES_PM_PIN, LOW);
            digitalWrite(LAST_AM_PIN, LOW);
            digitalWrite(LAST_PM_PIN, LOW);

            String messages[3] = {overrideMessageLine1, overrideMessageLine2, overrideMessageLine3};
            DisplayRow* rows[3] = {&destRow, &presRow, &lastRow};
            const unsigned long scrollSpeed = 250; // Milliseconds between scroll steps

            for (int i = 0; i < 3; i++) {
                // Check if the message for this row has changed
                if (messages[i].c_str() != previousOverrideMessage[i]) {
                    previousOverrideMessage[i] = messages[i].c_str();
                    overrideScrollPosition[i] = 0; // Reset scroll position
                }

                String currentMessage = messages[i];
                currentMessage.toUpperCase();
                String output_buffer;

                if (currentMessage.length() > 13) {
                    // --- Scrolling Marquee Logic ---
                    String padded_message = "  " + currentMessage + "  ";
                    if (millis() - lastOverrideScrollTime[i] > scrollSpeed) {
                        lastOverrideScrollTime[i] = millis();
                        overrideScrollPosition[i]++;
                        if (overrideScrollPosition[i] > padded_message.length() - 13) {
                            overrideScrollPosition[i] = 0;
                        }
                    }
                    output_buffer = padded_message.substring(overrideScrollPosition[i], overrideScrollPosition[i] + 13);
                } else {
                    // --- Centered Static Text Logic ---
                    int padding = (13 - currentMessage.length()) / 2;
                    output_buffer = "";
                    for (int p = 0; p < padding; p++) {
                        output_buffer += " ";
                    }
                    output_buffer += currentMessage;
                    while (output_buffer.length() < 13) {
                        output_buffer += " ";
                    }
                }

                // Split the 13-character buffer into the four display segments and print
                printToDisplay(rows[i]->month, output_buffer.substring(0, 3).c_str(), 1);
                printToDisplay(rows[i]->day, output_buffer.substring(3, 5).c_str(), 2);
                printToDisplay(rows[i]->year, output_buffer.substring(5, 9).c_str());
                printToDisplay(rows[i]->time, output_buffer.substring(9, 13).c_str());

                // Write the changes to the physical display row
                rows[i]->month.writeDisplay();
                rows[i]->day.writeDisplay();
                rows[i]->year.writeDisplay();
                rows[i]->time.writeDisplay();
                vTaskDelay(pdMS_TO_TICKS(2));
            }
            xSemaphoreGive(xDisplayHardwareMutex);
        }
        xSemaphoreGive(xDisplayDataMutex);
    }
#endif
}

/**
 * @brief Sets or clears manual text for a specific display segment or an entire row.
 * @details This is a key function for sequencer-based animations and external control.
 * It updates an internal buffer (`manualDisplayText`) with the given text and sets a
 * flag (`isRowInManualMode`) that causes the main display loop to show this text
 * instead of the normal clock time for that row.
 * @param row The target display row (0-2).
 * @param segment The target segment (0-3), or -1 to update the entire row at once.
 * @param text The text to display. An empty string clears the manual override for that segment.
 */
void updateDisplaySegment(int row, int segment, const std::string& text) {
    if (row < 0 || row > 2) { // Invalid row
        return;
    }

    // --- FIX: When updating a single segment, do not clear the others. ---
    // The previous logic for segment == -1 was clearing other segments
    // unintentionally when only one segment was meant to be updated.
    if (segment >= 0 && segment <= 3) {
        // This is the correct logic for updating a single, specific segment.
        manualDisplayText[row][segment] = text;
    } else if (segment == -1) {
        // This is for updating the entire row at once (e.g., for marquees).
        // The text is assumed to be 13 characters long.
        std::string safe_text = text;
        if (safe_text.length() > 13) {
            safe_text = safe_text.substr(0, 13);
        } else {
            safe_text.append(13 - safe_text.length(), ' ');
        }
        manualDisplayText[row][0] = safe_text.substr(0, 3);
        manualDisplayText[row][1] = safe_text.substr(3, 2);
        manualDisplayText[row][2] = safe_text.substr(5, 4);
        manualDisplayText[row][3] = safe_text.substr(9, 4);
    } else {
        // An invalid segment was provided, so we do nothing.
        return;
    }

    // A row is in manual mode if any of its segments have text, OR if the text for the whole row is empty.
    // This ensures that clearing the last segment of a row correctly returns it to clock mode.
    isRowInManualMode[row] = !manualDisplayText[row][0].empty() ||
                             !manualDisplayText[row][1].empty() ||
                             !manualDisplayText[row][2].empty() ||
                             !manualDisplayText[row][3].empty();

    // After any manual update, we must redraw the clock display to show the changes.
    updateNormalClockDisplay();
}

/**
 * @brief Restores a display row to its normal clock function.
 * @details This function clears any manual text overrides for the specified row and
 * resets its mode, causing it to display the standard time information again on the
 * next display update cycle.
 * @param row The display row (0-2) to restore.
 */
void restoreDisplayRow(int row) {
    if (row < 0 || row > 2) return;

    // Clear all manual text for the row
    for (int i = 0; i < 4; ++i) {
        manualDisplayText[row][i].clear();
    }

    // Set the row back to normal clock mode
    isRowInManualMode[row] = false;

    // Trigger an update to show the clock again
    updateNormalClockDisplay();
}

/**
 * @brief The primary function for updating the three main time circuit displays.
 * @details This is one of the most critical functions in the firmware. It's responsible
 * for calculating the correct times for all three rows (Destination, Present, and
 * Last Time Departed), handling timezone conversions, and displaying them. It also
 * manages the logic for overriding a specific row with manually set text.
 * @param updateDest If true, the destination time row is updated.
 * @param updatePres If true, the present time row is updated.
 * @param updateLast If true, the last time departed row is updated.
 */
/**
 * @brief The internal implementation for updating the three main time circuit displays.
 * @details This is the core rendering function. It calculates the correct times for all
 * three rows (Destination, Present, and Last Time Departed), handling timezone conversions,
 * 24-hour format, and AM/PM LEDs. It also manages the logic for overriding a specific
 * row with manually set text from the sequencer or an external source. It's called by
 * the public `updateNormalClockDisplay` wrapper and other display modes that need to
 * show a partial clock (like the stock ticker).
 * @param updateDest If true, the destination time row is updated.
 * @param updatePres If true, the present time row is updated.
 * @param updateLast If true, the last time departed row is updated.
 */
void updateNormalClockDisplay_internal(bool updateDest, bool updatePres, bool updateLast) {
    if (isDisplayAsleep || isAnimating || !hardwareInitialized) {
        return;
    }

#if ENABLE_HARDWARE
    // Helper lambda to format and print a row with blinking logic
    auto printRow = [&](DisplayRow& row, const struct tm& timeinfo, int year, bool showDecimal, int rowIndex) {
        char s_month[4], s_day[3], s_year[5], s_time[5];
        char ampm_char;
        SequencerTrack& track = sequencerTracks[rowIndex];

        strftime(s_month, sizeof(s_month), "%b", &timeinfo);
        s_month[0] = toupper(s_month[0]);
        s_month[1] = toupper(s_month[1]);
        s_month[2] = toupper(s_month[2]);

        strftime(s_day, sizeof(s_day), "%d", &timeinfo);
        snprintf(s_year, sizeof(s_year), "%d", year);

        if (currentSettings.displayFormat24h) {
            strftime(s_time, sizeof(s_time), "%H%M", &timeinfo);
            ampm_char = ' ';
        } else {
            strftime(s_time, sizeof(s_time), "%I%M", &timeinfo);
            strftime(&ampm_char, 2, "%p", &timeinfo);
        }

        uint8_t am_pin = -1, pm_pin = -1;
        switch(rowIndex) {
            case 0: am_pin = DEST_AM_PIN; pm_pin = DEST_PM_PIN; break;
            case 1: am_pin = PRES_AM_PIN; pm_pin = PRES_PM_PIN; break;
            case 2: am_pin = LAST_AM_PIN; pm_pin = LAST_PM_PIN; break;
        }

        if (am_pin != -1 && pm_pin != -1 && !isSequencerActive) {
            if (ampm_char == 'A') {
                digitalWrite(am_pin, HIGH);
                digitalWrite(pm_pin, LOW);
            } else if (ampm_char == 'P') {
                digitalWrite(am_pin, LOW);
                digitalWrite(pm_pin, HIGH);
            } else {
                digitalWrite(am_pin, LOW);
                digitalWrite(pm_pin, LOW);
            }
        }

        // Month (Segment 0)
        if ((track.isPulsing[0] && !track.pulseStates[0]) || (track.isFlashing[0] && !track.flashStates[0])) printToDisplay(row.month, "   ", 1);
        else printToDisplay(row.month, s_month, 1);

        // Day (Segment 1)
        if ((track.isPulsing[1] && !track.pulseStates[1]) || (track.isFlashing[1] && !track.flashStates[1])) printToDisplay(row.day, "  ", 2);
        else printToDisplay(row.day, s_day, 2);

        // Year (Segment 2)
        if ((track.isPulsing[2] && !track.pulseStates[2]) || (track.isFlashing[2] && !track.flashStates[2])) printToDisplay(row.year, "    ");
        else printToDisplay(row.year, s_year);

        // Time (Segment 3)
        if ((track.isPulsing[3] && !track.pulseStates[3]) || (track.isFlashing[3] && !track.flashStates[3])) {
            printToDisplay(row.time, "    ");
        } else {
            // We manually write the time to control the decimal point (blinking colon)
            row.time.clear();
            row.time.writeDigitAscii(0, s_time[0]);
            row.time.writeDigitAscii(1, s_time[1], showDecimal);
            row.time.writeDigitAscii(2, s_time[2]);
            row.time.writeDigitAscii(3, s_time[3]);
        }
    };

    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
        if (timeSynchronized) {
            // --- MEMORY-SAFE TIME CALCULATION ---
            // This is the core of the fix. Instead of changing the system timezone in this
            // high-frequency loop, we get the current UTC time and simply add the pre-calculated,
            // cached offsets. This is extremely fast and allocates no memory.
            time_t now_utc = time(nullptr);
            time_t present_time_t = now_utc + present_tz_offset_seconds;
            time_t dest_time_t = now_utc + dest_tz_offset_seconds;

            struct tm dest_timeinfo, present_timeinfo;

            // --- Simplified Time Calculation ---
            // The `gmtime_r` function correctly interprets a `time_t` as a UTC value
            // and converts it to a `struct tm`. The previous complex logic was unnecessary.
            gmtime_r(&dest_time_t, &dest_timeinfo);
            gmtime_r(&present_time_t, &present_timeinfo);

            if (updateDest) {
                dest_timeinfo.tm_year = currentSettings.destinationYear - 1900;
                if (!isRowInManualMode[0]) {
                    printRow(destRow, dest_timeinfo, currentSettings.destinationYear, true, 0);
                } else {
                    SequencerTrack& track = sequencerTracks[0];
                    if ((track.isPulsing[0] && !track.pulseStates[0]) || (track.isFlashing[0] && !track.flashStates[0])) printToDisplay(destRow.month, "   ", 1); else printToDisplay(destRow.month, manualDisplayText[0][0].c_str(), 1);
                    if ((track.isPulsing[1] && !track.pulseStates[1]) || (track.isFlashing[1] && !track.flashStates[1])) printToDisplay(destRow.day, "  ", 2); else printToDisplay(destRow.day, manualDisplayText[0][1].c_str(), 2);
                    if ((track.isPulsing[2] && !track.pulseStates[2]) || (track.isFlashing[2] && !track.flashStates[2])) printToDisplay(destRow.year, "    "); else printToDisplay(destRow.year, manualDisplayText[0][2].c_str());
                    if ((track.isPulsing[3] && !track.pulseStates[3]) || (track.isFlashing[3] && !track.flashStates[3])) printToDisplay(destRow.time, "    "); else printToDisplay(destRow.time, manualDisplayText[0][3].c_str());
                }
                destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();
            }

            if (updatePres) {
                bool showDecimalForPresent = (millis() / 500) % 2 == 0;
                if (!isRowInManualMode[1]) {
                    printRow(presRow, present_timeinfo, present_timeinfo.tm_year + 1900, showDecimalForPresent, 1);
                } else {
                    SequencerTrack& track = sequencerTracks[1];
                    if ((track.isPulsing[0] && !track.pulseStates[0]) || (track.isFlashing[0] && !track.flashStates[0])) printToDisplay(presRow.month, "   ", 1); else printToDisplay(presRow.month, manualDisplayText[1][0].c_str(), 1);
                    if ((track.isPulsing[1] && !track.pulseStates[1]) || (track.isFlashing[1] && !track.flashStates[1])) printToDisplay(presRow.day, "  ", 2); else printToDisplay(presRow.day, manualDisplayText[1][1].c_str(), 2);
                    if ((track.isPulsing[2] && !track.pulseStates[2]) || (track.isFlashing[2] && !track.flashStates[2])) printToDisplay(presRow.year, "    "); else printToDisplay(presRow.year, manualDisplayText[1][2].c_str());
                    if ((track.isPulsing[3] && !track.pulseStates[3]) || (track.isFlashing[3] && !track.flashStates[3])) printToDisplay(presRow.time, "    "); else printToDisplay(presRow.time, manualDisplayText[1][3].c_str());
                }
                presRow.month.writeDisplay(); presRow.day.writeDisplay(); presRow.year.writeDisplay(); presRow.time.writeDisplay();
            }
        }

        if (updateLast) {
            struct tm lastTimeDepartedInfo = {0};
            lastTimeDepartedInfo.tm_year = currentSettings.lastTimeDepartedYear - 1900;
            lastTimeDepartedInfo.tm_mon = currentSettings.lastTimeDepartedMonth - 1;
            lastTimeDepartedInfo.tm_mday = currentSettings.lastTimeDepartedDay;
            lastTimeDepartedInfo.tm_hour = currentSettings.lastTimeDepartedHour;
            lastTimeDepartedInfo.tm_min = currentSettings.lastTimeDepartedMinute;

            if (!isRowInManualMode[2]) {
                printRow(lastRow, lastTimeDepartedInfo, currentSettings.lastTimeDepartedYear, true, 2);
            } else {
                SequencerTrack& track = sequencerTracks[2];
                if ((track.isPulsing[0] && !track.pulseStates[0]) || (track.isFlashing[0] && !track.flashStates[0])) printToDisplay(lastRow.month, "   ", 1); else printToDisplay(lastRow.month, manualDisplayText[2][0].c_str(), 1);
                if ((track.isPulsing[1] && !track.pulseStates[1]) || (track.isFlashing[1] && !track.flashStates[1])) printToDisplay(lastRow.day, "  ", 2); else printToDisplay(lastRow.day, manualDisplayText[2][1].c_str(), 2);
                if ((track.isPulsing[2] && !track.pulseStates[2]) || (track.isFlashing[2] && !track.flashStates[2])) printToDisplay(lastRow.year, "    "); else printToDisplay(lastRow.year, manualDisplayText[2][2].c_str());
                if ((track.isPulsing[3] && !track.pulseStates[3]) || (track.isFlashing[3] && !track.flashStates[3])) printToDisplay(lastRow.time, "    "); else printToDisplay(lastRow.time, manualDisplayText[2][3].c_str());
            }
            lastRow.month.writeDisplay(); lastRow.day.writeDisplay(); lastRow.year.writeDisplay(); lastRow.time.writeDisplay();
        }
        xSemaphoreGive(xDisplayHardwareMutex);
    }
#endif
}

/**
 * @brief A thread-safe public wrapper for updating the main clock display.
 * @details This function acquires the display data mutex before calling the internal
 * `updateNormalClockDisplay_internal` function, ensuring that display updates do not

 * conflict with other tasks that might be modifying display-related data.
 * @param updateDest If true, the destination time row is updated.
 * @param updatePres If true, the present time row is updated.
 * @param updateLast If true, the last time departed row is updated.
 */
void updateNormalClockDisplay(bool updateDest, bool updatePres, bool updateLast) {
#if ENABLE_HARDWARE
  if (xSemaphoreTake(xDisplayDataMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    updateNormalClockDisplay_internal(updateDest, updatePres, updateLast);
    xSemaphoreGive(xDisplayDataMutex);
  }
#endif
}

/**
 * @brief Manages the display logic for the weather mode.
 * @details This function implements a state machine for the weather display. It handles
 * states for fetching data, displaying scrolling weather information pages (current,
 * forecast, etc.), pausing between pages, and showing error messages. It uses the
 * bottom display row, leaving the top two as a clock.
 */
void handleWeatherDisplay() {
#if ENABLE_HARDWARE
    // When the weather display is active, we must explicitly turn off the AM/PM LEDs for the last row,
    // as the weather display logic doesn't use the `updateDisplayRow` function which normally handles this.
    digitalWrite(LAST_AM_PIN, LOW);
    digitalWrite(LAST_PM_PIN, LOW);

    bool shouldWriteToDisplay = false;

    if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
        // Always update the top two rows with the normal clock display
        updateNormalClockDisplay_internal(true, true, false);

        if (initialFetchTimedOut) {
            updateNormalClockDisplay_internal(true, true, false);
            xSemaphoreGive(xDisplayDataMutex);
            return;
        }

        const unsigned long scrollSpeed = 250;
        const unsigned long pauseDuration = 1000;
        const unsigned long errorRetryDelay = 10000; // 10 seconds

        if (!currentWeatherData.dataValid) {
            std::string message;
            if (!currentWeatherData.errorReason.empty()) {
                message = "WEATHER ERROR: " + currentWeatherData.errorReason;
            } else if (isFetchingWeather) {
                message = "FETCHING WEATHER DATA...";
            } else {
                message = "WEATHER DATA UNAVAILABLE";
            }

            if (weatherState != WD_ERROR) {
                weatherState = WD_ERROR; // Use the error state for all non-valid data messages
                snprintf(weatherBuffer, sizeof(weatherBuffer), "             %s", message.c_str());
                for (int i = 0; weatherBuffer[i]; i++) weatherBuffer[i] = toupper(weatherBuffer[i]);
                weatherScrollPosition = 0;
                lastWeatherUpdate = millis();
            }

            if (initialFetchTriggered && initialFetchStartTime > 0 && millis() - initialFetchStartTime > 30000) {
                Log_printf(LOG_LEVEL_WARN, "Weather fetch task timed out. Deleting task.");
                if (weatherTaskHandle != NULL) {
                    vTaskDelete(weatherTaskHandle);
                    weatherTaskHandle = NULL;
                }
                isFetchingWeather = false;
                handleWeatherTimeout();
                xSemaphoreGive(xDisplayDataMutex);
                return;
            }

            if (!initialFetchTriggered && !isFetchingWeather) {
                Log_printf(LOG_LEVEL_INFO, "Weather data is invalid, triggering initial fetch.");
                isFetchingWeather = true;
                if (weatherTaskHandle != NULL) {
                    vTaskDelete(weatherTaskHandle);
                }
                xTaskCreate(fetchWeatherDataTask, "fetchWeatherDataTask", WEATHER_TASK_STACK_SIZE, NULL, 1, &weatherTaskHandle);
                initialFetchTriggered = true;
                initialFetchStartTime = millis();
                lastWeatherFetchTime = 0;
                weatherState = WD_ERROR; // Set state to immediately show "FETCHING..."
            }
        } else {
            initialFetchTriggered = false;
            initialFetchStartTime = 0;
            initialFetchTimedOut = false;

            // --- NEW: Sanity check the data before using it ---
            if (!isWeatherDataSane(currentWeatherData)) {
                currentWeatherData.dataValid = false;
                currentWeatherData.errorReason = "INVALID WEATHER DATA";
                // By setting dataValid to false, the logic will now fall through to the
                // error handling part of the state machine on the next iteration.
            }

            // The rest of this block will now only execute if the data is *still* considered valid
            // after the sanity check.
            if(currentWeatherData.dataValid) {
                if (lastWeatherFetchTime == 0) {
                    lastWeatherFetchTime = millis();
                }

                if ((millis() - lastWeatherFetchTime > WEATHER_REFRESH_INTERVAL) && !isFetchingWeather) {
                    Log_printf(LOG_LEVEL_INFO, "Periodic weather refresh triggered.");
                    lastWeatherFetchTime = millis();
                    isFetchingWeather = true;
                    initialFetchTriggered = true;
                    initialFetchStartTime = millis();
                    if (weatherTaskHandle != NULL) {
                        vTaskDelete(weatherTaskHandle);
                    }
                    xTaskCreate(fetchWeatherDataTask, "fetchWeatherDataTask", WEATHER_TASK_STACK_SIZE, NULL, 1, &weatherTaskHandle);
                }

                if (weatherDataUpdated || isWeatherBufferDirty) {
                    Log_printf(LOG_LEVEL_DEBUG, "Weather data updated, resetting state to WD_START_PAGE");
                    weatherState = WD_START_PAGE;
                    weatherPage = 0;
                    weatherDataUpdated = false;
                    isWeatherBufferDirty = false;
                    if (weatherTaskHandle != NULL) {
                        weatherTaskHandle = NULL;
                    }
                }
            }

            // This buffer is used to safely construct the display segments.
            char viewport[14];
            char segment_month[4], segment_day[3], segment_year[5], segment_time[5];

            switch (weatherState) {
                case WD_ERROR: {
                    if (millis() - lastWeatherUpdate > scrollSpeed) {
                        lastWeatherUpdate = millis();

                        // If we've scrolled past the end of the error message
                        if (weatherScrollPosition > strlen(weatherBuffer)) {
                            // After a delay, clear the error and try fetching data again
                            if (millis() - lastWeatherUpdate > errorRetryDelay) {
                                currentWeatherData.errorReason = ""; // Clear reason
                                weatherState = WD_START_PAGE;
                                initialFetchTriggered = false; // Allow a new fetch
                            }
                        } else {
                            // Manually construct the viewport without a large temporary buffer
                            int text_len = strlen(weatherBuffer);
                            for (int i = 0; i < 13; i++) {
                                int source_idx = weatherScrollPosition + i;
                                if (source_idx < text_len) {
                                    viewport[i] = weatherBuffer[source_idx];
                                } else {
                                    viewport[i] = ' '; // Pad with spaces
                                }
                            }
                            viewport[13] = '\0';

                            // Safely extract segments
                            strncpy(segment_month, viewport, 3); segment_month[3] = '\0';
                            strncpy(segment_day, viewport + 3, 2); segment_day[2] = '\0';
                            strncpy(segment_year, viewport + 5, 4); segment_year[4] = '\0';
                            strncpy(segment_time, viewport + 9, 4); segment_time[4] = '\0';

                            printToDisplay(lastRow.month, segment_month, 1);
                            printToDisplay(lastRow.day, segment_day, 2);
                            printToDisplay(lastRow.year, segment_year, 0);
                            printToDisplay(lastRow.time, segment_time, 0);
                            shouldWriteToDisplay = true;

                            weatherScrollPosition++;
                        }
                    }
                    break;
                }
                case WD_START_PAGE: {
                    // This entire case needs to be in its own scope to prevent "crosses initialization" errors.
                    {
                        // Clear display before showing new text
                        if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
                            printToDisplay(lastRow.month, "   ", 0); printToDisplay(lastRow.day, "  ", 0);
                            printToDisplay(lastRow.year, "    ", 0); printToDisplay(lastRow.time, "    ", 0);
                            lastRow.month.writeDisplay(); lastRow.day.writeDisplay();
                            lastRow.year.writeDisplay(); lastRow.time.writeDisplay();
                            vTaskDelay(pdMS_TO_TICKS(2));
                            xSemaphoreGive(xDisplayHardwareMutex);
                        }

                        // Build the weather string safely in the char buffer
                        char temp_buf[20];
                        const char* unit = currentSettings.useMetricUnits ? "C" : "F";
                        const char* windUnit = currentSettings.useMetricUnits ? "KPH" : "MPH";
                        char high_buf[8], low_buf[8];

                        snprintf(weatherBuffer, sizeof(weatherBuffer), "             "); // Start with padding

                        switch (weatherPage) {
                            case 0: // Current Weather
                                dtostrf(currentWeatherData.temperature, 4, 1, temp_buf);
                                snprintf(weatherBuffer + strlen(weatherBuffer), sizeof(weatherBuffer) - strlen(weatherBuffer), "CURRENTLY %s%s, %s", temp_buf, unit, getWeatherDescriptionForCode(currentWeatherData.weatherCode));
                                break;
                            case 1: // Tomorrow's Forecast
                                dtostrf(currentWeatherData.tomorrowHigh, 1, 0, high_buf);
                                dtostrf(currentWeatherData.tomorrowLow, 1, 0, low_buf);
                                snprintf(weatherBuffer + strlen(weatherBuffer), sizeof(weatherBuffer) - strlen(weatherBuffer), "TOMORROW HIGH %s%s, LOW %s%s, %s", high_buf, unit, low_buf, unit, getWeatherDescriptionForCode(currentWeatherData.tomorrowWeatherCode));
                                break;
                            case 2: // Wind & Rain
                                snprintf(weatherBuffer + strlen(weatherBuffer), sizeof(weatherBuffer) - strlen(weatherBuffer), "WIND %d %s, MAX %d %s, PRECIP %d%%", (int)currentWeatherData.windSpeed, windUnit, (int)currentWeatherData.maxWindSpeed, windUnit, currentWeatherData.precipitationProbability);
                                break;
                            case 3: { // Sunrise & Sunset
                                struct tm timeinfo; char timeStr[8];
                                time_t sunrise = currentWeatherData.sunrise;
                                time_t sunset = currentWeatherData.sunset;

                                // --- START: MODIFICATION - Use Weather Location Timezone for Sunrise/Sunset ---
                                const char* weatherTz = nullptr;
                                if (!currentWeatherData.timezone.empty()) {
                                    for (const auto& tzData : TZ_DATA) {
                                        if (currentWeatherData.timezone == tzData.ianaTzName) {
                                            weatherTz = tzData.tzString;
                                            break;
                                        }
                                    }
                                }

                                // Fallback to present time timezone if weather one isn't found
                                if (weatherTz == nullptr) {
                                    weatherTz = TZ_DATA[currentSettings.presentTimezoneIndex].tzString;
                                }

                                // Set the timezone for sunrise/sunset calculation
                                setenv("TZ", weatherTz, 1);
                                tzset();

                                // Format sunrise
                                localtime_r(&sunrise, &timeinfo);
                                strftime(timeStr, sizeof(timeStr), currentSettings.displayFormat24h ? "%H%M" : "%l%M%p", &timeinfo);
                                char* p_sunrise = timeStr;
                                if (p_sunrise[0] == ' ') p_sunrise++;
                                snprintf(weatherBuffer + strlen(weatherBuffer), sizeof(weatherBuffer) - strlen(weatherBuffer), "SUNRISE %s, SUNSET ", p_sunrise);

                                // Format sunset
                                localtime_r(&sunset, &timeinfo);
                                strftime(timeStr, sizeof(timeStr), currentSettings.displayFormat24h ? "%H%M" : "%l%M%p", &timeinfo);
                                char* p_sunset = timeStr;
                                if (p_sunset[0] == ' ') p_sunset++;
                                snprintf(weatherBuffer + strlen(weatherBuffer), sizeof(weatherBuffer) - strlen(weatherBuffer), "%s", p_sunset);

                                // Restore the original timezone for the main clock display
                                setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
                                tzset();
                                break;
                            }
                            case 4: { // Hourly Forecast
                                strncat(weatherBuffer, "NEXT 3 HRS ", sizeof(weatherBuffer) - strlen(weatherBuffer) - 1);
                                bool hourlyDataOk = true;
                                for(int i=0; i<3; ++i) if(currentWeatherData.hourlyCode[i] == -1) hourlyDataOk = false;

                                if (!hourlyDataOk) {
                                    strncat(weatherBuffer, "HOURLY DATA UNAVAILABLE", sizeof(weatherBuffer) - strlen(weatherBuffer) - 1);
                                } else {
                                    for (int i = 0; i < 3; ++i) {
                                        dtostrf(currentWeatherData.hourlyTemp[i], 1, 0, temp_buf);
                                        snprintf(weatherBuffer + strlen(weatherBuffer), sizeof(weatherBuffer) - strlen(weatherBuffer), "%s%s %s%s", temp_buf, unit, getWeatherDescriptionForCode(currentWeatherData.hourlyCode[i]), (i < 2 ? ", " : ""));
                                    }
                                }
                                break;
                            }
                            case 5: // Feels Like & Humidity
                                dtostrf(currentWeatherData.apparentTemperature, 1, 0, temp_buf);
                                snprintf(weatherBuffer + strlen(weatherBuffer), sizeof(weatherBuffer) - strlen(weatherBuffer), "FEELS LIKE %s%s, HUMIDITY %d%%", temp_buf, unit, currentWeatherData.humidity);
                                break;
                            case 6: // Today's High/Low
                                dtostrf(currentWeatherData.dailyHigh, 1, 0, high_buf);
                                dtostrf(currentWeatherData.dailyLow, 1, 0, low_buf);
                                snprintf(weatherBuffer + strlen(weatherBuffer), sizeof(weatherBuffer) - strlen(weatherBuffer), "TODAY HIGH %s%s, LOW %s%s", high_buf, unit, low_buf, unit);
                                break;
                        }
                        for (int i = 0; weatherBuffer[i]; i++) weatherBuffer[i] = toupper(weatherBuffer[i]);
                        weatherScrollPosition = 0;
                        weatherState = WD_SCROLLING;
                        lastWeatherUpdate = millis();
                    }
                    break; // End of WD_START_PAGE
                }

                case WD_SCROLLING: {
                    if (millis() - lastWeatherUpdate > scrollSpeed) {
                        lastWeatherUpdate = millis();

                        // Check if we have scrolled past the end of the text
                        if (weatherScrollPosition > strlen(weatherBuffer)) {
                            Log_printf(LOG_LEVEL_DEBUG, "Scrolling finished, transitioning to WD_PAUSING state");
                            weatherState = WD_PAUSING;
                            lastWeatherUpdate = millis();
                        } else {
                            // Manually construct the viewport without a large temporary buffer
                            int text_len = strlen(weatherBuffer);
                            for (int i = 0; i < 13; i++) {
                                int source_idx = weatherScrollPosition + i;
                                if (source_idx < text_len) {
                                    viewport[i] = weatherBuffer[source_idx];
                                } else {
                                    viewport[i] = ' '; // Pad with spaces
                                }
                            }
                            viewport[13] = '\0';

                            strncpy(segment_month, viewport, 3); segment_month[3] = '\0';
                            strncpy(segment_day, viewport + 3, 2); segment_day[2] = '\0';
                            strncpy(segment_year, viewport + 5, 4); segment_year[4] = '\0';
                            strncpy(segment_time, viewport + 9, 4); segment_time[4] = '\0';

                            printToDisplay(lastRow.month, segment_month, 1);
                            printToDisplay(lastRow.day, segment_day, 2);
                            printToDisplay(lastRow.year, segment_year, 0);
                            printToDisplay(lastRow.time, segment_time, 0);
                            shouldWriteToDisplay = true;

                            weatherScrollPosition++;
                        }
                    }
                    break;
                }

                case WD_PAUSING: {
                    if (millis() - lastWeatherUpdate > pauseDuration) {
                        weatherPage = (weatherPage + 1) % 7;
                        Log_printf(LOG_LEVEL_DEBUG, "Pause finished, transitioning to WD_START_PAGE for page %d", weatherPage);
                        weatherState = WD_START_PAGE;
                    }
                    break;
                }
            }
        }
        xSemaphoreGive(xDisplayDataMutex);

        if (shouldWriteToDisplay) {
            if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
                lastRow.month.writeDisplay();
                lastRow.day.writeDisplay();
                lastRow.year.writeDisplay();
                lastRow.time.writeDisplay();
                vTaskDelay(pdMS_TO_TICKS(2));
                xSemaphoreGive(xDisplayHardwareMutex);
            }
        }
    }
#endif
}

/**
 * @brief Manages the display logic for the Data Link marquee mode.
 * @details This function implements a state machine that scrolls through user-configured
 * data points on the bottom display row. It handles states for starting a new page,
 * scrolling the text, and pausing before moving to the next page. It keeps the top
 * two rows as a standard clock display.
 */
void updateMarqueeDisplay() {
#if ENABLE_HARDWARE
    // In marquee mode, the top two rows (Destination and Present Time) should always show the clock.
    // This call ensures that only those rows are updated, leaving the bottom row untouched
    // for the scrolling marquee information.
    updateNormalClockDisplay_internal(true, true, false);

    DisplayRow* targetRow = &lastRow;
    static char marqueePageBuffer[256];

    if (isMarqueeBufferDirty) {
        Log_printf(LOG_LEVEL_DEBUG, "Marquee buffer is dirty, forcing state to M_START_PAGE");
        marqueeState = M_START_PAGE;
        isMarqueeBufferDirty = false;
    }

    // Turn off AM/PM LEDs for the last row, as they are not used in this mode.
    digitalWrite(LAST_AM_PIN, LOW);
    digitalWrite(LAST_PM_PIN, LOW);

    if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
        DataPoint point;
        if (currentSettings.numDataPoints > 0) {
            point = currentSettings.dataPoints[currentPageIndex];
        }
        const unsigned long scrollSpeed = point.scrollSpeed > 0 ? point.scrollSpeed : 150;
        const unsigned long pauseDuration = 250; // 0.25-second pause between pages

        switch (marqueeState) {
            case M_START_PAGE: {
                // Clear display before showing new text
                if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
                    printToDisplay(targetRow->month, "   ", 0); printToDisplay(targetRow->day, "  ", 0);
                    printToDisplay(targetRow->year, "    ", 0); printToDisplay(targetRow->time, "    ", 0);
                    targetRow->month.writeDisplay(); targetRow->day.writeDisplay();
                    targetRow->year.writeDisplay(); targetRow->time.writeDisplay();
                    vTaskDelay(pdMS_TO_TICKS(2));
                    xSemaphoreGive(xDisplayHardwareMutex);
                }

                std::string fullText;
                if (currentSettings.numDataPoints == 0) {
                    fullText = "NO DATA POINTS";
                } else {
                    // --- Build the content string (simplified) ---
                    // The MQTT callback now correctly populates `scrollingText` for all MQTT-based
                    // sources, so we no longer need the complex switch-case. We can just use
                    // `scrollingText` as the authoritative source for the marquee content.
                    std::string content_text = point.scrollingText;

                    // --- Assemble the final string with prefix and suffix ---
                    if (!point.prefixText.empty()) {
                        fullText += point.prefixText;
                    }
                    if (!content_text.empty()) {
                        if (!fullText.empty()) fullText += " ";
                        fullText += content_text;
                    }
                    if (!point.suffixText.empty()) {
                        if (!fullText.empty()) fullText += " ";
                        fullText += point.suffixText;
                    }
                }

                if (fullText.empty() && currentSettings.numDataPoints > 0) {
                    // This page has no content, so skip it immediately.
                    currentPageIndex = (currentPageIndex + 1) % currentSettings.numDataPoints;
                    marqueeState = M_START_PAGE; // Go back to the start state for the *next* page
                    xSemaphoreGive(xDisplayDataMutex); // Release the mutex before we return
                    return; // Exit the function for this cycle
                }

                // Convert the entire marquee text to uppercase for readability
                std::transform(fullText.begin(), fullText.end(), fullText.begin(),
                               [](unsigned char c){ return std::toupper(c); });

                // Build the full string for the current page, with padding for scrolling effect
                snprintf(marqueePageBuffer, sizeof(marqueePageBuffer), "             %s ", fullText.c_str());
                marqueeScrollPosition = 0;

                marqueeState = M_SCROLLING;
                lastMarqueeStateChange = millis();
                break;
            }
            case M_SCROLLING: {
                // Yield the CPU for a moment to prevent the polling loop from starving other tasks,
                // especially the audio streaming task. This is the fix for the stuttering issue.
                vTaskDelay(pdMS_TO_TICKS(1));
                if (millis() - lastMarqueeStateChange > scrollSpeed) {
                    lastMarqueeStateChange = millis();

                    if (marqueeScrollPosition > strlen(marqueePageBuffer)) {
                        marqueeState = M_PAUSED; // End of scroll, move to pausing
                        lastMarqueeStateChange = millis();
                    } else {
                        char viewport[14];
                        int text_len = strlen(marqueePageBuffer);
                        for (int i = 0; i < 13; i++) {
                            int source_idx = marqueeScrollPosition + i;
                            if (source_idx < text_len) {
                                viewport[i] = marqueePageBuffer[source_idx];
                            } else {
                                viewport[i] = ' '; // Pad with spaces
                            }
                        }
                        viewport[13] = '\0';

                        char segment_month[4], segment_day[3], segment_year[5], segment_time[5];
                        strncpy(segment_month, viewport, 3); segment_month[3] = '\0';
                        strncpy(segment_day, viewport + 3, 2); segment_day[2] = '\0';
                        strncpy(segment_year, viewport + 5, 4); segment_year[4] = '\0';
                        strncpy(segment_time, viewport + 9, 4); segment_time[4] = '\0';

                        if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
                            // Apply correct justification to match physical display constraints
                            printToDisplay(targetRow->month, segment_month, 1); // Right justified
                            printToDisplay(targetRow->day, segment_day, 2);   // Center justified
                            printToDisplay(targetRow->year, segment_year, 0);  // Left justified
                            printToDisplay(targetRow->time, segment_time, 0);  // Left justified

                            targetRow->month.writeDisplay();
                            targetRow->day.writeDisplay();
                            targetRow->year.writeDisplay();
                            targetRow->time.writeDisplay();
                            vTaskDelay(pdMS_TO_TICKS(2));
                            xSemaphoreGive(xDisplayHardwareMutex);
                        }
                        marqueeScrollPosition++;
                    }
                }
                break;
            }
            case M_PAUSED: {
                if (millis() - lastMarqueeStateChange > pauseDuration) {
                    if (currentSettings.numDataPoints > 0) {
                        currentPageIndex = (currentPageIndex + 1) % currentSettings.numDataPoints;
                    }
                    marqueeState = M_START_PAGE;
                }
                break;
            }
        }
        xSemaphoreGive(xDisplayDataMutex);
    }
#endif
}