#include "NTPManager.h"
#include "EventManager.h"
#include <esp_log.h>

const unsigned long NTP_INITIAL_SYNC_DELAY = 2000;
const unsigned long NTP_UPDATE_INTERVAL = 3600000; // 1 hour

const char *NTP_SERVERS[] = { "pool.ntp.org", "time.google.com", "time.nist.gov" };
const int NUM_NTP_SERVERS = sizeof(NTP_SERVERS) / sizeof(NTP_SERVERS[0]);

extern const TimeZoneEntry TZ_DATA[];

NTPManager::NTPManager() :
    timeSynchronized(false),
    lastNtpUpdate(0),
    currentNtpServerIndex(0) {
}

void NTPManager::init() {
    // Initial sync is handled in the first loop iteration
}

bool NTPManager::isTimeSynchronized() {
    return timeSynchronized;
}

void NTPManager::loop() {
    if (!timeSynchronized && millis() > NTP_INITIAL_SYNC_DELAY) {
        syncTime();
    } else if (timeSynchronized && (millis() - lastNtpUpdate > NTP_UPDATE_INTERVAL)) {
        syncTime();
    }
}

void NTPManager::syncTime() {
    ESP_LOGI("NTP", "Attempting to synchronize time...");

    for (int i = 0; i < NUM_NTP_SERVERS; i++) {
        configTime(0, 0, NTP_SERVERS[currentNtpServerIndex]);
        setenv("TZ", TZ_DATA[settingsManager.settings.presentTimezoneIndex].tzString, 1);
        tzset();

        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 10000)) {
            timeSynchronized = true;
            lastNtpUpdate = millis();
            ESP_LOGI("NTP", "Time synchronized successfully with %s", NTP_SERVERS[currentNtpServerIndex]);
            return; // Success
        } else {
            ESP_LOGW("NTP", "Failed to sync with %s. Trying next server.", NTP_SERVERS[currentNtpServerIndex]);
            currentNtpServerIndex = (currentNtpServerIndex + 1) % NUM_NTP_SERVERS;
        }
    }

    ESP_LOGE("NTP", "Failed to synchronize time with any NTP server.");
}
