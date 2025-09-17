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
#include <string>

// Define and initialize the dirty flags and buffers for scrolling text
bool isMarqueeBufferDirty = true;
bool isWeatherBufferDirty = true;
bool isMarqueeOverrideBufferDirty = true;

std::string marqueeBuffer;
std::string weatherBuffer;
std::string marqueeOverrideBuffer;
#include "HardwareControl.h"

// Forward declaration for the timeout handler in the main .ino file
void handleWeatherTimeout();

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
        static int marqueeOverrideScrollPosition = 0;
        static unsigned long lastScrollTime = 0;

        // If the data has changed, rebuild the buffer
        if (isMarqueeOverrideBufferDirty) {
            String tempBuffer = "  " + marqueeOverrideMessage + "  ";
            marqueeOverrideBuffer = tempBuffer.c_str();
            marqueeOverrideScrollPosition = 0;
            isMarqueeOverrideBufferDirty = false;
        }

        // Animation logic using the buffer
        if (millis() - lastScrollTime > 150) { // Using a fixed scroll speed for now
            lastScrollTime = millis();

            // No need to check length here, substring handles it.
            std::string viewport_str = marqueeOverrideBuffer.substr(marqueeOverrideScrollPosition, 13);
            const char* viewport = viewport_str.c_str();

            if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
                printToDisplay(lastRow.month, std::string(viewport).substr(0, 3).c_str(), 0);
                printToDisplay(lastRow.day, std::string(viewport).substr(3, 5).c_str(), 0);
                printToDisplay(lastRow.year, std::string(viewport).substr(5, 9).c_str(), 0);
                printToDisplay(lastRow.time, std::string(viewport).substr(9, 13).c_str(), 0);

                lastRow.month.writeDisplay();
                lastRow.day.writeDisplay();
                lastRow.year.writeDisplay();
                lastRow.time.writeDisplay();
                vTaskDelay(pdMS_TO_TICKS(2));
                xSemaphoreGive(xDisplayHardwareMutex);
            }

            // Update scroll position
            if (marqueeOverrideBuffer.length() > 13) {
                marqueeOverrideScrollPosition++;
                if (marqueeOverrideScrollPosition > marqueeOverrideBuffer.length() - 13) {
                    marqueeOverrideScrollPosition = 0;
                }
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
                weatherBuffer = "             WEATHER ERROR: " + currentWeatherData.errorReason;
                weatherScrollPosition = 0;
                lastWeatherUpdate = millis(); // Start the timer for the error display
            }
            // If the fetch has been triggered and 30 seconds have passed, call the main timeout handler
            else if (initialFetchTriggered && initialFetchStartTime > 0 && millis() - initialFetchStartTime > 30000) {
                Log_printf(LOG_LEVEL_WARN, "Weather fetch task timed out. Deleting task.");
                if (weatherTaskHandle != NULL) {
                    vTaskDelete(weatherTaskHandle);
                    weatherTaskHandle = NULL;
                }
                isFetchingWeather = false; // Reset the flag here since the task was killed
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
                isFetchingWeather = true;
                if (weatherTaskHandle != NULL) { // Defensive delete of any old handle
                    vTaskDelete(weatherTaskHandle);
                }
                xTaskCreate(fetchWeatherDataTask, "fetchWeatherDataTask", WEATHER_TASK_STACK_SIZE, NULL, 1, &weatherTaskHandle);
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
                isFetchingWeather = true; // Set the flag to prevent concurrent fetches
                // Reuse the timeout mechanism for this fetch as well
                initialFetchTriggered = true;
                initialFetchStartTime = millis();
                if (weatherTaskHandle != NULL) { // Defensive delete of any old handle
                    vTaskDelete(weatherTaskHandle);
                }
                // Create a new task to fetch weather data in the background
                xTaskCreate(fetchWeatherDataTask, "fetchWeatherDataTask", WEATHER_TASK_STACK_SIZE, NULL, 1, &weatherTaskHandle);
            }

            if (weatherDataUpdated || isWeatherBufferDirty) {
                weatherState = WD_START_PAGE;
                weatherPage = 0;
                weatherDataUpdated = false;
                isWeatherBufferDirty = false;
                initialFetchTriggered = false;
                initialFetchStartTime = 0;
                initialFetchTimedOut = false;
                if (weatherTaskHandle != NULL) {
                    weatherTaskHandle = NULL;
                }
            }

            switch (weatherState) {
                case WD_ERROR: {
                    if (millis() - lastWeatherUpdate > scrollSpeed) {
                        lastWeatherUpdate = millis();
                        std::string tempScrollText = weatherBuffer + "             ";
                        std::string viewport_str = tempScrollText.substr(weatherScrollPosition, 13);
                        const char* viewport = viewport_str.c_str();

                        printToDisplay(lastRow.month, std::string(viewport).substr(0, 3).c_str(), 1);
                        printToDisplay(lastRow.day, std::string(viewport).substr(3, 5).c_str(), 2);
                        printToDisplay(lastRow.year, std::string(viewport).substr(5, 9).c_str(), 0);
                        printToDisplay(lastRow.time, std::string(viewport).substr(9, 13).c_str(), 0);
                        shouldWriteToDisplay = true;

                        weatherScrollPosition++;
                        if (weatherScrollPosition > weatherBuffer.length()) {
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
                    String tempWeatherString;
                    switch (weatherPage) {
                        case 0: { // Current Weather
                            // The global dataValid flag is checked before this switch, so we can assume data is present.
                            dtostrf(currentWeatherData.temperature, 4, 1, buffer);
                            const char* desc = getWeatherDescriptionForCode(currentWeatherData.weatherCode);
                            String unit = currentSettings.useMetricUnits ? "C" : "F";
                            tempWeatherString = "CURRENTLY " + String(buffer) + unit + ", " + desc;
                            break;
                        }
                        case 1: { // Tomorrow's Forecast
                            // The global dataValid flag is checked before this switch, so we can assume data is present.
                            char high_buf[8], low_buf[8];
                            dtostrf(currentWeatherData.tomorrowHigh, 1, 0, high_buf);
                            dtostrf(currentWeatherData.tomorrowLow, 1, 0, low_buf);
                            const char* desc = getWeatherDescriptionForCode(currentWeatherData.tomorrowWeatherCode);
                            String unit = currentSettings.useMetricUnits ? "C" : "F";
                            tempWeatherString = "TOMORROW HIGH " + String(high_buf) + unit + ", LOW " + String(low_buf) + unit + ", " + desc;
                            break;
                        }
                        case 2: { // Wind & Rain
                            // The global dataValid flag is checked before this switch, so we can assume data is present.
                            String windUnit = currentSettings.useMetricUnits ? "KPH" : "MPH";
                            tempWeatherString = "WIND " + String((int)currentWeatherData.windSpeed) + " " + windUnit +
                                                ", MAX " + String((int)currentWeatherData.maxWindSpeed) + " " + windUnit +
                                                ", PRECIP " + String(currentWeatherData.precipitationProbability) + "%";
                            break;
                        }
                        case 3: { // Sunrise & Sunset
                            // The global dataValid flag is checked before this switch, so we can assume data is present.
                            struct tm timeinfo;
                            char timeBuffer[8];
                            time_t sunriseTime = currentWeatherData.sunrise;
                            time_t sunsetTime = currentWeatherData.sunset;

                            // Format sunrise time
                            localtime_r(&sunriseTime, &timeinfo);
                                int sunriseHour = timeinfo.tm_hour;
                                if (!currentSettings.displayFormat24h) {
                                    const char* sunriseAmpm = (sunriseHour >= 12) ? "PM" : "AM";
                                    if (sunriseHour > 12) sunriseHour -= 12;
                                    if (sunriseHour == 0) sunriseHour = 12;
                                    sprintf(timeBuffer, "%d%02d%s", sunriseHour, timeinfo.tm_min, sunriseAmpm);
                                } else {
                                    sprintf(timeBuffer, "%02d%02d", sunriseHour, timeinfo.tm_min);
                                }
                                String sunriseStr(timeBuffer);

                                // Format sunset time
                                localtime_r(&sunsetTime, &timeinfo);
                                int sunsetHour = timeinfo.tm_hour;
                                if (!currentSettings.displayFormat24h) {
                                    const char* sunsetAmpm = (sunsetHour >= 12) ? "PM" : "AM";
                                    if (sunsetHour > 12) sunsetHour -= 12;
                                    if (sunsetHour == 0) sunsetHour = 12;
                                    sprintf(timeBuffer, "%d%02d%s", sunsetHour, timeinfo.tm_min, sunsetAmpm);
                                } else {
                                    sprintf(timeBuffer, "%02d%02d", sunsetHour, timeinfo.tm_min);
                                }
                                String sunsetStr(timeBuffer);

                                tempWeatherString = "SUNRISE " + sunriseStr + ", SUNSET " + sunsetStr;
                            break;
                        }
                        case 4: { // Hourly Forecast
                            String unit = currentSettings.useMetricUnits ? "C" : "F";
                            tempWeatherString = "NEXT 3 HRS ";
                            bool hourlyDataOk = true;
                            for(int i=0; i<3; ++i) {
                                if(currentWeatherData.hourlyCode[i] == -1) {
                                    hourlyDataOk = false;
                                    break;
                                }
                            }

                            if (!hourlyDataOk) {
                                tempWeatherString = "HOURLY DATA UNAVAILABLE";
                            } else {
                                for (int i = 0; i < 3; ++i) {
                                    char temp_buf[8];
                                    dtostrf(currentWeatherData.hourlyTemp[i], 1, 0, temp_buf);
                                    const char* desc = getWeatherDescriptionForCode(currentWeatherData.hourlyCode[i]);
                                    tempWeatherString += String(temp_buf) + unit + " " + desc;
                                    if (i < 2) {
                                        tempWeatherString += ", ";
                                    }
                                }
                            }
                            break;
                        }
                        case 5: { // Feels Like & Humidity
                            // The global dataValid flag is checked before this switch, so we can assume data is present.
                            char feels_like_buf[8];
                            dtostrf(currentWeatherData.apparentTemperature, 1, 0, feels_like_buf);
                            String unit = currentSettings.useMetricUnits ? "C" : "F";
                            tempWeatherString = "FEELS LIKE " + String(feels_like_buf) + unit + ", HUMIDITY " + String(currentWeatherData.humidity) + "%";
                            break;
                        }
                        case 6: { // Today's High/Low
                            // The global dataValid flag is checked before this switch, so we can assume data is present.
                            char high_buf[8], low_buf[8];
                            dtostrf(currentWeatherData.dailyHigh, 1, 0, high_buf);
                            dtostrf(currentWeatherData.dailyLow, 1, 0, low_buf);
                            String unit = currentSettings.useMetricUnits ? "C" : "F";
                            tempWeatherString = "TODAY HIGH " + String(high_buf) + unit + ", LOW " + String(low_buf) + unit;
                            break;
                        }
                    }
                    weatherBuffer = "             " + std::string(tempWeatherString.c_str());
                    weatherScrollPosition = 0;
                    weatherState = WD_SCROLLING;
                    lastWeatherUpdate = millis();
                }

                case WD_SCROLLING: {
                    if (millis() - lastWeatherUpdate > scrollSpeed) {
                        lastWeatherUpdate = millis();
                        std::string tempScrollText = weatherBuffer + "             ";
                        std::string viewport_str = tempScrollText.substr(weatherScrollPosition, 13);
                        const char* viewport = viewport_str.c_str();

                        printToDisplay(lastRow.month, std::string(viewport).substr(0, 3).c_str(), 1);
                        printToDisplay(lastRow.day, std::string(viewport).substr(3, 5).c_str(), 2);
                        printToDisplay(lastRow.year, std::string(viewport).substr(5, 9).c_str(), 0);
                        printToDisplay(lastRow.time, std::string(viewport).substr(9, 13).c_str(), 0);
                        shouldWriteToDisplay = true;

                        weatherScrollPosition++;
                        if (weatherScrollPosition > weatherBuffer.length()) {
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
    static int marqueeScrollPosition = 0;
    static int marqueeScrollPositionYear = 0;

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
        if (marqueeState == M_IDLE) {
            currentPageIndex = (currentPageIndex + 1) % currentSettings.numDataPoints;
            marqueeScrollPosition = 0;
            marqueeScrollPositionYear = 0;
            isMarqueeBufferDirty = true; // Force a rebuild for the new page
            marqueeState = M_PAUSED;
            lastMarqueeStateChange = millis();
        }

        DataPoint point = currentSettings.dataPoints[currentPageIndex];
        static std::string yearBuffer, timeBuffer;

        if (isMarqueeBufferDirty) {
            std::string yearContent = point.yearPrefix + displayPages[currentPageIndex].year + point.yearSuffix;
            std::string timeContent = point.prefix + displayPages[currentPageIndex].time + point.suffix;
            yearBuffer = "   " + yearContent + "   ";
            timeBuffer = "   " + timeContent + "   ";
            isMarqueeBufferDirty = false;
        }

        if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
            printToDisplay(targetRow->month, displayPages[currentPageIndex].month.c_str());
            if (!point.icon.empty()) {
                printToDisplay(targetRow->day, point.icon.c_str(), 2);
            } else {
                printToDisplay(targetRow->day, displayPages[currentPageIndex].day.c_str(), 2);
            }

            std::string yearViewport = yearBuffer.substr(marqueeScrollPositionYear, 4);
            printToDisplay(targetRow->year, yearViewport.c_str());

            std::string timeViewport = timeBuffer.substr(marqueeScrollPosition, 4);
            printToDisplay(targetRow->time, timeViewport.c_str());

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

            if (timeBuffer.length() > 4) {
                marqueeScrollPosition++;
                if (marqueeScrollPosition > timeBuffer.length() - 4) {
                    timeDone = true;
                }
            } else {
                timeDone = true;
            }

            if (yearBuffer.length() > 4) {
                marqueeScrollPositionYear++;
                if (marqueeScrollPositionYear > yearBuffer.length() - 4) {
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
        xSemaphoreGive(xDisplayDataMutex);
    }
#endif
}