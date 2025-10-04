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
        JsonDocument doc;
        doc["action"] = "stateUpdate";
        doc["key"] = key;
        doc["value"] = value;
        String jsonString;
        serializeJson(doc, jsonString);
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
        JsonDocument doc;
        doc["action"] = "weatherUpdate";

        // Create a nested 'data' object to hold the weather information.
        // This keeps the message structure consistent with other actions.
        JsonObject data = doc["data"].to<JsonObject>();
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
        serializeJson(doc, jsonString);
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
        JsonDocument doc;
        doc["action"] = "stockUpdate";
        String jsonString;
        serializeJson(doc, jsonString);
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
        JsonDocument doc;
        doc["action"] = "radioStatusUpdate";

        // Convert enum to a string for the UI
        switch (status) {
            case RADIO_STATUS_STOPPED:
                doc["status"] = "stopped";
                break;
            case RADIO_STATUS_CONNECTING:
                doc["status"] = "connecting";
                break;
            case RADIO_STATUS_PLAYING:
                doc["status"] = "playing";
                break;
            case RADIO_STATUS_ERROR:
                doc["status"] = "error";
                break;
        }

        doc["message"] = message;

        String jsonString;
        serializeJson(doc, jsonString);
        ws.textAll(jsonString);
        Log_printf(LOG_LEVEL_INFO, "Broadcasted radio status: %s", jsonString.c_str());
    }
}

/**
 * @brief Broadcasts a notification that the radio station list has been updated.
 */
void broadcastRadioStationsUpdated() {
    if (ws.count() > 0) {
        JsonDocument doc;
        doc["action"] = "radioStationsUpdated";
        String jsonString;
        serializeJson(doc, jsonString);
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
        JsonDocument doc;
        doc["action"] = "radioMetadataUpdate";
        doc["stationName"] = stationName;
        doc["songTitle"] = songTitle;
        String jsonString;
        serializeJson(doc, jsonString);
        ws.textAll(jsonString);
        Log_printf(LOG_LEVEL_INFO, "Broadcasted radio metadata: %s", jsonString.c_str());
    }
}

/**
 * @brief Overloaded function to broadcast an integer state update via WebSocket.
 */
void broadcastWsStateUpdate(const char* key, int value) {
    JsonDocument doc;
    doc.set(value);
    broadcastWsStateUpdate(key, doc.as<JsonVariant>());
}

void broadcastPresetUpdate(const std::string& name, int year, int month, int day, int hour, int minute) {
    if (ws.count() > 0) {
        JsonDocument doc;
        doc["action"] = "presetUpdate";
        doc["name"] = name.c_str();
        char value[20];
        sprintf(value, "%d-%02d-%02d-%02d-%02d", year, month, day, hour, minute);
        doc["value"] = value;
        String jsonString;
        serializeJson(doc, jsonString);
        ws.textAll(jsonString);
    }
}

/**
 * @brief Overloaded function to broadcast a boolean state update via WebSocket.
 */
void broadcastWsStateUpdate(const char* key, bool value) {
    JsonDocument doc;
    doc.set(value);
    broadcastWsStateUpdate(key, doc.as<JsonVariant>());
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
        JsonDocument responseJson;
        responseJson["action"] = action;
        responseJson["rowIndex"] = rowIndex; // Pass as String

        if (httpCode > 0) {
            if (httpCode == HTTP_CODE_OK) {
                JsonDocument payloadDoc;
                DeserializationError error = deserializeJson(payloadDoc, http.getStream());

                if (error == DeserializationError::Ok) {
                    responseJson["status"] = "success";
                    responseJson["payload"] = payloadDoc.as<JsonVariant>();
                } else {
                    responseJson["status"] = "error";
                    responseJson["payload"] = "JSON Parsing Failed: " + String(error.c_str());
                }
            } else {
                responseJson["status"] = "error";
                // --- START: MODIFICATION ---
                // Grab the response body to provide a more detailed error message.
                String responseBody = http.getString();
                responseJson["payload"] = "HTTP Error: " + String(httpCode) + " - " + responseBody;
                // --- END: MODIFICATION ---
            }
        } else {
            responseJson["status"] = "error";
            responseJson["payload"] = "Request Failed: " + http.errorToString(httpCode);
        }
        
        http.end();
        serializeJson(responseJson, responseString);
        ws.text(clientId, responseString);
    } else {
        // --- START: MODIFICATION - Handle http.begin() failure ---
        String responseString;
        JsonDocument responseJson;
        responseJson["action"] = action;
        responseJson["rowIndex"] = rowIndex; // Pass as String
        responseJson["status"] = "error";
        responseJson["payload"] = "Connection Failed. Check URL/DNS.";
        serializeJson(responseJson, responseString);
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
            
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (error) {
                Log_printf(LOG_LEVEL_ERROR, "deserializeJson() failed: %s", error.c_str());
                return;
            }

            String action = doc["action"];
             if (action == "testApi") {
                Log_printf(LOG_LEVEL_DEBUG, "'testApi' action received.");
                if (!timeSynchronized) {
                    String responseString;
                    JsonDocument responseJson;
                    responseJson["action"] = "apiResult";
                    responseJson["status"] = "error";
                    responseJson["payload"] = "Time not sync'd. Go to System->Sync Time.";
                    serializeJson(responseJson, responseString);
                    ws.text(client->id(), responseString);
                    return;
                }

                String url = doc["data"]["url"];
                String authKey = doc["data"]["authKey"];
                String authValue = doc["data"]["authValue"];
                 Log_printf(LOG_LEVEL_DEBUG, "API Test URL: %s", url.c_str());

                ApiTestParams* params = new ApiTestParams{url, authKey, authValue, client->id(), "apiResult", String(0)};

                BaseType_t taskCreated = xTaskCreate(makeApiRequestTask, "apiTestTask", 8192, params, 1, NULL);
                if (taskCreated != pdPASS) {
                    delete params;
                     Log_printf(LOG_LEVEL_ERROR, "Failed to create API test task!");
                }
            } else if (action == "play_radio") {
                String url = doc["url"];
                if (url.length() > 0) {
                    Log_printf(LOG_LEVEL_INFO, "WebSocket: Play radio command received for URL: %s", url.c_str());
                    startAudioStream(url.c_str(), false);
                }
            } else if (action == "stop_radio") {
                Log_printf(LOG_LEVEL_INFO, "WebSocket: Stop radio command received.");
                stopAudioStream();
            } else if (action == "run_sequence") {
                String payload = doc["payload"];
                if (payload.length() > 0) {
                    Log_printf(LOG_LEVEL_INFO, "WebSocket: Run sequence command received.");
                    handleSequencerCommand(payload.c_str());
                }
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
    JsonDocument doc;
    doc["action"] = "fullSettings";

    // --- Add Time Circuits settings ---
    doc["destinationYear"] = currentSettings.destinationYear;
    doc["destinationTimezoneIndex"] = currentSettings.destinationTimezoneIndex;
    doc["lastTimeDepartedYear"] = currentSettings.lastTimeDepartedYear;
    doc["lastTimeDepartedMonth"] = currentSettings.lastTimeDepartedMonth;
    doc["lastTimeDepartedDay"] = currentSettings.lastTimeDepartedDay;
    doc["lastTimeDepartedHour"] = currentSettings.lastTimeDepartedHour;
    doc["lastTimeDepartedMinute"] = currentSettings.lastTimeDepartedMinute;
    doc["presentTimezoneIndex"] = currentSettings.presentTimezoneIndex;

    // --- Add Temporal settings ---
    doc["departureHour"] = currentSettings.departureHour;
    doc["departureMinute"] = currentSettings.departureMinute;
    doc["arrivalHour"] = currentSettings.arrivalHour;
    doc["arrivalMinute"] = currentSettings.arrivalMinute;
    doc["brightness"] = currentSettings.brightness;
    doc["notificationVolume"] = currentSettings.notificationVolume;
    doc["timeTravelAnimationDuration"] = currentSettings.timeTravelAnimationDuration;
    doc["timeTravelAnimationInterval"] = currentSettings.timeTravelAnimationInterval;
    doc["animationStyle"] = currentSettings.animationStyle;
    doc["timeTravelSoundToggle"] = currentSettings.timeTravelSoundToggle;
    doc["presetCycleInterval"] = currentSettings.presetCycleInterval;
    doc["displayFormat24h"] = currentSettings.displayFormat24h;

    // --- Add Data Link and other settings ---
    doc["displayMode"] = currentSettings.displayMode;
    doc["numDataPoints"] = currentSettings.numDataPoints;
    doc["mqttBroker"] = currentSettings.mqttBroker.c_str();
    doc["mqttPort"] = currentSettings.mqttPort;
    doc["mqttUser"] = currentSettings.mqttUser.c_str();
    doc["mqttPassword"] = currentSettings.mqttPassword.c_str();
    doc["cityName"] = currentSettings.cityName.c_str();
    doc["useMetricUnits"] = currentSettings.useMetricUnits;
    doc["latitude"] = currentSettings.latitude;
    doc["longitude"] = currentSettings.longitude;
    doc["stockRefreshInterval"] = currentSettings.stockRefreshInterval;
    doc["financialModelingPrepApiKey"] = currentSettings.financialModelingPrepApiKey.c_str();
    doc["stockRow1_symbol"] = currentSettings.stockRow1_symbol.c_str();
    doc["stockRow2_symbol"] = currentSettings.stockRow2_symbol.c_str();
    doc["stockRow3_symbol"] = currentSettings.stockRow3_symbol.c_str();

    JsonArray dataPoints = doc["dataPoints"].to<JsonArray>();
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
    serializeJson(doc, response);
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
        playSound("/EASTER_EGG.mp3");
    }
    request->send(200, "text/plain", "Great Scott!");
  });

  server.on("/api/settings/BTTF_TC", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["destinationYear"] = currentSettings.destinationYear;
    doc["destinationTimezoneIndex"] = currentSettings.destinationTimezoneIndex;
    doc["lastTimeDepartedYear"] = currentSettings.lastTimeDepartedYear;
    doc["lastTimeDepartedMonth"] = currentSettings.lastTimeDepartedMonth;
    doc["lastTimeDepartedDay"] = currentSettings.lastTimeDepartedDay;
    doc["lastTimeDepartedHour"] = currentSettings.lastTimeDepartedHour;
    doc["lastTimeDepartedMinute"] = currentSettings.lastTimeDepartedMinute;
    doc["presentTimezoneIndex"] = currentSettings.presentTimezoneIndex;
    String jsonString;
    serializeJson(doc, jsonString);
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

  server.on("/api/stocks/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (currentSettings.displayMode != DMS_STOCK_TICKER) {
        request->send(400, "application/json", "{\"error\":\"Stock Ticker Mode is disabled.\"}");
        return;
    }
    if (stockManager.getApiKey().isEmpty()) {
        request->send(400, "application/json", "{\"error\":\"API key is not set.\"}");
        return;
    }

    JsonDocument doc;
    doc["api_usage"] = stockManager.getApiUsage();
    JsonArray assets = doc["assets"].to<JsonArray>();
    for (const auto& asset : stockManager.getAssets()) {
        JsonObject assetObj = assets.add<JsonObject>();
        assetObj["symbol"] = asset.symbol;
        assetObj["price"] = asset.price;
        assetObj["change_percent"] = asset.change_percent;
        assetObj["data_valid"] = asset.data_valid;
        assetObj["error_reason"] = asset.error_reason;
    }
    String jsonString;
    serializeJson(doc, jsonString);
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
    JsonDocument doc;
    doc["marqueeText"] = marqueeLine;
    String jsonString;
    serializeJson(doc, jsonString);
    request->send(200, "application/json", jsonString);
  });

  server.on("/api/stocks", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (currentSettings.displayMode != DMS_STOCK_TICKER || stockManager.getAssets().empty()) {
        request->send(200, "application/json", "[]");
        return;
    }

    JsonDocument doc;
    JsonArray assets = doc.to<JsonArray>();
    for (const auto& asset : stockManager.getAssets()) {
        JsonObject assetObj = assets.add<JsonObject>();
        assetObj["symbol"] = asset.symbol;
        assetObj["name"] = asset.name;
        assetObj["price"] = asset.price;
        assetObj["change_percent"] = asset.change_percent;
        assetObj["data_valid"] = asset.data_valid;
    }
    String jsonString;
    serializeJson(doc, jsonString);
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
    JsonDocument doc;
    doc["departureHour"] = currentSettings.departureHour;
    doc["departureMinute"] = currentSettings.departureMinute;
    doc["arrivalHour"] = currentSettings.arrivalHour;
    doc["arrivalMinute"] = currentSettings.arrivalMinute;
    doc["brightness"] = currentSettings.brightness;
    doc["notificationVolume"] = currentSettings.notificationVolume;
    doc["timeTravelAnimationDuration"] = currentSettings.timeTravelAnimationDuration;
    doc["timeTravelAnimationInterval"] = currentSettings.timeTravelAnimationInterval;
    doc["animationStyle"] = currentSettings.animationStyle;
    doc["timeTravelSoundToggle"] = currentSettings.timeTravelSoundToggle;
    doc["presetCycleInterval"] = currentSettings.presetCycleInterval;
    doc["displayFormat24h"] = currentSettings.displayFormat24h;
    String jsonString;
    serializeJson(doc, jsonString);
    request->send(200, "application/json", jsonString);
  });

  server.on("/api/settings/datalink", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["dataLinkEnabled"] = (currentSettings.displayMode == DMS_DATA_LINK);
    doc["numDataPoints"] = currentSettings.numDataPoints;
    doc["mqttBroker"] = currentSettings.mqttBroker.c_str();
    doc["mqttPort"] = currentSettings.mqttPort;
    doc["mqttUser"] = currentSettings.mqttUser.c_str();
    doc["mqttPassword"] = currentSettings.mqttPassword.c_str();
    doc["weatherModeEnabled"] = (currentSettings.displayMode == DMS_WEATHER);
    doc["cityName"] = currentSettings.cityName.c_str();
    doc["useMetricUnits"] = currentSettings.useMetricUnits;
    doc["latitude"] = currentSettings.latitude;
    doc["longitude"] = currentSettings.longitude;
    doc["stockTickerModeEnabled"] = (currentSettings.displayMode == DMS_STOCK_TICKER);
    doc["stockRefreshInterval"] = currentSettings.stockRefreshInterval;
    doc["financialModelingPrepApiKey"] = currentSettings.financialModelingPrepApiKey.c_str();
    doc["stockRow1_symbol"] = currentSettings.stockRow1_symbol.c_str();
    doc["stockRow2_symbol"] = currentSettings.stockRow2_symbol.c_str();
    doc["stockRow3_symbol"] = currentSettings.stockRow3_symbol.c_str();

    JsonArray dataPoints = doc["dataPoints"].to<JsonArray>();
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
    serializeJson(doc, response);
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

    JsonDocument doc;
    File file = LittleFS.open("/radio_stations.json", "r");
    if (file) {
        deserializeJson(doc, file);
        file.close();
    }
    JsonArray stations = doc.as<JsonArray>();

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
    serializeJson(doc, file);
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

    JsonDocument doc;
    File file = LittleFS.open("/radio_stations.json", "r");
    if (!file) {
        request->send(500, "text/plain", "Could not open stations file");
        return;
    }
    deserializeJson(doc, file);
    file.close();
    JsonArray stations = doc.as<JsonArray>();

    if (index >= 0 && index < stations.size()) {
        stations.remove(index);
    } else {
        request->send(400, "text/plain", "Invalid index");
        return;
    }

    file = LittleFS.open("/radio_stations.json", "w");
    serializeJson(doc, file);
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
    JsonDocument doc;
    doc["unixTime"] = now;
    doc["timeSynchronized"] = timeSynchronized;
    String jsonString;
    serializeJson(doc, jsonString);
    request->send(200, "application/json", jsonString);
  });

  server.on("/api/weather", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (currentWeatherData.dataValid) {
        JsonDocument doc;
        doc["temperature"] = currentWeatherData.temperature;
        doc["apparentTemperature"] = currentWeatherData.apparentTemperature;
        doc["windSpeed"] = currentWeatherData.windSpeed;
        doc["humidity"] = currentWeatherData.humidity;
        doc["weatherCode"] = currentWeatherData.weatherCode;
        doc["dailyHigh"] = currentWeatherData.dailyHigh;
        doc["dailyLow"] = currentWeatherData.dailyLow;
        doc["latitude"] = currentWeatherData.latitude;
        doc["longitude"] = currentWeatherData.longitude;
        doc["sunrise"] = currentWeatherData.sunrise;
        doc["sunset"] = currentWeatherData.sunset;
        doc["precipitationProbability"] = currentWeatherData.precipitationProbability;
        doc["maxWindSpeed"] = currentWeatherData.maxWindSpeed;
        doc["tomorrowHigh"] = currentWeatherData.tomorrowHigh;
        doc["tomorrowLow"] = currentWeatherData.tomorrowLow;
        doc["tomorrowWeatherCode"] = currentWeatherData.tomorrowWeatherCode;
        
        JsonArray hourly = doc["hourly"].to<JsonArray>();
        for (int i = 0; i < 3; i++) {
            JsonObject hour = hourly.add<JsonObject>();
            hour["temp"] = currentWeatherData.hourlyTemp[i];
            hour["code"] = currentWeatherData.hourlyCode[i];
        }

        String jsonString;
        serializeJson(doc, jsonString);
        request->send(200, "application/json", jsonString);
    } else {
        JsonDocument errorDoc;
        errorDoc["error"] = true;
        errorDoc["reason"] = currentWeatherData.errorReason.c_str();
        String jsonString;
        serializeJson(errorDoc, jsonString);
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
    Log_printf(LOG_LEVEL_INFO, "DIAG: --- /api/saveSettings endpoint hit ---");

    // --- START: FIX - Removed risky JSON serialization to prevent heap allocation failure ---
    // The following lines were removed as they could cause a crash on the ESP32
    // when saving complex settings due to large memory allocation on the heap.
    // String payload;
    // serializeJson(json, payload);
    // Log_printf(LOG_LEVEL_INFO, "DIAG: Received payload: %s", payload.c_str());
    // --- END: FIX ---

    Log_printf(LOG_LEVEL_INFO, "DIAG: Calling applyAndSaveSettings...");
    applyAndSaveSettings(json);
    Log_printf(LOG_LEVEL_INFO, "DIAG: Returned from applyAndSaveSettings.");

    // After saving, check if this was a time travel request
    JsonObject obj = json.as<JsonObject>();
    if (obj["timeTravelEngaged"] | false) {
        Log_printf(LOG_LEVEL_INFO, "DIAG: timeTravelEngaged is true. Calling startTimeTravelAnimation().");
        startTimeTravelAnimation();
    } else {
        Log_printf(LOG_LEVEL_INFO, "DIAG: timeTravelEngaged is false or missing. Calling startStyledAnimation().");
        startStyledAnimation();
    }

    // Immediately send a response to the client to unblock the UI.
    request->send(200, "text/plain", "Settings Saved!");
    Log_printf(LOG_LEVEL_INFO, "DIAG: --- /api/saveSettings finished ---");
  });
  server.addHandler(saveSettingsHandler);

  server.on("/api/triggerAnimation", HTTP_POST, [](AsyncWebServerRequest *request){
    Log_printf(LOG_LEVEL_INFO, "DIAG: /api/triggerAnimation endpoint hit. Calling startTimeTravelAnimation().");
    startTimeTravelAnimation();
    request->send(200, "text/plain", "Animation triggered!");
  });

  server.on("/api/addPreset", HTTP_POST, [](AsyncWebServerRequest *request){
    preferences.begin(PREFERENCES_NAMESPACE, false);
    String presetsJson = preferences.getString("customPresets", "[]");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, presetsJson);
    if (error) {
        request->send(500, "text/plain", "Failed to parse presets");
        preferences.end();
        return;
    }
    JsonArray presets = doc.as<JsonArray>();
    JsonObject newPreset = presets.add<JsonObject>();
    newPreset["name"] = request->getParam("name", true)->value();
    newPreset["value"] = request->getParam("value", true)->value();

    String newPresetsJson;
    serializeJson(doc, newPresetsJson);
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
    JsonDocument doc;
    deserializeJson(doc, presetsJson);
    JsonArray presets = doc.as<JsonArray>();
    for (JsonObject preset : presets) {
        if (preset["name"] == name) {
            preset["name"] = newName;
            preset["value"] = value;
            break;
        }
    }
    String newPresetsJson;
    serializeJson(doc, newPresetsJson);
    preferences.putString("customPresets", newPresetsJson);
    preferences.end();
    request->send(200, "text/plain", "Preset updated!");
  });
  server.on("/api/deletePreset", HTTP_POST, [](AsyncWebServerRequest *request){
    preferences.begin(PREFERENCES_NAMESPACE, false);
    String name = request->getParam("name", true)->value();
    String presetsJson = preferences.getString("customPresets", "[]");
    JsonDocument doc;
    deserializeJson(doc, presetsJson);
    JsonArray presets = doc.as<JsonArray>();
    for (int i = 0; i < presets.size(); i++) {
        if (presets[i]["name"] == name) {
            presets.remove(i);
            break;
        }
    }
    String newPresetsJson;
    serializeJson(doc, newPresetsJson);
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
    JsonDocument doc;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["rssi"] = WiFi.RSSI();
    doc["uptime"] = millis() / 1000;
    String jsonString;
    serializeJson(doc, jsonString);
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
            JsonDocument doc;
            doc["action"] = "uploadError";
            doc["type"] = "ui";
            doc["message"] = "Invalid file. Only UI files are allowed.";
            String jsonString;
            serializeJson(doc, jsonString);
            ws.textAll(jsonString);
            return;
        }

        if (!index) {
            if (LittleFS.totalBytes() - LittleFS.usedBytes() < request->contentLength()) {
                JsonDocument doc;
                doc["action"] = "uploadError";
                doc["type"] = "ui";
                doc["message"] = "Not enough space on the device.";
                String jsonString;
                serializeJson(doc, jsonString);
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
            JsonDocument doc;
            doc["action"] = "uploadProgress";
            doc["type"] = "ui";
            doc["filename"] = filename;
            doc["progress"] = 100;
            String jsonString;
            serializeJson(doc, jsonString);
            ws.textAll(jsonString);
        } else {
            JsonDocument doc;
            doc["action"] = "uploadProgress";
            doc["type"] = "ui";
            doc["filename"] = filename;
            doc["progress"] = (index + len) * 100 / request->contentLength();
            String jsonString;
            serializeJson(doc, jsonString);
            ws.textAll(jsonString);
        }
    });
}