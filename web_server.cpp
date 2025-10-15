/**
 * @file web_server.cpp
 * @brief Implements the asynchronous web server and WebSocket communication.
 * @details This file sets up all the necessary routes for serving the web interface
 * (HTML, CSS, JS), provides a RESTful API for getting and setting the clock's
 * configuration, and manages a WebSocket connection for real-time, bidirectional
 * communication with the web UI.
 */
#include "timezone.h"
#include "DebugLog.h"
#include "web_server.h"
#include "StockManager.h"
#include "api_templates.h"
#include "DataManager.h"
#include "timezone.h"
#include "EventManager.h"
#include "AnimationTypes.h"
#include "MqttManager.h"
#include <AsyncJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <string>
#include <mutex>
#include <set>
#include <WiFi.h>
#include <Update.h>
#include <ArduinoOTA.h>
#include "FS.h"
#include <LITTLEFS.h>

// --- Mutexes and state for thread-safe operations ---
static std::mutex httpClientMutex;

// --- Statically Allocated JSON Document for Web Requests ---
// This single, static JsonDocument is reused for all web server and WebSocket
// operations to prevent heap fragmentation from repeated allocations.
// The size is chosen to be large enough for the biggest JSON payload (full settings).
static StaticJsonDocument<4096> webRequestJsonDoc;

// --- Extern Global Variables ---
// These are defined in the main .ino file and are made available here.

// --- Forward Declarations ---
// These functions are defined in the main .ino file and are called from here.
void applyAndSaveSettings(JsonVariant& json);
void startStyledAnimation();


/**
 * @brief A PROGMEM string containing a JSON object of API endpoint examples.
 * @details This large string is stored in flash memory to save RAM. It provides a list of
 * pre-configured API examples that the user can select from in the "Data Link" tab of the
 * web interface. This allows users to quickly populate the URL and other fields for common
 * data sources like weather, stocks, and cryptocurrency without having to manually look up
 * the API documentation.
 */
const char apiTemplates[] PROGMEM = "{\n"
    "    \"stock_aapl_price\": {\n"
    "        \"name\": \"Stock: Apple Price\",\n"
    "        \"url\": \"https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=AAPL&apikey=YOUR_API_KEY\",\n"
    "        \"note\": \"Requires a free API key from alphavantage.co\"\n"
    "    },\n"
    "    \"stock_aapl_change\": {\n"
    "        \"name\": \"Stock: Apple Change %\",\n"
    "        \"url\": \"https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=AAPL&apikey=YOUR_API_KEY\",\n"
    "        \"note\": \"Requires a free API key from alphavantage.co\"\n"
    "    },\n"
    "    \"crypto_btc_price\": {\n"
    "        \"name\": \"Crypto: Bitcoin Price\",\n"
    "        \"url\": \"https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd\"\n"
    "    },\n"
    "    \"crypto_eth_change\": {\n"
    "        \"name\": \"Crypto: Ethereum Change %\",\n"
    "        \"url\": \"https://api.coingecko.com/api/v3/simple/price?ids=ethereum&vs_currencies=usd&include_24hr_change=true\"\n"
    "    },\n"
    "    \"weather_temp\": {\n"
    "        \"name\": \"Weather: Temperature (°F)\",\n"
    "        \"url\": \"https://api.open-meteo.com/v1/forecast?latitude=40.71&longitude=-74.01&current=temperature_2m&temperature_unit=fahrenheit\"\n"
    "    },\n"
    "    \"weather_feels_like\": {\n"
    "        \"name\": \"Weather: Feels Like (°F)\",\n"
    "        \"url\": \"https://api.open-meteo.com/v1/forecast?latitude=40.71&longitude=-74.01&current=apparent_temperature&temperature_unit=fahrenheit\"\n"
    "    },\n"
    "    \"weather_humidity\": {\n"
    "        \"name\": \"Weather: Humidity\",\n"
    "        \"url\": \"https://api.open-meteo.com/v1/forecast?latitude=40.71&longitude=-74.01&current=relative_humidity_2m\"\n"
    "    },\n"
    "    \"weather_wind_speed\": {\n"
    "        \"name\": \"Weather: Wind Speed\",\n"
    "        \"url\": \"https://api.open-meteo.com/v1/forecast?latitude=40.71&longitude=-74.01&current=wind_speed_10m&wind_speed_unit=mph\"\n"
    "    },\n"
    "    \"space_iss_pos\": {\n"
    "        \"name\": \"Space: ISS Position\",\n"
    "        \"url\": \"http://api.open-notify.org/iss-now.json\"\n"
    "    },\n"
    "    \"space_astros\": {\n"
    "        \"name\": \"Space: People in Space\",\n"
    "        \"url\": \"http://api.open-notify.org/astros.json\"\n"
    "    },\n"
    "    \"space_sun_dist\": {\n"
    "        \"name\": \"Space: Sun Distance\",\n"
    "        \"url\": \"https://api.le-systeme-solaire.net/rest/bodies/soleil\"\n"
    "    },\n"
    "    \"util_ip\": {\n"
    "        \"name\": \"Utility: Public IP\",\n"
    "        \"url\": \"http://ip-api.com/json\"\n"
    "    },\n"
    "    \"util_network_info\": {\n"
    "        \"name\": \"Utility: Network Info\",\n"
    "        \"url\": \"http://ip-api.com/json\"\n"
    "    },\n"
    "    \"util_day_of_year\": {\n"
    "        \"name\": \"Utility: Day of Year\",\n"
    "        \"url\": \"http://worldtimeapi.org/api/ip\"\n"
    "    },\n"
    "    \"util_github_commits\": {\n"
    "        \"name\": \"Utility: GitHub Commits\",\n"
    "        \"url\": \"https://api.github.com/repos/octocat/Hello-World/commits\"\n"
    "    },\n"
    "    \"fun_yt_subs\": {\n"
    "        \"name\": \"Fun: YouTube Subscribers\",\n"
    "        \"url\": \"https://www.googleapis.com/youtube/v3/channels?part=statistics&id=UC_x5XG1OV2P6uZZ5FSM9Ttw&key=YOUR_API_KEY\",\n"
    "        \"note\": \"Requires a free API key from Google Cloud\"\n"
    "    },\n"
    "    \"fun_twitch_viewers\": {\n"
    "        \"name\": \"Fun: Twitch Viewers\",\n"
    "        \"url\": \"https://api.twitch.tv/helix/streams?user_login=shroud\",\n"
    "        \"note\": \"Requires Authentication (Client ID and OAuth Token)\"\n"
    "    },\n"
    "    \"fun_holiday_countdown\": {\n"
    "        \"name\": \"Fun: Holiday Countdown (see note)\",\n"
    "        \"url\": \"http://worldtimeapi.org/api/ip\"\n"
    "    },\n"
    "    \"fun_game_users\": {\n"
    "        \"name\": \"Fun: Game Server Users\",\n"
    "        \"url\": \"https://api.steampowered.com/ISteamUserStats/GetNumberOfCurrentPlayers/v1/?appid=730\"\n"
    "    }\n"
    "}";

AsyncWebSocket ws("/ws");

// Declare the new function that will be defined in the .ino file
void forceFetchWeatherDataTask(void* p);

void broadcastWsStateUpdate(const char* key, const JsonVariant& value) {
    if (ws.count() > 0) {
        webRequestJsonDoc.clear();
        webRequestJsonDoc["action"] = "stateUpdate";
        webRequestJsonDoc["key"] = key;
        webRequestJsonDoc["value"] = value;
        String jsonString;
        serializeJson(webRequestJsonDoc, jsonString);
        ws.textAll(jsonString);
    }
}

/**
 * @brief Broadcasts the latest weather data to all connected WebSocket clients.
 * @details This function is called whenever new weather data is successfully fetched.
 * It serializes the `currentWeatherData` struct into a JSON object and sends it
 * to the UI to update the weather display in real-time without needing a page refresh.
 */
void broadcastWeatherUpdate() {
    // Only proceed if there are active clients and the weather data is valid
    if (ws.count() > 0 && currentWeatherData.dataValid) {
        webRequestJsonDoc.clear();
        webRequestJsonDoc["action"] = "weatherUpdate";

        // Create a nested 'data' object to hold the weather information.
        // This keeps the message structure consistent with other actions.
        JsonObject data = webRequestJsonDoc["data"].to<JsonObject>();
        data["temperature"] = currentWeatherData.temperature;
        data["apparentTemperature"] = currentWeatherData.apparentTemperature;
        data["windSpeed"] = currentWeatherData.windSpeed;
        data["humidity"] = currentWeatherData.humidity;
        data["weatherCode"] = currentWeatherData.weatherCode;
        data["dailyHigh"] = currentWeatherData.dailyHigh;
        data["dailyLow"] = currentWeatherData.dailyLow;
        data["latitude"] = currentWeatherData.latitude;
        data["longitude"] = currentWeatherData.longitude;
        data["sunrise"] = currentWeatherData.sunrise;
        data["sunset"] = currentWeatherData.sunset;
        data["precipitationProbability"] = currentWeatherData.precipitationProbability;
        data["maxWindSpeed"] = currentWeatherData.maxWindSpeed;
        data["tomorrowHigh"] = currentWeatherData.tomorrowHigh;
        data["tomorrowLow"] = currentWeatherData.tomorrowLow;
        data["tomorrowWeatherCode"] = currentWeatherData.tomorrowWeatherCode;

        // Create a nested array for the 3-hour forecast
        JsonArray hourly = data["hourly"].to<JsonArray>();
        for (int i = 0; i < 3; i++) {
            JsonObject hour = hourly.add<JsonObject>();
            hour["temp"] = currentWeatherData.hourlyTemp[i];
            hour["code"] = currentWeatherData.hourlyCode[i];
        }

        // Serialize the JSON document to a string and send it to all clients
        String jsonString;
        serializeJson(webRequestJsonDoc, jsonString);
        ws.textAll(jsonString);
        Log_printf(LOG_LEVEL_INFO, "Broadcasted weather update to %d clients.", ws.count());
    }
}

/**
 * @brief Broadcasts a stock update notification to all connected WebSocket clients.
 * @details This function is called after a newly added stock has its data fetched,
 * prompting the UI to refresh its stock information display.
 */
void broadcastStockUpdate() {
    if (ws.count() > 0) {
        webRequestJsonDoc.clear();
        webRequestJsonDoc["action"] = "stockUpdate";
        String jsonString;
        serializeJson(webRequestJsonDoc, jsonString);
        ws.textAll(jsonString);
        Log_printf(LOG_LEVEL_INFO, "Broadcasted stock update to %d clients.", ws.count());
    }
}

/**
 * @brief Broadcasts the current internet radio status to all connected WebSocket clients.
 * @param status The current status of the radio (e.g., PLAYING, STOPPED).
 * @param message An optional message, typically used for error details.
 */
void broadcastRadioStatus(RadioStatus status, const char* message) {
    if (ws.count() > 0) {
        webRequestJsonDoc.clear();
        webRequestJsonDoc["action"] = "radioStatusUpdate";

        // Convert enum to a string for the UI
        switch (status) {
            case RADIO_STATUS_STOPPED:
                webRequestJsonDoc["status"] = "stopped";
                break;
            case RADIO_STATUS_CONNECTING:
                webRequestJsonDoc["status"] = "connecting";
                break;
            case RADIO_STATUS_PLAYING:
                webRequestJsonDoc["status"] = "playing";
                break;
            case RADIO_STATUS_ERROR:
                webRequestJsonDoc["status"] = "error";
                break;
        }

        webRequestJsonDoc["message"] = message;

        String jsonString;
        serializeJson(webRequestJsonDoc, jsonString);
        ws.textAll(jsonString);
        Log_printf(LOG_LEVEL_INFO, "Broadcasted radio status: %s", jsonString.c_str());
    }
}

/**
 * @brief Broadcasts a notification that the radio station list has been updated.
 */
void broadcastRadioStationsUpdated() {
    if (ws.count() > 0) {
        webRequestJsonDoc.clear();
        webRequestJsonDoc["action"] = "radioStationsUpdated";
        String jsonString;
        serializeJson(webRequestJsonDoc, jsonString);
        ws.textAll(jsonString);
        Log_printf(LOG_LEVEL_INFO, "Broadcasted radio stations updated notification.");
    }
}

/**
 * @brief Broadcasts the current radio metadata to all connected WebSocket clients.
 * @param stationName The name of the radio station.
 * @param songTitle The title of the currently playing song.
 */
void broadcastRadioMetadata(const char* stationName, const char* songTitle) {
    if (ws.count() > 0) {
        webRequestJsonDoc.clear();
        webRequestJsonDoc["action"] = "radioMetadataUpdate";
        webRequestJsonDoc["stationName"] = stationName;
        webRequestJsonDoc["songTitle"] = songTitle;
        String jsonString;
        serializeJson(webRequestJsonDoc, jsonString);
        ws.textAll(jsonString);
        Log_printf(LOG_LEVEL_INFO, "Broadcasted radio metadata: %s", jsonString.c_str());
    }
}

/**
 * @brief Overloaded function to broadcast an integer state update via WebSocket.
 */
void broadcastWsStateUpdate(const char* key, int value) {
    webRequestJsonDoc.clear();
    webRequestJsonDoc.set(value);
    broadcastWsStateUpdate(key, webRequestJsonDoc.as<JsonVariant>());
}

void broadcastPresetUpdate(const std::string& name, int year, int month, int day, int hour, int minute) {
    if (ws.count() > 0) {
        webRequestJsonDoc.clear();
        webRequestJsonDoc["action"] = "presetUpdate";
        webRequestJsonDoc["name"] = name.c_str();
        char value[20];
        sprintf(value, "%d-%02d-%02d-%02d-%02d", year, month, day, hour, minute);
        webRequestJsonDoc["value"] = value;
        String jsonString;
        serializeJson(webRequestJsonDoc, jsonString);
        ws.textAll(jsonString);
    }
}

/**
 * @brief Overloaded function to broadcast a boolean state update via WebSocket.
 */
void broadcastWsStateUpdate(const char* key, bool value) {
    webRequestJsonDoc.clear();
    webRequestJsonDoc.set(value);
    broadcastWsStateUpdate(key, webRequestJsonDoc.as<JsonVariant>());
}

// This function runs in a separate task to prevent blocking
void makeApiRequestTask(void* p) {
    ApiTestParams* params = (ApiTestParams*)p;
    String urlStr = params->url;
    String authKey = params->authKey;
    String authValue = params->authValue;
    uint32_t clientId = params->clientId;
    String action = params->action;
    String rowIndex = params->rowIndex;
    String symbol = params->symbol; // Retrieve symbol
    delete params; // Clean up the params object immediately

    std::lock_guard<std::mutex> lock(httpClientMutex);
    
    WiFiClientSecure client;
    HTTPClient http;
    client.setInsecure();

    if (http.begin(client, urlStr)) {
        if (authKey.length() > 0 && authValue.length() > 0) {
            http.addHeader(authKey, authValue);
        }
        
        int httpCode = http.GET();
        String responseString;
        webRequestJsonDoc.clear();
        webRequestJsonDoc["action"] = action;
        webRequestJsonDoc["rowIndex"] = rowIndex; // Pass as String

        if (httpCode > 0) {
            if (httpCode == HTTP_CODE_OK) {
                // Use a temporary doc for the payload to avoid nesting issues
                StaticJsonDocument<2048> payloadDoc;
                DeserializationError error = deserializeJson(payloadDoc, http.getStream());

                if (error == DeserializationError::Ok) {
                    webRequestJsonDoc["status"] = "success";
                    webRequestJsonDoc["payload"] = payloadDoc.as<JsonVariant>();
                } else {
                    webRequestJsonDoc["status"] = "error";
                    webRequestJsonDoc["payload"] = "JSON Parsing Failed: " + String(error.c_str());
                }
            } else {
                webRequestJsonDoc["status"] = "error";
                // --- START: MODIFICATION ---
                // Grab the response body to provide a more detailed error message.
                String responseBody = http.getString();
                webRequestJsonDoc["payload"] = "HTTP Error: " + String(httpCode) + " - " + responseBody;
                // --- END: MODIFICATION ---
            }
        } else {
            webRequestJsonDoc["status"] = "error";
            webRequestJsonDoc["payload"] = "Request Failed: " + http.errorToString(httpCode);
        }
        
        http.end();
        serializeJson(webRequestJsonDoc, responseString);
        ws.text(clientId, responseString);
    } else {
        // --- START: MODIFICATION - Handle http.begin() failure ---
        String responseString;
        webRequestJsonDoc.clear();
        webRequestJsonDoc["action"] = action;
        webRequestJsonDoc["rowIndex"] = rowIndex; // Pass as String
        webRequestJsonDoc["status"] = "error";
        webRequestJsonDoc["payload"] = "Connection Failed. Check URL/DNS.";
        serializeJson(webRequestJsonDoc, responseString);
        ws.text(clientId, responseString);
        // --- END: MODIFICATION ---
    }

    vTaskDelete(NULL); // End the task
}

/**
 * @brief Handles all incoming WebSocket events.
 * @details This function is the central callback for the WebSocket server. It handles
 * new client connections, disconnections, and incoming data messages. For data
 * messages, it parses the JSON payload to determine the requested action (e.g.,
 * 'testApi', 'testStock') and then creates a dedicated FreeRTOS task to handle
 * the request asynchronously.
 * @param server Pointer to the WebSocket server instance.
 * @param client Pointer to the client that triggered the event.
 * @param type The type of WebSocket event that occurred.
 * @param arg A pointer to additional event-specific arguments.
 * @param data A pointer to the payload data for data events.
 * @param len The length of the payload data.
 */
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Log_printf(LOG_LEVEL_INFO, "WebSocket client #%u connected from %s", client->id(), client->remoteIP().toString().c_str());
        // Send the full settings object to the newly connected client
        sendFullSettingsToClient(client->id());
    } else if (type == WS_EVT_DISCONNECT) {
        Log_printf(LOG_LEVEL_INFO, "WebSocket client #%u disconnected", client->id());
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            
            webRequestJsonDoc.clear();
            DeserializationError error = deserializeJson(webRequestJsonDoc, data, len);

            if (error) {
                Log_printf(LOG_LEVEL_ERROR, "deserializeJson() failed: %s", error.c_str());
                return;
            }

            String action = webRequestJsonDoc["action"].as<String>();
             if (action == "testApi") {
                Log_printf(LOG_LEVEL_DEBUG, "'testApi' action received.");
                if (!timeSynchronized) {
                    String responseString;
                    webRequestJsonDoc.clear();
                    webRequestJsonDoc["action"] = "apiResult";
                    webRequestJsonDoc["status"] = "error";
                    webRequestJsonDoc["payload"] = "Time not sync'd. Go to System->Sync Time.";
                    serializeJson(webRequestJsonDoc, responseString);
                    ws.text(client->id(), responseString);
                    return;
                }

                String url = webRequestJsonDoc["data"]["url"];
                String authKey = webRequestJsonDoc["data"]["authKey"];
                String authValue = webRequestJsonDoc["data"]["authValue"];
                 Log_printf(LOG_LEVEL_DEBUG, "API Test URL: %s", url.c_str());

                ApiTestParams* params = new ApiTestParams{url, authKey, authValue, client->id(), "apiResult", String(0)};

                BaseType_t taskCreated = xTaskCreate(makeApiRequestTask, "apiTestTask", 8192, params, 1, NULL);
                if (taskCreated != pdPASS) {
                    delete params;
                     Log_printf(LOG_LEVEL_ERROR, "Failed to create API test task!");
                }
            } else if (action == "play_favorite_radio") {
                Log_printf(LOG_LEVEL_INFO, "WebSocket: Play favorite radio command received.");
                if (!currentSettings.favoriteRadioUrl.empty()) {
                    startAudioStream(currentSettings.favoriteRadioUrl.c_str(), false);
                } else {
                    Log_printf(LOG_LEVEL_WARN, "Favorite radio URL is not set. Cannot play.");
                }
            } else if (action == "stop_radio") {
                Log_printf(LOG_LEVEL_INFO, "WebSocket: Stop radio command received.");
                stopAudioStream();
            } else if (action == "run_sequence") {
                String payload = webRequestJsonDoc["payload"].as<String>();
                if (payload.length() > 0) {
                    Log_printf(LOG_LEVEL_INFO, "WebSocket: Run sequence command received.");
                    handleSequencerCommand(payload.c_str());
                }
            } else if (action == "preview_animation") {
                int animationId = webRequestJsonDoc["payload"];
                Log_printf(LOG_LEVEL_INFO, "WebSocket: Preview animation command received for ID: %d", animationId);
                triggerAnimation(static_cast<AnimationType>(animationId));
            }
        }
    }
}


/**
 * @brief Sends a comprehensive JSON object of all current settings to a specific WebSocket client.
 * @details This function is called when a new UI client connects. It gathers all settings
 * from the `currentSettings` global object, serializes them into a single JSON message,
 * and pushes it directly to the connecting client. This ensures the UI is immediately
 * synchronized with the device's state without needing to make multiple API calls.
 * @param clientId The unique ID of the WebSocket client to send the settings to.
 */
void sendFullSettingsToClient(uint32_t clientId) {
    webRequestJsonDoc.clear();
    webRequestJsonDoc["action"] = "fullSettings";

    // --- Add Time Circuits settings ---
    webRequestJsonDoc["destinationYear"] = currentSettings.destinationYear;
    webRequestJsonDoc["destinationTimezoneIndex"] = currentSettings.destinationTimezoneIndex;
    webRequestJsonDoc["lastTimeDepartedYear"] = currentSettings.lastTimeDepartedYear;
    webRequestJsonDoc["lastTimeDepartedMonth"] = currentSettings.lastTimeDepartedMonth;
    webRequestJsonDoc["lastTimeDepartedDay"] = currentSettings.lastTimeDepartedDay;
    webRequestJsonDoc["lastTimeDepartedHour"] = currentSettings.lastTimeDepartedHour;
    webRequestJsonDoc["lastTimeDepartedMinute"] = currentSettings.lastTimeDepartedMinute;
    webRequestJsonDoc["presentTimezoneIndex"] = currentSettings.presentTimezoneIndex;

    // --- Add Temporal settings ---
    webRequestJsonDoc["departureHour"] = currentSettings.departureHour;
    webRequestJsonDoc["departureMinute"] = currentSettings.departureMinute;
    webRequestJsonDoc["arrivalHour"] = currentSettings.arrivalHour;
    webRequestJsonDoc["arrivalMinute"] = currentSettings.arrivalMinute;
    webRequestJsonDoc["brightness"] = currentSettings.brightness;
    webRequestJsonDoc["notificationVolume"] = currentSettings.notificationVolume;
    webRequestJsonDoc["timeTravelAnimationDuration"] = currentSettings.timeTravelAnimationDuration;
    webRequestJsonDoc["timeTravelAnimationInterval"] = currentSettings.timeTravelAnimationInterval;
    webRequestJsonDoc["animationStyle"] = currentSettings.animationStyle;
    webRequestJsonDoc["animationSequence"] = animationTypeToString(currentSettings.animationSequence);
    webRequestJsonDoc["timeTravelSoundToggle"] = currentSettings.timeTravelSoundToggle;
    webRequestJsonDoc["presetCycleInterval"] = currentSettings.presetCycleInterval;
    webRequestJsonDoc["displayFormat24h"] = currentSettings.displayFormat24h;
    webRequestJsonDoc["favoriteRadioName"] = currentSettings.favoriteRadioName.c_str();
    webRequestJsonDoc["favoriteRadioUrl"] = currentSettings.favoriteRadioUrl.c_str();

    // --- Add Data Link and other settings ---
    webRequestJsonDoc["displayMode"] = currentSettings.displayMode;
    webRequestJsonDoc["numDataPoints"] = currentSettings.numDataPoints;
    webRequestJsonDoc["mqttBroker"] = currentSettings.mqttBroker.c_str();
    webRequestJsonDoc["mqttPort"] = currentSettings.mqttPort;
    webRequestJsonDoc["mqttUser"] = currentSettings.mqttUser.c_str();
    webRequestJsonDoc["mqttPassword"] = currentSettings.mqttPassword.c_str();
    webRequestJsonDoc["cityName"] = currentSettings.cityName.c_str();
    webRequestJsonDoc["useMetricUnits"] = currentSettings.useMetricUnits;
    webRequestJsonDoc["latitude"] = currentSettings.latitude;
    webRequestJsonDoc["longitude"] = currentSettings.longitude;
    webRequestJsonDoc["stockRefreshInterval"] = currentSettings.stockRefreshInterval;
    webRequestJsonDoc["financialModelingPrepApiKey"] = currentSettings.financialModelingPrepApiKey.c_str();
    webRequestJsonDoc["stockRow1_symbol"] = currentSettings.stockRow1_symbol.c_str();
    webRequestJsonDoc["stockRow2_symbol"] = currentSettings.stockRow2_symbol.c_str();
    webRequestJsonDoc["stockRow3_symbol"] = currentSettings.stockRow3_symbol.c_str();

    JsonArray dataPoints = webRequestJsonDoc["dataPoints"].to<JsonArray>();
    for (int i = 0; i < 5; i++) {
        JsonObject dp = dataPoints.add<JsonObject>();
        dp["dataSourceType"] = (int)currentSettings.dataPoints[i].dataSourceType;
        dp["mqttTopic"] = currentSettings.dataPoints[i].mqttTopic.c_str();
        dp["scrollingText"] = currentSettings.dataPoints[i].scrollingText.c_str();
        dp["scrollSpeed"] = currentSettings.dataPoints[i].scrollSpeed;
        dp["prefixText"] = currentSettings.dataPoints[i].prefixText.c_str();
        dp["suffixText"] = currentSettings.dataPoints[i].suffixText.c_str();
    }

    String response;
    serializeJson(webRequestJsonDoc, response);
    ws.text(clientId, response);
    Log_printf(LOG_LEVEL_INFO, "Pushed full settings to client #%u.", clientId);
}

/**
 * @brief Configures and attaches all web server and WebSocket routes.
 * @details This function is called once from the main `setup()` function. It sets up
 * the WebSocket event handler and then defines all the routes for the web server.
 * This includes routes for serving static files (HTML, CSS, JS) from LittleFS and
 * a series of RESTful API endpoints for interacting with the clock's settings and state.
 */
void setupWebRoutes() {
  Log_printf(LOG_LEVEL_INFO, "Inside setupWebRoutes(). Attaching handlers...");

  // Attach the WebSocket event handler.
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ 
    Log_printf(LOG_LEVEL_DEBUG, "Client requested /index.html");
    request->send(LittleFS, "/index.html", "text/html"); 
  });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){ 
    Log_printf(LOG_LEVEL_DEBUG, "Client requested /style.css");
    request->send(LittleFS, "/style.css", "text/css"); 
  });
  server.on("/data_handling.js", HTTP_GET, [](AsyncWebServerRequest *request){ 
    Log_printf(LOG_LEVEL_DEBUG, "Client requested /data_handling.js");
    request->send(LittleFS, "/data_handling.js", "application/javascript"); 
  });
  server.on("/main_ui.js", HTTP_GET, [](AsyncWebServerRequest *request){ 
    Log_printf(LOG_LEVEL_DEBUG, "Client requested /main_ui.js");
    request->send(LittleFS, "/main_ui.js", "application/javascript"); 
  });
  server.on("/ui_functions.js", HTTP_GET, [](AsyncWebServerRequest *request){
    Log_printf(LOG_LEVEL_DEBUG, "Client requested /ui_functions.js");
    request->send(LittleFS, "/ui_functions.js", "application/javascript");
  });
  server.on("/api/isReady", HTTP_GET, [](AsyncWebServerRequest *request){ 
    Log_printf(LOG_LEVEL_DEBUG, "Client requested /api/isReady");
    request->send(200, "text/plain", "READY"); 
  });
  
  server.on("/api/greatScott", HTTP_POST, [](AsyncWebServerRequest *request){
    if (hardwareInitialized) {
        playSound("/EASTER_EGG.mp3", false, -1);
    }
    request->send(200, "text/plain", "Great Scott!");
  });

  server.on("/api/settings/BTTF_TC", HTTP_GET, [](AsyncWebServerRequest *request) {
    webRequestJsonDoc.clear();
    webRequestJsonDoc["destinationYear"] = currentSettings.destinationYear;
    webRequestJsonDoc["destinationTimezoneIndex"] = currentSettings.destinationTimezoneIndex;
    webRequestJsonDoc["lastTimeDepartedYear"] = currentSettings.lastTimeDepartedYear;
    webRequestJsonDoc["lastTimeDepartedMonth"] = currentSettings.lastTimeDepartedMonth;
    webRequestJsonDoc["lastTimeDepartedDay"] = currentSettings.lastTimeDepartedDay;
    webRequestJsonDoc["lastTimeDepartedHour"] = currentSettings.lastTimeDepartedHour;
    webRequestJsonDoc["lastTimeDepartedMinute"] = currentSettings.lastTimeDepartedMinute;
    webRequestJsonDoc["presentTimezoneIndex"] = currentSettings.presentTimezoneIndex;
    String jsonString;
    serializeJson(webRequestJsonDoc, jsonString);
    request->send(200, "application/json", jsonString);
  });

  server.on("/api/stocks/search", HTTP_GET, [](AsyncWebServerRequest *request) {
    Log_printf(LOG_LEVEL_WARN, "Handling /api/stocks/search request");
    if (!request->hasParam("q") || !request->hasParam("apikey")) {
        request->send(400, "text/plain", "Missing query or apikey parameter");
        return;
    }
    String query = request->getParam("q")->value();
    String apiKey = request->getParam("apikey")->value();

    WiFiClientSecure client;
    client.setInsecure(); // For simplicity, though not recommended for production
    HTTPClient http;
    String url = "https://financialmodelingprep.com/stable/quote?symbol=" + query + "&apikey=" + apiKey;
    String log_url = "https://financialmodelingprep.com/stable/quote?symbol=" + query + "&apikey=REDACTED";
    Log_printf(LOG_LEVEL_WARN, "Proxying stock search to: %s", log_url.c_str());

    if (http.begin(client, url)) {
        int httpCode = http.GET();
        if (httpCode > 0) {
            if (httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                Log_printf(LOG_LEVEL_INFO, "Stock search response: %s", payload.c_str());
                request->send(200, "application/json", payload);
            } else {
                String errorPayload = http.getString();
                Log_printf(LOG_LEVEL_ERROR, "Stock search error response: %s", errorPayload.c_str());
                request->send(httpCode, "text/plain", errorPayload);
            }
        } else {
            request->send(500, "text/plain", "Request failed");
        }
        http.end();
    } else {
        request->send(500, "text/plain", "Unable to connect");
    }
  });

  server.on("/api/stocks/validate", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("symbol")) {
        request->send(400, "application/json", "{\"error\":\"Missing symbol parameter\"}");
        return;
    }
    String symbol = request->getParam("symbol")->value();
    Log_printf(LOG_LEVEL_INFO, "Handling /api/stocks/validate request for symbol: %s", symbol.c_str());

    String response = stockManager.validateSymbol(symbol);

    // The response is already JSON, so send it directly.
    // Set the content type to application/json to ensure browsers render it correctly.
    request->send(200, "application/json", response);
  });

  server.on("/api/stocks/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (currentSettings.displayMode != DMS_STOCK_TICKER) {
        request->send(400, "application/json", "{\"error\":\"Stock Ticker Mode is disabled.\"}");
        return;
    }
    if (stockManager.getApiKey().isEmpty()) {
        request->send(400, "application/json", "{\"error\":\"API key is not set.\"}");
        return;
    }

    webRequestJsonDoc.clear();
    webRequestJsonDoc["api_usage"] = stockManager.getApiUsage();
    JsonArray assets = webRequestJsonDoc["assets"].to<JsonArray>();
    for (const auto& asset : stockManager.getAssets()) {
        JsonObject assetObj = assets.add<JsonObject>();
        assetObj["symbol"] = asset.symbol;
        assetObj["price"] = asset.price;
        assetObj["change_percent"] = asset.change_percent;
        assetObj["data_valid"] = asset.data_valid;
        assetObj["error_reason"] = asset.error_reason;
    }
    String jsonString;
    serializeJson(webRequestJsonDoc, jsonString);
    request->send(200, "application/json", jsonString);
  });

  server.on("/api/stocks/marquee", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (currentSettings.displayMode != DMS_STOCK_TICKER) {
        request->send(400, "application/json", "{\"error\":\"Stock Ticker Mode is disabled.\"}");
        return;
    }
    if (stockManager.getApiKey().isEmpty()) {
        request->send(400, "application/json", "{\"error\":\"API key is not set.\"}");
        return;
    }
    String marqueeLine = stockManager.getMarqueeLine();
    webRequestJsonDoc.clear();
    webRequestJsonDoc["marqueeText"] = marqueeLine;
    String jsonString;
    serializeJson(webRequestJsonDoc, jsonString);
    request->send(200, "application/json", jsonString);
  });

  server.on("/api/stocks", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (stockManager.getAssets().empty()) {
        request->send(200, "application/json", "[]");
        return;
    }

    webRequestJsonDoc.clear();
    JsonArray assets = webRequestJsonDoc.to<JsonArray>();
    for (const auto& asset : stockManager.getAssets()) {
        JsonObject assetObj = assets.add<JsonObject>();
        assetObj["symbol"] = asset.symbol;
        assetObj["name"] = asset.name;
        assetObj["price"] = asset.price;
        assetObj["change_percent"] = asset.change_percent;
        assetObj["data_valid"] = asset.data_valid;
    }
    String jsonString;
    serializeJson(webRequestJsonDoc, jsonString);
    request->send(200, "application/json", jsonString);
  });

  AsyncCallbackJsonWebHandler* addStockHandler = new AsyncCallbackJsonWebHandler("/api/stocks/add", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject obj = json.as<JsonObject>();
    if (obj["symbol"].isNull()) {
        request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Missing symbol.\"}");
        return;
    }

    String symbol;
    if (obj["symbol"].is<JsonObject>()) {
        symbol = obj["symbol"]["symbol"].as<String>();
    } else {
        symbol = obj["symbol"].as<String>();
    }
    AssetAddResult result = stockManager.addAsset(symbol);

    switch (result) {
        case SUCCESS: {
            stockManager.saveAssets();
            // --- START: MODIFICATION - Send a success message on add ---
            request->send(200, "application/json", "{\"status\":\"success\"}");
            // --- END: MODIFICATION ---
            break;
        }
        case ALREADY_EXISTS:
            request->send(409, "application/json", "{\"status\":\"error\", \"message\":\"Asset already exists.\"}");
            break;
        case INVALID_SYMBOL:
            request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Invalid symbol or symbol not found.\"}");
            break;
        case ADD_ERROR:
        default:
            request->send(500, "application/json", "{\"status\":\"error\", \"message\":\"An unexpected error occurred.\"}");
            break;
    }
  });
  server.addHandler(addStockHandler);

  AsyncCallbackJsonWebHandler* deleteStockHandler = new AsyncCallbackJsonWebHandler("/api/stocks/delete", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject obj = json.as<JsonObject>();
    if (!obj["symbol"].isNull()) {
        String symbol = obj["symbol"];
        if (stockManager.removeAsset(symbol)) {
            stockManager.saveAssets();
            stockManager.fetchData();
            request->send(200, "application/json", "{\"status\":\"success\"}");
        } else {
            request->send(404, "application/json", "{\"status\":\"error\", \"message\":\"Asset not found.\"}");
        }
    } else {
        request->send(400, "application/json", "{\"status\":\"error\", \"message\":\"Missing symbol.\"}");
    }
  });
  server.addHandler(deleteStockHandler);

  server.on("/api/settings/temporal", HTTP_GET, [](AsyncWebServerRequest *request) {
    webRequestJsonDoc.clear();
    webRequestJsonDoc["departureHour"] = currentSettings.departureHour;
    webRequestJsonDoc["departureMinute"] = currentSettings.departureMinute;
    webRequestJsonDoc["arrivalHour"] = currentSettings.arrivalHour;
    webRequestJsonDoc["arrivalMinute"] = currentSettings.arrivalMinute;
    webRequestJsonDoc["brightness"] = currentSettings.brightness;
    webRequestJsonDoc["notificationVolume"] = currentSettings.notificationVolume;
    webRequestJsonDoc["timeTravelAnimationDuration"] = currentSettings.timeTravelAnimationDuration;
    webRequestJsonDoc["timeTravelAnimationInterval"] = currentSettings.timeTravelAnimationInterval;
    webRequestJsonDoc["animationStyle"] = currentSettings.animationStyle;
    webRequestJsonDoc["animationSequence"] = animationTypeToString(currentSettings.animationSequence);
    webRequestJsonDoc["timeTravelSoundToggle"] = currentSettings.timeTravelSoundToggle;
    webRequestJsonDoc["presetCycleInterval"] = currentSettings.presetCycleInterval;
    webRequestJsonDoc["displayFormat24h"] = currentSettings.displayFormat24h;
    String jsonString;
    serializeJson(webRequestJsonDoc, jsonString);
    request->send(200, "application/json", jsonString);
  });

  server.on("/api/settings/datalink", HTTP_GET, [](AsyncWebServerRequest *request) {
    webRequestJsonDoc.clear();
    webRequestJsonDoc["dataLinkEnabled"] = (currentSettings.displayMode == DMS_DATA_LINK);
    webRequestJsonDoc["numDataPoints"] = currentSettings.numDataPoints;
    webRequestJsonDoc["mqttBroker"] = currentSettings.mqttBroker.c_str();
    webRequestJsonDoc["mqttPort"] = currentSettings.mqttPort;
    webRequestJsonDoc["mqttUser"] = currentSettings.mqttUser.c_str();
    webRequestJsonDoc["mqttPassword"] = currentSettings.mqttPassword.c_str();
    webRequestJsonDoc["weatherModeEnabled"] = (currentSettings.displayMode == DMS_WEATHER);
    webRequestJsonDoc["cityName"] = currentSettings.cityName.c_str();
    webRequestJsonDoc["useMetricUnits"] = currentSettings.useMetricUnits;
    webRequestJsonDoc["latitude"] = currentSettings.latitude;
    webRequestJsonDoc["longitude"] = currentSettings.longitude;
    webRequestJsonDoc["stockTickerModeEnabled"] = (currentSettings.displayMode == DMS_STOCK_TICKER);
    webRequestJsonDoc["stockRefreshInterval"] = currentSettings.stockRefreshInterval;
    webRequestJsonDoc["financialModelingPrepApiKey"] = currentSettings.financialModelingPrepApiKey.c_str();
    webRequestJsonDoc["stockRow1_symbol"] = currentSettings.stockRow1_symbol.c_str();
    webRequestJsonDoc["stockRow2_symbol"] = currentSettings.stockRow2_symbol.c_str();
    webRequestJsonDoc["stockRow3_symbol"] = currentSettings.stockRow3_symbol.c_str();

    JsonArray dataPoints = webRequestJsonDoc["dataPoints"].to<JsonArray>();
    for (int i = 0; i < currentSettings.numDataPoints; i++) {
        JsonObject dp = dataPoints.add<JsonObject>();
        dp["scrollSpeed"] = currentSettings.dataPoints[i].scrollSpeed;
        dp["dataSourceType"] = (int)currentSettings.dataPoints[i].dataSourceType;
        dp["mqttTopic"] = currentSettings.dataPoints[i].mqttTopic.c_str();
        dp["scrollingText"] = currentSettings.dataPoints[i].scrollingText.c_str();
        dp["prefixText"] = currentSettings.dataPoints[i].prefixText.c_str();
        dp["suffixText"] = currentSettings.dataPoints[i].suffixText.c_str();
    }

    String response;
    serializeJson(webRequestJsonDoc, response);
    request->send(200, "application/json", response);
  });

  server.on("/api/timezones", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", TZ_JSON);
  });

  server.on("/api/radio_stations", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (LittleFS.exists("/radio_stations.json")) {
        request->send(LittleFS, "/radio_stations.json", "application/json");
    } else {
        request->send(200, "application/json", "[]");
    }
  });

  server.on("/api/sequences", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (LittleFS.exists("/sequences.json")) {
        request->send(LittleFS, "/sequences.json", "application/json");
    } else {
        request->send(200, "application/json", "[]");
    }
  });

  AsyncCallbackJsonWebHandler* saveStationHandler = new AsyncCallbackJsonWebHandler("/api/station/save", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject obj = json.as<JsonObject>();
    String name = obj["name"];
    String url = obj["url"];
    int index = obj["index"]; // Will be -1 if not provided

    if (name.isEmpty() || url.isEmpty()) {
        request->send(400, "text/plain", "Missing name or URL");
        return;
    }

    webRequestJsonDoc.clear();
    File file = LittleFS.open("/radio_stations.json", "r");
    if (file) {
        deserializeJson(webRequestJsonDoc, file);
        file.close();
    }
    JsonArray stations = webRequestJsonDoc.as<JsonArray>();

    if (index >= 0 && index < stations.size()) {
        // Update existing station
        JsonObject station = stations[index];
        station["name"] = name;
        station["url"] = url;
    } else {
        // Add new station
        JsonObject newStation = stations.add<JsonObject>();
        newStation["name"] = name;
        newStation["url"] = url;
    }

    file = LittleFS.open("/radio_stations.json", "w");
    serializeJson(webRequestJsonDoc, file);
    file.close();

    request->send(200, "text/plain", "Station saved");
    broadcastRadioStationsUpdated();
  });
  server.addHandler(saveStationHandler);

  AsyncCallbackJsonWebHandler* deleteStationHandler = new AsyncCallbackJsonWebHandler("/api/station/delete", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject obj = json.as<JsonObject>();
    if (obj["index"].isNull()) {
        request->send(400, "text/plain", "Missing index");
        return;
    }
    int index = obj["index"];

    webRequestJsonDoc.clear();
    File file = LittleFS.open("/radio_stations.json", "r");
    if (!file) {
        request->send(500, "text/plain", "Could not open stations file");
        return;
    }
    deserializeJson(webRequestJsonDoc, file);
    file.close();
    JsonArray stations = webRequestJsonDoc.as<JsonArray>();

    if (index >= 0 && index < stations.size()) {
        stations.remove(index);
    } else {
        request->send(400, "text/plain", "Invalid index");
        return;
    }

    file = LittleFS.open("/radio_stations.json", "w");
    serializeJson(webRequestJsonDoc, file);
    file.close();

    request->send(200, "text/plain", "Station deleted");
    broadcastRadioStationsUpdated();
  });
  server.addHandler(deleteStationHandler);

  server.on("/api/getPresets", HTTP_GET, [](AsyncWebServerRequest *request) {
    preferences.begin(PREFERENCES_NAMESPACE, true);
    String presets = preferences.getString("customPresets", "[]");
    preferences.end();
    request->send(200, "application/json", presets);
  });

  server.on("/api/time", HTTP_GET, [](AsyncWebServerRequest *request) {
    time_t now;
    time(&now);
    webRequestJsonDoc.clear();
    webRequestJsonDoc["unixTime"] = now;
    webRequestJsonDoc["timeSynchronized"] = timeSynchronized;
    String jsonString;
    serializeJson(webRequestJsonDoc, jsonString);
    request->send(200, "application/json", jsonString);
  });

  server.on("/api/weather", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (currentWeatherData.dataValid) {
        webRequestJsonDoc.clear();
        webRequestJsonDoc["temperature"] = currentWeatherData.temperature;
        webRequestJsonDoc["apparentTemperature"] = currentWeatherData.apparentTemperature;
        webRequestJsonDoc["windSpeed"] = currentWeatherData.windSpeed;
        webRequestJsonDoc["humidity"] = currentWeatherData.humidity;
        webRequestJsonDoc["weatherCode"] = currentWeatherData.weatherCode;
        webRequestJsonDoc["dailyHigh"] = currentWeatherData.dailyHigh;
        webRequestJsonDoc["dailyLow"] = currentWeatherData.dailyLow;
        webRequestJsonDoc["latitude"] = currentWeatherData.latitude;
        webRequestJsonDoc["longitude"] = currentWeatherData.longitude;
        webRequestJsonDoc["sunrise"] = currentWeatherData.sunrise;
        webRequestJsonDoc["sunset"] = currentWeatherData.sunset;
        webRequestJsonDoc["precipitationProbability"] = currentWeatherData.precipitationProbability;
        webRequestJsonDoc["maxWindSpeed"] = currentWeatherData.maxWindSpeed;
        webRequestJsonDoc["tomorrowHigh"] = currentWeatherData.tomorrowHigh;
        webRequestJsonDoc["tomorrowLow"] = currentWeatherData.tomorrowLow;
        webRequestJsonDoc["tomorrowWeatherCode"] = currentWeatherData.tomorrowWeatherCode;
        
        JsonArray hourly = webRequestJsonDoc["hourly"].to<JsonArray>();
        for (int i = 0; i < 3; i++) {
            JsonObject hour = hourly.add<JsonObject>();
            hour["temp"] = currentWeatherData.hourlyTemp[i];
            hour["code"] = currentWeatherData.hourlyCode[i];
        }

        String jsonString;
        serializeJson(webRequestJsonDoc, jsonString);
        request->send(200, "application/json", jsonString);
    } else {
        webRequestJsonDoc.clear();
        webRequestJsonDoc["error"] = true;
        webRequestJsonDoc["reason"] = currentWeatherData.errorReason.c_str();
        String jsonString;
        serializeJson(webRequestJsonDoc, jsonString);
        request->send(503, "application/json", jsonString);
    }
  });
  
  AsyncCallbackJsonWebHandler* refreshWeatherHandler = new AsyncCallbackJsonWebHandler("/api/weather/refresh", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject obj = json.as<JsonObject>();
    WeatherTaskParams* params = nullptr;

    // Check if latitude and longitude are provided for a direct weather fetch
    if (obj["latitude"] && obj["longitude"]) {
        float lat = obj["latitude"].as<float>();
        float lon = obj["longitude"].as<float>();
        // We pass an empty city name and set forceGeocode to false, but add lat/lon.
        // The fetchWeatherData function will be modified to use these coordinates directly.
        params = new WeatherTaskParams{"", false, lat, lon};
        Log_printf(LOG_LEVEL_INFO, "Weather refresh triggered by coordinates. Lat: %f, Lon: %f", lat, lon);

        if (xTaskCreate(forceFetchWeatherDataTask, "forceFetchWeatherDataTask", 8192, params, 1, NULL) == pdPASS) {
            request->send(202, "text/plain", "Weather refresh triggered by coordinates.");
        } else {
            delete params;
            request->send(500, "text/plain", "Failed to create weather task.");
        }
    } else {
        // If coordinates are not provided, it's a bad request.
        request->send(400, "text/plain", "Bad Request: Missing latitude or longitude.");
    }
  });
  server.addHandler(refreshWeatherHandler);

  AsyncCallbackJsonWebHandler* saveSettingsHandler = new AsyncCallbackJsonWebHandler("/api/saveSettings", [](AsyncWebServerRequest *request, JsonVariant &json) {
    // --- FIX: Respond immediately to prevent UI hanging from race condition ---
    // The main loop might restart the web server for mDNS before this handler completes.
    // Sending the response first ensures the UI doesn't get stuck waiting.
    request->send(200, "text/plain", "Settings Saved!");

    // Now, process the settings in the background.
    applyAndSaveSettings(json);

    // After saving, check if this was a time travel request
    JsonObject obj = json.as<JsonObject>();
    if (obj["timeTravelEngaged"] | false) {
        startTimeTravelAnimation();
    } else {
        // --- FIX: When saving settings without time travel, always use the lock-in animation ---
        triggerAnimation(ANIMATION_TIME_CIRCUITS_LOCK_IN);
    }
  });
  server.addHandler(saveSettingsHandler);

  server.on("/api/triggerAnimation", HTTP_POST, [](AsyncWebServerRequest *request){
    Log_printf(LOG_LEVEL_INFO, "DIAG: /api/triggerAnimation endpoint hit. Calling triggerAnimation().");
    triggerAnimation(currentSettings.animationSequence);
    request->send(200, "text/plain", "Animation triggered!");
  });

  server.on("/api/addPreset", HTTP_POST, [](AsyncWebServerRequest *request){
    preferences.begin(PREFERENCES_NAMESPACE, false);
    String presetsJson = preferences.getString("customPresets", "[]");
    webRequestJsonDoc.clear();
    DeserializationError error = deserializeJson(webRequestJsonDoc, presetsJson);
    if (error) {
        request->send(500, "text/plain", "Failed to parse presets");
        preferences.end();
        return;
    }
    JsonArray presets = webRequestJsonDoc.as<JsonArray>();
    JsonObject newPreset = presets.add<JsonObject>();
    newPreset["name"] = request->getParam("name", true)->value();
    newPreset["value"] = request->getParam("value", true)->value();

    String newPresetsJson;
    serializeJson(webRequestJsonDoc, newPresetsJson);
    preferences.putString("customPresets", newPresetsJson);
    preferences.end();
    request->send(200, "text/plain", "Custom preset saved!");
  });
  server.on("/api/updatePreset", HTTP_POST, [](AsyncWebServerRequest *request){
    preferences.begin(PREFERENCES_NAMESPACE, false);
    String name = request->getParam("name", true)->value();
    String newName = request->getParam("newName", true)->value();
    String value = request->getParam("value", true)->value();
    String presetsJson = preferences.getString("customPresets", "[]");
    webRequestJsonDoc.clear();
    deserializeJson(webRequestJsonDoc, presetsJson);
    JsonArray presets = webRequestJsonDoc.as<JsonArray>();
    for (JsonObject preset : presets) {
        if (preset["name"] == name) {
            preset["name"] = newName;
            preset["value"] = value;
            break;
        }
    }
    String newPresetsJson;
    serializeJson(webRequestJsonDoc, newPresetsJson);
    preferences.putString("customPresets", newPresetsJson);
    preferences.end();
    request->send(200, "text/plain", "Preset updated!");
  });
  server.on("/api/deletePreset", HTTP_POST, [](AsyncWebServerRequest *request){
    preferences.begin(PREFERENCES_NAMESPACE, false);
    String name = request->getParam("name", true)->value();
    String presetsJson = preferences.getString("customPresets", "[]");
    webRequestJsonDoc.clear();
    deserializeJson(webRequestJsonDoc, presetsJson);
    JsonArray presets = webRequestJsonDoc.as<JsonArray>();
    for (int i = 0; i < presets.size(); i++) {
        if (presets[i]["name"] == name) {
            presets.remove(i);
            break;
        }
    }
    String newPresetsJson;
    serializeJson(webRequestJsonDoc, newPresetsJson);
    preferences.putString("customPresets", newPresetsJson);
    preferences.end();
    request->send(200, "text/plain", "Preset deleted!");
  });
  server.on("/api/resetSettings", HTTP_POST, [](AsyncWebServerRequest *request){
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.clear();
    preferences.end();
    preferences.remove("customPresets");
    preferences.remove(THEME_PREF_KEY);
    LittleFS.remove("/api_templates.json");
    loadSettings();
    request->send(200, "text/plain", "Settings have been reset to default.");
  });
  server.on("/api/syncTime", HTTP_POST, [](AsyncWebServerRequest *request){
    ntpSyncRequested = true;
    request->send(200, "text/plain", "NTP time sync requested.");
  });
  server.on("/api/getTheme", HTTP_GET, [](AsyncWebServerRequest *request){
      preferences.begin(PREFERENCES_NAMESPACE, true);
      String themeName = preferences.getString(THEME_PREF_KEY, "theme-time-circuits");
      preferences.end();
      request->send(200, "text/plain", themeName);
  });
  server.on("/api/api_examples", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", apiTemplates);
  });
  server.on("/api/setTheme", HTTP_POST, [](AsyncWebServerRequest *request){
    String theme = request->getParam("theme", true)->value();

    int themeEnum = THEME_TIME_CIRCUITS;
    if (theme == "theme-outatime") themeEnum = THEME_OUTATIME;
    else if (theme == "theme-88mph") themeEnum = THEME_88MPH;
    else if (theme == "theme-plutonium-glow") themeEnum = THEME_PLUTONIUM_GLOW;
    else if (theme == "theme-mr-fusion") themeEnum = THEME_MR_FUSION;
    else if (theme == "theme-clock-tower") themeEnum = THEME_CLOCK_TOWER;

    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.putString(THEME_PREF_KEY, theme);
    preferences.putInt("theme", themeEnum);
    preferences.end();

    currentSettings.theme = themeEnum;

    request->send(200, "text/plain", "Theme saved.");
  });
  
  server.on("/api/system/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    webRequestJsonDoc.clear();
    webRequestJsonDoc["freeHeap"] = ESP.getFreeHeap();
    webRequestJsonDoc["rssi"] = WiFi.RSSI();
    webRequestJsonDoc["uptime"] = millis() / 1000;
    webRequestJsonDoc["deviceId"] = MQTT_UNIQUE_ID;
    String jsonString;
    serializeJson(webRequestJsonDoc, jsonString);
    request->send(200, "application/json", jsonString);
  });

  server.onNotFound([](AsyncWebServerRequest *request){
    Log_printf(LOG_LEVEL_WARN, "404 Not Found: %s", request->url().c_str());
    request->send(404, "text/plain", "Not found");
  });

  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
        // Final response after the upload is complete
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
        response->addHeader("Connection", "close");
        request->send(response);
        if (!Update.hasError()) {
            ESP.restart();
        }
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        // This is the upload handler, which is called for each chunk of the file
        if (!index) {
            // --- START: MODIFICATION - OTA Security ---
            if (!request->hasHeader("X-Auth-Password") || request->header("X-Auth-Password") != "1.21gigawatts") {
                // Abort the request if the password is wrong
                request->send(401, "text/plain", "Unauthorized");
                return;
            }
            // --- END: MODIFICATION ---
            
            Serial.printf("Update Start: %s\n", filename.c_str());
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                Update.printError(Serial);
            }
        }
        if (len) {
            if (Update.write(data, len) != len) {
                Update.printError(Serial);
            }
        }
        if (final) {
            if (Update.end(true)) {
                Serial.println("Update complete");
            } else {
                Update.printError(Serial);
            }
        }
    });

    server.on("/upload-ui", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "UI update successful! Please refresh the page.");
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
       bool isAllowed = filename.endsWith(".html") ||
                 filename.endsWith(".css") ||
                 filename.endsWith(".js") ||
                 filename.endsWith(".mp3");
        if (!isAllowed) {
            webRequestJsonDoc.clear();
            webRequestJsonDoc["action"] = "uploadError";
            webRequestJsonDoc["type"] = "ui";
            webRequestJsonDoc["message"] = "Invalid file. Only UI files are allowed.";
            String jsonString;
            serializeJson(webRequestJsonDoc, jsonString);
            ws.textAll(jsonString);
            return;
        }

        if (!index) {
            if (LittleFS.totalBytes() - LittleFS.usedBytes() < request->contentLength()) {
                webRequestJsonDoc.clear();
                webRequestJsonDoc["action"] = "uploadError";
                webRequestJsonDoc["type"] = "ui";
                webRequestJsonDoc["message"] = "Not enough space on the device.";
                String jsonString;
                serializeJson(webRequestJsonDoc, jsonString);
                ws.textAll(jsonString);
                return;
            }
            request->_tempFile = LittleFS.open("/" + filename, "w");
        }
        if (len) {
            request->_tempFile.write(data, len);
        }
        if (final) {
            request->_tempFile.close();
            webRequestJsonDoc.clear();
            webRequestJsonDoc["action"] = "uploadProgress";
            webRequestJsonDoc["type"] = "ui";
            webRequestJsonDoc["filename"] = filename;
            webRequestJsonDoc["progress"] = 100;
            String jsonString;
            serializeJson(webRequestJsonDoc, jsonString);
            ws.textAll(jsonString);
        } else {
            webRequestJsonDoc.clear();
            webRequestJsonDoc["action"] = "uploadProgress";
            webRequestJsonDoc["type"] = "ui";
            webRequestJsonDoc["filename"] = filename;
            webRequestJsonDoc["progress"] = (index + len) * 100 / request->contentLength();
            String jsonString;
            serializeJson(webRequestJsonDoc, jsonString);
            ws.textAll(jsonString);
        }
    });
}