#include "SettingsManager.h"
#include <esp_log.h>
#include "EventManager.h" // For TZ_DATA

// --- CONSTANTS ---
#define PREFERENCES_NAMESPACE "bttf-clock"

// --- HELPER MACROS for saving settings ---
// These macros reduce boilerplate when saving values to Preferences. They check if the
// new value is different from the existing one before writing, minimizing flash wear.

#define SAVE_IF_CHANGED(key, type, value) \
    if (preferences.get##type(key) != value) { \
        preferences.put##type(key, value); \
        ESP_LOGI("SETTINGS", "SAVING: %s -> %d", #key, (int)value); \
    }

#define SAVE_FLOAT_IF_CHANGED(key, value) \
    if (preferences.getFloat(key, 0.0f) != value) { \
        preferences.putFloat(key, value); \
        ESP_LOGI("SETTINGS", "SAVING: %s -> %f", #key, value); \
    }

#define SAVE_BOOL_IF_CHANGED(key, value) \
    if (preferences.getBool(key, false) != value) { \
        preferences.putBool(key, value); \
        ESP_LOGI("SETTINGS", "SAVING: %s -> %s", #key, value ? "true" : "false"); \
    }

#define SAVE_STRING_IF_CHANGED(key, value) \
    if (!preferences.isKey(key) || preferences.getString(key, "") != value.c_str()) { \
        preferences.putString(key, value.c_str()); \
        ESP_LOGI("SETTINGS", "SAVING: %s -> %s", #key, value.c_str()); \
    }


SettingsManager::SettingsManager() {
    // Constructor can be used for initial setup if needed.
}

void SettingsManager::load() {
    ESP_LOGI("SETTINGS", "--- Loading Settings ---");
    preferences.begin(PREFERENCES_NAMESPACE, true); // Open preferences in read-only mode first.

    bool needsInit = !preferences.isKey("destYear");
    if (needsInit) {
        ESP_LOGI("SETTINGS", "No settings found. Initializing with defaults.");
        initializeDefaultSettings();
        // After setting defaults, we need to save them.
        // The save() method will open preferences in read-write mode.
        save();
    } else {
        ESP_LOGI("SETTINGS", "Loading settings from NVS.");
        settings.destinationYear = preferences.getInt("destYear", 1955);
        settings.destinationTimezoneIndex = preferences.getInt("destTzIndex", 6); // PST
        settings.departureHour = preferences.getInt("depHour", 22);
        settings.departureMinute = preferences.getInt("depMinute", 0);
        settings.arrivalHour = preferences.getInt("arrHour", 7);
        settings.arrivalMinute = preferences.getInt("arrMinute", 0);
        settings.lastTimeDepartedYear = preferences.getInt("lastYear", 1985);
        settings.lastTimeDepartedMonth = preferences.getInt("lastMonth", 10);
        settings.lastTimeDepartedDay = preferences.getInt("lastDay", 26);
        settings.lastTimeDepartedHour = preferences.getInt("lastHour", 1);
        settings.lastTimeDepartedMinute = preferences.getInt("lastMinute", 21);
        settings.brightness = preferences.getUChar("brightness", 5);
        settings.notificationVolume = preferences.getUChar("volume", 15);
        settings.timeTravelSoundToggle = preferences.getBool("soundToggle", true);
        settings.presentTimezoneIndex = preferences.getInt("presTzIndex", 6); // PST
        settings.presetCycleInterval = preferences.getInt("presetCycle", 10);
        settings.displayFormat24h = preferences.getBool("format24h", false);
        settings.theme = preferences.getInt("theme", THEME_TIME_CIRCUITS);
        settings.timeTravelAnimationInterval = preferences.getInt("animInterval", 15);
        settings.timeTravelAnimationDuration = preferences.getInt("animDuration", 4000);
        settings.animationStyle = preferences.getInt("animStyle", ANIMATION_SEQUENTIAL_FLICKER);
        settings.glitchEffectFrequency = preferences.getInt("glitchFreq", 0);
        settings.dataLinkEnabled = preferences.getBool("dlEnabled", false);
        settings.dataLinkTargetRow = preferences.getInt("dlTargetRow", 2);
        settings.dataLinkRefreshInterval = preferences.getInt("dlRefresh", 10);
        settings.numDataPoints = preferences.getInt("numDataPoints", 0);
        settings.mqttBroker = preferences.getString("mqttBroker", "broker.emqx.io").c_str();
        settings.mqttPort = preferences.getInt("mqttPort", 1883);
        settings.mqttUser = preferences.getString("mqttUser", "").c_str();
        settings.mqttPassword = preferences.getString("mqttPass", "").c_str();
        settings.weatherModeEnabled = preferences.getBool("weatherMode", false);
        settings.cityName = preferences.getString("cityName", "Los Angeles").c_str();
        settings.useMetricUnits = preferences.getBool("useMetric", false);
        settings.latitude = preferences.getFloat("latitude", 34.0522);
        settings.longitude = preferences.getFloat("longitude", -118.2437);
        settings.stockTickerModeEnabled = preferences.getBool("stModeEnabled", false);
        settings.stockRow1_symbol = preferences.getString("stRow1Sym", "^GSPC").c_str();
        settings.stockRow2_symbol = preferences.getString("stRow2Sym", "^GSPTSE").c_str();
        settings.stockRow3_symbol = preferences.getString("stRow3Sym", "^IXIC").c_str();
        settings.financialModelingPrepApiKey = preferences.getString("fmpApiKey", "").c_str();

        for (int i = 0; i < 5; i++) {
            String prefix = "dp" + String(i) + "_";
            settings.dataPoints[i].url = preferences.getString((prefix + "url").c_str(), "").c_str();
            settings.dataPoints[i].monthPath = preferences.getString((prefix + "monthPath").c_str(), "").c_str();
            settings.dataPoints[i].dayPath = preferences.getString((prefix + "dayPath").c_str(), "").c_str();
            settings.dataPoints[i].yearPath = preferences.getString((prefix + "yearPath").c_str(), "").c_str();
            settings.dataPoints[i].timePath = preferences.getString((prefix + "timePath").c_str(), "").c_str();
            settings.dataPoints[i].prefix = preferences.getString((prefix + "prefix").c_str(), "").c_str();
            settings.dataPoints[i].suffix = preferences.getString((prefix + "suffix").c_str(), "").c_str();
            settings.dataPoints[i].icon = preferences.getString((prefix + "icon").c_str(), "").c_str();
            settings.dataPoints[i].scrollSpeed = preferences.getInt((prefix + "scroll").c_str(), 0);
            settings.dataPoints[i].dataSourceType = (DataSourceType)preferences.getInt((prefix + "srcType").c_str(), 0);
            settings.dataPoints[i].mqttTopic = preferences.getString((prefix + "topic").c_str(), "").c_str();
            settings.dataPoints[i].yearPrefix = preferences.getString((prefix + "yearPrefix").c_str(), "").c_str();
            settings.dataPoints[i].yearSuffix = preferences.getString((prefix + "yearSuffix").c_str(), "").c_str();
            settings.dataPoints[i].displayMode = (DisplayMode)preferences.getInt((prefix + "dispMode").c_str(), 0);
            settings.dataPoints[i].scrollingText = preferences.getString((prefix + "scrollTxt").c_str(), "").c_str();
            settings.dataPoints[i].authHeaderKey = preferences.getString((prefix + "authKey").c_str(), "").c_str();
            settings.dataPoints[i].authHeaderValue = preferences.getString((prefix + "authVal").c_str(), "").c_str();
            settings.dataPoints[i].httpMethod = (HttpMethod)preferences.getInt((prefix + "httpMethod").c_str(), 0);
            settings.dataPoints[i].requestBody = preferences.getString((prefix + "reqBody").c_str(), "").c_str();
            settings.dataPoints[i].apiExampleKey = preferences.getString((prefix + "apiKey").c_str(), "").c_str();
        }
    }
    preferences.end();
    ESP_LOGI("SETTINGS", "--- Settings Loaded ---");

    // Apply the timezone setting immediately after loading.
    // Note: This relies on TZ_DATA being available. We need to ensure it's accessible.
    // A better approach might be to pass it in or have a global reference.
    extern const TimeZoneEntry TZ_DATA[];
    setenv("TZ", TZ_DATA[settings.presentTimezoneIndex].tzString, 1);
    tzset();
}

void SettingsManager::save() {
    ESP_LOGI("SETTINGS", "--- Saving Settings ---");
    preferences.begin(PREFERENCES_NAMESPACE, false); // Open in read-write mode.

    SAVE_IF_CHANGED("destYear", Int, settings.destinationYear);
    SAVE_IF_CHANGED("destTzIndex", Int, settings.destinationTimezoneIndex);
    SAVE_IF_CHANGED("depHour", Int, settings.departureHour);
    SAVE_IF_CHANGED("depMinute", Int, settings.departureMinute);
    SAVE_IF_CHANGED("arrHour", Int, settings.arrivalHour);
    SAVE_IF_CHANGED("arrMinute", Int, settings.arrivalMinute);
    SAVE_IF_CHANGED("lastYear", Int, settings.lastTimeDepartedYear);
    SAVE_IF_CHANGED("lastMonth", Int, settings.lastTimeDepartedMonth);
    SAVE_IF_CHANGED("lastDay", Int, settings.lastTimeDepartedDay);
    SAVE_IF_CHANGED("lastHour", Int, settings.lastTimeDepartedHour);
    SAVE_IF_CHANGED("lastMinute", Int, settings.lastTimeDepartedMinute);
    SAVE_IF_CHANGED("brightness", UChar, settings.brightness);
    SAVE_IF_CHANGED("volume", UChar, settings.notificationVolume);
    SAVE_BOOL_IF_CHANGED("soundToggle", settings.timeTravelSoundToggle);
    SAVE_IF_CHANGED("presTzIndex", Int, settings.presentTimezoneIndex);
    SAVE_IF_CHANGED("presetCycle", Int, settings.presetCycleInterval);
    SAVE_BOOL_IF_CHANGED("format24h", settings.displayFormat24h);
    SAVE_IF_CHANGED("theme", Int, settings.theme);
    SAVE_IF_CHANGED("animInterval", Int, settings.timeTravelAnimationInterval);
    SAVE_IF_CHANGED("animDuration", Int, settings.timeTravelAnimationDuration);
    SAVE_IF_CHANGED("animStyle", Int, settings.animationStyle);
    SAVE_IF_CHANGED("glitchFreq", Int, settings.glitchEffectFrequency);
    SAVE_BOOL_IF_CHANGED("dlEnabled", settings.dataLinkEnabled);
    SAVE_IF_CHANGED("dlTargetRow", Int, settings.dataLinkTargetRow);
    SAVE_IF_CHANGED("dlRefresh", Int, settings.dataLinkRefreshInterval);
    SAVE_IF_CHANGED("numDataPoints", Int, settings.numDataPoints);
    SAVE_STRING_IF_CHANGED("mqttBroker", settings.mqttBroker);
    SAVE_IF_CHANGED("mqttPort", Int, settings.mqttPort);
    SAVE_STRING_IF_CHANGED("mqttUser", settings.mqttUser);
    SAVE_STRING_IF_CHANGED("mqttPass", settings.mqttPassword);
    SAVE_BOOL_IF_CHANGED("weatherMode", settings.weatherModeEnabled);
    SAVE_STRING_IF_CHANGED("cityName", settings.cityName);
    SAVE_BOOL_IF_CHANGED("useMetric", settings.useMetricUnits);
    SAVE_FLOAT_IF_CHANGED("latitude", settings.latitude);
    SAVE_FLOAT_IF_CHANGED("longitude", settings.longitude);
    SAVE_BOOL_IF_CHANGED("stModeEnabled", settings.stockTickerModeEnabled);
    SAVE_STRING_IF_CHANGED("stRow1Sym", settings.stockRow1_symbol);
    SAVE_STRING_IF_CHANGED("stRow2Sym", settings.stockRow2_symbol);
    SAVE_STRING_IF_CHANGED("stRow3Sym", settings.stockRow3_symbol);
    SAVE_STRING_IF_CHANGED("fmpApiKey", settings.financialModelingPrepApiKey);

    for (int i = 0; i < 5; i++) {
        String prefix = "dp" + String(i) + "_";
        SAVE_STRING_IF_CHANGED((prefix + "url").c_str(), settings.dataPoints[i].url);
        SAVE_STRING_IF_CHANGED((prefix + "monthPath").c_str(), settings.dataPoints[i].monthPath);
        SAVE_STRING_IF_CHANGED((prefix + "dayPath").c_str(), settings.dataPoints[i].dayPath);
        SAVE_STRING_IF_CHANGED((prefix + "yearPath").c_str(), settings.dataPoints[i].yearPath);
        SAVE_STRING_IF_CHANGED((prefix + "timePath").c_str(), settings.dataPoints[i].timePath);
        SAVE_STRING_IF_CHANGED((prefix + "prefix").c_str(), settings.dataPoints[i].prefix);
        SAVE_STRING_IF_CHANGED((prefix + "suffix").c_str(), settings.dataPoints[i].suffix);
        SAVE_STRING_IF_CHANGED((prefix + "icon").c_str(), settings.dataPoints[i].icon);
        SAVE_IF_CHANGED((prefix + "scroll").c_str(), Int, settings.dataPoints[i].scrollSpeed);
        SAVE_IF_CHANGED((prefix + "srcType").c_str(), Int, (int)settings.dataPoints[i].dataSourceType);
        SAVE_STRING_IF_CHANGED((prefix + "topic").c_str(), settings.dataPoints[i].mqttTopic);
        SAVE_STRING_IF_CHANGED((prefix + "yearPrefix").c_str(), settings.dataPoints[i].yearPrefix);
        SAVE_STRING_IF_CHANGED((prefix + "yearSuffix").c_str(), settings.dataPoints[i].yearSuffix);
        SAVE_IF_CHANGED((prefix + "dispMode").c_str(), Int, (int)settings.dataPoints[i].displayMode);
        SAVE_STRING_IF_CHANGED((prefix + "scrollTxt").c_str(), settings.dataPoints[i].scrollingText);
        SAVE_STRING_IF_CHANGED((prefix + "authKey").c_str(), settings.dataPoints[i].authHeaderKey);
        SAVE_STRING_IF_CHANGED((prefix + "authVal").c_str(), settings.dataPoints[i].authHeaderValue);
        SAVE_IF_CHANGED((prefix + "httpMethod").c_str(), Int, (int)settings.dataPoints[i].httpMethod);
        SAVE_STRING_IF_CHANGED((prefix + "reqBody").c_str(), settings.dataPoints[i].requestBody);
        SAVE_STRING_IF_CHANGED((prefix + "apiKey").c_str(), settings.dataPoints[i].apiExampleKey);
    }

    preferences.end();
    ESP_LOGI("SETTINGS", "--- Settings Saved ---");

    // Apply the timezone setting immediately after saving.
    extern const TimeZoneEntry TZ_DATA[];
    setenv("TZ", TZ_DATA[settings.presentTimezoneIndex].tzString, 1);
    tzset();
}

void SettingsManager::initializeDefaultSettings() {
    settings.destinationYear = 1955;
    settings.destinationTimezoneIndex = 6; // Default to Pacific Time
    settings.departureHour = 22;
    settings.departureMinute = 0;
    settings.arrivalHour = 7;
    settings.arrivalMinute = 0;
    settings.lastTimeDepartedYear = 1985;
    settings.lastTimeDepartedMonth = 10;
    settings.lastTimeDepartedDay = 26;
    settings.lastTimeDepartedHour = 1;
    settings.lastTimeDepartedMinute = 21;
    settings.brightness = 5;
    settings.notificationVolume = 15;
    settings.timeTravelSoundToggle = true;
    settings.presentTimezoneIndex = 6; // Default to Pacific Time
    settings.presetCycleInterval = 10;
    settings.displayFormat24h = false;
    settings.theme = THEME_TIME_CIRCUITS;
    settings.timeTravelAnimationInterval = 15;
    settings.timeTravelAnimationDuration = 4000;
    settings.animationStyle = ANIMATION_SEQUENTIAL_FLICKER;
    settings.glitchEffectFrequency = 0;
    settings.dataLinkEnabled = false;
    settings.dataLinkTargetRow = 2;
    settings.dataLinkRefreshInterval = 10;
    settings.numDataPoints = 0;
    settings.mqttBroker = "broker.emqx.io";
    settings.mqttPort = 1883;
    settings.mqttUser = "";
    settings.mqttPassword = "";
    settings.weatherModeEnabled = false;
    settings.cityName = "Los Angeles";
    settings.useMetricUnits = false;
    settings.latitude = 34.0522;
    settings.longitude = -118.2437;
    settings.stockTickerModeEnabled = false;
    settings.stockRow1_symbol = "^GSPC";
    settings.stockRow2_symbol = "^GSPTSE";
    settings.stockRow3_symbol = "^IXIC";
    settings.financialModelingPrepApiKey = "";
    for (int i = 0; i < 5; i++) {
        settings.dataPoints[i] = {};
    }
}
