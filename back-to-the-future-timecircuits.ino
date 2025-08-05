#include "esp_log.h"
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFiUdp.h>
#include <time.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <HTTPClient.h> // Required for making API requests

#include "HardwareControl.h"

// --- FIX FOR LED_BUILTIN ERROR ---
#ifndef LED_BUILTIN
  #define LED_BUILTIN 2
#endif

// =================================================================
// == GLOBAL DEFINITIONS & OBJECTS                                ==
// =================================================================

// --- Timing and Interval Constants ---
#define WIFI_CHECK_INTERVAL_MS 10000
#define NTP_RETRY_INTERVAL_MS 30000
#define NTP_SUCCESS_INTERVAL_MS 3600000
#define SERIAL_REPORT_INTERVAL_MS 30000
#define ANIMATION_UPDATE_INTERVAL_MS 50
#define COUNT_UPDATE_INTERVAL_MS 20
#define SCROLL_INTERVAL_MS 250
#define BOOT_STATE_CHANGE_INTERVAL_MS 3000
#define BOOT_ANIMATION_WAIT_MS 500
#define NTP_TIMEOUT_MS 2000
#define WIND_SPEED_FETCH_INTERVAL_MS 900000 // 15 minutes
#define GLITCH_EFFECT_INTERVAL_MS 60000 // 1 minute

// Global instances and non-hardware-specific variables
ClockSettings currentSettings = {
  1955, 4,
  22, 0, 7, 0, 1, 21, 1985, 10, 26, 5, 15,
  true, 15, 10, false, THEME_TIME_CIRCUITS, 1,
  4000,
  ANIMATION_SEQUENTIAL_FLICKER,
  true,
  false, // windSpeedModeEnabled
  -80.52, 43.47 // Default coordinates (Kitchener, ON, Canada)
};
ClockSettings defaultSettings = {
  1955, 4, 22, 0, 7, 0, 1, 21, 1985, 10, 26, 5, 15, true, 15, 10, false, THEME_TIME_CIRCUITS, 1,
  4000,
  ANIMATION_SEQUENTIAL_FLICKER,
  true,
  false, // windSpeedModeEnabled
  -80.52, 43.47
};
const TimeZoneEntry TZ_DATA[] = {
  { "UTC0", "UTC", "Etc/UTC", "Global" },
  { "EST5EDT,M3.2.0,M11.1.0", "Eastern (New York)", "America/New_York", "Americas" },
  { "CST6CDT,M3.2.0,M11.1.0", "Central (Chicago)", "America/Chicago", "Americas" },
  { "MST7MDT,M3.2.0,M11.1.0", "Mountain (Denver)", "America/Denver", "Americas" },
  { "PST8PDT,M3.2.0,M11.1.0", "Pacific (Los Angeles)", "America/Los_Angeles", "Americas" },
  { "AKST9AKDT,M3.2.0,M11.1.0", "Alaska (Anchorage)", "America/Anchorage", "Americas" },
  { "MST7", "Mountain (Phoenix, No DST)", "America/Phoenix", "Americas" },
  { "HST10", "Hawaii (Honolulu, No DST)", "Pacific/Honolulu", "Americas" },
  { "GMT0BST,M3.5.0/1,M10.5.0", "GMT/BST (London)", "Europe/London", "Europe/Africa" },
  { "CET-1CEST,M3.5.0,M10.5.0", "CET/CEST (Berlin)", "Europe/Berlin", "Europe/Africa" }
};
const int NUM_TIMEZONE_OPTIONS = sizeof(TZ_DATA) / sizeof(TZ_DATA[0]);

const char *NTP_SERVERS[] = { "pool.ntp.org", "time.google.com", "time.nist.gov" };
const int NUM_NTP_SERVERS = sizeof(NTP_SERVERS) / sizeof(NTP_SERVERS[0]);
int currentNtpServerIndex = 0;
WiFiUDP Udp;
unsigned long lastNtpRequestSent = 0;
unsigned long currentNtpInterval = NTP_RETRY_INTERVAL_MS;
bool timeSynchronized = false;
time_t lastNtpSyncTime = 0;
char currentNtpServerUsed[32] = "N/A";
bool ntpSyncRequested = false;
WiFiManager wifiManager;
AsyncWebServer server(80);
Preferences preferences;

// Animation and sleep state
bool isAnimating = false;
unsigned long animationStartTime = 0;
unsigned long lastAnimationFrameTime = 0;
unsigned long lastTimeTravelAnimationTime = 0;
enum AnimationPhase { ANIM_INACTIVE, ANIM_VOLUME_FADE_IN, ANIM_DIM_IN, ANIM_CAPACITOR_CHARGING, ANIM_PRE_FLICKER_88MPH, ANIM_FLICKER, ANIM_DIM_OUT, ANIM_VOLUME_FADE_OUT, ANIM_COMPLETE };
AnimationPhase currentPhase = ANIM_INACTIVE;
bool isDisplayAsleep = false;
byte initialBrightness = 0;
struct tm currentTimeInfo;
struct tm destinationTimeInfo;
int currentVolume = 0;
#define MDNS_HOSTNAME "timecircuits"
const char* const ANIMATION_STYLE_NAMES[] = { "Sequential Flicker", "Random Flicker", "All Displays Random", "Counting Up", "Wave Flicker", "Glitch Effect" };
// New enums and variables to manage the non-blocking boot sequence
enum BootSequenceState { BOOT_INACTIVE, BOOT_ANIMATION_START, BOOT_ANIMATION_WAIT, BOOT_88MPH_DISPLAY, BOOT_RECALIBRATING_DISPLAY, BOOT_RELAY_TEST_DISPLAY, BOOT_CAPACITOR_FULL_DISPLAY, BOOT_COMPLETE };
BootSequenceState bootState = BOOT_INACTIVE;
unsigned long bootStateStartTime = 0;
unsigned long ntpRequestSentTime = 0;
// New state for NTP sync animation
bool isNtpSyncAnimating = false;
unsigned long ntpSyncAnimStartTime = 0;

// NEW: Variables for Wind Speed and Glitch Effect
unsigned long lastWindSpeedFetch = 0;
float currentWindSpeed = 0.0;
unsigned long lastGlitchTime = 0;

// Helper function to display scrolling text
void displayScrollingText(DisplayRow &row, const char* text) {
  if (!ENABLE_HARDWARE || !ENABLE_I2C_HARDWARE) return;
  static int position = 0;
  static unsigned long lastScrollTime = 0;
  if (millis() - lastScrollTime > SCROLL_INTERVAL_MS) {
    lastScrollTime = millis();
    char buffer[5];
    strncpy(buffer, text + position, 4);
    buffer[4] = '\0';
    
    row.month.print(buffer);
    row.month.writeDisplay();
    
    position++;
    if (position >= strlen(text)) {
      position = 0;
    }
  }
}


// =================================================================
// == FUNCTION IMPLEMENTATIONS                                    ==
// =================================================================

// NEW: Function to fetch wind speed from Open-Meteo API
void fetchWindSpeed() {
  if (WiFi.status() != WL_CONNECTED) {
    ESP_LOGW("Weather", "Cannot fetch wind speed, WiFi not connected.");
    return;
  }
  
  HTTPClient http;
  String apiURL = "http://api.open-meteo.com/v1/forecast?latitude=" + String(currentSettings.latitude, 2) + "&longitude=" + String(currentSettings.longitude, 2) + "&current_weather=true";
  
  http.begin(apiURL);
  int httpCode = http.GET();

  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      
      if (doc.containsKey("current_weather")) {
        currentWindSpeed = doc["current_weather"]["windspeed"];
        ESP_LOGI("Weather", "Successfully fetched wind speed: %.2f km/h", currentWindSpeed);
      } else {
        ESP_LOGW("Weather", "API response did not contain current_weather object.");
      }
    } else {
      ESP_LOGW("Weather", "API request failed, error: %s", http.errorToString(httpCode).c_str());
    }
  } else {
    ESP_LOGE("Weather", "API request failed, error: %s", http.errorToString(httpCode).c_str());
  }
  http.end();
  lastWindSpeedFetch = millis();
}

void saveSettings() {
  ESP_LOGI("Settings", "Saving settings to preferences...");
  preferences.putBytes("settings", &currentSettings, sizeof(currentSettings));
  ESP_LOGI("Settings", "Settings saved.");
  setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
  tzset();
}

void loadSettings() {
  ESP_LOGI("Settings", "Loading settings from preferences...");
  size_t savedSize = preferences.getBytesLength("settings");
  if (savedSize != sizeof(currentSettings)) {
    ESP_LOGW("Settings", "Size mismatch. Loading default settings.");
    currentSettings = defaultSettings;
    saveSettings();
  } else {
    preferences.getBytes("settings", &currentSettings, sizeof(currentSettings));
    ESP_LOGI("Settings", "Settings loaded from preferences.");
  }
  setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
  tzset();
}

void animateDisplayRowRandomly(DisplayRow &row) {
    if (random(0, 100) < 90) animateMonthDisplay(row);
    if (random(0, 100) < 60) animateDayDisplay(row);
    if (random(0, 100) < 30) animateYearDisplay(row);
    if (random(0, 100) < 80) animateTimeDisplay(row);
    if (random(0, 100) < 70) animateAmPmDisplay(row);
}

void handleDisplayAnimation() {
  if (!isAnimating) return;
  
  static AnimationPhase lastPhase = ANIM_INACTIVE;
  if (currentPhase != lastPhase) {
    ESP_LOGI("Animation", "Transitioning to phase: %d", currentPhase);
    lastPhase = currentPhase;
  }
  unsigned long currentTime = millis();
  unsigned long elapsed = currentTime - animationStartTime;
  const unsigned long TOTAL_ANIMATION_DURATION = currentSettings.timeTravelAnimationDuration;
  const float VOLUME_FADE_IN_PERCENTAGE = 0.10;
  const float DIM_IN_PERCENTAGE = 0.10;
  const float CAPACITOR_CHARGING_PERCENTAGE = 0.20;
  const float PRE_FLICKER_88MPH_PERCENTAGE = 0.30;
  const float FLICKER_PERCENTAGE = 0.50;
  const float DIM_OUT_PERCENTAGE = 0.10;
  const float VOLUME_FADE_OUT_PERCENTAGE = 0.10;
  const unsigned long VOLUME_FADE_IN_DURATION = TOTAL_ANIMATION_DURATION * VOLUME_FADE_IN_PERCENTAGE;
  const unsigned long DIM_IN_DURATION = TOTAL_ANIMATION_DURATION * DIM_IN_PERCENTAGE;
  const unsigned long CAPACITOR_CHARGING_DURATION = TOTAL_ANIMATION_DURATION * CAPACITOR_CHARGING_PERCENTAGE;
  const unsigned long PRE_FLICKER_88MPH_DURATION = TOTAL_ANIMATION_DURATION * PRE_FLICKER_88MPH_PERCENTAGE;
  const unsigned long FLICKER_DURATION = TOTAL_ANIMATION_DURATION * FLICKER_PERCENTAGE;
  const unsigned long DIM_OUT_DURATION = TOTAL_ANIMATION_DURATION * DIM_OUT_PERCENTAGE;
  const unsigned long VOLUME_FADE_OUT_DURATION = TOTAL_ANIMATION_DURATION * VOLUME_FADE_OUT_PERCENTAGE;

  switch (currentPhase) {
    case ANIM_VOLUME_FADE_IN:
      if (currentSettings.timeTravelSoundToggle) {
        if (ENABLE_HARDWARE) myDFPlayer.volume(0);
        playSound(SOUND_TIME_TRAVEL);
      }
      currentPhase = ANIM_DIM_IN;
      animationStartTime = currentTime;
      // Intentionally fall-through to the next case to start the fade immediately
    case ANIM_DIM_IN: {
      if (elapsed < DIM_IN_DURATION) {
        byte currentBrightness = map(elapsed, 0, DIM_IN_DURATION, 0, initialBrightness);
        setDisplayBrightness(currentBrightness);
        if (currentSettings.timeTravelVolumeFade) {
          int newVolume = map(elapsed, 0, DIM_IN_DURATION, 0, currentSettings.notificationVolume);
          if (newVolume != currentVolume) {
            currentVolume = newVolume;
            if (ENABLE_HARDWARE) myDFPlayer.volume(currentVolume);
          }
        }
      } else {
        setDisplayBrightness(initialBrightness);
        blankAllDisplays();
        currentPhase = ANIM_CAPACITOR_CHARGING;
        animationStartTime = currentTime;
      }
      break;
    }
    case ANIM_CAPACITOR_CHARGING:
      {
        static const char* chargingText = "FLUX CAPACITOR CHARGING";
        static unsigned long textDisplayTime = 0;
        const unsigned long displayDuration = 3000;
        if (millis() - textDisplayTime < displayDuration) {
          // A simple text animation, maybe scrolling or flickering
          displayScrollingText(destRow, chargingText);
        } else {
          // Transition to the next phase after the text has been displayed for a while
          blankAllDisplays();
          if (currentSettings.timeTravelSoundToggle && !currentSettings.timeTravelVolumeFade) {
            playSound(SOUND_ACCELERATION);
          }
          currentPhase = ANIM_PRE_FLICKER_88MPH;
          animationStartTime = millis();
        }
      }
      break;
    case ANIM_PRE_FLICKER_88MPH: {
      if (elapsed < PRE_FLICKER_88MPH_DURATION) {
        float speed = 0.0;
        float progress = (float)elapsed / PRE_FLICKER_88MPH_DURATION;
        if (progress < 0.5) { 
          speed = map(elapsed, 0, PRE_FLICKER_88MPH_DURATION / 2, 0, 80);
        } else {
          speed = map(elapsed, PRE_FLICKER_88MPH_DURATION / 2, PRE_FLICKER_88MPH_DURATION, 80, 88);
        }
        
        if (ENABLE_HARDWARE && ENABLE_I2C_HARDWARE) {
          // Display "ACCELERATING" on the middle row as speed increases
          presRow.month.clear();
          presRow.day.clear();
          presRow.year.clear();
          presRow.time.clear();
          
          presRow.month.print("ACC");
          presRow.month.writeDisplay();
          presRow.day.print("EL");
          presRow.day.writeDisplay();
          presRow.year.print("ERAT");
          presRow.year.writeDisplay();
          presRow.time.print("ING");
          presRow.time.writeDisplay();
        }

        display88MphSpeed(speed);
        if (currentTime - lastAnimationFrameTime > 100) {
           lastAnimationFrameTime = currentTime;
        }
      } else {
        blankAllDisplays();
        if (currentSettings.timeTravelSoundToggle && !currentSettings.timeTravelVolumeFade) {
          playSound(SOUND_WARP_WHOOSH);
        }
        currentPhase = ANIM_FLICKER;
        animationStartTime = currentTime;
      }
      break;
    }
    case ANIM_FLICKER: {
      if (elapsed < FLICKER_DURATION) {
        if (currentTime - lastAnimationFrameTime > ANIMATION_UPDATE_INTERVAL_MS) {
          lastAnimationFrameTime = currentTime;
          switch (currentSettings.animationStyle) {
            case 0: { // Sequential Flicker (Default)
              unsigned long currentPhaseElapsed = currentTime - animationStartTime;
              int thirdPhaseDuration = FLICKER_DURATION / 3;
              if (currentPhaseElapsed < thirdPhaseDuration) {
                animateDisplayRowRandomly(destRow);
              } else if (currentPhaseElapsed < (thirdPhaseDuration * 2)) {
                animateDisplayRowRandomly(presRow);
              } else {
                animateDisplayRowRandomly(lastRow);
              }
              break;
            }
            case 1: { // Random Flicker with varied segment speed
              animateDisplayRowRandomly(destRow);
              animateDisplayRowRandomly(presRow);
              animateDisplayRowRandomly(lastRow);
              break;
            }
            case 2: { // All Displays Random Flicker
                animateDisplayRowRandomly(destRow);
                animateDisplayRowRandomly(presRow);
                animateDisplayRowRandomly(lastRow);
                break;
            }
            case 3: { // Counting Up
                static int counter = 0;
                static unsigned long lastCountUpdateTime = 0;
                if (currentTime - lastCountUpdateTime > COUNT_UPDATE_INTERVAL_MS) {
                    lastCountUpdateTime = currentTime;
                    counter = (counter + 1) % 10000;
                    if (!ENABLE_HARDWARE) {
                        ESP_LOGD("Display (Disabled)", "Counting Up animation skipped. Counter: %d", counter);
                    } else {
                        destRow.month.print("---");
                        destRow.month.writeDisplay();
                        destRow.day.print(random(0, 100)); destRow.day.writeDisplay();
                        destRow.year.print(random(0, 10000)); destRow.year.writeDisplay();
                        destRow.time.print(counter); destRow.time.writeDisplay();
                        presRow.month.print("---"); presRow.month.writeDisplay();
                        presRow.day.print(random(0, 100)); presRow.day.writeDisplay();
                        presRow.year.print(random(0, 10000)); presRow.year.writeDisplay();
                        presRow.time.print(counter); presRow.time.writeDisplay();
                        lastRow.month.print("---");
                        lastRow.month.writeDisplay();
                        lastRow.day.print(random(0, 100)); lastRow.day.writeDisplay();
                        lastRow.year.print(random(0, 10000)); lastRow.year.writeDisplay();
                        lastRow.time.print(counter); lastRow.time.writeDisplay();
                    }
                    animateAmPmDisplay(destRow);
                    animateAmPmDisplay(presRow);
                    animateAmPmDisplay(lastRow);
                }
                break;
            }
            case 4: { // Wave Flicker
              unsigned long currentPhaseElapsed = currentTime - animationStartTime;
              int phaseDuration = FLICKER_DURATION / 5; // 5 phases for a wave effect
              if (currentPhaseElapsed < phaseDuration) {
                animateDisplayRowRandomly(destRow);
              } else if (currentPhaseElapsed < (phaseDuration * 2)) {
                clearDisplayRow(destRow);
                animateDisplayRowRandomly(presRow);
              } else if (currentPhaseElapsed < (phaseDuration * 3)) {
                clearDisplayRow(presRow);
                animateDisplayRowRandomly(lastRow);
              } else if (currentPhaseElapsed < (phaseDuration * 4)) {
                clearDisplayRow(lastRow);
              }
              // Last phase is blank before dim out
              break;
            }
            default:
                // Default to sequential flicker
                unsigned long currentPhaseElapsed = currentTime - animationStartTime;
                int thirdPhaseDuration = FLICKER_DURATION / 3;
                if (currentPhaseElapsed < thirdPhaseDuration) {
                    animateDisplayRowRandomly(destRow);
                } else if (currentPhaseElapsed < (thirdPhaseDuration * 2)) {
                    animateDisplayRowRandomly(presRow);
                } else {
                    animateDisplayRowRandomly(lastRow);
                }
                break;
          }
        }
      } else {
        blankAllDisplays();
        currentPhase = ANIM_DIM_OUT;
        animationStartTime = currentTime;
      }
      break;
    }
    case ANIM_DIM_OUT: {
      if (elapsed < DIM_OUT_DURATION) {
        byte currentBrightness = map(elapsed, 0, DIM_OUT_DURATION, initialBrightness, 0);
        setDisplayBrightness(currentBrightness);
      } else {
        setDisplayBrightness(0);
        currentPhase = ANIM_VOLUME_FADE_OUT;
        animationStartTime = currentTime;
      }
      break;
    }
    case ANIM_VOLUME_FADE_OUT: {
      if (elapsed < VOLUME_FADE_OUT_DURATION) {
        if (currentSettings.timeTravelVolumeFade) {
          int newVolume = map(elapsed, 0, VOLUME_FADE_OUT_DURATION, currentSettings.notificationVolume, 0);
          if (newVolume != currentVolume) {
            currentVolume = newVolume;
            if (ENABLE_HARDWARE) myDFPlayer.volume(currentVolume);
          }
        }
      } else {
        if (currentSettings.timeTravelSoundToggle) {
          if (ENABLE_HARDWARE) myDFPlayer.volume(currentSettings.notificationVolume);
          playSound(SOUND_ARRIVAL_THUD);
        }
        currentPhase = ANIM_COMPLETE;
        animationStartTime = currentTime;
      }
      break;
    }
    case ANIM_COMPLETE:
      if (elapsed < 100) {
      } else {
        ESP_LOGI("Animation", "Animation complete.");
        isAnimating = false;
        currentPhase = ANIM_INACTIVE;
        isDisplayAsleep = false;
        setDisplayBrightness(currentSettings.brightness);
        if (currentSettings.timeTravelSoundToggle) {
          if (ENABLE_HARDWARE) myDFPlayer.volume(currentSettings.notificationVolume);
        }
      }
      break;
    case ANIM_INACTIVE:
      break;
  }
}

// NEW: Function to handle the occasional glitch effect
void handleGlitchEffect() {
  if (isAnimating || isDisplayAsleep || currentSettings.animationStyle != 5) return;

  if (millis() - lastGlitchTime > GLITCH_EFFECT_INTERVAL_MS) {
    if (random(0, 100) < 25) { // 25% chance of a glitch every minute
      ESP_LOGD("Glitch", "Triggering glitch effect!");
      
      // Select a random row and display to glitch
      int rowNum = random(0, 3);
      int displayNum = random(0, 4);

      DisplayRow* rowToGlitch = (rowNum == 0) ? &destRow : (rowNum == 1) ? &presRow : &lastRow;
      Adafruit_7segment* displayToGlitch;

      switch(displayNum) {
        case 0: displayToGlitch = &(rowToGlitch->month); break;
        case 1: displayToGlitch = &(rowToGlitch->day); break;
        case 2: displayToGlitch = &(rowToGlitch->year); break;
        default: displayToGlitch = &(rowToGlitch->time); break;
      }

      if (ENABLE_HARDWARE && ENABLE_I2C_HARDWARE) {
        displayToGlitch->clear();
        // Print some random garbage
        for (int i=0; i<4; i++) {
          displayToGlitch->writeDigitRaw(i, random(0, 255));
        }
        displayToGlitch->writeDisplay();

        // After a short delay, restore the display
        delay(75);
        // The main updateNormalClockDisplay function will restore it on its next run
      }
    }
    lastGlitchTime = millis();
  }
}

void updateNormalClockDisplay() {
  static unsigned long lastDisplayUpdate = 0;
  static int lastPresentTimezoneIndex = -1;
  static int lastDestinationTimezoneIndex = -1;
  static bool lastDisplayFormat24h = false;
  static int lastLastTimeDepartedHour = -1, lastLastTimeDepartedMinute = -1, lastLastTimeDepartedYear = -1, lastLastTimeDepartedMonth = -1, lastLastTimeDepartedDay = -1;
  
  if (currentSettings.presentTimezoneIndex != lastPresentTimezoneIndex) {
    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
    tzset();
    lastPresentTimezoneIndex = currentSettings.presentTimezoneIndex;
    ESP_LOGD("Time", "Present Timezone changed to %s", TZ_DATA[currentSettings.presentTimezoneIndex].displayName);
  }
  
  if (isDisplayAsleep) {
    if (millis() - lastDisplayUpdate > 1000) {
      lastDisplayUpdate = millis();
      clearDisplayRow(destRow);
      clearDisplayRow(presRow);
      clearDisplayRow(lastRow);
    }
    return;
  }
  
  bool presentTimeNeedsUpdate = (millis() - lastDisplayUpdate > 1000) ||
                                (currentSettings.displayFormat24h != lastDisplayFormat24h);
  bool destinationTimeNeedsUpdate = (currentSettings.destinationTimezoneIndex != lastDestinationTimezoneIndex) || presentTimeNeedsUpdate;
  bool lastDepartedNeedsUpdate = (!currentSettings.windSpeedModeEnabled && (
                                  currentSettings.lastTimeDepartedHour != lastLastTimeDepartedHour ||
                                  currentSettings.lastTimeDepartedMinute != lastLastTimeDepartedMinute ||
                                  currentSettings.lastTimeDepartedYear != lastLastTimeDepartedYear ||
                                  currentSettings.lastTimeDepartedMonth != lastLastTimeDepartedMonth ||
                                  currentSettings.lastTimeDepartedDay != lastLastTimeDepartedDay ||
                                  presentTimeNeedsUpdate));

  if (timeSynchronized && (presentTimeNeedsUpdate || destinationTimeNeedsUpdate || lastDepartedNeedsUpdate || currentSettings.windSpeedModeEnabled)) {
    lastDisplayUpdate = millis();
    lastDisplayFormat24h = currentSettings.displayFormat24h;
    time_t now;
    time(&now);
    
    if (presentTimeNeedsUpdate) {
      localtime_r(&now, &currentTimeInfo);
      updateDisplayRow(presRow, currentTimeInfo, currentTimeInfo.tm_year + 1900);
    }
    
    if (destinationTimeNeedsUpdate) {
      setenv("TZ", TZ_DATA[currentSettings.destinationTimezoneIndex].tzString, 1);
      tzset();
      localtime_r(&now, &destinationTimeInfo);
      updateDisplayRow(destRow, destinationTimeInfo, currentSettings.destinationYear);
      lastDestinationTimezoneIndex = currentSettings.destinationTimezoneIndex;
      setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
      tzset();
    }

    if (currentSettings.windSpeedModeEnabled) {
      displayWindSpeed(currentWindSpeed);
      // Clear the saved last departed time to force an update when switching back
      lastLastTimeDepartedHour = -1; 
    } else if (lastDepartedNeedsUpdate) {
      struct tm lastTimeDepartedInfo = { 0, currentSettings.lastTimeDepartedMinute, currentSettings.lastTimeDepartedHour, currentSettings.lastTimeDepartedDay, currentSettings.lastTimeDepartedMonth - 1, currentSettings.lastTimeDepartedYear - 1900 };
      updateDisplayRow(lastRow, lastTimeDepartedInfo, currentSettings.lastTimeDepartedYear);
      lastLastTimeDepartedHour = currentSettings.lastTimeDepartedHour;
      lastLastTimeDepartedMinute = currentSettings.lastTimeDepartedMinute;
      lastLastTimeDepartedYear = currentSettings.lastTimeDepartedYear;
      lastLastTimeDepartedMonth = currentSettings.lastTimeDepartedMonth;
      lastLastTimeDepartedDay = currentSettings.lastTimeDepartedDay;
    }
  } else if (!timeSynchronized) {
    if (millis() - lastDisplayUpdate > 1000) {
      lastDisplayUpdate = millis();
      clearDisplayRow(destRow);
      clearDisplayRow(presRow);
      clearDisplayRow(lastRow);
      if (ENABLE_I2C_HARDWARE) {
        destRow.month.print("NTP");
        destRow.month.writeDisplay();
        presRow.month.print("ERR");
        presRow.month.writeDisplay();
      }
    }
  }
}

void showNtpSyncAnimation() {
  if (!ENABLE_HARDWARE || !ENABLE_I2C_HARDWARE) return;
  isNtpSyncAnimating = true;
  ntpSyncAnimStartTime = millis();
  blankAllDisplays();
  
  // Display "SYNCING" on the present time row
  presRow.month.print("SYNC");
  presRow.month.writeDisplay();
  presRow.day.print("ING");
  presRow.day.writeDisplay();
  ESP_LOGI("NTP", "Displaying NTP sync animation.");
}

void startTimeTravelAnimation() {
  if (isAnimating) return;
  isAnimating = true;
  animationStartTime = millis();
  lastAnimationFrameTime = millis();
  initialBrightness = currentSettings.brightness;
  currentPhase = ANIM_VOLUME_FADE_IN;
  ESP_LOGI("Animation", "Starting physical time travel animation.");
  blankAllDisplays();
  lastTimeTravelAnimationTime = millis();
  // Reset the timer for the automatic animation
}

void sendNTPrequest() {
  IPAddress ntpServerIP;
  const char *serverToUse = NTP_SERVERS[currentNtpServerIndex];
  if (WiFi.hostByName(serverToUse, ntpServerIP)) {
    byte packetBuffer[48];
    memset(packetBuffer, 0, 48);
    packetBuffer[0] = 0b11100011;
    Udp.beginPacket(ntpServerIP, 123);
    Udp.write(packetBuffer, 48);
    Udp.endPacket();
    ntpRequestSentTime = millis();
    ESP_LOGI("NTP", "NTP request sent to %s", serverToUse);
    showNtpSyncAnimation();
    // Show animation when request is sent
  } else {
    ESP_LOGW("NTP", "Failed to resolve NTP server: %s", serverToUse);
  }
}

void handleSleepSchedule() {
  if (!timeSynchronized) return;
  time_t now_t;
  time(&now_t);
  struct tm *now_tm = localtime(&now_t);
  int now_minutes = now_tm->tm_hour * 60 + now_tm->tm_min;
  int sleep_minutes = currentSettings.departureHour * 60 + currentSettings.departureMinute;
  int wake_minutes = currentSettings.arrivalHour * 60 + currentSettings.arrivalMinute;
  
  bool shouldBeAsleep = false;
  if (sleep_minutes < wake_minutes) { // Same-day sleep period (e.g., sleep at 08:00, wake at 17:00)
    shouldBeAsleep = (now_minutes >= sleep_minutes && now_minutes < wake_minutes);
  } else { // Overnight sleep period (e.g., sleep at 22:00, wake at 07:00)
    shouldBeAsleep = (now_minutes >= sleep_minutes || now_minutes < wake_minutes);
  }
  
  if (shouldBeAsleep && !isDisplayAsleep) {
    isDisplayAsleep = true;
    playSound(SOUND_SLEEP_ON);
    ESP_LOGI("Sleep", "Entering sleep mode.");
  } else if (!shouldBeAsleep && isDisplayAsleep) {
    isDisplayAsleep = false;
    playSound(SOUND_CONFIRM_ON);
    ESP_LOGI("Sleep", "Exiting sleep mode.");
  }
}

void setupWebRoutes() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      request->send(404, "text/plain", "index.html not found. Please upload files to LittleFS.");
    }
  });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/style.css", "text/css");
  });
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/script.js", "application/javascript");
  });
  server.on("/api/time", HTTP_GET, [](AsyncWebServerRequest *request) {
    time_t now;
    time(&now);
    StaticJsonDocument<256> doc;
    doc["unixTime"] = now;
    doc["timeSynchronized"] = timeSynchronized;
    if (lastNtpSyncTime != 0) {
      struct tm *lastSyncTm = localtime(&lastNtpSyncTime);
      char buf[11];
      strftime(buf, sizeof(buf), "%m/%d/%Y", lastSyncTm);
      doc["lastSyncTime"] = buf;
    } else {
      doc["lastSyncTime"] = "Never";
    }
    doc["lastNtpServer"] = currentNtpServerUsed;
    
    String jsonString;
    serializeJson(doc, jsonString);
    request->send(200, "application/json", jsonString);
  });
  server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<1024> doc;
    doc["destinationYear"] = currentSettings.destinationYear;
    doc["destinationTimezoneIndex"] = currentSettings.destinationTimezoneIndex;
    doc["departureHour"] = currentSettings.departureHour;
    doc["departureMinute"] = currentSettings.departureMinute;
    doc["arrivalHour"] = currentSettings.arrivalHour;
    doc["arrivalMinute"] = currentSettings.arrivalMinute;
    doc["lastTimeDepartedHour"] = currentSettings.lastTimeDepartedHour;
    doc["lastTimeDepartedMinute"] = currentSettings.lastTimeDepartedMinute;
    doc["lastTimeDepartedYear"] = currentSettings.lastTimeDepartedYear;
    doc["lastTimeDepartedMonth"] = currentSettings.lastTimeDepartedMonth;
    doc["lastTimeDepartedDay"] = currentSettings.lastTimeDepartedDay;
    doc["brightness"] = currentSettings.brightness;
    doc["notificationVolume"] = currentSettings.notificationVolume;
    doc["timeTravelSoundToggle"] = currentSettings.timeTravelSoundToggle;
    doc["timeTravelAnimationInterval"] = currentSettings.timeTravelAnimationInterval;
    doc["presetCycleInterval"] = currentSettings.presetCycleInterval;
    doc["displayFormat24h"] = currentSettings.displayFormat24h;
    doc["theme"] = currentSettings.theme;
    doc["presentTimezoneIndex"] = currentSettings.presentTimezoneIndex;
    doc["timeTravelAnimationDuration"] = currentSettings.timeTravelAnimationDuration;
    doc["animationStyle"] = currentSettings.animationStyle;
    doc["timeTravelVolumeFade"] = currentSettings.timeTravelVolumeFade;
    doc["windSpeedModeEnabled"] = currentSettings.windSpeedModeEnabled;
    doc["latitude"] = currentSettings.latitude;
    doc["longitude"] = currentSettings.longitude;
    String jsonString;
    serializeJson(doc, jsonString);
    request->send(200, "application/json", jsonString);
  });
  server.on("/api/saveSettings", HTTP_POST, [](AsyncWebServerRequest *request){
    ESP_LOGI("WebUI", "Received Settings from UI");
    if (request->hasParam("destinationYear", true)) { 
        currentSettings.destinationYear = request->getParam("destinationYear", true)->value().toInt(); 
    }
    if (request->hasParam("destinationTimezoneIndex", true)) { 
        currentSettings.destinationTimezoneIndex = request->getParam("destinationTimezoneIndex", true)->value().toInt(); 
    }
    if (request->hasParam("departureHour", true)) { 
        currentSettings.departureHour = request->getParam("departureHour", true)->value().toInt(); 
    }
    if (request->hasParam("departureMinute", true)) { 
        currentSettings.departureMinute = request->getParam("departureMinute", true)->value().toInt();
    }
    if (request->hasParam("arrivalHour", true)) { 
        currentSettings.arrivalHour = request->getParam("arrivalHour", true)->value().toInt();
    }
    if (request->hasParam("arrivalMinute", true)) { 
        currentSettings.arrivalMinute = request->getParam("arrivalMinute", true)->value().toInt();
    }
    if (request->hasParam("lastTimeDepartedHour", true)) { 
        currentSettings.lastTimeDepartedHour = request->getParam("lastTimeDepartedHour", true)->value().toInt();
    }
    if (request->hasParam("lastTimeDepartedMinute", true)) { 
        currentSettings.lastTimeDepartedMinute = request->getParam("lastTimeDepartedMinute", true)->value().toInt();
    }
    if (request->hasParam("lastTimeDepartedYear", true)) { 
        currentSettings.lastTimeDepartedYear = request->getParam("lastTimeDepartedYear", true)->value().toInt();
    }
    if (request->hasParam("lastTimeDepartedMonth", true)) { 
        currentSettings.lastTimeDepartedMonth = request->getParam("lastTimeDepartedMonth", true)->value().toInt();
    }
    if (request->hasParam("lastTimeDepartedDay", true)) { 
        currentSettings.lastTimeDepartedDay = request->getParam("lastTimeDepartedDay", true)->value().toInt();
    }
    if (request->hasParam("brightness", true)) { 
        currentSettings.brightness = request->getParam("brightness", true)->value().toInt();
        setDisplayBrightness(currentSettings.brightness);
    }
    if (request->hasParam("notificationVolume", true)) { 
        currentSettings.notificationVolume = request->getParam("notificationVolume", true)->value().toInt();
        if (ENABLE_HARDWARE) myDFPlayer.volume(currentSettings.notificationVolume);
    }
    if (request->hasParam("timeTravelAnimationInterval", true)) { 
        currentSettings.timeTravelAnimationInterval = request->getParam("timeTravelAnimationInterval", true)->value().toInt();
    }
    if (request->hasParam("presetCycleInterval", true)) { 
        currentSettings.presetCycleInterval = request->getParam("presetCycleInterval", true)->value().toInt();
    }
    if (request->hasParam("theme", true)) { 
        currentSettings.theme = request->getParam("theme", true)->value().toInt();
    }
    if (request->hasParam("presentTimezoneIndex", true)) { 
        currentSettings.presentTimezoneIndex = request->getParam("presentTimezoneIndex", true)->value().toInt();
    }
    if (request->hasParam("timeTravelAnimationDuration", true)) { 
        currentSettings.timeTravelAnimationDuration = request->getParam("timeTravelAnimationDuration", true)->value().toInt();
    }
    if (request->hasParam("animationStyle", true)) { 
        currentSettings.animationStyle = request->getParam("animationStyle", true)->value().toInt();
    }
    if (request->hasParam("timeTravelSoundToggle", true)) { 
        currentSettings.timeTravelSoundToggle = (request->getParam("timeTravelSoundToggle", true)->value() == "true");
    }
    if (request->hasParam("displayFormat24h", true)) { 
        currentSettings.displayFormat24h = (request->getParam("displayFormat24h", true)->value() == "true");
    }
    if (request->hasParam("timeTravelVolumeFade", true)) {
        currentSettings.timeTravelVolumeFade = (request->getParam("timeTravelVolumeFade", true)->value() == "true");
    }
    if (request->hasParam("windSpeedModeEnabled", true)) {
        currentSettings.windSpeedModeEnabled = (request->getParam("windSpeedModeEnabled", true)->value() == "true");
        if (currentSettings.windSpeedModeEnabled) {
          fetchWindSpeed(); // Fetch immediately when enabled
        }
    }
    if (request->hasParam("latitude", true)) {
        currentSettings.latitude = request->getParam("latitude", true)->value().toFloat();
    }
    if (request->hasParam("longitude", true)) {
        currentSettings.longitude = request->getParam("longitude", true)->value().toFloat();
    }
    saveSettings();
    playSound(SOUND_CONFIRM_ON);
    request->send(200, "text/plain", "Settings Saved!");
  });
  server.on("/api/resetWifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    wifiManager.resetSettings();
    request->send(200, "text/plain", "WiFi Reset. Restarting...");
    delay(2000);
    ESP.restart();
  });
  server.on("/api/syncNtp", HTTP_POST, [](AsyncWebServerRequest *request) {
    ntpSyncRequested = true;
    request->send(200, "text/plain", "NTP Sync Requested!");
  });
  server.on("/api/timeTravel", HTTP_GET, [](AsyncWebServerRequest *request) {
    startTimeTravelAnimation();
    request->send(200, "text/plain", "Time Travel Sequence Initiated!");
  });
  server.on("/api/timezones", HTTP_GET, [](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(4096);
    JsonObject root = doc.to<JsonObject>();
    for (int i = 0; i < NUM_TIMEZONE_OPTIONS; i++) {
      const char *country = TZ_DATA[i].country;
      if (!root.containsKey(country)) {
        root.createNestedArray(country);
      }
      JsonObject obj = root[country].createNestedObject();
      obj["value"] = i;
      obj["text"] = TZ_DATA[i].displayName;
      obj["ianaTzName"] = TZ_DATA[i].ianaTzName;
    }
  
    String jsonString;
    serializeJson(doc, jsonString);
    request->send(200, "application/json", jsonString);
  });
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<128> doc;
    doc["rssi"] = WiFi.RSSI();
    doc["ssid"] = WiFi.SSID();
    String jsonString;
    serializeJson(doc, jsonString);
    request->send(200, "application/json", jsonString);
  });
  server.on("/api/testSound", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (currentSettings.timeTravelSoundToggle) {
      playSound(SOUND_CONFIRM_ON);
    }
    request->send(200, "text/plain", "Sound test initiated.");
  });
  server.on("/api/timeTravelSound", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (currentSettings.timeTravelSoundToggle) {
      playSound(SOUND_TIME_TRAVEL);
    }
    request->send(200, "text/plain", "Time travel sound initiated.");
  });
  server.on("/api/greatScott", HTTP_POST, [](AsyncWebServerRequest *request) {
    playSound(SOUND_EASTER_EGG);
    request->send(200, "text/plain", "Great Scott!");
  });
  server.on("/api/resetSettings", HTTP_POST, [](AsyncWebServerRequest *request) {
    preferences.remove("customPresets");
    currentSettings = defaultSettings;
    saveSettings();
    request->send(200, "text/plain", "Settings have been reset to default.");
  });
  server.on("/api/previewSetting", HTTP_GET, [](AsyncWebServerRequest *request) {
    ESP_LOGI("WebUI", "Live preview request received");
    if (request->hasParam("setting") && request->hasParam("value")) {
      String setting = request->getParam("setting")->value();
      String value = request->getParam("value")->value();
      if (setting == "brightness") {
        int brightness = value.toInt();
        if (brightness >= 0 && brightness <= 7) {
          setDisplayBrightness(brightness);
        } else {
          request->send(400, "text/plain", "Invalid brightness value.");
          return;
        }
      } else if (setting == "notificationVolume") {
        int volume = value.toInt();
        if (volume >= 0 && volume <= 30) {
          if (ENABLE_HARDWARE) myDFPlayer.volume(volume);
        } else {
            request->send(400, "text/plain", "Invalid volume value.");
            return;
        }
      } else if (setting == "displayFormat24h") {
          currentSettings.displayFormat24h = (value == "true");
      } else if (setting == "animationStyle") {
          int style = value.toInt();
          if (style >=0 && style <=5) {
            currentSettings.animationStyle = style;
          } else {
            request->send(400, "text/plain", "Invalid animation style.");
            return;
          }
      } else if (setting == "timeTravelAnimationDuration") {
          int duration = value.toInt();
          if (duration >=1000 && duration <= 10000) {
            currentSettings.timeTravelAnimationDuration = duration;
            playSound(SOUND_CONFIRM_ON); // Audible feedback for this change
          } else {
            request->send(400, "text/plain", "Invalid animation duration.");
            return;
          }
      } else if (setting == "destinationYear") {
          currentSettings.destinationYear = value.toInt();
      } else if (setting == "destinationTimezoneIndex") {
          currentSettings.destinationTimezoneIndex = value.toInt();
      } else if (setting == "presentTimezoneIndex") {
          currentSettings.presentTimezoneIndex = value.toInt();
      } else if (setting == "timeTravelVolumeFade") {
        currentSettings.timeTravelVolumeFade = (value == "true");
      } else if (setting == "presetCycleInterval") {
          int interval = value.toInt();
          if (interval >= 0 && interval <= 60) {
              currentSettings.presetCycleInterval = interval;
              playSound(SOUND_CONFIRM_ON); // Provide audible feedback
          } else {
              request->send(400, "text/plain", "Invalid preset cycle interval.");
              return;
          }
      } else if (setting == "timeTravelAnimationInterval") {
          int interval = value.toInt();
          if (interval >= 0 && interval <= 120) {
              currentSettings.timeTravelAnimationInterval = interval;
              playSound(SOUND_CONFIRM_ON); // Provide audible feedback
          } else {
              request->send(400, "text/plain", "Invalid animation interval.");
              return;
          }
      } else {
        request->send(400, "text/plain", "Unknown setting.");
        return;
      }
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing parameters.");
    }
  });
  server.on("/api/getPresets", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", preferences.getString("customPresets", "[]"));
  });
  server.on("/api/addPreset", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("name", true) && request->hasParam("value", true)) {
      String name = request->getParam("name", true)->value();
      if (name.length() == 0) {
        request->send(400, "text/plain", "Preset name cannot be empty.");
        return;
      }
      String value = request->getParam("value", true)->value();
      String presetsJson = preferences.getString("customPresets", "[]");
      DynamicJsonDocument doc(2048);
      deserializeJson(doc, presetsJson);
    
      JsonArray array = doc.as<JsonArray>();
      for (JsonObject existingPreset : array) {
        if (existingPreset["value"].as<String>() == value) {
          request->send(400, "text/plain", "A preset with this date/time already exists.");
          return;
        }
      }
      JsonObject newPreset = array.createNestedObject();
      newPreset["name"] = name;
      newPreset["value"] = value;
 
      String newPresetsJson;
      serializeJson(doc, newPresetsJson);
      preferences.putString("customPresets", newPresetsJson);
      request->send(200, "text/plain", "Preset saved!");
    } else {
      request->send(400, "text/plain", "Missing name or value.");
    }
  });
  server.on("/api/updatePreset", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("originalValue", true) && request->hasParam("newName", true) && request->hasParam("newValue", true)) {
      String originalValue = request->getParam("originalValue", true)->value();
      String newName = request->getParam("newName", true)->value();
      String newValue = request->getParam("newValue", true)->value();
      String presetsJson = preferences.getString("customPresets", "[]");
      DynamicJsonDocument doc(2048);
      deserializeJson(doc, presetsJson);
      JsonArray array = doc.as<JsonArray>();
      bool updated = false;
      for (JsonObject preset : array) {
        if (preset["value"].as<String>() == originalValue) {
          preset["name"] = newName;
          preset["value"] = newValue;
          updated = true;
          break;
        }
        if (preset["value"].as<String>() == newValue && preset["value"].as<String>() != originalValue) {
          request->send(400, "text/plain", "Preset with this value already exists.");
          return;
        }
      }
      if (updated) {
        String newPresetsJson;
        serializeJson(doc, newPresetsJson);
        preferences.putString("customPresets", newPresetsJson);
        request->send(200, "text/plain", "Preset updated!");
      } else {
        request->send(404, "text/plain", "Preset to update not found.");
      }
    } else {
      request->send(400, "text/plain", "Missing parameters for update.");
    }
  });
  server.on("/api/clearPresets", HTTP_POST, [](AsyncWebServerRequest *request) {
    preferences.remove("customPresets");
    request->send(200, "text/plain", "All custom presets cleared!");
  });
  server.on("/api/deletePreset", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("value", true)) {
      String valueToDelete = request->getParam("value", true)->value();
      String presetsJson = preferences.getString("customPresets", "[]");
      DynamicJsonDocument doc(2048);
      deserializeJson(doc, presetsJson);
      JsonArray oldArray = doc.as<JsonArray>();
      DynamicJsonDocument newDoc(2048);
      JsonArray newArray = newDoc.to<JsonArray>();
      for (JsonObject preset : oldArray) {
        if (preset["value"].as<String>() != valueToDelete) {
          newArray.add(preset);
        }
      }
      String newPresetsJson;
      serializeJson(newDoc, newPresetsJson);
      preferences.putString("customPresets", newPresetsJson);
      request->send(200, "text/plain", "Preset deleted!");
    } else {
      request->send(400, "text/plain", "Missing preset value.");
    }
  });
  server.on("/api/setLastDeparted", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("value", true)) {
      String value = request->getParam("value", true)->value();
      sscanf(value.c_str(), "%d-%d-%d-%d-%d",
             &currentSettings.lastTimeDepartedYear,
             &currentSettings.lastTimeDepartedMonth,
             &currentSettings.lastTimeDepartedDay,
             &currentSettings.lastTimeDepartedHour,
             &currentSettings.lastTimeDepartedMinute);
    }
    request->send(200, "text/plain", "OK");
  });
  server.on("/api/clearPreferences", HTTP_POST, [](AsyncWebServerRequest *request) {
    ESP_LOGI("WebUI", "Clearing all preferences...");
    preferences.clear();
    request->send(200, "text/plain", "Preferences cleared!");
  });
}

// New function to run a post-boot display sequence
void runBootSequence() {
  ESP_LOGI("Boot", "Starting non-blocking boot sequence.");
  bootState = BOOT_ANIMATION_START;
  bootStateStartTime = millis();
}

void handleBootSequence() {
  if (bootState == BOOT_INACTIVE || bootState == BOOT_COMPLETE) {
    return;
  }

  unsigned long currentTime = millis();
  static unsigned long lastUpdate = 0;
  switch (bootState) {
    case BOOT_ANIMATION_START:
      blankAllDisplays();
      // Ensure displays are blank at the start of the sequence
      if (ENABLE_HARDWARE) {
        startTimeTravelAnimation();
      }
      bootState = BOOT_ANIMATION_WAIT;
      bootStateStartTime = currentTime;
      break;
    case BOOT_ANIMATION_WAIT:
      if (!isAnimating && (currentTime - bootStateStartTime > currentSettings.timeTravelAnimationDuration + BOOT_ANIMATION_WAIT_MS)) { // Add a small buffer after the animation
        bootState = BOOT_88MPH_DISPLAY;
        bootStateStartTime = currentTime;
      }
      break;
    case BOOT_88MPH_DISPLAY:
      if (ENABLE_HARDWARE) {
        static int speed = 0;
        if (currentTime - lastUpdate > 50 && speed <= 88) { // Update every 50ms
            display88MphSpeed(speed);
            speed++;
            lastUpdate = currentTime;
        }
        if (speed > 88 && (currentTime - bootStateStartTime > 1000)) { // Pause for 1 second when 88 is reached
            bootState = BOOT_RECALIBRATING_DISPLAY;
            bootStateStartTime = currentTime;
        }
      } else { // Skip this state if hardware is disabled
        if (currentTime - bootStateStartTime > 1000) {
            bootState = BOOT_RECALIBRATING_DISPLAY;
            bootStateStartTime = currentTime;
        }
      }
      break;
    case BOOT_RECALIBRATING_DISPLAY:
      if (ENABLE_HARDWARE) {
        destRow.month.print("REC"); destRow.month.writeDisplay();
        destRow.day.print("AL");
        destRow.day.writeDisplay();
        destRow.year.print("IBRA"); destRow.year.writeDisplay();
        destRow.time.print("TING"); destRow.time.writeDisplay();
      }
      if (currentTime - bootStateStartTime > BOOT_STATE_CHANGE_INTERVAL_MS) {
        bootState = BOOT_RELAY_TEST_DISPLAY;
        bootStateStartTime = currentTime;
      }
      break;
    case BOOT_RELAY_TEST_DISPLAY:
      if (ENABLE_HARDWARE) {
        presRow.month.print("REL"); presRow.month.writeDisplay();
        presRow.day.print("AY");
        presRow.day.writeDisplay();
        presRow.year.print("SELF"); presRow.year.writeDisplay();
        presRow.time.print("TEST"); presRow.time.writeDisplay();
      }
      if (currentTime - bootStateStartTime > BOOT_STATE_CHANGE_INTERVAL_MS) {
        bootState = BOOT_CAPACITOR_FULL_DISPLAY;
        bootStateStartTime = currentTime;
      }
      break;
    case BOOT_CAPACITOR_FULL_DISPLAY:
      if (ENABLE_HARDWARE) {
        lastRow.month.print("CAP"); lastRow.month.writeDisplay();
        lastRow.day.print("AC");
        lastRow.day.writeDisplay();
        lastRow.year.print("ITOR"); lastRow.year.writeDisplay();
        lastRow.time.print("FULL"); lastRow.time.writeDisplay();
      }
      if (currentTime - bootStateStartTime > BOOT_STATE_CHANGE_INTERVAL_MS) {
        bootState = BOOT_COMPLETE;
        blankAllDisplays();
        ESP_LOGI("Boot", "Boot sequence complete.");
      }
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_DEBUG);
  randomSeed(analogRead(0));
  ESP_LOGI("Boot", "--- BOOTING UP ---");
  pinMode(LED_BUILTIN, OUTPUT);
  if (!LittleFS.begin(true)) {
    ESP_LOGE("FS", "CRITICAL ERROR: An Error has occurred while mounting LittleFS.");
    while(1) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
    } // Halt
  }
  ESP_LOGI("FS", "LittleFS Mounted Successfully.");
  ESP_LOGD("FS", "Listing Files in LittleFS:");
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  int fileCount = 0;
  while (file) {
    ESP_LOGD("FS", "  FILE: %s	SIZE: %d", file.name(), file.size());
    file = root.openNextFile();
    fileCount++;
  }
  if (fileCount == 0) {
    ESP_LOGW("FS", "No files found in LittleFS. Did you upload the data folder?");
  }
  ESP_LOGD("FS", "---------------------------------");
  preferences.begin("bttf-clock", false);
  loadSettings();
  ESP_LOGI("Sound", "Initializing DFPlayer... (May take 3-5 seconds)");
  if (ENABLE_HARDWARE && !myDFPlayer.begin(dfpSerial, true, false)) {
    ESP_LOGE("Sound", "Unable to begin DFPlayer");
  } else if (ENABLE_HARDWARE) {
    ESP_LOGI("Sound", "DFPlayer Mini online.");
    myDFPlayer.volume(currentSettings.notificationVolume);
    setupSoundFiles();
  } else {
    ESP_LOGI("Sound (Disabled)", "DFPlayer Mini initialization skipped.");
  }
  wifiManager.autoConnect(MDNS_HOSTNAME);
  ESP_LOGI("WiFi", "WiFi connected! IP: %s", WiFi.localIP().toString().c_str());
  ArduinoOTA.begin();
  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    ESP_LOGI("mDNS", "mDNS started: http://%s.local/", MDNS_HOSTNAME);
  }

  // ***** FIX: INITIALIZE HARDWARE *BEFORE* USING IT *****
  setupPhysicalDisplay();

  Udp.begin(123);
  sendNTPrequest();
  setupWebRoutes();
  server.begin();
  ESP_LOGI("Web", "HTTP server started");
  
  // Run the post-boot animation sequence
  runBootSequence();
}

void loop() {
  ArduinoOTA.handle();
  if (bootState != BOOT_COMPLETE) {
    handleBootSequence();
  }

  // Handle continuous animation logic
  if (isAnimating) {
    handleDisplayAnimation();
  }

  if (bootState == BOOT_COMPLETE) {
    // Handle automatic time travel animation trigger
    if (currentSettings.timeTravelAnimationInterval > 0 && !isAnimating) {
        unsigned long intervalMillis = (unsigned long)currentSettings.timeTravelAnimationInterval * 60 * 1000;
        if (millis() - lastTimeTravelAnimationTime >= intervalMillis) {
            startTimeTravelAnimation();
        }
    }
    
    if (currentSettings.animationStyle == 5) {
      handleGlitchEffect();
    }

    if (currentSettings.windSpeedModeEnabled && (millis() - lastWindSpeedFetch >= WIND_SPEED_FETCH_INTERVAL_MS)) {
      fetchWindSpeed();
    }

    static unsigned long lastOneSecondUpdate = 0;
    if (millis() - lastOneSecondUpdate >= 1000) {
      lastOneSecondUpdate = millis();
      // Check Wi-Fi connection and reconnect if needed
      static unsigned long lastWifiCheck = 0;
      if (millis() - lastWifiCheck > WIFI_CHECK_INTERVAL_MS) {
        lastWifiCheck = millis();
        if (WiFi.status() != WL_CONNECTED) {
          ESP_LOGW("WiFi", "WiFi Disconnected. Attempting to reconnect...");
          WiFi.reconnect();
        }
      }

      // Check for NTP response if a request was sent
      if (ntpRequestSentTime > 0 && millis() - ntpRequestSentTime < NTP_TIMEOUT_MS) {
        int packetSize = Udp.parsePacket();
        if (packetSize >= 48) {
          byte packetBuffer[48];
          Udp.read(packetBuffer, 48);
          unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
          unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
          time_t epoch = (highWord << 16 | lowWord) - 2208988800UL;
          struct timeval tv;
          tv.tv_sec = epoch;
          tv.tv_usec = 0;
          settimeofday(&tv, NULL);
          ESP_LOGI("NTP", "NTP Time Sync Successful!");
          timeSynchronized = true;
          lastNtpSyncTime = epoch;
          currentNtpInterval = NTP_SUCCESS_INTERVAL_MS;
          ntpRequestSentTime = 0;
          // Reset the request time
        }
      } else if (ntpRequestSentTime > 0) {
        // Timeout
        timeSynchronized = false;
        currentNtpServerIndex = (currentNtpServerIndex + 1) % NUM_NTP_SERVERS;
        currentNtpInterval = NTP_RETRY_INTERVAL_MS;
        ESP_LOGW("NTP", "Sync failed. Retrying with %s in 30s.", NTP_SERVERS[currentNtpServerIndex]);
        ntpRequestSentTime = 0;
      }
      
      // Check if it's time to send a new NTP sync request
      if ( (WiFi.status() == WL_CONNECTED && (timeSynchronized && millis() - lastNtpSyncTime * 1000 >= currentNtpInterval)) || ntpSyncRequested) {
        sendNTPrequest();
        ntpSyncRequested = false; // Reset the flag
      }

      // Handle the sleep schedule logic
      handleSleepSchedule();
      // Update the clock displays only if an animation is not active
      if (!isAnimating) {
        updateNormalClockDisplay();
      }
      
      // Periodically report status to the serial monitor every 30 seconds
      static unsigned long lastSerialReport = 0;
      if (timeSynchronized && millis() - lastSerialReport > SERIAL_REPORT_INTERVAL_MS) {
        lastSerialReport = millis();
        char destBuffer[30], presentBuffer[30], lastBuffer[30];
        time_t now_t;
        time(&now_t);
        localtime_r(&now_t, &currentTimeInfo);

        // Correctly build the destination time struct based on live time and destination year
        struct tm destinationTimeInfo_local = *localtime(&now_t);
        destinationTimeInfo_local.tm_year = currentSettings.destinationYear - 1900;
        
        // Correctly build the last departed time struct for consistency
        struct tm lastTimeDepartedInfo = { 0, currentSettings.lastTimeDepartedMinute, currentSettings.lastTimeDepartedHour, currentSettings.lastTimeDepartedDay, currentSettings.lastTimeDepartedMonth - 1, currentSettings.lastTimeDepartedYear - 1900 };
        // Set the timezone for the destination time for proper formatting in the log
        setenv("TZ", TZ_DATA[currentSettings.destinationTimezoneIndex].tzString, 1);
        tzset();

        strftime(destBuffer, sizeof(destBuffer), "%b %d %Y %I:%M %p", &destinationTimeInfo_local);
        strftime(presentBuffer, sizeof(presentBuffer), "%b %d %Y %I:%M %p", &currentTimeInfo);
        strftime(lastBuffer, sizeof(lastBuffer), "%b %d %Y %I:%M %p", &lastTimeDepartedInfo);
        
        // Restore the timezone
        setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
        tzset();
        
        ESP_LOGI("Status", "\n--- TIME CIRCUITS STATUS ---");
        ESP_LOGI("Status", " Display Asleep: %s", isDisplayAsleep ? "Yes" : "No");
        ESP_LOGI("Status", "DESTINATION TIME  : %s", destBuffer);
        ESP_LOGI("Status", "PRESENT TIME      : %s", presentBuffer);
        ESP_LOGI("Status", "LAST TIME DEPARTED: %s", lastBuffer);
        ESP_LOGI("Status", "--------------------------");
        ESP_LOGI("Status", "--- CURRENT SETTINGS ---");
        ESP_LOGI("Status", " Destination Year: %d", currentSettings.destinationYear);
        ESP_LOGI("Status", " Destination Timezone: %s", TZ_DATA[currentSettings.destinationTimezoneIndex].displayName);
        ESP_LOGI("Status", " Present Timezone: %s", TZ_DATA[currentSettings.presentTimezoneIndex].displayName);
        ESP_LOGI("Status", " Last Departed: %02d/%02d/%d %02d:%02d", currentSettings.lastTimeDepartedMonth, currentSettings.lastTimeDepartedDay, currentSettings.lastTimeDepartedYear, currentSettings.lastTimeDepartedHour, currentSettings.lastTimeDepartedMinute);
        ESP_LOGI("Status", " Departure Time (Sleep): %02d:%02d", currentSettings.departureHour, currentSettings.departureMinute);
        ESP_LOGI("Status", " Arrival Time (Wake): %02d:%02d", currentSettings.arrivalHour, currentSettings.arrivalMinute);
        ESP_LOGI("Status", " Brightness: %d/7", currentSettings.brightness);
        ESP_LOGI("Status", " Volume: %d/30", currentSettings.notificationVolume);
        ESP_LOGI("Status", " 24h Format: %s", currentSettings.displayFormat24h ? "On" : "Off");
        ESP_LOGI("Status", " Time Travel FX: %s", currentSettings.timeTravelSoundToggle ? "On" : "Off");
        ESP_LOGI("Status", " Time Travel Animation Interval: %d min", currentSettings.timeTravelAnimationInterval);
        ESP_LOGI("Status", " Preset Cycle: %d min", currentSettings.presetCycleInterval);
        ESP_LOGI("Status", " Animation Duration: %d ms", currentSettings.timeTravelAnimationDuration);
        ESP_LOGI("Status", " Animation Style: %d (%s)", currentSettings.animationStyle, ANIMATION_STYLE_NAMES[currentSettings.animationStyle]);
        ESP_LOGI("Status", " Theme: %d", currentSettings.theme);
        ESP_LOGI("Status", " Volume Fade: %s", currentSettings.timeTravelVolumeFade ? "On" : "Off");
        ESP_LOGI("Status", " Wind Speed Mode: %s", currentSettings.windSpeedModeEnabled ? "On" : "Off");
        ESP_LOGI("Status", " Location: %.2f, %.2f", currentSettings.latitude, currentSettings.longitude);
        ESP_LOGI("Status", "------------------------");
      } else if (!timeSynchronized) {
        // Show NTP error status if not synchronized
        static unsigned long lastNtpStatusReport = 0;
        if (millis() - lastNtpStatusReport > 5000) {
          lastNtpStatusReport = millis();
          ESP_LOGW("Status", "Time not synchronized. NTP Sync status: Retrying with %s (current interval: %lu ms).", NTP_SERVERS[currentNtpServerIndex], currentNtpInterval);
        }
      }
    }
  }
}