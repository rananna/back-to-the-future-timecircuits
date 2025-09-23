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

extern StockManager stockManager;

// Define and initialize the dirty flags and buffers for scrolling text
bool isMarqueeBufferDirty = true;
bool isWeatherBufferDirty = true;

std::string marqueeBuffer;
char weatherBuffer[512]; // Increased size for safety, changed to char array
std::string marqueeOverrideBuffer;
#include "HardwareControl.h"
#include <cmath> // For std::isnan and std::isinf

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
bool isRowInManualMode[3] = {false, false, false};

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
        vTaskDelay(pdMS_TO_TICKS(2));
        xSemaphoreGive(xDisplayHardwareMutex);
    }
    delay(duration);
#endif
}

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

        // State machine for stock ticker display
        switch (stockState) {
            case SD_CONNECTING: {
                // Determine the status and display the appropriate message.
                // The state will only transition away from SD_CONNECTING when data is ready.
                const char* msg_year = "          ";
                const char* msg_time = "          ";

                if (stockManager.getAssets().empty()) {
                    msg_year = "ADD STOCKS";
                    msg_time = "IN UI";
                } else if (!stockManager.isTimeSynchronized()) {
                    msg_year = "CONNECTING";
                    msg_time = "....";
                } else if (stockManager.isFetching()) {
                    msg_year = "LOADING";
                    msg_time = "STOCKS";
                } else if (stockManager.hasDataBeenUpdated() || stockManager.hasAnyValidData()) {
                    // Data is ready. Clear the flag and transition to the display state.
                    if (stockManager.hasDataBeenUpdated()) {
                        stockManager.clearDataUpdatedFlag();
                    }
                    stockState = SD_START_PAGE;
                } else {
                    // Not fetching, but no data yet. This can happen right after startup
                    // before the first fetch is triggered by the interval timer.
                    msg_year = "WAIT";
                    msg_time = "....";
                }

                // If we are still in the connecting state, display the message.
                if (stockState == SD_CONNECTING) {
                    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
                        printToDisplay(lastRow.month, "   ", 0);
                        printToDisplay(lastRow.day, "  ", 0);
                        printToDisplay(lastRow.year, msg_year, 0);
                        printToDisplay(lastRow.time, msg_time, 0);
                        lastRow.month.writeDisplay(); lastRow.day.writeDisplay(); lastRow.year.writeDisplay(); lastRow.time.writeDisplay();
                        vTaskDelay(pdMS_TO_TICKS(2));
                        xSemaphoreGive(xDisplayHardwareMutex);
                    }
                }
                break;
            }
            case SD_START_PAGE: {
                // Clear display before showing new text
                if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
                    printToDisplay(lastRow.month, "   ", 0); printToDisplay(lastRow.day, "  ", 0);
                    printToDisplay(lastRow.year, "    ", 0); printToDisplay(lastRow.time, "    ", 0);
                    lastRow.month.writeDisplay(); lastRow.day.writeDisplay(); lastRow.year.writeDisplay(); lastRow.time.writeDisplay();
                    vTaskDelay(pdMS_TO_TICKS(2));
                    xSemaphoreGive(xDisplayHardwareMutex);
                }

                String marqueeLine = stockManager.getMarqueeLine();
                marqueeLine.toUpperCase();

                // All messages (data, errors, status) will now scroll.
                // We prepare the buffer with padding on the *left* only, to allow it to scroll into view.
                snprintf(stockMarqueeBuffer, sizeof(stockMarqueeBuffer), "             %s", marqueeLine.c_str());
                stockScrollPosition = 0;
                stockState = SD_SCROLLING;
                lastStockUpdate = millis();
                break;
            }

            case SD_SCROLLING: {
                if (millis() - lastStockUpdate > scrollSpeed) {
                    lastStockUpdate = millis();

                    // The scroll is finished once the scroll position has exceeded the length of the entire message.
                    // Since there is no right-side padding, this happens as soon as the last character disappears.
                    if (stockScrollPosition > strlen(stockMarqueeBuffer)) {
                        stockState = SD_PAUSING;
                        lastStockUpdate = millis();
                    } else {
                        char viewport[14];
                        int text_len = strlen(stockMarqueeBuffer);
                        for (int i = 0; i < 13; i++) {
                            int source_idx = stockScrollPosition + i;
                            // We now pad with spaces on the right *dynamically* if the source index is out of bounds.
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
                            // Apply correct justification to match physical display constraints
                            printToDisplay(lastRow.month, segment_month, 1); // Right justified
                            printToDisplay(lastRow.day, segment_day, 2);   // Center justified
                            printToDisplay(lastRow.year, segment_year, 0);  // Left justified
                            printToDisplay(lastRow.time, segment_time, 0);  // Left justified
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
                    stockManager.nextPage();
                    stockState = SD_START_PAGE;
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

void displayOverrideMessage() {
    if (!hardwareInitialized) return;
#if ENABLE_HARDWARE
    if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
        if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
            printToDisplay(destRow.month, overrideMessageLine1.substring(0, 3).c_str(), 1);
            printToDisplay(destRow.day, overrideMessageLine1.substring(3, 5).c_str(), 2);
            printToDisplay(destRow.year, overrideMessageLine1.substring(5, 9).c_str());
            printToDisplay(destRow.time, overrideMessageLine1.substring(9, 13).c_str());
            destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();
            vTaskDelay(pdMS_TO_TICKS(2));

            printToDisplay(presRow.month, overrideMessageLine2.substring(0, 3).c_str(), 1);
            printToDisplay(presRow.day, overrideMessageLine2.substring(3, 5).c_str(), 2);
            printToDisplay(presRow.year, overrideMessageLine2.substring(5, 9).c_str());
            printToDisplay(presRow.time, overrideMessageLine2.substring(9, 13).c_str());
            presRow.month.writeDisplay(); presRow.day.writeDisplay(); presRow.year.writeDisplay(); presRow.time.writeDisplay();
            vTaskDelay(pdMS_TO_TICKS(2));

            printToDisplay(lastRow.month, overrideMessageLine3.substring(0, 3).c_str(), 1);
            printToDisplay(lastRow.day, overrideMessageLine3.substring(3, 5).c_str(), 2);
            printToDisplay(lastRow.year, overrideMessageLine3.substring(5, 9).c_str());
            printToDisplay(lastRow.time, overrideMessageLine3.substring(9, 13).c_str());
            lastRow.month.writeDisplay(); lastRow.day.writeDisplay(); lastRow.year.writeDisplay(); lastRow.time.writeDisplay();
            vTaskDelay(pdMS_TO_TICKS(2));
            xSemaphoreGive(xDisplayHardwareMutex);
        }
        xSemaphoreGive(xDisplayDataMutex);
    }
#endif
}

void updateDisplaySegment(int row, int segment, const std::string& text) {
    // ... function content remains the same ...
    if (row < 0 || row > 2 || segment < 0 || segment > 3) return;
    
    manualDisplayText[row][segment] = text;

    bool manualActive = false;
    for(int i=0; i<4; ++i) {
        if(!manualDisplayText[row][i].empty()) {
            manualActive = true;
            break;
        }
    }
    isRowInManualMode[row] = manualActive;

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
void updateNormalClockDisplay_internal(bool updateDest, bool updatePres, bool updateLast) {
  if (isDisplayAsleep || isAnimating || !hardwareInitialized) {
    if(isAnimating) {
    }
    return;
  }

#if ENABLE_HARDWARE
    if (timeSynchronized) {
      time_t now_t;
      struct tm dest_timeinfo, present_timeinfo;

      if (xSemaphoreTake(xTimeLibMutex, portMAX_DELAY) == pdTRUE) {
        time(&now_t);
        // --- Destination Time ---
        setenv("TZ", TZ_DATA[currentSettings.destinationTimezoneIndex].tzString, 1);
        tzset();
        localtime_r(&now_t, &dest_timeinfo);
        // --- Present Time ---
        setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
        tzset();
        localtime_r(&now_t, &present_timeinfo);
        xSemaphoreGive(xTimeLibMutex);
      }

      if (updateDest) {
          dest_timeinfo.tm_year = currentSettings.destinationYear - 1900;
          if (!isRowInManualMode[0]) {
              updateDisplayRow(destRow, dest_timeinfo, currentSettings.destinationYear, true);
          } else {
              if (!manualDisplayText[0][0].empty()) printToDisplay(destRow.month, manualDisplayText[0][0].c_str(), 1);
              if (!manualDisplayText[0][1].empty()) printToDisplay(destRow.day, manualDisplayText[0][1].c_str(), 2);
              if (!manualDisplayText[0][2].empty()) printToDisplay(destRow.year, manualDisplayText[0][2].c_str());
              if (!manualDisplayText[0][3].empty()) printToDisplay(destRow.time, manualDisplayText[0][3].c_str());
          }
          if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
            destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();
            vTaskDelay(pdMS_TO_TICKS(2));
            xSemaphoreGive(xDisplayHardwareMutex);
          }
      }

      if (updatePres) {
          bool showDecimalForPresent = (millis() / 1000) % 2 == 0;
          if(!isRowInManualMode[1]) {
              updateDisplayRow(presRow, present_timeinfo, present_timeinfo.tm_year + 1900, showDecimalForPresent);
          } else {
              if (!manualDisplayText[1][0].empty()) printToDisplay(presRow.month, manualDisplayText[1][0].c_str(), 1);
              if (!manualDisplayText[1][1].empty()) printToDisplay(presRow.day, manualDisplayText[1][1].c_str(), 2);
              if (!manualDisplayText[1][2].empty()) printToDisplay(presRow.year, manualDisplayText[1][2].c_str());
              if (!manualDisplayText[1][3].empty()) printToDisplay(presRow.time, manualDisplayText[1][3].c_str());
          }
          if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
            presRow.month.writeDisplay(); presRow.day.writeDisplay(); presRow.year.writeDisplay(); presRow.time.writeDisplay();
            vTaskDelay(pdMS_TO_TICKS(2));
            xSemaphoreGive(xDisplayHardwareMutex);
          }
      }

      // --- Last Time Departed ---
      if (updateLast) {
          struct tm lastTimeDepartedInfo = {0};
          lastTimeDepartedInfo.tm_year = currentSettings.lastTimeDepartedYear - 1900;
          lastTimeDepartedInfo.tm_mon = currentSettings.lastTimeDepartedMonth - 1;
          lastTimeDepartedInfo.tm_mday = currentSettings.lastTimeDepartedDay;
          lastTimeDepartedInfo.tm_hour = currentSettings.lastTimeDepartedHour;
          lastTimeDepartedInfo.tm_min = currentSettings.lastTimeDepartedMinute;

          if(!isRowInManualMode[2]) {
              updateDisplayRow(lastRow, lastTimeDepartedInfo, currentSettings.lastTimeDepartedYear, true);
          } else {
              if (!manualDisplayText[2][0].empty()) printToDisplay(lastRow.month, manualDisplayText[2][0].c_str(), 1);
              if (!manualDisplayText[2][1].empty()) printToDisplay(lastRow.day, manualDisplayText[2][1].c_str(), 2);
              if (!manualDisplayText[2][2].empty()) printToDisplay(lastRow.year, manualDisplayText[2][2].c_str());
              if (!manualDisplayText[2][3].empty()) printToDisplay(lastRow.time, manualDisplayText[2][3].c_str());
          }
          if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
            lastRow.month.writeDisplay(); lastRow.day.writeDisplay(); lastRow.year.writeDisplay(); lastRow.time.writeDisplay();
            vTaskDelay(pdMS_TO_TICKS(2));
            xSemaphoreGive(xDisplayHardwareMutex);
          }
      }
    }
#endif
}

void updateNormalClockDisplay(bool updateDest, bool updatePres, bool updateLast) {
#if ENABLE_HARDWARE
  if (xSemaphoreTake(xDisplayDataMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    updateNormalClockDisplay_internal(updateDest, updatePres, updateLast);
    xSemaphoreGive(xDisplayDataMutex);
  }
#endif
}

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
            updateNormalClockDisplay_internal(false, false, true);
            xSemaphoreGive(xDisplayDataMutex);
            return;
        }

        const unsigned long scrollSpeed = 250;
        const unsigned long pauseDuration = 1000;
        const unsigned long errorRetryDelay = 10000; // 10 seconds

        if (!currentWeatherData.dataValid) {
            if (!currentWeatherData.errorReason.empty() && weatherState != WD_ERROR) {
                weatherState = WD_ERROR;
                snprintf(weatherBuffer, sizeof(weatherBuffer), "             WEATHER ERROR: %s", currentWeatherData.errorReason.c_str());
                weatherScrollPosition = 0;
                lastWeatherUpdate = millis();
            } else if (initialFetchTriggered && initialFetchStartTime > 0 && millis() - initialFetchStartTime > 30000) {
                Log_printf(LOG_LEVEL_WARN, "Weather fetch task timed out. Deleting task.");
                if (weatherTaskHandle != NULL) {
                    vTaskDelete(weatherTaskHandle);
                    weatherTaskHandle = NULL;
                }
                isFetchingWeather = false;
                handleWeatherTimeout();
                xSemaphoreGive(xDisplayDataMutex);
                return;
            } else {
                printToDisplay(lastRow.month, "WEA", 1);
                printToDisplay(lastRow.day, "TH", 2);
                printToDisplay(lastRow.year, "ER");
                printToDisplay(lastRow.time, "----");
                shouldWriteToDisplay = true;
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
                                snprintf(weatherBuffer + strlen(weatherBuffer), sizeof(weatherBuffer) - strlen(weatherBuffer), "SUNRISE %s, SUNSET ", timeStr);

                                // Format sunset
                                localtime_r(&sunset, &timeinfo);
                                strftime(timeStr, sizeof(timeStr), currentSettings.displayFormat24h ? "%H%M" : "%l%M%p", &timeinfo);
                                snprintf(weatherBuffer + strlen(weatherBuffer), sizeof(weatherBuffer) - strlen(weatherBuffer), "%s", timeStr);

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

void updateMarqueeDisplay() {
#if ENABLE_HARDWARE
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

    if (currentSettings.numDataPoints == 0) {
        if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
            printToDisplay(targetRow->month, "NO");
            printToDisplay(targetRow->day, "DATA", 2);
            printToDisplay(targetRow->year, "POINTS");
            printToDisplay(targetRow->time, "----");
            targetRow->month.writeDisplay();
            targetRow->day.writeDisplay();
            targetRow->year.writeDisplay();
            targetRow->time.writeDisplay();
            vTaskDelay(pdMS_TO_TICKS(2));
            xSemaphoreGive(xDisplayHardwareMutex);
        }
        return;
    }

    if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
        DataPoint point = currentSettings.dataPoints[currentPageIndex];
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

                // Get the configuration and data for the current page
                DataPoint point = currentSettings.dataPoints[currentPageIndex];

                // --- Build the content string based on the data source type ---
                std::string content_text;
                switch (point.dataSourceType) {
                    case DATA_SOURCE_STATIC:
                        content_text = point.scrollingText;
                        break;
                    case DATA_SOURCE_MQTT:
                        // For MQTT, the raw data is in the 'year' field.
                        content_text = displayPages[currentPageIndex].year;
                        break;
                    case DATA_SOURCE_HA:
                        // For Home Assistant, assemble the text from the four separate fields.
                        content_text = displayPages[currentPageIndex].month;
                        if (!displayPages[currentPageIndex].day.empty()) {
                            if (!content_text.empty()) content_text += " ";
                            content_text += displayPages[currentPageIndex].day;
                        }
                        if (!displayPages[currentPageIndex].year.empty()) {
                            if (!content_text.empty()) content_text += " ";
                            content_text += displayPages[currentPageIndex].year;
                        }
                        if (!displayPages[currentPageIndex].time.empty()) {
                            if (!content_text.empty()) content_text += " ";
                            content_text += displayPages[currentPageIndex].time;
                        }
                        break;
                }

                // --- Assemble the final string with prefix and suffix ---
                // This logic is now centralized and works correctly for all data source types.
                std::string fullText;
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

                // Build the full string for the current page, with padding for scrolling effect
                snprintf(marqueePageBuffer, sizeof(marqueePageBuffer), "             %s ", fullText.c_str());
                marqueeScrollPosition = 0;

                marqueeState = M_SCROLLING;
                lastMarqueeStateChange = millis();
                break;
            }
            case M_SCROLLING: {
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
                    currentPageIndex = (currentPageIndex + 1) % currentSettings.numDataPoints;
                    marqueeState = M_START_PAGE;
                }
                break;
            }
        }
        xSemaphoreGive(xDisplayDataMutex);
    }
#endif
}