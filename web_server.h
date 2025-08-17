#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include "HardwareControl.h"

#define THEME_PREF_KEY "ui_theme"
#define PREFERENCES_NAMESPACE "bttf-clock"

extern AsyncWebServer server;
extern ClockSettings currentSettings;
extern String apiTemplatesJson;
extern Preferences preferences;
extern bool timeSynchronized;
extern bool ntpSyncRequested;
extern PubSubClient mqttClient;
extern bool mqttReconnectRequired;
extern const char TZ_JSON[] PROGMEM;

// Struct to pass parameters to the API request task
struct ApiTestParams {
    String url;
    String authKey;
    String authValue;
    uint32_t clientId;
};

extern JsonVariant getJsonVariant(JsonVariant root, const char* path);
extern void saveSettings();
extern void loadSettings();
extern void startTimeTravelAnimation();

void setupWebRoutes();

#endif // WEB_SERVER_H