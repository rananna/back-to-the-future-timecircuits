#include "DisplayManager.h"
#include "EventManager.h"

/**
 * @brief Displays a temporary message on the bottom display row.
 * @param month Text for the MONTH display (3 chars).
 * @param day Text for the DAY display (2 chars).
 * @param year Text for the YEAR display (4 chars).
 * @param time Text for the TIME display (4 chars).
 * @param duration The duration in milliseconds to show the message.
 */
void showTemporaryMessage(const char* month, const char* day, const char* year, const char* time, int duration) {
    #if ENABLE_HARDWARE
    printToDisplay(lastRow.month, month, 1);
    printToDisplay(lastRow.day, day, 2);
    printToDisplay(lastRow.year, year);
    printToDisplay(lastRow.time, time);
    lastRow.month.writeDisplay();
    lastRow.day.writeDisplay();
    lastRow.year.writeDisplay();
    lastRow.time.writeDisplay();
    delay(duration); // Note: This is a blocking delay. Use sparingly.
    #endif
}

/**
 * @brief Maps a WMO weather code to a 2-character display icon.
 * @param code The integer weather code from the Open-Meteo API.
 * @return A 2-character string representing the weather icon.
 */
const char* getIconForWeatherCode(int code) {
    switch (code) {
        case 0: case 1: return "SU"; // Clear, Mainly clear
        case 2: return "CL"; // Partly cloudy
        case 3: return "CL"; // Overcast
        case 45: case 48: return "CL"; // Fog
        case 51: case 53: case 55: return "RN"; // Drizzle
        case 61: case 63: case 65: return "RN"; // Rain
        case 66: case 67: return "RN"; // Freezing Rain
        case 71: case 73: case 75: return "SN"; // Snow
        case 77: return "SN"; // Snow grains
        case 80: case 81: case 82: return "RN"; // Rain showers
        case 85: case 86: return "SN"; // Snow showers
        case 95: case 96: case 99: return "ST"; // Thunderstorm
        default: return "--";
    }
}

/**
 * @brief HA-MARQUEE: New display function for the marquee override mode.
 */
void displayMarqueeOverride() {
    #if ENABLE_HARDWARE
    String textToDisplay = marqueeOverrideMessage;
    
    if (textToDisplay.length() > 13) {
        textToDisplay = "  " + textToDisplay + "  ";
    }

    static unsigned long lastScrollTime = 0;
    static int scrollPosition = 0;

    if (millis() - lastScrollTime > currentSettings.dataPoints[currentPageIndex].scrollSpeed) { // Scroll Speed
        lastScrollTime = millis();
        
        String viewport = textToDisplay.substring(scrollPosition, scrollPosition + 13);
        
        printToDisplay(lastRow.month, viewport.substring(0, 3).c_str(), 0);
        printToDisplay(lastRow.day, viewport.substring(3, 5).c_str(), 0);
        printToDisplay(lastRow.year, viewport.substring(5, 9).c_str(), 0);
        printToDisplay(lastRow.time, viewport.substring(9, 13).c_str(), 0);

        lastRow.month.writeDisplay();
        lastRow.day.writeDisplay();
        lastRow.year.writeDisplay();
        lastRow.time.writeDisplay();

        if (textToDisplay.length() > 13) {
            scrollPosition++;
            if (scrollPosition > textToDisplay.length() - 13) {
                scrollPosition = 0;
            }
        } else {
            scrollPosition = 0;
        }
    }
    #endif
}

/**
 * @brief HA-ENHANCEMENT: New display function for message override mode.
 */
void displayOverrideMessage() {
    #if ENABLE_HARDWARE
    // Display the override message, splitting it across the three rows.
    // Line 1 on Destination Row (top)
    printToDisplay(destRow.month, overrideMessageLine1.substring(0, 3).c_str(), 1);
    printToDisplay(destRow.day, overrideMessageLine1.substring(3, 5).c_str(), 2);
    printToDisplay(destRow.year, overrideMessageLine1.substring(5, 9).c_str());
    printToDisplay(destRow.time, overrideMessageLine1.substring(9, 13).c_str());
    destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();

    // Line 2 on Present Row (middle)
    printToDisplay(presRow.month, overrideMessageLine2.substring(0, 3).c_str(), 1);
    printToDisplay(presRow.day, overrideMessageLine2.substring(3, 5).c_str(), 2);
    printToDisplay(presRow.year, overrideMessageLine2.substring(5, 9).c_str());
    printToDisplay(presRow.time, overrideMessageLine2.substring(9, 13).c_str());
    presRow.month.writeDisplay(); presRow.day.writeDisplay(); presRow.year.writeDisplay(); presRow.time.writeDisplay();

    // Line 3 on Last Departed Row (bottom)
    printToDisplay(lastRow.month, overrideMessageLine3.substring(0, 3).c_str(), 1);
    printToDisplay(lastRow.day, overrideMessageLine3.substring(3, 5).c_str(), 2);
    printToDisplay(lastRow.year, overrideMessageLine3.substring(5, 9).c_str());
    printToDisplay(lastRow.time, overrideMessageLine3.substring(9, 13).c_str());
    lastRow.month.writeDisplay(); lastRow.day.writeDisplay(); lastRow.year.writeDisplay(); lastRow.time.writeDisplay();
    #endif
}

/**
 * @brief Updates all three display rows with their normal time data.
 */
void updateNormalClockDisplay() {
  if (isDisplayAsleep || isAnimating || isGlitching || isMalfunctioning) return;
  #if ENABLE_HARDWARE
  if (timeSynchronized) {
    time_t now;
    time(&now);
    struct tm timeinfo;
    
    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
    tzset();
    localtime_r(&now, &timeinfo);
    updateDisplayRow(presRow, timeinfo, timeinfo.tm_year + 1900);
    
    setenv("TZ", TZ_DATA[currentSettings.destinationTimezoneIndex].tzString, 1);
    tzset();
    localtime_r(&now, &timeinfo);
    updateDisplayRow(destRow, timeinfo, currentSettings.destinationYear);
    
    struct tm lastTimeDepartedInfo = {0};
    lastTimeDepartedInfo.tm_year = currentSettings.lastTimeDepartedYear - 1900;
    lastTimeDepartedInfo.tm_mon = currentSettings.lastTimeDepartedMonth - 1;
    lastTimeDepartedInfo.tm_mday = currentSettings.lastTimeDepartedDay;
    lastTimeDepartedInfo.tm_hour = currentSettings.lastTimeDepartedHour;
    lastTimeDepartedInfo.tm_min = currentSettings.lastTimeDepartedMinute;
    updateDisplayRow(lastRow, lastTimeDepartedInfo, currentSettings.lastTimeDepartedYear);
  }
  #endif
}

/**
 * @brief Handles the multi-page display logic for the live weather mode.
 */
void handleWeatherDisplay() {
    #if ENABLE_HARDWARE
    if (!currentSettings.weatherModeEnabled) return;
    if (xSemaphoreTake(xDisplayDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (!currentWeatherData.dataValid) {
            printToDisplay(lastRow.month, "WEA", 1);
            printToDisplay(lastRow.day, "TH", 2);
            printToDisplay(lastRow.year, "ER");
            printToDisplay(lastRow.time, "----");
        } else {
            static int weatherPage = 0;
            static unsigned long lastPageChange = 0;
            char buffer[6];
            if (millis() - lastPageChange > 4000) {
                weatherPage = (weatherPage + 1) % 4;
                lastPageChange = millis();
            }
            
            const char* icon = getIconForWeatherCode(currentWeatherData.weatherCode);
            switch(weatherPage) {
                case 0: // Current Conditions
                    printToDisplay(lastRow.month, "NOW", 1);
                    printToDisplay(lastRow.day, icon, 2);
                    dtostrf(currentWeatherData.temperature, 4, 1, buffer);
                    printToDisplay(lastRow.year, buffer);
                    printToDisplay(lastRow.time, currentSettings.useMetricUnits ? "CEL" : "DEG");
                    digitalWrite(LAST_AM_PIN, LOW);
                    digitalWrite(LAST_PM_PIN, LOW);
                    break;
                case 1: // Tomorrow's Forecast
                    printToDisplay(lastRow.month, "TMRW", 1);
                    printToDisplay(lastRow.day, getIconForWeatherCode(currentWeatherData.tomorrowWeatherCode), 2);
                    dtostrf(currentWeatherData.tomorrowHigh, 4, 0, buffer);
                    printToDisplay(lastRow.year, buffer);
                    dtostrf(currentWeatherData.tomorrowLow, 4, 0, buffer);
                    printToDisplay(lastRow.time, buffer);
                    digitalWrite(LAST_AM_PIN, HIGH);
                    digitalWrite(LAST_PM_PIN, LOW);
                    break;
                case 2: // Wind and Rain
                    printToDisplay(lastRow.month, "WIND", 1);
                    dtostrf(currentWeatherData.maxWindSpeed, 2, 0, buffer);
                    strcat(buffer, "M");
                    printToDisplay(lastRow.day, buffer, 2);
                    printToDisplay(lastRow.year, "RAIN");
                    sprintf(buffer, "%d%%", currentWeatherData.precipitationProbability);
                    printToDisplay(lastRow.time, buffer);
                    digitalWrite(LAST_AM_PIN, LOW);
                    digitalWrite(LAST_PM_PIN, HIGH);
                    break;
                case 3: // Sunrise / Sunset
                    struct tm timeinfo;
                    char timeStr[5];
                    printToDisplay(lastRow.month, "SUN", 1);
                    localtime_r(&currentWeatherData.sunrise, &timeinfo);
                    sprintf(timeStr, "%02d%02d", timeinfo.tm_hour, timeinfo.tm_min);
                    printToDisplay(lastRow.day, timeStr, 2);
                    localtime_r(&currentWeatherData.sunset, &timeinfo);
                    sprintf(timeStr, "%02d%02d", timeinfo.tm_hour, timeinfo.tm_min);
                    printToDisplay(lastRow.year, timeStr);
                    printToDisplay(lastRow.time, "RISE/SET");
                    digitalWrite(LAST_AM_PIN, HIGH);
                    digitalWrite(LAST_PM_PIN, HIGH);
                    break;
            }
        }
        xSemaphoreGive(xDisplayDataMutex);
        lastRow.month.writeDisplay();
        lastRow.day.writeDisplay();
        lastRow.year.writeDisplay();
        lastRow.time.writeDisplay();
    }
    #endif
}

/**
 * @brief Handles the state machine for the Data Link marquee display.
 */
void updateMarqueeDisplay() {
    #if ENABLE_HARDWARE
    if (!currentSettings.dataLinkEnabled || currentSettings.numDataPoints == 0) return;
    DisplayRow* targetRow = &lastRow;
    if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
        if (marqueeState == M_IDLE) {
            currentPageIndex = (currentPageIndex + 1) % currentSettings.numDataPoints;
            marqueeScrollPosition = 0;
            marqueeScrollPositionYear = 0;
            marqueeState = M_PAUSED;
            lastMarqueeStateChange = millis();
        }

        DataPoint point = currentSettings.dataPoints[currentPageIndex];
        printToDisplay(targetRow->month, displayPages[currentPageIndex].month.c_str());
        if (!point.icon.empty()) {
            printToDisplay(targetRow->day, point.icon.c_str(), 2);
        } else {
            printToDisplay(targetRow->day, displayPages[currentPageIndex].day.c_str(), 2);
        }

        std::string yearContent = point.yearPrefix + displayPages[currentPageIndex].year + point.yearSuffix;
        std::string timeContent = point.prefix + displayPages[currentPageIndex].time + point.suffix;
        
        xSemaphoreGive(xDisplayDataMutex);

        String yearCanvas = "   " + String(yearContent.c_str()) + "   ";
        if (yearCanvas.length() <= 4) {
            printToDisplay(targetRow->year, yearCanvas.c_str());
        } else {
            String yearViewport = yearCanvas.substring(marqueeScrollPositionYear, marqueeScrollPositionYear + 4);
            printToDisplay(targetRow->year, yearViewport.c_str());
        }

        String timeCanvas = "   " + String(timeContent.c_str()) + "   ";
        if (timeCanvas.length() <= 4) {
            printToDisplay(targetRow->time, timeCanvas.c_str());
        } else {
            String viewport = timeCanvas.substring(marqueeScrollPosition, marqueeScrollPosition + 4);
            printToDisplay(targetRow->time, viewport.c_str());
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

        targetRow->month.writeDisplay();
        targetRow->day.writeDisplay();
        targetRow->year.writeDisplay();
        targetRow->time.writeDisplay();
    }
    #endif
}