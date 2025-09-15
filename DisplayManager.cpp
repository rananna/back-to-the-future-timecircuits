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
#include "EventManager.h"
#include "HardwareControl.h"

// Forward declaration for the timeout handler in the main .ino file
void handleWeatherTimeout();

// File-scoped state variables for the weather fetch process
static bool initialFetchTriggered = false;
static unsigned long initialFetchStartTime = 0;
static bool initialFetchTimedOut = false;
static unsigned long lastWeatherFetchTime = 0;
const unsigned long WEATHER_REFRESH_INTERVAL = 300000; // 5 minutes
const int WEATHER_TASK_STACK_SIZE = 8192;

// File-scoped state variables for the weather display state machine
static WeatherDisplayState weatherState = WD_START_PAGE;
static int weatherPage = 0;
static char weatherScrollText[256]; // MODIFIED: Use a static char array
static int weatherScrollPosition = 0;
static unsigned long lastWeatherUpdate = 0;

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

void displayMarqueeOverride() {
    if (!hardwareInitialized) return;
#if ENABLE_HARDWARE
    if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
        String textToDisplay = marqueeOverrideMessage;

        if (textToDisplay.length() > 13) {
            textToDisplay = "  " + textToDisplay + "  ";
        }

        static unsigned long lastScrollTime = 0;
        static int scrollPosition = 0;

        if (millis() - lastScrollTime > currentSettings.dataPoints[currentPageIndex].scrollSpeed) {
            lastScrollTime = millis();

            String viewport = textToDisplay.substring(scrollPosition, scrollPosition + 13);
            if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
                printToDisplay(lastRow.month, viewport.substring(0, 3).c_str(), 0);
                printToDisplay(lastRow.day, viewport.substring(3, 5).c_str(), 0);
                printToDisplay(lastRow.year, viewport.substring(5, 9).c_str(), 0);
                printToDisplay(lastRow.time, viewport.substring(9, 13).c_str(), 0);

                lastRow.month.writeDisplay();
                lastRow.day.writeDisplay();
                lastRow.year.writeDisplay();
                lastRow.time.writeDisplay();
                vTaskDelay(pdMS_TO_TICKS(2));
                xSemaphoreGive(xDisplayHardwareMutex);
            }

            if (textToDisplay.length() > 13) {
                scrollPosition++;
                if (scrollPosition > textToDisplay.length() - 13) {
                    scrollPosition = 0;
                }
            } else {
                scrollPosition = 0;
            }
        }
        xSemaphoreGive(xDisplayDataMutex);
    }
#endif
}

void updateStockTickerDisplay() {
    // ... function content remains the same ...
    if (isDisplayAsleep || isAnimating || !hardwareInitialized) return;
#if ENABLE_HARDWARE
    DisplayRow* rows[] = {&destRow, &presRow, &lastRow};
    for (int i = 0; i < 3; ++i) {
        if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
            if (stockData[i].dataValid) {
                String symbol = String(stockData[i].symbol.c_str());
                if(symbol.startsWith("^")) symbol.remove(0,1);
                printToDisplay(rows[i]->month, symbol.substring(0, 3).c_str());
                printToDisplay(rows[i]->day, symbol.substring(3, 5).c_str(), 2);

                printToDisplay(rows[i]->year, stockData[i].price.c_str());

                printToDisplay(rows[i]->time, stockData[i].change_percent.c_str());

            } else {
                std::string symbol;
                if (i == 0) symbol = currentSettings.stockRow1_symbol;
                else if (i == 1) symbol = currentSettings.stockRow2_symbol;
                else symbol = currentSettings.stockRow3_symbol;

                if (symbol.empty()) {
                    printToDisplay(rows[i]->month, "---");
                    printToDisplay(rows[i]->day, "--", 2);
                    printToDisplay(rows[i]->year, "EMPTY");
                    printToDisplay(rows[i]->time, "----");
                } else if (currentSettings.financialModelingPrepApiKey.empty()) {
                    printToDisplay(rows[i]->month, "NO");
                    printToDisplay(rows[i]->day, "API", 2);
                    printToDisplay(rows[i]->year, "KEY");
                    printToDisplay(rows[i]->time, "----");
                } else {
                    printToDisplay(rows[i]->month, "---");
                    printToDisplay(rows[i]->day, "--", 2);
                    printToDisplay(rows[i]->year, "LOAD");
                    printToDisplay(rows[i]->time, "ING");
                }
            }
             xSemaphoreGive(xDisplayDataMutex);
        }
        if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
            rows[i]->month.writeDisplay();
            rows[i]->day.writeDisplay();
            rows[i]->year.writeDisplay();
            rows[i]->time.writeDisplay();
            vTaskDelay(pdMS_TO_TICKS(2));
            xSemaphoreGive(xDisplayHardwareMutex);
        }
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
void updateNormalClockDisplay(bool updateDest, bool updatePres, bool updateLast) {
  if (isDisplayAsleep || isAnimating || !hardwareInitialized) {
    if(isAnimating) {
    }
    return;
  }

#if ENABLE_HARDWARE
  if (xSemaphoreTake(xDisplayDataMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
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

    String viewport;
    bool shouldWriteToDisplay = false;

    if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
        if (initialFetchTimedOut) {
            xSemaphoreGive(xDisplayDataMutex);
            updateNormalClockDisplay(true, true, true);
            return;
        }

        const unsigned long scrollSpeed = 250;
        const unsigned long pauseDuration = 1000;
        const unsigned long errorRetryDelay = 10000; // 10 seconds

        if (!currentWeatherData.dataValid) {
            // If there's a specific error reason, switch to the error state.
            if (!currentWeatherData.errorReason.empty() && weatherState != WD_ERROR) {
                weatherState = WD_ERROR;
                snprintf(weatherScrollText, sizeof(weatherScrollText), "             WEATHER ERROR: %s", currentWeatherData.errorReason.c_str());
                weatherScrollPosition = 0;
                lastWeatherUpdate = millis(); // Start the timer for the error display
            }
            // If the fetch has been triggered and 30 seconds have passed, call the main timeout handler
            else if (initialFetchTriggered && initialFetchStartTime > 0 && millis() - initialFetchStartTime > 30000) {
                Log_printf(LOG_LEVEL_WARN, "Weather fetch task timed out.");
                isFetchingWeather = false; // Reset the flag
                handleWeatherTimeout();
                xSemaphoreGive(xDisplayDataMutex);
                return;
            }
            // Otherwise, show the initial "Loading..." state
            else {
                printToDisplay(lastRow.month, "WEA", 1);
                printToDisplay(lastRow.day, "TH", 2);
                printToDisplay(lastRow.year, "ER");
                printToDisplay(lastRow.time, "----");
                shouldWriteToDisplay = true;
            }

            if (!initialFetchTriggered && !isFetchingWeather) {
                Log_printf(LOG_LEVEL_INFO, "Weather data is invalid, triggering initial fetch.");
                triggerWeatherFetch();
                initialFetchTriggered = true;
                initialFetchStartTime = millis();
                lastWeatherFetchTime = 0; // Reset this so it gets set upon successful fetch
            }
        } else {
            // If we have valid data, make sure our state flags are reset for the next time we need them.
            initialFetchTriggered = false;
            initialFetchStartTime = 0;
            initialFetchTimedOut = false;

            // Start the periodic refresh timer after the first successful fetch
            if (lastWeatherFetchTime == 0) {
                lastWeatherFetchTime = millis();
            }

            // Check if it's time to refresh the weather data
            if ((millis() - lastWeatherFetchTime > WEATHER_REFRESH_INTERVAL) && !isFetchingWeather) {
                Log_printf(LOG_LEVEL_INFO, "Periodic weather refresh triggered (5-minute interval).");
                lastWeatherFetchTime = millis(); // Reset the timer immediately
                // Reuse the timeout mechanism for this fetch as well
                initialFetchTriggered = true;
                initialFetchStartTime = millis();
                triggerWeatherFetch();
            }

            if (weatherDataUpdated) {
                weatherState = WD_START_PAGE;
                weatherPage = 0;
                weatherDataUpdated = false;
                initialFetchTriggered = false;
                initialFetchStartTime = 0;
                initialFetchTimedOut = false;
                // The persistent task is now managed in DataManager, no need to touch the handle here.
            }

            switch (weatherState) {
                case WD_ERROR: {
                    if (millis() - lastWeatherUpdate > scrollSpeed) {
                        lastWeatherUpdate = millis();

                        char tempScrollText[256 + 14];
                        snprintf(tempScrollText, sizeof(tempScrollText), "%s             ", weatherScrollText);

                        char monthStr[4], dayStr[3], yearStr[5], timeStr[5];
                        strncpy(monthStr, tempScrollText + weatherScrollPosition, 3);
                        monthStr[3] = '\0';
                        strncpy(dayStr, tempScrollText + weatherScrollPosition + 3, 2);
                        dayStr[2] = '\0';
                        strncpy(yearStr, tempScrollText + weatherScrollPosition + 5, 4);
                        yearStr[4] = '\0';
                        strncpy(timeStr, tempScrollText + weatherScrollPosition + 9, 4);
                        timeStr[4] = '\0';

                        printToDisplay(lastRow.month, monthStr, 1);
                        printToDisplay(lastRow.day, dayStr, 2);
                        printToDisplay(lastRow.year, yearStr, 0);
                        printToDisplay(lastRow.time, timeStr, 0);
                        shouldWriteToDisplay = true;

                        weatherScrollPosition++;
                        // When the full message has scrolled, pause, then reset to try again.
                        if (weatherScrollPosition > strlen(weatherScrollText)) {
                            if (millis() - lastWeatherUpdate > errorRetryDelay) {
                                currentWeatherData.errorReason = ""; // Clear reason
                                weatherState = WD_START_PAGE;
                                initialFetchTriggered = false; // Allow a new fetch
                            }
                        }
                    }
                    break;
                }
                case WD_START_PAGE: {
                    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
                        // Clear the row before displaying new data
                        printToDisplay(lastRow.month, "   ", 0);
                        printToDisplay(lastRow.day, "  ", 0);
                        printToDisplay(lastRow.year, "    ", 0);
                        printToDisplay(lastRow.time, "    ", 0);
                        lastRow.month.writeDisplay();
                        lastRow.day.writeDisplay();
                        lastRow.year.writeDisplay();
                        lastRow.time.writeDisplay();
                        vTaskDelay(pdMS_TO_TICKS(2));
                        xSemaphoreGive(xDisplayHardwareMutex);
                    }
                    char buffer[20];
                    switch (weatherPage) {
                        case 0: { // Current Weather
                            dtostrf(currentWeatherData.temperature, 4, 1, buffer);
                            const char* desc = getWeatherDescriptionForCode(currentWeatherData.weatherCode);
                            const char* unit = currentSettings.useMetricUnits ? "C" : "F";
                            snprintf(weatherScrollText, sizeof(weatherScrollText), "CURRENTLY %s%s, %s", buffer, unit, desc);
                            break;
                        }
                        case 1: { // Tomorrow's Forecast
                            char high_buf[8], low_buf[8];
                            dtostrf(currentWeatherData.tomorrowHigh, 1, 0, high_buf);
                            dtostrf(currentWeatherData.tomorrowLow, 1, 0, low_buf);
                            const char* desc = getWeatherDescriptionForCode(currentWeatherData.tomorrowWeatherCode);
                            const char* unit = currentSettings.useMetricUnits ? "C" : "F";
                            snprintf(weatherScrollText, sizeof(weatherScrollText), "TOMORROW HIGH %s%s, LOW %s%s, %s", high_buf, unit, low_buf, unit, desc);
                            break;
                        }
                        case 2: { // Wind & Rain
                            const char* windUnit = currentSettings.useMetricUnits ? "KPH" : "MPH";
                            snprintf(weatherScrollText, sizeof(weatherScrollText), "WIND %d %s, MAX %d %s, PRECIP %d%%",
                                     (int)currentWeatherData.windSpeed, windUnit,
                                     (int)currentWeatherData.maxWindSpeed, windUnit,
                                     currentWeatherData.precipitationProbability);
                            break;
                        }
                        case 3: { // Sunrise & Sunset
                            struct tm timeinfo;
                            char sunriseStr[8], sunsetStr[8];

                            // Manually format sunrise time to avoid locale issues with %p
                            localtime_r(&currentWeatherData.sunrise, &timeinfo);
                            int sunriseHour = timeinfo.tm_hour;
                            const char* sunriseAmpm = (sunriseHour >= 12) ? "PM" : "AM";
                            if (sunriseHour > 12) sunriseHour -= 12;
                            if (sunriseHour == 0) sunriseHour = 12;
                            snprintf(sunriseStr, sizeof(sunriseStr), "%d%02d%s", sunriseHour, timeinfo.tm_min, sunriseAmpm);

                            // Manually format sunset time
                            localtime_r(&currentWeatherData.sunset, &timeinfo);
                            int sunsetHour = timeinfo.tm_hour;
                            const char* sunsetAmpm = (sunsetHour >= 12) ? "PM" : "AM";
                            if (sunsetHour > 12) sunsetHour -= 12;
                            if (sunsetHour == 0) sunsetHour = 12;
                            snprintf(sunsetStr, sizeof(sunsetStr), "%d%02d%s", sunsetHour, timeinfo.tm_min, sunsetAmpm);

                            snprintf(weatherScrollText, sizeof(weatherScrollText), "SUNRISE %s, SUNSET %s", sunriseStr, sunsetStr);
                            break;
                        }
                        case 4: { // Hourly Forecast
                            const char* unit = currentSettings.useMetricUnits ? "C" : "F";
                            char* p = weatherScrollText;
                            p += snprintf(p, sizeof(weatherScrollText), "NEXT 3 HRS ");
                            for (int i = 0; i < 3; ++i) {
                                if (currentWeatherData.hourlyCode[i] != -1) {
                                    char temp_buf[8];
                                    dtostrf(currentWeatherData.hourlyTemp[i], 1, 0, temp_buf);
                                    const char* desc = getWeatherDescriptionForCode(currentWeatherData.hourlyCode[i]);
                                    p += snprintf(p, sizeof(weatherScrollText) - (p - weatherScrollText), "%s%s %s", temp_buf, unit, desc);
                                    if (i < 2) {
                                        p += snprintf(p, sizeof(weatherScrollText) - (p - weatherScrollText), ", ");
                                    }
                                }
                            }
                            break;
                        }
                        case 5: { // Feels Like & Humidity
                            char feels_like_buf[8];
                            dtostrf(currentWeatherData.apparentTemperature, 1, 0, feels_like_buf);
                            const char* unit = currentSettings.useMetricUnits ? "C" : "F";
                            snprintf(weatherScrollText, sizeof(weatherScrollText), "FEELS LIKE %s%s, HUMIDITY %d%%", feels_like_buf, unit, currentWeatherData.humidity);
                            break;
                        }
                        case 6: { // Today's High/Low
                            char high_buf[8], low_buf[8];
                            dtostrf(currentWeatherData.dailyHigh, 1, 0, high_buf);
                            dtostrf(currentWeatherData.dailyLow, 1, 0, low_buf);
                            const char* unit = currentSettings.useMetricUnits ? "C" : "F";
                            snprintf(weatherScrollText, sizeof(weatherScrollText), "TODAY HIGH %s%s, LOW %s%s", high_buf, unit, low_buf, unit);
                            break;
                        }
                    }
                    // Prepend spaces for scrolling effect. We need to shift the existing string.
                    char temp[256];
                    snprintf(temp, sizeof(temp), "             %s", weatherScrollText);
                    strncpy(weatherScrollText, temp, sizeof(weatherScrollText) - 1);
                    weatherScrollText[sizeof(weatherScrollText) - 1] = '\0';

                    weatherScrollPosition = 0;
                    weatherState = WD_SCROLLING;
                    lastWeatherUpdate = millis();
                }

                case WD_SCROLLING: {
                    if (millis() - lastWeatherUpdate > scrollSpeed) {
                        lastWeatherUpdate = millis();

                        char tempScrollText[256 + 14];
                        snprintf(tempScrollText, sizeof(tempScrollText), "%s             ", weatherScrollText);

                        char monthStr[4], dayStr[3], yearStr[5], timeStr[5];
                        strncpy(monthStr, tempScrollText + weatherScrollPosition, 3);
                        monthStr[3] = '\0';
                        strncpy(dayStr, tempScrollText + weatherScrollPosition + 3, 2);
                        dayStr[2] = '\0';
                        strncpy(yearStr, tempScrollText + weatherScrollPosition + 5, 4);
                        yearStr[4] = '\0';
                        strncpy(timeStr, tempScrollText + weatherScrollPosition + 9, 4);
                        timeStr[4] = '\0';

                        printToDisplay(lastRow.month, monthStr, 1);
                        printToDisplay(lastRow.day, dayStr, 2);
                        printToDisplay(lastRow.year, yearStr, 0);
                        printToDisplay(lastRow.time, timeStr, 0);
                        shouldWriteToDisplay = true;

                        weatherScrollPosition++;
                        if (weatherScrollPosition > strlen(weatherScrollText)) {
                            weatherState = WD_PAUSING;
                            lastWeatherUpdate = millis();
                        }
                    }
                    break;
                }

                case WD_PAUSING: {
                    if (millis() - lastWeatherUpdate > pauseDuration) {
                        weatherPage = (weatherPage + 1) % 7; // MODIFIED: Increased page count
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
    char timeCanvas[256], yearCanvas[256];

    // ✅ FIX: Add this check at the beginning of the function.
    if (currentSettings.numDataPoints == 0) {
        if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
            // If there's nothing to display, just show a blank or default state.
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
        return; // Exit the function to prevent the crash.
    }

    if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
        if (marqueeState == M_IDLE) {
            // This line is now safe because we already checked numDataPoints.
            currentPageIndex = (currentPageIndex + 1) % currentSettings.numDataPoints;
            marqueeScrollPosition = 0;
            marqueeScrollPositionYear = 0;
            marqueeState = M_PAUSED;
            lastMarqueeStateChange = millis();
        }

        // ... the rest of the function remains the same ...
        DataPoint point = currentSettings.dataPoints[currentPageIndex];

        if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
            printToDisplay(targetRow->month, displayPages[currentPageIndex].month.c_str());
            if (!point.icon.empty()) {
                printToDisplay(targetRow->day, point.icon.c_str(), 2);
            } else {
                printToDisplay(targetRow->day, displayPages[currentPageIndex].day.c_str(), 2);
            }
            xSemaphoreGive(xDisplayHardwareMutex);
        }

        std::string yearContent = point.yearPrefix + displayPages[currentPageIndex].year + point.yearSuffix;
        std::string timeContent = point.prefix + displayPages[currentPageIndex].time + point.suffix;
        
        xSemaphoreGive(xDisplayDataMutex);

        if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
            snprintf(yearCanvas, sizeof(yearCanvas), "   %s   ", yearContent.c_str());
            if (strlen(yearCanvas) <= 4) {
                printToDisplay(targetRow->year, yearCanvas);
            } else {
                char yearViewport[5];
                strncpy(yearViewport, yearCanvas + marqueeScrollPositionYear, 4);
                yearViewport[4] = '\0';
                printToDisplay(targetRow->year, yearViewport);
            }

            snprintf(timeCanvas, sizeof(timeCanvas), "   %s   ", timeContent.c_str());
            if (strlen(timeCanvas) <= 4) {
                printToDisplay(targetRow->time, timeCanvas);
            } else {
                char viewport[5];
                strncpy(viewport, timeCanvas + marqueeScrollPosition, 4);
                viewport[4] = '\0';
                printToDisplay(targetRow->time, viewport);
            }
            xSemaphoreGive(xDisplayHardwareMutex);
        }

        if (marqueeState == M_PAUSED && millis() - lastMarqueeStateChange > 2000) {
            marqueeState = M_SCROLLING;
            lastMarqueeStateChange = millis();
        }

        if (marqueeState == M_SCROLLING && millis() - lastMarqueeStateChange > (unsigned long)point.scrollSpeed) {
            lastMarqueeStateChange = millis();
            bool timeDone = false;
            bool yearDone = false;

            if (strlen(timeCanvas) > 4) {
                marqueeScrollPosition++;
                if (marqueeScrollPosition > strlen(timeCanvas) - 4) {
                    timeDone = true;
                }
            } else {
                timeDone = true;
            }

            if (strlen(yearCanvas) > 4) {
                marqueeScrollPositionYear++;
                if (marqueeScrollPositionYear > strlen(yearCanvas) - 4) {
                    yearDone = true;
                }
            } else {
                yearDone = true;
            }

            if (timeDone && yearDone) {
                marqueeState = M_IDLE;
            }
        }
        if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
            targetRow->month.writeDisplay();
            targetRow->day.writeDisplay();
            targetRow->year.writeDisplay();
            targetRow->time.writeDisplay();
            vTaskDelay(pdMS_TO_TICKS(2));
            xSemaphoreGive(xDisplayHardwareMutex);
        }
    }
#endif
}