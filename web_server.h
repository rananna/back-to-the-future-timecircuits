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
#define PREFERENCES_NAMESPACE "BTTF_TC"

class StockManager;

extern AsyncWebServer server;
extern AsyncWebSocket ws;
extern ClockSettings currentSettings;
extern WeatherData currentWeatherData;
extern StockManager stockManager;
extern String apiTemplatesJson;
extern Preferences preferences;
extern bool timeSynchronized;
extern bool ntpSyncRequested;
extern PubSubClient mqttClient;
extern bool mqttReconnectRequired;
extern const char TZ_JSON[] PROGMEM;

extern std::string lastCityName;
extern SemaphoreHandle_t xDisplayDataMutex;

struct ApiTestParams {
    String url;
    String authKey;
    String authValue;
    uint32_t clientId;
    String action;
    String rowIndex;
    String symbol;
};

extern JsonVariant getJsonVariant(JsonVariant root, const char* path);
extern void saveSettings();
extern void loadSettings();
extern void startTimeTravelAnimation();
void fetchWeatherDataTask(void* p);

void setupWebRoutes();
void broadcastPresetUpdate(const std::string& name, int year, int month, int day, int hour, int minute);
void broadcastWsStateUpdate(const char* key, const JsonVariant& value);
void broadcastWsStateUpdate(const char* key, int value);
void broadcastWsStateUpdate(const char* key, bool value);
void broadcastWeatherUpdate();
void sendFullSettingsToClient(uint32_t clientId);


#endif // WEB_SERVER_H