#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "HardwareControl.h"
#include "certs.h" // Include the new certificate header

// Define constants that the web routes need access to
#define THEME_PREF_KEY "ui_theme"
#define PREFERENCES_NAMESPACE "bttf-clock"

// Extern variables: these are defined in the main .ino file
extern AsyncWebServer server;
extern ClockSettings currentSettings;
extern String apiTemplatesJson;
// REMOVED the extern char rootCACert[2048]; declaration
extern Preferences preferences;
extern bool timeSynchronized;
extern bool ntpSyncRequested;
extern PubSubClient mqttClient;
extern bool mqttReconnectRequired;
extern const char TZ_JSON[] PROGMEM;

// Extern functions: these are defined in the main .ino file
extern JsonVariant getJsonVariant(JsonVariant root, const char* path);
extern void saveSettings();
extern void loadSettings();
extern void startTimeTravelAnimation();

// Function prototype for the function in web_server.cpp
void setupWebRoutes();

#endif // WEB_SERVER_H