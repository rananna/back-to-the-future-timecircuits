#include "globals.h"
#include "HardwareControl.h"
#include "AnimationManager.h"
#include "DisplayManager.h"
#include "SettingsManager.h"
#include "types.h"
#include "config.h"
#include "time_manager.h"
#include "FS.h"
#include <LittleFS.h>
#include <string>

// Global variable definitions for the display state
DisplayPage displayPages[NUM_PAGES];
DisplayPage lastGoodDisplayPages[NUM_PAGES];

// Marquee animation variables
int marqueeScrollPosition = 0;
int marqueeScrollPositionYear = 0;
unsigned long lastMarqueeStateChange = 0;
MarqueeState marqueeState = M_SCROLLING;
int currentPageIndex = 0;

void setupDisplay() {
    initLEDs();
    initDisplayPages();
    updateDisplayContent(false);
}

void initDisplayPages() {
    // Initial display page setup
    for (int i = 0; i < NUM_PAGES; i++) {
        displayPages[i].month = "JAN";
        displayPages[i].day = "01";
        displayPages[i].year = "1985";
        displayPages[i].time = "12:00A";
    }
}

void updateDisplayContent(bool force) {
    // Logic to update display based on current settings and page
    if (isDisplayAsleep && !force) return;

    // Check for manual overrides
    if (isMessageOverrideActive) {
        updateManualOverrideDisplay();
        return;
    }

    if (isMarqueeOverrideActive) {
        updateMarqueeDisplay();
        return;
    }

    // Use current settings to determine display content
    int displayMode = currentSettings.displayMode;
    switch (displayMode) {
        case MODE_TIME_CIRCUITS:
            updateTimeCircuitsDisplay();
            break;
        case MODE_WEATHER:
            updateWeatherDisplay();
            break;
        case MODE_STOCK_TICKER:
            updateStockTickerDisplay();
            break;
        case MODE_DATA_LINK:
            updateDataLinkDisplay();
            break;
        case MODE_MANUAL:
            updateManualDisplay();
            break;
        case MODE_SCROLL_MARQUEE:
            updateMarqueeDisplay();
            break;
        case MODE_SCROLL_DATE:
            updateMarqueeDisplay();
            break;
        case MODE_PRESET_CYCLE:
            updatePresetCycleDisplay();
            break;
        case MODE_TIME_TRAVEL:
            // Handled by animation manager
            break;
        case MODE_BOOT_SEQUENCE:
            // Handled by boot sequence
            break;
        case MODE_GLITCHING:
            // Handled by malfunction manager
            break;
        case MODE_OFF:
            allOff();
            break;
        case MODE_IDLE:
        default:
            updateDisplayFromPage();
            break;
    }
}

void updateTimeCircuitsDisplay() {
    // The core function for the standard time circuits display
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        // Destination Time
        updateDisplaySegment(0, 0, time_format_dest_month(timeinfo.tm_mon));
        updateDisplaySegment(0, 1, time_format_day(timeinfo.tm_mday));
        updateDisplaySegment(0, 2, time_format_year(timeinfo.tm_year));
        updateDisplaySegment(0, 3, time_format_time(timeinfo.tm_hour, timeinfo.tm_min));

        // Present Time
        updateDisplaySegment(1, 0, time_format_pres_month(timeinfo.tm_mon));
        updateDisplaySegment(1, 1, time_format_day(timeinfo.tm_mday));
        updateDisplaySegment(1, 2, time_format_year(timeinfo.tm_year));
        updateDisplaySegment(1, 3, time_format_time(timeinfo.tm_hour, timeinfo.tm_min));

        // Last Time Departed
        updateDisplaySegment(2, 0, time_format_last_month(currentSettings.lastDeparted_month));
        updateDisplaySegment(2, 1, time_format_day(currentSettings.lastDeparted_day));
        updateDisplaySegment(2, 2, time_format_year(currentSettings.lastDeparted_year));
        updateDisplaySegment(2, 3, time_format_time(currentSettings.lastDeparted_hour, currentSettings.lastDeparted_minute));
    }
}

void updateWeatherDisplay() {
    // Display weather data
    updateDisplaySegment(0, 0, "NOW");
    updateDisplaySegment(0, 1, weather_format_temp(currentWeatherData.temperature, currentSettings.tempUnit == FAHRENHEIT));
    updateDisplaySegment(0, 2, weather_format_humidity(currentWeatherData.humidity));
    updateDisplaySegment(0, 3, weather_format_pressure(currentWeatherData.pressure));

    updateDisplaySegment(1, 0, "HI/LO");
    updateDisplaySegment(1, 1, weather_format_temp(currentWeatherData.maxTemperature, currentSettings.tempUnit == FAHRENHEIT));
    updateDisplaySegment(1, 2, weather_format_temp(currentWeatherData.minTemperature, currentSettings.tempUnit == FAHRENHEIT));
    updateDisplaySegment(1, 3, "TEMP");

    updateDisplaySegment(2, 0, currentWeatherData.location.c_str());
    updateDisplaySegment(2, 1, weather_format_weather_condition(currentWeatherData.weatherId));
    updateDisplaySegment(2, 2, weather_format_wind_speed(currentWeatherData.windSpeed, currentSettings.windSpeedUnit == MPH));
    updateDisplaySegment(2, 3, weather_format_wind_direction(currentWeatherData.windDirection));
}

void updateStockTickerDisplay() {
    // Display stock data
    char tempBuffer[16];
    for (int i = 0; i < 3; ++i) {
        std::string symbol = "";
        std::string price = "";
        std::string change = "";
        std::string percent = "";

        if (i == 0) {
            symbol = currentSettings.stockRow1_symbol.c_str(); // Fixed type mismatch
            price = stockData[0].price.c_str(); // Fixed type mismatch
            change = stockData[0].change.c_str(); // Fixed type mismatch
            percent = stockData[0].percentChange.c_str(); // Fixed type mismatch
        } else if (i == 1) {
            symbol = currentSettings.stockRow2_symbol.c_str(); // Fixed type mismatch
            price = stockData[1].price.c_str(); // Fixed type mismatch
            change = stockData[1].change.c_str(); // Fixed type mismatch
            percent = stockData[1].percentChange.c_str(); // Fixed type mismatch
        } else {
            symbol = currentSettings.stockRow3_symbol.c_str(); // Fixed type mismatch
            price = stockData[2].price.c_str(); // Fixed type mismatch
            change = stockData[2].change.c_str(); // Fixed type mismatch
            percent = stockData[2].percentChange.c_str(); // Fixed type mismatch
        }

        updateDisplaySegment(i, 0, symbol.c_str());

        // Price and Change/Percent
        if (currentSettings.stockChangeDisplay == STOCK_CHANGE_DISPLAY_AMOUNT) {
            updateDisplaySegment(i, 1, price.c_str());
            updateDisplaySegment(i, 2, change.c_str());
            updateDisplaySegment(i, 3, percent.c_str());
        } else if (currentSettings.stockChangeDisplay == STOCK_CHANGE_DISPLAY_PERCENT) {
            updateDisplaySegment(i, 1, price.c_str());
            updateDisplaySegment(i, 2, percent.c_str());
            updateDisplaySegment(i, 3, change.c_str());
        }
    }
}

void updateDataLinkDisplay() {
    // Display API data
    std::string month = displayPages[currentPageIndex].month;
    std::string day = displayPages[currentPageIndex].day;
    std::string year = displayPages[currentPageIndex].year;
    std::string time = displayPages[currentPageIndex].time;

    updateDisplaySegment(0, 0, month.c_str());
    updateDisplaySegment(0, 1, day.c_str());
    updateDisplaySegment(0, 2, year.c_str());
    updateDisplaySegment(0, 3, time.c_str());

    updateDisplaySegment(1, 0, displayPages[currentPageIndex].message_line_1.c_str());
    updateDisplaySegment(1, 1, displayPages[currentPageIndex].message_line_2.c_str());
    updateDisplaySegment(1, 2, displayPages[currentPageIndex].message_line_3.c_str());
    updateDisplaySegment(1, 3, displayPages[currentPageIndex].message_line_4.c_str());

    updateDisplaySegment(2, 0, displayPages[currentPageIndex].message_line_5.c_str());
    updateDisplaySegment(2, 1, displayPages[currentPageIndex].message_line_6.c_str());
    updateDisplaySegment(2, 2, displayPages[currentPageIndex].message_line_7.c_str());
    updateDisplaySegment(2, 3, displayPages[currentPageIndex].message_line_8.c_str());
}

void updateManualDisplay() {
    for (int i = 0; i < 3; ++i) {
        if (isRowInManualMode[i]) {
            updateDisplaySegment(i, 0, manualDisplayText[i][0].c_str());
            updateDisplaySegment(i, 1, manualDisplayText[i][1].c_str());
            updateDisplaySegment(i, 2, manualDisplayText[i][2].c_str());
            updateDisplaySegment(i, 3, manualDisplayText[i][3].c_str());
        }
    }
}

void updateManualOverrideDisplay() {
    updateDisplaySegment(0, 0, overrideMessageLine1.c_str());
    updateDisplaySegment(0, 1, "");
    updateDisplaySegment(0, 2, "");
    updateDisplaySegment(0, 3, "");
    updateDisplaySegment(1, 0, overrideMessageLine2.c_str());
    updateDisplaySegment(1, 1, "");
    updateDisplaySegment(1, 2, "");
    updateDisplaySegment(1, 3, "");
    updateDisplaySegment(2, 0, overrideMessageLine3.c_str());
    updateDisplaySegment(2, 1, "");
    updateDisplaySegment(2, 2, "");
    updateDisplaySegment(2, 3, "");
}

void updateDisplayFromPage() {
    // Update all three displays from the current DisplayPage
    updateDisplaySegment(0, 0, displayPages[currentPageIndex].month.c_str());
    updateDisplaySegment(0, 1, displayPages[currentPageIndex].day.c_str());
    updateDisplaySegment(0, 2, displayPages[currentPageIndex].year.c_str());
    updateDisplaySegment(0, 3, displayPages[currentPageIndex].time.c_str());

    updateDisplaySegment(1, 0, displayPages[currentPageIndex].message_line_1.c_str());
    updateDisplaySegment(1, 1, displayPages[currentPageIndex].message_line_2.c_str());
    updateDisplaySegment(1, 2, displayPages[currentPageIndex].message_line_3.c_str());
    updateDisplaySegment(1, 3, displayPages[currentPageIndex].message_line_4.c_str());

    updateDisplaySegment(2, 0, displayPages[currentPageIndex].message_line_5.c_str());
    updateDisplaySegment(2, 1, displayPages[currentPageIndex].message_line_6.c_str());
    updateDisplaySegment(2, 2, displayPages[currentPageIndex].message_line_7.c_str());
    updateDisplaySegment(2, 3, displayPages[currentPageIndex].message_line_8.c_str());
}

void updateMarqueeDisplay() {
    if (isMarqueeOverrideActive && millis() > marqueeOverrideEndTime) {
        isMarqueeOverrideActive = false;
        marqueeScrollPosition = 0;
        marqueeScrollPositionYear = 0;
        marqueeState = M_PAUSED;
        lastMarqueeStateChange = millis();
    }

    // Determine the active display row based on settings
    int rowToScroll = 0;
    if (currentSettings.displayMode == MODE_SCROLL_MARQUEE) {
        rowToScroll = 1;
    } else if (currentSettings.displayMode == MODE_SCROLL_DATE) {
        rowToScroll = 0;
    }

    DisplayRow* targetRow = &displayPages[currentPageIndex];
    if (isMarqueeOverrideActive) {
        targetRow = &displayPages[NUM_PAGES]; // Use the last page for override
    }

    std::string timeContent = std::string(targetRow->month.c_str()) + " " + std::string(targetRow->day.c_str()) + " " + std::string(targetRow->year.c_str());
    std::string yearContent = std::string(targetRow->message_line_5.c_str()) + std::string(targetRow->message_line_6.c_str()) + std::string(targetRow->message_line_7.c_str()) + std::string(targetRow->message_line_8.c_str());

    std::string timeCanvas = "    " + timeContent + "    ";
    std::string yearCanvas = "    " + yearContent + "    ";

    unsigned long currentTime = millis();

    // Handle the scrolling animation state
    switch (marqueeState) {
        case M_PAUSED:
            if (currentTime - lastMarqueeStateChange >= MARQUEE_PAUSE_TIME) {
                marqueeState = M_SCROLLING;
                lastMarqueeStateChange = currentTime;
            }
            break;
        case M_SCROLLING: {
            if (currentSettings.displayMode == MODE_SCROLL_DATE) {
                // Handle year/date scrolling
                std::string yearViewport = yearCanvas.substr(marqueeScrollPositionYear, 4);
                printToDisplay(targetRow->year, yearViewport.c_str());
            } else if (currentSettings.displayMode == MODE_SCROLL_MARQUEE) {
                // Handle marquee scrolling
                std::string viewport = timeCanvas.substr(marqueeScrollPosition, 4);
                printToDisplay(targetRow->time, viewport.c_str());
            }

            if (currentTime - lastMarqueeStateChange >= MARQUEE_SCROLL_SPEED) {
                if (currentSettings.displayMode == MODE_SCROLL_MARQUEE) {
                    marqueeScrollPosition++;
                    if (marqueeScrollPosition > timeCanvas.length() - 4) {
                        marqueeScrollPosition = 0;
                        marqueeState = M_PAUSED;
                    }
                } else if (currentSettings.displayMode == MODE_SCROLL_DATE) {
                    marqueeScrollPositionYear++;
                    if (marqueeScrollPositionYear > yearCanvas.length() - 4) {
                        marqueeScrollPositionYear = 0;
                        marqueeState = M_PAUSED;
                    }
                }
                lastMarqueeStateChange = currentTime;
            }
            break;
        }
    }
}

void updatePresetCycleDisplay() {
    unsigned long currentTime = millis();
    if (currentTime - lastPresetCycleTime > (unsigned long)currentSettings.presetCycleTime * 1000) {
        int nextPageIndex = (currentPageIndex + 1) % NUM_PAGES;
        if (nextPageIndex == 0) { // Wrap around
            // Reset to the first enabled page instead of page 0
            for (int i = 0; i < NUM_PAGES; ++i) {
                if (currentSettings.pages[i].enabled) {
                    currentPageIndex = i;
                    break;
                }
            }
        } else {
            currentPageIndex = nextPageIndex;
        }
        lastPresetCycleTime = currentTime;
    }
    updateDisplayFromPage();
}

void setDisplayPage(int pageIndex) {
    if (pageIndex >= 0 && pageIndex < NUM_PAGES) {
        currentPageIndex = pageIndex;
        updateDisplayFromPage();
    }
}

void nextDisplayPage() {
    int nextIndex = (currentPageIndex + 1) % NUM_PAGES;
    while (!currentSettings.pages[nextIndex].enabled) {
        nextIndex = (nextIndex + 1) % NUM_PAGES;
    }
    currentPageIndex = nextIndex;
    updateDisplayContent(false);
}