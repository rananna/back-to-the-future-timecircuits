#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include "HardwareControl.h"
#include <string>
#include <ArduinoOTA.h>


#define THEME_PREF_KEY "ui_theme"
#define PREFERENCES_NAMESPACE "bttf-clock"

extern AsyncWebServer server;
extern AsyncWebSocket ws;
extern ClockSettings currentSettings;
extern WeatherData currentWeatherData;
extern String apiTemplatesJson;
extern Preferences preferences;
extern bool timeSynchronized;
extern bool ntpSyncRequested;
extern PubSubClient mqttClient;
extern bool mqttReconnectRequired;
extern const char TZ_JSON[] PROGMEM;
extern const int NUM_TIMEZONE_OPTIONS;

extern std::string lastCityName;
extern SemaphoreHandle_t xDisplayDataMutex;

struct ApiTestParams {
    String url;
    String authKey;
    String authValue;
    uint32_t clientId;
    String action;
    int rowIndex;
};

extern JsonVariant getJsonVariant(JsonVariant root, const char* path);
extern void saveSettings();
extern void loadSettings();
extern void startTimeTravelAnimation();
void fetchWeatherDataTask(void* p);

void setupWebRoutes();
void broadcastWsStateUpdate(const char* key, const JsonVariant& value);
void broadcastWsStateUpdate(const char* key, int value);
void broadcastWsStateUpdate(const char* key, bool value);


#endif // WEB_SERVER_H