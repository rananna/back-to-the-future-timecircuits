#include "DisplayManager.h"
#include "EventManager.h"
#include "HardwareControl.h"
#include <time.h>
#include "globals.h"

extern StockData stockData[3];
extern std::string manualDisplayText[3][4];
extern bool isRowInManualMode[3];

void showTemporaryMessage(const char* month, const char* day, const char* year, const char* time, int duration) {
    if (!hardwareInitialized) return;
#if ENABLE_HARDWARE
    printToDisplay(lastRow.month, month, 1);
    printToDisplay(lastRow.day, day, 2);
    printToDisplay(lastRow.year, year);
    printToDisplay(lastRow.time, time);
    lastRow.month.writeDisplay();
    lastRow.day.writeDisplay();
    lastRow.year.writeDisplay();
    lastRow.time.writeDisplay();
    delay(duration);
#endif
}

const char* getIconForWeatherCode(int code) {
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
    String textToDisplay = marqueeOverrideMessage;

    if (textToDisplay.length() > 13) {
        textToDisplay = "  " + textToDisplay + "  ";
    }

    static unsigned long lastScrollTime = 0;
    static int scrollPosition = 0;

    if (millis() - lastScrollTime > 50) {
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

void updateStockTickerDisplay() {
    if (isDisplayAsleep || isAnimating || isGlitching || isMalfunctioning || !hardwareInitialized) return;
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
                } else if (currentSettings.alphaVantageApiKey.empty()) {
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
        rows[i]->month.writeDisplay();
        rows[i]->day.writeDisplay();
        rows[i]->year.writeDisplay();
        rows[i]->time.writeDisplay();
    }
#endif
}

void displayOverrideMessage() {
    if (!hardwareInitialized) return;
#if ENABLE_HARDWARE
    printToDisplay(destRow.month, overrideMessageLine1.substring(0, 3).c_str(), 1);
    printToDisplay(destRow.day, overrideMessageLine1.substring(3, 5).c_str(), 2);
    printToDisplay(destRow.year, overrideMessageLine1.substring(5, 9).c_str());
    printToDisplay(destRow.time, overrideMessageLine1.substring(9, 13).c_str());
    destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();

    printToDisplay(presRow.month, overrideMessageLine2.substring(0, 3).c_str(), 1);
    printToDisplay(presRow.day, overrideMessageLine2.substring(3, 5).c_str(), 2);
    printToDisplay(presRow.year, overrideMessageLine2.substring(5, 9).c_str());
    printToDisplay(presRow.time, overrideMessageLine2.substring(9, 13).c_str());
    presRow.month.writeDisplay(); presRow.day.writeDisplay(); presRow.year.writeDisplay(); presRow.time.writeDisplay();

    printToDisplay(lastRow.month, overrideMessageLine3.substring(0, 3).c_str(), 1);
    printToDisplay(lastRow.day, overrideMessageLine3.substring(3, 5).c_str(), 2);
    printToDisplay(lastRow.year, overrideMessageLine3.substring(5, 9).c_str());
    printToDisplay(lastRow.time, overrideMessageLine3.substring(9, 13).c_str());
    lastRow.month.writeDisplay(); lastRow.day.writeDisplay(); lastRow.year.writeDisplay(); lastRow.time.writeDisplay();
#endif
}

// Function signature corrected to match the prototype in config.h
void updateDisplaySegment(int row, int segment, const char* text) {
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


void updateDisplayRow(DisplayRow& row, struct tm& timeinfo, int year, int rowIndex) {
    char buffer[10];

    // Month
    if (isRowInManualMode[rowIndex] && !manualDisplayText[rowIndex][0].empty()) {
        printToDisplay(row.month, manualDisplayText[rowIndex][0].c_str(), 1);
    } else {
        strftime(buffer, sizeof(buffer), "%b", &timeinfo);
        for (char *p = buffer; *p; ++p) *p = toupper((unsigned char)*p);
        printToDisplay(row.month, buffer, 1);
    }

    // Day
    if (isRowInManualMode[rowIndex] && !manualDisplayText[rowIndex][1].empty()) {
        printToDisplay(row.day, manualDisplayText[rowIndex][1].c_str(), 2);
    } else {
        strftime(buffer, sizeof(buffer), "%d", &timeinfo);
        printToDisplay(row.day, buffer, 2);
    }

    // Year
    if (isRowInManualMode[rowIndex] && !manualDisplayText[rowIndex][2].empty()) {
        printToDisplay(row.year, manualDisplayText[rowIndex][2].c_str());
    } else {
        sprintf(buffer, "%d", year);
        printToDisplay(row.year, buffer);
    }

    // Time
    if (isRowInManualMode[rowIndex] && !manualDisplayText[rowIndex][3].empty()) {
        printToDisplay(row.time, manualDisplayText[rowIndex][3].c_str());
    } else {
        if (currentSettings.displayFormat24h) {
            strftime(buffer, sizeof(buffer), "%H%M", &timeinfo);
        } else {
            strftime(buffer, sizeof(buffer), "%I%M", &timeinfo);
        }
        printToDisplay(row.time, buffer);
    }

    row.month.writeDisplay();
    row.day.writeDisplay();
    row.year.writeDisplay();
    row.time.writeDisplay();
}


void updateNormalClockDisplay() {
  if (isDisplayAsleep || isAnimating || isGlitching || isMalfunctioning || !hardwareInitialized) return;
#if ENABLE_HARDWARE
  if (timeSynchronized) {
    time_t now;
    time(&now);
    struct tm timeinfo;

    // Destination Time
    setenv("TZ", TZ_DATA[currentSettings.destinationTimezoneIndex].tzString, 1);
    tzset();
    localtime_r(&now, &timeinfo);
    updateDisplayRow(destRow, timeinfo, currentSettings.destinationYear, 0);

    // Present Time
    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
    tzset();
    localtime_r(&now, &timeinfo);
    int presentYear = timeinfo.tm_year + 1900;
    updateDisplayRow(presRow, timeinfo, presentYear, 1);

    // Last Time Departed
    struct tm lastTimeDepartedInfo = {0};
    lastTimeDepartedInfo.tm_year = currentSettings.lastTimeDepartedYear - 1900;
    lastTimeDepartedInfo.tm_mon = currentSettings.lastTimeDepartedMonth - 1;
    lastTimeDepartedInfo.tm_mday = currentSettings.lastTimeDepartedDay;
    lastTimeDepartedInfo.tm_hour = currentSettings.lastTimeDepartedHour;
    lastTimeDepartedInfo.tm_min = currentSettings.lastTimeDepartedMinute;
    updateDisplayRow(lastRow, lastTimeDepartedInfo, currentSettings.lastTimeDepartedYear, 2);
  }
#endif
}

void handleWeatherDisplay() {
    if (!currentSettings.weatherModeEnabled || !hardwareInitialized) return;
#if ENABLE_HARDWARE
    if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
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

            const char* icon = getIconForWeatherCode(currentWeatherData.icon);
            switch(weatherPage) {
                case 0:
                    printToDisplay(lastRow.month, "NOW", 1);
                    printToDisplay(lastRow.day, icon, 2);
                    dtostrf(currentWeatherData.temp, 4, 1, buffer);
                    printToDisplay(lastRow.year, buffer);
                    printToDisplay(lastRow.time, currentSettings.useMetricUnits ? "CEL" : "DEG");
                    digitalWrite(LAST_AM_PIN, LOW);
                    digitalWrite(LAST_PM_PIN, LOW);
                    break;
                case 1:
                    printToDisplay(lastRow.month, "TMRW", 1);
                    printToDisplay(lastRow.day, getIconForWeatherCode(currentWeatherData.tomorrow_icon), 2);
                    dtostrf(currentWeatherData.tomorrow_temp_max, 4, 0, buffer);
                    printToDisplay(lastRow.year, buffer);
                    dtostrf(currentWeatherData.tomorrow_temp_min, 4, 0, buffer);
                    printToDisplay(lastRow.time, buffer);
                    digitalWrite(LAST_AM_PIN, HIGH);
                    digitalWrite(LAST_PM_PIN, LOW);
                    break;
                case 2:
                    printToDisplay(lastRow.month, "WIND", 1);
                    dtostrf(currentWeatherData.wind_gust, 2, 0, buffer);
                    strcat(buffer, "M");
                    printToDisplay(lastRow.day, buffer, 2);
                    printToDisplay(lastRow.year, "RAIN");
                    sprintf(buffer, "%d%%", currentWeatherData.precip_chance);
                    printToDisplay(lastRow.time, buffer);
                    digitalWrite(LAST_AM_PIN, LOW);
                    digitalWrite(LAST_PM_PIN, HIGH);
                    break;
                case 3:
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

void updateMarqueeDisplay() {
    if (!currentSettings.dataLinkEnabled || currentSettings.numDataPoints == 0 || !hardwareInitialized) return;
#if ENABLE_HARDWARE
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