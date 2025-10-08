/**
 * @file web_server.h
 * @brief Public interface for the device's web server, including API routes and WebSocket communication.
 * @details This file declares the web server and WebSocket server instances, defines the API
 * endpoints, and provides function prototypes for broadcasting state updates to all connected
 * WebSocket clients. It is the primary interface for the web-based user interface.
 */
#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include "HardwareControl.h"
#include "MqttManager.h"
#include "AnimationSequences.h"
#include <string>
#include <ArduinoOTA.h>

// Forward declaration for a helper function defined in the main .ino file.
const char* animationTypeToString(AnimationType type);


/**
 * @name Preferences Keys
 * @brief Keys used for storing settings in the device's non-volatile storage.
 * @{
 */
#define THEME_PREF_KEY "ui_theme"           /**< The key for storing the selected UI theme. */
#define PREFERENCES_NAMESPACE "BTTF_TC"     /**< The namespace for all preferences to avoid conflicts. */
/** @} */

// Forward declaration of the StockManager class.
class StockManager;

/**
 * @name Global Extern Variables
 * @brief Extern declarations for globally accessible objects and state variables used by the web server.
 * @{
 */
extern AsyncWebServer server;           /**< The global instance of the asynchronous web server. */
extern AsyncWebSocket ws;               /**< The global instance of the WebSocket server. */
extern ClockSettings currentSettings;   /**< The global struct holding all current device settings. */
extern WeatherData currentWeatherData;  /**< The global struct holding the latest fetched weather data. */
extern StockManager stockManager;       /**< The global instance of the stock manager. */
extern String apiTemplatesJson;         /**< A string holding the JSON for API test templates. */
extern Preferences preferences;         /**< The global instance for accessing non-volatile storage. */
extern bool timeSynchronized;           /**< A flag indicating if the device time has been synchronized via NTP. */
extern bool ntpSyncRequested;           /**< A flag to request an NTP time synchronization. */
extern PubSubClient mqttClient;         /**< The global instance of the MQTT client. */
extern bool mqttReconnectRequired;      /**< A flag to signal that the MQTT client needs to reconnect. */
extern const char TZ_JSON[] PROGMEM;    /**< A PROGMEM string containing the list of timezones in JSON format. */
extern std::string lastCityName;        /**< The last known city name, used to detect changes. */
extern SemaphoreHandle_t xDisplayDataMutex; /**< A mutex to protect shared data structures related to display content. */
/** @} */


/**
 * @brief A struct to hold parameters for testing API endpoints from the web UI.
 */
struct ApiTestParams {
    String url;         /**< The URL of the API endpoint to test. */
    String authKey;     /**< The authentication key, if required. */
    String authValue;   /**< The authentication value, if required. */
    uint32_t clientId;  /**< The ID of the WebSocket client that initiated the test. */
    String action;      /**< The specific test action to perform. */
    String rowIndex;    /**< The display row index associated with the test. */
    String symbol;      /**< The stock symbol associated with the test. */
};


/**
 * @name Forward-Declared Functions
 * @brief Functions defined in other files (typically the main .ino) but needed by the web server.
 * @{
 */
extern JsonVariant getJsonVariant(JsonVariant root, const char* path);
extern void saveSettings();
extern void loadSettings();
extern void startTimeTravelAnimation();
void fetchWeatherDataTask(void* p);
/** @} */


/**
 * @name Web Server and WebSocket Functions
 * @{
 */

/** @brief Sets up all the web server routes (API endpoints and file serving). */
void setupWebRoutes();

/**
 * @brief Sends a full snapshot of all current settings to a specific WebSocket client.
 * @param clientId The ID of the client to send the settings to.
 */
void sendFullSettingsToClient(uint32_t clientId);

/** @} */


/**
 * @name WebSocket Broadcasting Functions
 * @brief Functions to send targeted state updates to all connected WebSocket clients.
 * @details These functions are used to keep the UI synchronized with the device's state in real-time.
 * @{
 */
void broadcastPresetUpdate(const std::string& name, int year, int month, int day, int hour, int minute);
void broadcastWsStateUpdate(const char* key, const JsonVariant& value);
void broadcastWsStateUpdate(const char* key, int value);
void broadcastWsStateUpdate(const char* key, bool value);
void broadcastWeatherUpdate();
void broadcastRadioStatus(RadioStatus status, const char* message = "");
void broadcastRadioStationsUpdated();
void broadcastRadioMetadata(const char* stationName, const char* songTitle);
/** @} */


#endif // WEB_SERVER_H