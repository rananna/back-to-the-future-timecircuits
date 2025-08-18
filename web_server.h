#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include "HardwareControl.h"
#include <string> // Required for std::string

#define THEME_PREF_KEY "ui_theme"
#define PREFERENCES_NAMESPACE "bttf-clock"

extern AsyncWebServer server;
extern ClockSettings currentSettings;
extern WeatherData currentWeatherData;
extern String apiTemplatesJson;
extern Preferences preferences;
extern bool timeSynchronized;
extern bool ntpSyncRequested;
extern PubSubClient mqttClient;
extern bool mqttReconnectRequired;
extern const char TZ_JSON[] PROGMEM;

// EXTERN DECLARATIONS TO FIX COMPILER ERROR
extern std::string lastCityName;
extern SemaphoreHandle_t xDisplayDataMutex;

// Struct to pass parameters to the API request task
struct ApiTestParams {
    String url;
    String authKey;
    String authValue;
    uint32_t clientId;
};

// Struct to pass parameters for weather fetching tasks
struct WeatherTaskParams {
    std::string cityName;
    bool forceGeocode;
};


// Forward declarations
extern JsonVariant getJsonVariant(JsonVariant root, const char* path);
extern void saveSettings();
extern void loadSettings();
extern void startTimeTravelAnimation();
void fetchWeatherDataTask(void* p);

void setupWebRoutes();

#endif // WEB_SERVER_H