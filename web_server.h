#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include "types.h"
#include <ArduinoJson.h>

extern AsyncWebServer server;
extern DNSServer dnsServer; // Add this line

void setupWebRoutes();
void broadcastWsStateUpdate(const char* key, int value);
void broadcastWsStateUpdate(const char* key, bool value);
void broadcastWsStateUpdate(const char* key, const JsonVariant& value);
void broadcastLog(const char* message);
void forceFetchWeatherDataTask(void* p);

// Forward declarations for WiFi provisioning handlers
void handleSaveCredentials(AsyncWebServerRequest *request);
void handleRootPage(AsyncWebServerRequest *request);

// Fix: Add forward declaration for saveWiFiCredentials
void saveWiFiCredentials(const char* ssid, const char* password);

#endif