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

// File-scoped state variables for the initial weather fetch process
static bool initialFetchTriggered = false;
static unsigned long initialFetchStartTime = 0;
static bool initialFetchTimedOut = false;

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

enum WeatherDisplayState {
    WD_START_PAGE,
    WD_SCROLLING,
    WD_PAUSING
};

void handleWeatherDisplay() {
#if ENABLE_HARDWARE
    String viewport;
    bool shouldWriteToDisplay = false;

    if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
        if (initialFetchTimedOut) {
            xSemaphoreGive(xDisplayDataMutex);
            updateNormalClockDisplay(true, true, true);
            return;
        }

        if (!currentWeatherData.dataValid) {
            // If the fetch has been triggered and 30 seconds have passed, call the main timeout handler
            if (initialFetchTriggered && initialFetchStartTime > 0 && millis() - initialFetchStartTime > 30000) {
                handleWeatherTimeout();
                // We don't need to do anything else; the handler will change the state.
                // We just need to release the mutex and return.
                xSemaphoreGive(xDisplayDataMutex);
                return;
            }

            printToDisplay(lastRow.month, "WEA", 1);
            printToDisplay(lastRow.day, "TH", 2);
            printToDisplay(lastRow.year, "ER");
            printToDisplay(lastRow.time, "----");
            shouldWriteToDisplay = true;

            if (!initialFetchTriggered) {
                Log_printf(LOG_LEVEL_INFO, "Weather data is invalid, triggering initial fetch.");
                xTaskCreate(fetchWeatherDataTask, "fetchWeatherDataTask", 8192, NULL, 1, NULL);
                initialFetchTriggered = true;
                initialFetchStartTime = millis();
            }
        } else {
            // If we have valid data, make sure our state flags are reset for the next time we need them.
            initialFetchTriggered = false;
            initialFetchStartTime = 0;
            initialFetchTimedOut = false;
            static WeatherDisplayState weatherState = WD_START_PAGE;
            static int weatherPage = 0;
            static String weatherScrollText;
            static int weatherScrollPosition = 0;
            static unsigned long lastWeatherUpdate = 0;
            const unsigned long scrollSpeed = 250;
            const unsigned long pauseDuration = 1000;

            if (weatherDataUpdated) {
                weatherState = WD_START_PAGE;
                weatherPage = 0;
                weatherDataUpdated = false;
                initialFetchTriggered = false;
                initialFetchStartTime = 0;
                initialFetchTimedOut = false;
            }

            switch (weatherState) {
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
                            String unit = currentSettings.useMetricUnits ? "C" : "F";
                            weatherScrollText = "CURRENTLY: " + String(buffer) + unit + ", " + desc;
                            break;
                        }
                        case 1: { // Tomorrow's Forecast
                            char high_buf[8], low_buf[8];
                            dtostrf(currentWeatherData.tomorrowHigh, 1, 0, high_buf);
                            dtostrf(currentWeatherData.tomorrowLow, 1, 0, low_buf);
                            const char* desc = getWeatherDescriptionForCode(currentWeatherData.tomorrowWeatherCode);
                            String unit = currentSettings.useMetricUnits ? "C" : "F";
                            weatherScrollText = "TOMORROW: HIGH " + String(high_buf) + unit + ", LOW " + String(low_buf) + unit + ", " + desc;
                            break;
                        }
                        case 2: { // Wind & Rain
                            String windUnit = currentSettings.useMetricUnits ? "KPH" : "MPH";
                            weatherScrollText = "WIND: " + String((int)currentWeatherData.maxWindSpeed) + " " + windUnit +
                                              ", PRECIPITATION: " + String(currentWeatherData.precipitationProbability) + "%";
                            break;
                        }
                        case 3: { // Sunrise & Sunset
                            struct tm timeinfo;
                            char timeStr[9]; // "12:00AM" + null
                            localtime_r(&currentWeatherData.sunrise, &timeinfo);
                            strftime(timeStr, sizeof(timeStr), "%l%M %p", &timeinfo);
                            String sunriseStr = timeStr;
                            sunriseStr.trim();
                            localtime_r(&currentWeatherData.sunset, &timeinfo);
                            strftime(timeStr, sizeof(timeStr), "%l%M %p", &timeinfo);
                            String sunsetStr = timeStr;
                            sunsetStr.trim();
                            weatherScrollText = "SUNRISE: " + sunriseStr + ", SUNSET: " + sunsetStr;
                            break;
                        }
                    }
                    weatherScrollText = "             " + weatherScrollText;
                    weatherScrollPosition = 0;
                    weatherState = WD_SCROLLING;
                    lastWeatherUpdate = millis();
                }

                case WD_SCROLLING: {
                    if (millis() - lastWeatherUpdate > scrollSpeed) {
                        lastWeatherUpdate = millis();
                        String tempScrollText = weatherScrollText + "             ";
                        viewport = tempScrollText.substring(weatherScrollPosition, weatherScrollPosition + 13);

                        String monthStr = viewport.substring(0, 3);
                        String dayStr = viewport.substring(3, 5);

                        printToDisplay(lastRow.month, monthStr.c_str(), 1);
                        printToDisplay(lastRow.day, dayStr.c_str(), 2);
                        printToDisplay(lastRow.year, viewport.substring(5, 9).c_str(), 0);
                        printToDisplay(lastRow.time, viewport.substring(9, 13).c_str(), 0);
                        shouldWriteToDisplay = true;

                        weatherScrollPosition++;
                        if (weatherScrollPosition > weatherScrollText.length()) {
                            weatherState = WD_PAUSING;
                            lastWeatherUpdate = millis();
                        }
                    }
                    break;
                }

                case WD_PAUSING: {
                    if (millis() - lastWeatherUpdate > pauseDuration) {
                        weatherPage = (weatherPage + 1) % 4;
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
    String timeCanvas, yearCanvas;

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
            yearCanvas = "   " + String(yearContent.c_str()) + "   ";
            if (yearCanvas.length() <= 4) {
                printToDisplay(targetRow->year, yearCanvas.c_str());
            } else {
                String yearViewport = yearCanvas.substring(marqueeScrollPositionYear, marqueeScrollPositionYear + 4);
                printToDisplay(targetRow->year, yearViewport.c_str());
            }

            timeCanvas = "   " + String(timeContent.c_str()) + "   ";
            if (timeCanvas.length() <= 4) {
                printToDisplay(targetRow->time, timeCanvas.c_str());
            } else {
                String viewport = timeCanvas.substring(marqueeScrollPosition, marqueeScrollPosition + 4);
                printToDisplay(targetRow->time, viewport.c_str());
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

            if (timeCanvas.length() > 4) {
                marqueeScrollPosition++;
                if (marqueeScrollPosition > timeCanvas.length() - 4) {
                    timeDone = true;
                }
            } else {
                timeDone = true;
            }

            if (yearCanvas.length() > 4) {
                marqueeScrollPositionYear++;
                if (marqueeScrollPositionYear > yearCanvas.length() - 4) {
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