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

#include "HardwareControl.h"

// =================================================================
// == GLOBAL DEFINITIONS & OBJECTS                                ==
// =================================================================

// Global instances and non-hardware-specific variables
ClockSettings currentSettings = {
  .destinationYear = 1955, .destinationTimezoneIndex = 4, .departureHour = 22, .departureMinute = 0, .arrivalHour = 7, .arrivalMinute = 0, .lastTimeDepartedHour = 1, .lastTimeDepartedMinute = 21, .lastTimeDepartedYear = 1985, .lastTimeDepartedMonth = 10, .lastTimeDepartedDay = 26, .brightness = 5, .notificationVolume = 15, .timeTravelSoundToggle 
= true, .greatScottSoundToggle = true, .timeTravelAnimationInterval = 15, .presetCycleInterval = 10, .displayFormat24h = false, .theme = THEME_TIME_CIRCUITS, .presentTimezoneIndex = 1,
  .timeTravelAnimationDuration = 4000,
  .animationStyle = ANIMATION_SEQUENTIAL_FLICKER,
  .timeTravelVolumeFade = true // NEW SETTING
};
ClockSettings defaultSettings = {
  .destinationYear = 1955, .destinationTimezoneIndex = 4, .departureHour = 22, .departureMinute = 0, .arrivalHour = 7, .arrivalMinute = 0, .lastTimeDepartedHour = 1, .lastTimeDepartedMinute = 21, .lastTimeDepartedYear = 1985, .lastTimeDepartedMonth = 10, .lastTimeDepartedDay = 26, .brightness = 5, .notificationVolume = 15, .timeTravelSoundToggle = true, .greatScottSoundToggle = true, .timeTravelAnimationInterval = 15, .presetCycleInterval = 10, .displayFormat24h = false, .theme = THEME_TIME_CIRCUITS, .presentTimezoneIndex = 1,
  .timeTravelAnimationDuration = 4000,
  .animationStyle = ANIMATION_SEQUENTIAL_FLICKER,
  .timeTravelVolumeFade = true // NEW SETTING
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
const long NTP_BASE_INTERVAL_MS = 60000;
unsigned long currentNtpInterval = NTP_BASE_INTERVAL_MS;
bool timeSynchronized = false;
time_t lastNtpSyncTime = 0;
char currentNtpServerUsed[32] = "N/A";
WiFiManager wifiManager;
AsyncWebServer server(80);
Preferences preferences;

// Animation and sleep state
bool isAnimating = false;
unsigned long animationStartTime = 0;
unsigned long lastAnimationFrameTime = 0;
enum AnimationPhase { ANIM_INACTIVE, ANIM_VOLUME_FADE_IN, ANIM_DIM_IN, ANIM_PRE_FLICKER_88MPH, ANIM_FLICKER, ANIM_DIM_OUT, ANIM_VOLUME_FADE_OUT, ANIM_COMPLETE }; // NEW ENUM
AnimationPhase currentPhase = ANIM_INACTIVE;
bool isDisplayAsleep = false;
byte initialBrightness = 0;
struct tm currentTimeInfo;
struct tm destinationTimeInfo;
int currentVolume = 0;
// NEW GLOBAL
#define MDNS_HOSTNAME "timecircuits"
const char* ANIMATION_STYLE_NAMES[] = { "Sequential Flicker", "Random Flicker", "All Displays Random Flicker", "Counting Up" };
// =================================================================
// == FUNCTION IMPLEMENTATIONS                                    ==
// =================================================================
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

void handleDisplayAnimation() {
  if (!isAnimating) return;
  unsigned long currentTime = millis();
unsigned long elapsed = currentTime - animationStartTime;
  const unsigned long TOTAL_ANIMATION_DURATION = currentSettings.timeTravelAnimationDuration;
  
  // Refactored "magic numbers" into constants for readability
  const float VOLUME_FADE_IN_PERCENTAGE = 0.10;
  const float DIM_IN_PERCENTAGE = 0.10;
  const float PRE_FLICKER_88MPH_PERCENTAGE = 0.30;
  const float FLICKER_PERCENTAGE = 0.50;
  const float DIM_OUT_PERCENTAGE = 0.10;
  const float VOLUME_FADE_OUT_PERCENTAGE = 0.10;

  const unsigned long VOLUME_FADE_IN_DURATION = TOTAL_ANIMATION_DURATION * VOLUME_FADE_IN_PERCENTAGE;
const unsigned long DIM_IN_DURATION = TOTAL_ANIMATION_DURATION * DIM_IN_PERCENTAGE;
  const unsigned long PRE_FLICKER_88MPH_DURATION = TOTAL_ANIMATION_DURATION * PRE_FLICKER_88MPH_PERCENTAGE;
const unsigned long FLICKER_DURATION = TOTAL_ANIMATION_DURATION * FLICKER_PERCENTAGE;
  const unsigned long DIM_OUT_DURATION = TOTAL_ANIMATION_DURATION * DIM_OUT_PERCENTAGE;
const unsigned long VOLUME_FADE_OUT_DURATION = TOTAL_ANIMATION_DURATION * VOLUME_FADE_OUT_PERCENTAGE;

  switch (currentPhase) {
    case ANIM_VOLUME_FADE_IN:
      if (currentSettings.timeTravelSoundToggle) {
        // Start playing the sound effect at zero volume
        myDFPlayer.volume(0);
playSound(SOUND_TIME_TRAVEL);
      }
      currentPhase = ANIM_DIM_IN;
      animationStartTime = currentTime;
// Fallthrough to the next phase to start fading immediately
    case ANIM_DIM_IN: {
      if (elapsed < DIM_IN_DURATION) {
        byte currentBrightness = map(elapsed, 0, DIM_IN_DURATION, 0, initialBrightness);
setDisplayBrightness(currentBrightness);
        // Fade in the volume
        if (currentSettings.timeTravelVolumeFade) {
          int newVolume = map(elapsed, 0, DIM_IN_DURATION, 0, currentSettings.notificationVolume);
if (newVolume != currentVolume) {
            currentVolume = newVolume;
            myDFPlayer.volume(currentVolume);
}
        }
      } else {
        setDisplayBrightness(initialBrightness);
blankAllDisplays();
        if (currentSettings.timeTravelSoundToggle && !currentSettings.timeTravelVolumeFade) {
          playSound("ACCELERATION");
}
        currentPhase = ANIM_PRE_FLICKER_88MPH;
        animationStartTime = currentTime;
}
      break;
    }
    case ANIM_PRE_FLICKER_88MPH: {
      if (elapsed < PRE_FLICKER_88MPH_DURATION) {
        float speed = 0.0;
float progress = (float)elapsed / PRE_FLICKER_88MPH_DURATION;
        if (progress < 0.5) { 
          speed = map(elapsed, 0, PRE_FLICKER_88MPH_DURATION / 2, 0, 80);
} else {
          speed = map(elapsed, PRE_FLICKER_88MPH_DURATION / 2, PRE_FLICKER_88MPH_DURATION, 80, 88);
}
        display88MphSpeed(speed);
        if (currentTime - lastAnimationFrameTime > 100) {
           lastAnimationFrameTime = currentTime;
}
      } else {
        blankAllDisplays();
if (currentSettings.timeTravelSoundToggle && !currentSettings.timeTravelVolumeFade) {
          playSound("WARP_WHOOSH");
}
        currentPhase = ANIM_FLICKER;
        animationStartTime = currentTime;
}
      break;
    }
    case ANIM_FLICKER:
      if (elapsed < FLICKER_DURATION) {
        if (currentTime - lastAnimationFrameTime > 50) {
          lastAnimationFrameTime = currentTime;
switch (currentSettings.animationStyle) {
            case 0: { // Sequential Flicker (Default)
              unsigned long currentPhaseElapsed = currentTime - animationStartTime;
int thirdPhaseDuration = FLICKER_DURATION / 3;
              if (currentPhaseElapsed < thirdPhaseDuration) {
                animateMonthDisplay(destRow);
animateDayDisplay(destRow);
                animateYearDisplay(destRow);
                animateTimeDisplay(destRow);
                animateAmPmDisplay(destRow);
              } else if (currentPhaseElapsed < (thirdPhaseDuration * 2)) {
                animateMonthDisplay(presRow);
animateDayDisplay(presRow);
                animateYearDisplay(presRow);
                animateTimeDisplay(presRow);
                animateAmPmDisplay(presRow);
              } else {
                animateMonthDisplay(lastRow);
animateDayDisplay(lastRow);
                animateYearDisplay(lastRow);
                animateTimeDisplay(lastRow);
                animateAmPmDisplay(lastRow);
              }
              break;
}
            case 1: { // Random Flicker with varied segment speed
              if (random(0, 100) < 90) animateMonthDisplay(destRow);
if (random(0, 100) < 60) animateDayDisplay(destRow);
              if (random(0, 100) < 30) animateYearDisplay(destRow);
              if (random(0, 100) < 80) animateTimeDisplay(destRow);
if (random(0, 100) < 70) animateAmPmDisplay(destRow);
              if (random(0, 100) < 90) animateMonthDisplay(presRow);
              if (random(0, 100) < 60) animateDayDisplay(presRow);
if (random(0, 100) < 30) animateYearDisplay(presRow);
              if (random(0, 100) < 80) animateTimeDisplay(presRow);
              if (random(0, 100) < 70) animateAmPmDisplay(presRow);
if (random(0, 100) < 90) animateMonthDisplay(lastRow);
              if (random(0, 100) < 60) animateDayDisplay(lastRow);
              if (random(0, 100) < 30) animateYearDisplay(lastRow);
if (random(0, 100) < 80) animateTimeDisplay(lastRow);
              if (random(0, 100) < 70) animateAmPmDisplay(lastRow);
              break;
}
            case 2: { // All Displays Random Flicker
                animateMonthDisplay(destRow);
animateDayDisplay(destRow);
                animateYearDisplay(destRow);
                animateTimeDisplay(destRow);
                animateAmPmDisplay(destRow);
                animateMonthDisplay(presRow);
                animateDayDisplay(presRow);
                animateYearDisplay(presRow);
                animateTimeDisplay(presRow);
                animateAmPmDisplay(presRow);
                animateMonthDisplay(lastRow);
                animateDayDisplay(lastRow);
                animateYearDisplay(lastRow);
                animateTimeDisplay(lastRow);
                animateAmPmDisplay(lastRow);
                break;
}
            case 3: { // Counting Up
                static int counter = 0;
static unsigned long lastCountUpdateTime = 0;
                const unsigned long COUNT_UPDATE_INTERVAL_MS = 20;
if (currentTime - lastCountUpdateTime > COUNT_UPDATE_INTERVAL_MS) {
                    lastCountUpdateTime = currentTime;
counter = (counter + 1) % 10000;
                    if (!ENABLE_HARDWARE) {
                        ESP_LOGD("Display (Disabled)", "Counting Up animation skipped. Counter: %d", counter);
} else {
                        if (ENABLE_I2C_HARDWARE) {
                            destRow.ht16k33.clear();
destRow.ht16k33.print(random(0, 1000));
                            destRow.ht16k33.writeDisplay();
                            presRow.ht16k33.clear();
                            presRow.ht16k33.print(random(0, 1000));
                            presRow.ht16k33.writeDisplay();
                            lastRow.ht16k33.clear();
                            lastRow.ht16k33.print(random(0, 1000));
                            lastRow.ht16k33.writeDisplay();
}
                        destRow.day.showNumberDec(random(0, 100));
destRow.year.showNumberDec(random(0, 10000));
                        destRow.time.showNumberDecEx(counter, 0b01000000, true);
                        presRow.day.showNumberDec(random(0, 100));
                        presRow.year.showNumberDec(random(0, 10000));
                        presRow.time.showNumberDecEx(counter, 0b01000000, true);
                        lastRow.day.showNumberDec(random(0, 100));
                        lastRow.year.showNumberDec(random(0, 10000));
                        lastRow.time.showNumberDecEx(counter, 0b01000000, true);
}
                    animateAmPmDisplay(destRow);
animateAmPmDisplay(presRow);
                    animateAmPmDisplay(lastRow);
                }
                break;
}
            default:
              unsigned long currentPhaseElapsed = currentTime - animationStartTime;
int thirdPhaseDuration = FLICKER_DURATION / 3;
              if (currentPhaseElapsed < thirdPhaseDuration) {
                animateMonthDisplay(destRow);
animateDayDisplay(destRow);
                animateYearDisplay(destRow);
                animateTimeDisplay(destRow);
                animateAmPmDisplay(destRow);
              } else if (currentPhaseElapsed < (thirdPhaseDuration * 2)) {
                animateMonthDisplay(presRow);
animateDayDisplay(presRow);
                animateYearDisplay(presRow);
                animateTimeDisplay(presRow);
                animateAmPmDisplay(presRow);
              } else {
                animateMonthDisplay(lastRow);
animateDayDisplay(lastRow);
                animateYearDisplay(lastRow);
                animateTimeDisplay(lastRow);
                animateAmPmDisplay(lastRow);
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
            myDFPlayer.volume(currentVolume);
}
        }
      } else {
        if (currentSettings.timeTravelSoundToggle) {
          myDFPlayer.volume(currentSettings.notificationVolume);
playSound("ARRIVAL_THUD");
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
// Reset volume to normal after animation finishes
        if (currentSettings.timeTravelSoundToggle) {
          myDFPlayer.volume(currentSettings.notificationVolume);
}
      }
      break;
case ANIM_INACTIVE:
      break;
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
bool lastDepartedNeedsUpdate = (currentSettings.lastTimeDepartedHour != lastLastTimeDepartedHour ||
                                  currentSettings.lastTimeDepartedMinute != lastLastTimeDepartedMinute ||
                                  currentSettings.lastTimeDepartedYear != lastLastTimeDepartedYear ||
                    
              currentSettings.lastTimeDepartedMonth != lastLastTimeDepartedMonth ||
                                  currentSettings.lastTimeDepartedDay != lastLastTimeDepartedDay ||
                                  presentTimeNeedsUpdate);
if (timeSynchronized && (presentTimeNeedsUpdate || destinationTimeNeedsUpdate || lastDepartedNeedsUpdate)) {
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
    if (lastDepartedNeedsUpdate) {
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
        destRow.ht16k33.print("NTP");
        destRow.ht16k33.writeDisplay();
        presRow.ht16k33.print("ERR");
        presRow.ht16k33.writeDisplay();
}
    }
  }
}

void startTimeTravelAnimation() {
  if (isAnimating) return;
  isAnimating = true;
  animationStartTime = millis();
lastAnimationFrameTime = millis();
  initialBrightness = currentSettings.brightness;
  currentPhase = ANIM_VOLUME_FADE_IN; // Start with the new fade-in phase
  ESP_LOGI("Animation", "Starting physical time travel animation.");
blankAllDisplays();
}

void sendNTPpacket(IPAddress &ntpServerIp) {
  byte packetBuffer[48];
  memset(packetBuffer, 0, 48);
  packetBuffer[0] = 0b11100011;
  Udp.beginPacket(ntpServerIp, 123);
  Udp.write(packetBuffer, 48);
  Udp.endPacket();
lastNtpRequestSent = millis();
}

void processNTPresponse() {
  IPAddress ntpServerIP;
  const char *serverToUse = NTP_SERVERS[currentNtpServerIndex];
if (WiFi.hostByName(serverToUse, ntpServerIP)) {
    sendNTPpacket(ntpServerIP);
    unsigned long startWaiting = millis();
    byte packetBuffer[48];
while (millis() - startWaiting < 2000) {
      int packetSize = Udp.parsePacket();
if (packetSize >= 48) {
        Udp.read(packetBuffer, 48);
        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
        time_t epoch = (highWord << 16 | lowWord) - 2208988800UL;
        struct timeval tv;
tv.tv_sec = epoch;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);
        ESP_LOGI("NTP", "NTP Time Sync Successful! Connected to %s", serverToUse);
        strcpy(currentNtpServerUsed, serverToUse);
timeSynchronized = true;
        lastNtpSyncTime = epoch;
        currentNtpInterval = 3600000;
        return;
      }
      delay(10);
}
  }
  timeSynchronized = false;
  currentNtpServerIndex = (currentNtpServerIndex + 1) % NUM_NTP_SERVERS;
  currentNtpInterval = 30000;
ESP_LOGW("NTP", "Sync failed. Retrying with %s in 30s.", NTP_SERVERS[currentNtpServerIndex]);
}

void handleSleepSchedule() {
  if (!timeSynchronized) return;
  time_t now_t;
  time(&now_t);
struct tm *now_tm = localtime(&now_t);
  int now_minutes = now_tm->tm_hour * 60 + now_tm->tm_min;
int sleep_minutes = currentSettings.departureHour * 60 + currentSettings.departureMinute;
  int wake_minutes = currentSettings.arrivalHour * 60 + now_tm->tm_min;
  bool shouldBeAsleep = false;
if (sleep_minutes < wake_minutes) {
    shouldBeAsleep = (now_minutes >= sleep_minutes && now_minutes < wake_minutes);
} else {
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
doc["greatScottSoundToggle"] = currentSettings.greatScottSoundToggle;
    doc["timeTravelAnimationInterval"] = currentSettings.timeTravelAnimationInterval;
    doc["presetCycleInterval"] = currentSettings.presetCycleInterval;
    doc["displayFormat24h"] = currentSettings.displayFormat24h;
    doc["theme"] = currentSettings.theme;
    doc["presentTimezoneIndex"] = currentSettings.presentTimezoneIndex;
doc["timeTravelAnimationDuration"] = currentSettings.timeTravelAnimationDuration;
    doc["animationStyle"] = currentSettings.animationStyle;
    doc["timeTravelVolumeFade"] = currentSettings.timeTravelVolumeFade; // NEW SETTING
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
     
    currentSettings.departureMinute 
= request->getParam("departureMinute", true)->value().toInt();
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
    if (request->hasParam("greatScottSoundToggle", true)) { 
        currentSettings.greatScottSoundToggle = (request->getParam("greatScottSoundToggle", true)->value() == "true");
}
    if (request->hasParam("displayFormat24h", true)) { 
        currentSettings.displayFormat24h = (request->getParam("displayFormat24h", true)->value() == "true");
}
    if (request->hasParam("timeTravelVolumeFade", true)) { // NEW
        currentSettings.timeTravelVolumeFade = (request->getParam("timeTravelVolumeFade", true)->value() == "true");
// NEW
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
    processNTPresponse();
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
server.on("/api/toggleGreatScottSound", HTTP_POST, [](AsyncWebServerRequest *request) {
    currentSettings.greatScottSoundToggle = !currentSettings.greatScottSoundToggle;
    if (currentSettings.greatScottSoundToggle) {
      playSound(SOUND_EASTER_EGG);
    }
    request->send(200, "application/json", currentSettings.greatScottSoundToggle ? "{\"state\":true}" : "{\"state\":false}");
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
          currentSettings.brightness = brightness;
   
      } else {
          request->send(400, "text/plain", "Invalid brightness value.");
          return;
        }
      } else if (setting == "notificationVolume") {
        int volume = value.toInt();
        if (volume >= 0 && volume <= 30) {
          currentSettings.notificationVolume = volume;
         
  if (ENABLE_HARDWARE) myDFPlayer.volume(volume);
       
  } else {
            request->send(400, "text/plain", "Invalid volume value.");
return;
        }
      } else if (setting == "displayFormat24h") {
          currentSettings.displayFormat24h = (value == "true");
} else if (setting == "animationStyle") {
          int style = value.toInt();
if (style >=0 && style <=3) {
            currentSettings.animationStyle = style;
} else {
            request->send(400, "text/plain", "Invalid animation style.");
            return;
}
      } else if (setting == "timeTravelAnimationDuration") {
          int duration = value.toInt();
if (duration >=1000 && duration <= 10000) {
            currentSettings.timeTravelAnimationDuration = duration;
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
} else if (setting == "timeTravelVolumeFade") { // NEW
        currentSettings.timeTravelVolumeFade = (value == "true");
// NEW
      }
      else {
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
      for (JsonObject preset : array) 
{
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

void setup() {
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_DEBUG);
  ESP_LOGI("Boot", "--- BOOTING UP ---");
if (!LittleFS.begin(true)) {
    ESP_LOGE("FS", "CRITICAL ERROR: An Error has occurred while mounting LittleFS.");
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
  Udp.begin(123);
  processNTPresponse();
  setupPhysicalDisplay();
  setupWebRoutes();
  server.begin();
  ESP_LOGI("Web", "HTTP server started");
}

void loop() {
  ArduinoOTA.handle();
  static unsigned long lastWifiCheck = 0;
if (millis() - lastWifiCheck > 10000) {
    lastWifiCheck = millis();
if (WiFi.status() != WL_CONNECTED) {
      ESP_LOGW("WiFi", "WiFi Disconnected. Attempting to reconnect...");
      WiFi.reconnect();
}
  }
  if (WiFi.status() == WL_CONNECTED && (millis() - lastNtpRequestSent >= currentNtpInterval)) {
    processNTPresponse();
}
  handleSleepSchedule();
  handleDisplayAnimation();
  if (!isAnimating) {
    updateNormalClockDisplay();
  }
  static unsigned long lastSerialReport = 0;
if (timeSynchronized && millis() - lastSerialReport > 30000) {
    lastSerialReport = millis();
    char destBuffer[30], presentBuffer[30], lastBuffer[30];
time_t now_t;
    time(&now_t);
    localtime_r(&now_t, &currentTimeInfo);

    // Build the destination time struct using the saved settings, not the current time
    struct tm destinationTimeInfo = { 0 };
destinationTimeInfo.tm_year = currentSettings.destinationYear - 1900;
    destinationTimeInfo.tm_mon = currentSettings.lastTimeDepartedMonth - 1; // tm_mon is 0-11
    destinationTimeInfo.tm_mday = currentSettings.lastTimeDepartedDay;
destinationTimeInfo.tm_hour = currentSettings.departureHour; // Assuming this is the hour to display
    destinationTimeInfo.tm_min = currentSettings.departureMinute;
// Assuming this is the minute to display
    
    // Set the timezone for the destination time
    setenv("TZ", TZ_DATA[currentSettings.destinationTimezoneIndex].tzString, 1);
tzset();

    // Re-create the last departed time struct for consistency
    struct tm lastTimeDepartedInfo = { 0, currentSettings.lastTimeDepartedMinute, currentSettings.lastTimeDepartedHour, currentSettings.lastTimeDepartedDay, currentSettings.lastTimeDepartedMonth - 1, currentSettings.lastTimeDepartedYear - 1900 };
// Format the strings for display in the serial monitor
    strftime(destBuffer, sizeof(destBuffer), "%b %d %Y %I:%M %p", &destinationTimeInfo);
strftime(presentBuffer, sizeof(presentBuffer), "%b %d %Y %I:%M %p", &currentTimeInfo);
    strftime(lastBuffer, sizeof(lastBuffer), "%b %d %Y %I:%M %p", &lastTimeDepartedInfo);
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
ESP_LOGI("Status", " Great Scott Sound: %s", currentSettings.greatScottSoundToggle ? "On" : "Off");
    ESP_LOGI("Status", " Time Travel Animation Interval: %d min", currentSettings.timeTravelAnimationInterval);
ESP_LOGI("Status", " Preset Cycle: %d min", currentSettings.presetCycleInterval);
    ESP_LOGI("Status", " Animation Duration: %d ms", currentSettings.timeTravelAnimationDuration);
ESP_LOGI("Status", " Animation Style: %d (%s)", currentSettings.animationStyle, ANIMATION_STYLE_NAMES[currentSettings.animationStyle]);
    ESP_LOGI("Status", " Theme: %d", currentSettings.theme);
ESP_LOGI("Status", " Volume Fade: %s", currentSettings.timeTravelVolumeFade ? "On" : "Off"); // NEW
    ESP_LOGI("Status", "------------------------");
} else if (!timeSynchronized) {
    static unsigned long lastNtpStatusReport = 0;
if (millis() - lastNtpStatusReport > 5000) {
      lastNtpStatusReport = millis();
ESP_LOGW("Status", "Time not synchronized. NTP Sync status: Retrying with %s (current interval: %lu ms).", NTP_SERVERS[currentNtpServerIndex], currentNtpInterval);
    }
  }
}