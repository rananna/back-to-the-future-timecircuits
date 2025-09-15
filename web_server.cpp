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
#include "api_templates.h"
#include "DataManager.h"
#include "timezone.h"
#include "EventManager.h"
#include <AsyncJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <string>
#include <WiFi.h>
#include <Update.h>
#include <ArduinoOTA.h>
#include "FS.h"
#include <LITTLEFS.h>

// --- Extern Global Variables ---
// These are defined in the main .ino file and are made available here.
extern volatile bool saveSettingsRequested;
extern String settingsToSaveJson;


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

void broadcastWsStateUpdate(const char* key, const JsonVariant& value) {
    if (ws.count() > 0) {
        DynamicJsonDocument doc(256);
        doc["action"] = "stateUpdate";
        doc["key"] = key;
        doc["value"] = value;
        String jsonString;
        serializeJson(doc, jsonString);
        ws.textAll(jsonString);
    }
}

/**
 * @brief Overloaded function to broadcast an integer state update via WebSocket.
 */
void broadcastWsStateUpdate(const char* key, int value) {
    StaticJsonDocument<32> doc;
    doc.set(value);
    broadcastWsStateUpdate(key, doc.as<JsonVariant>());
}

void broadcastPresetUpdate(const std::string& name, int year, int month, int day, int hour, int minute) {
    if (ws.count() > 0) {
        DynamicJsonDocument doc(256);
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
    StaticJsonDocument<32> doc;
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
    int rowIndex = params->rowIndex;
    delete params; // Clean up the params object immediately

    HTTPClient http;
    WiFiClientSecure client;
    
    client.setInsecure();

    if (http.begin(client, urlStr)) {
        if (authKey.length() > 0 && authValue.length() > 0) {
            http.addHeader(authKey, authValue);
        }
        
        int httpCode = http.GET();
        String responseString;
        DynamicJsonDocument responseJson(4096);
        responseJson["action"] = action;
        responseJson["rowIndex"] = rowIndex;

        if (httpCode > 0) {
            if (httpCode == HTTP_CODE_OK) {
                DynamicJsonDocument payloadDoc(4096);
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
                responseJson["payload"] = "HTTP Error: " + String(httpCode);
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
        DynamicJsonDocument responseJson(256);
        responseJson["action"] = action;
        responseJson["rowIndex"] = rowIndex;
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
    } else if (type == WS_EVT_DISCONNECT) {
        Log_printf(LOG_LEVEL_INFO, "WebSocket client #%u disconnected", client->id());
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, data, len);

            if (error) {
                Log_printf(LOG_LEVEL_ERROR, "deserializeJson() failed: %s", error.c_str());
                return;
            }

            String action = doc["action"];
             if (action == "testStock") {
                Log_printf(LOG_LEVEL_DEBUG, "'testStock' action received.");
                 if (!timeSynchronized) {
                    String responseString;
                    DynamicJsonDocument responseJson(256);
                    responseJson["action"] = "stockTestResult";
                    responseJson["status"] = "error";
                    responseJson["payload"] = "Time not sync'd. Go to System->Sync Time.";
                    serializeJson(responseJson, responseString);
                    ws.text(client->id(), responseString);
                    return;
                }
                String symbol = doc["data"]["symbol"];
                String apiKey = doc["data"]["apiKey"];
                int rowIndex = doc["data"]["rowIndex"];
                
                String url;
                if (symbol.startsWith("^")) {
                    url = "https://financialmodelingprep.com/api/v3/quote-short/" + symbol + "?apikey=" + apiKey;
                } else {
                    url = "https://financialmodelingprep.com/api/v3/quote/" + symbol + "?apikey=" + apiKey;
                }
                Log_printf(LOG_LEVEL_DEBUG, "Stock URL created: %s", url.c_str());

                ApiTestParams* params = new ApiTestParams{url, "", "", client->id(), "stockTestResult", rowIndex};
                BaseType_t taskCreated = xTaskCreate(makeApiRequestTask, "apiTestTask", 8192, params, 1, NULL);
                if (taskCreated != pdPASS) {
                    delete params;
                    Log_printf(LOG_LEVEL_ERROR, "Failed to create stock test task!");
                } else {
                    Log_printf(LOG_LEVEL_DEBUG, "Stock test task created successfully.");
                }
            } else if (action == "testApi") {
                Log_printf(LOG_LEVEL_DEBUG, "'testApi' action received.");
                if (!timeSynchronized) {
                    String responseString;
                    DynamicJsonDocument responseJson(256);
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

                ApiTestParams* params = new ApiTestParams{url, authKey, authValue, client->id(), "apiResult", 0};

                BaseType_t taskCreated = xTaskCreate(makeApiRequestTask, "apiTestTask", 8192, params, 1, NULL);
                if (taskCreated != pdPASS) {
                    delete params;
                     Log_printf(LOG_LEVEL_ERROR, "Failed to create API test task!");
                }
            }
        }
    }
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

  server.on("/api/settings/timecircuits", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<256> doc;
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

  server.on("/api/settings/temporal", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<384> doc;
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
    DynamicJsonDocument doc(2048);
    doc["dataLinkEnabled"] = currentSettings.dataLinkEnabled;
    doc["dataLinkRefreshInterval"] = currentSettings.dataLinkRefreshInterval;
    doc["numDataPoints"] = currentSettings.numDataPoints;
    doc["mqttBroker"] = currentSettings.mqttBroker.c_str();
    doc["mqttPort"] = currentSettings.mqttPort;
    doc["mqttUser"] = currentSettings.mqttUser.c_str();
    doc["mqttPassword"] = currentSettings.mqttPassword.c_str();
    doc["weatherModeEnabled"] = currentSettings.weatherModeEnabled;
    doc["cityName"] = currentSettings.cityName.c_str();
    doc["useMetricUnits"] = currentSettings.useMetricUnits;
    doc["stockTickerModeEnabled"] = currentSettings.stockTickerModeEnabled;
    doc["financialModelingPrepApiKey"] = currentSettings.financialModelingPrepApiKey.c_str();
    doc["stockRow1_symbol"] = currentSettings.stockRow1_symbol.c_str();
    doc["stockRow2_symbol"] = currentSettings.stockRow2_symbol.c_str();
    doc["stockRow3_symbol"] = currentSettings.stockRow3_symbol.c_str();

    JsonArray dataPoints = doc.createNestedArray("dataPoints");
    for (int i = 0; i < currentSettings.numDataPoints; i++) {
        JsonObject dp = dataPoints.createNestedObject();
        dp["url"] = currentSettings.dataPoints[i].url.c_str();
        dp["monthPath"] = currentSettings.dataPoints[i].monthPath.c_str();
        dp["dayPath"] = currentSettings.dataPoints[i].dayPath.c_str();
        dp["yearPath"] = currentSettings.dataPoints[i].yearPath.c_str();
        dp["timePath"] = currentSettings.dataPoints[i].timePath.c_str();
        dp["prefix"] = currentSettings.dataPoints[i].prefix.c_str();
        dp["suffix"] = currentSettings.dataPoints[i].suffix.c_str();
        dp["icon"] = currentSettings.dataPoints[i].icon.c_str();
        dp["scrollSpeed"] = currentSettings.dataPoints[i].scrollSpeed;
        dp["dataSourceType"] = (int)currentSettings.dataPoints[i].dataSourceType;
        dp["mqttTopic"] = currentSettings.dataPoints[i].mqttTopic.c_str();
        dp["yearPrefix"] = currentSettings.dataPoints[i].yearPrefix.c_str();
        dp["yearSuffix"] = currentSettings.dataPoints[i].yearSuffix.c_str();
        dp["displayMode"] = (int)currentSettings.dataPoints[i].displayMode;
        dp["scrollingText"] = currentSettings.dataPoints[i].scrollingText.c_str();
        dp["authHeaderKey"] = currentSettings.dataPoints[i].authHeaderKey.c_str();
        dp["authHeaderValue"] = currentSettings.dataPoints[i].authHeaderValue.c_str();
        dp["apiExampleKey"] = currentSettings.dataPoints[i].apiExampleKey.c_str();
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  server.on("/api/timezones", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", TZ_JSON);
  });

  server.on("/api/getPresets", HTTP_GET, [](AsyncWebServerRequest *request) {
    preferences.begin(PREFERENCES_NAMESPACE, true);
    String presets = preferences.getString("customPresets", "[]");
    preferences.end();
    request->send(200, "application/json", presets);
  });

  server.on("/api/time", HTTP_GET, [](AsyncWebServerRequest *request) {
    time_t now;
    time(&now);
    StaticJsonDocument<128> doc;
    doc["unixTime"] = now;
    doc["timeSynchronized"] = timeSynchronized;
    String jsonString;
    serializeJson(doc, jsonString);
    request->send(200, "application/json", jsonString);
  });

  server.on("/api/weather", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (currentWeatherData.dataValid) {
        StaticJsonDocument<512> doc;
        doc["temperature"] = currentWeatherData.temperature;
        doc["apparentTemperature"] = currentWeatherData.apparentTemperature;
        doc["windSpeed"] = currentWeatherData.windSpeed;
        doc["humidity"] = currentWeatherData.humidity;
        doc["weatherCode"] = currentWeatherData.weatherCode;
        doc["dailyHigh"] = currentWeatherData.dailyHigh;
        doc["dailyLow"] = currentWeatherData.dailyLow;
        doc["latitude"] = currentWeatherData.latitude;
        doc["longitude"] = currentWeatherData.longitude;
        
        JsonArray hourly = doc.createNestedArray("hourly");
        for (int i = 0; i < 3; i++) {
            JsonObject hour = hourly.createNestedObject();
            hour["temp"] = currentWeatherData.hourlyTemp[i];
            hour["code"] = currentWeatherData.hourlyCode[i];
        }

        String jsonString;
        serializeJson(doc, jsonString);
        request->send(200, "application/json", jsonString);
    } else {
        StaticJsonDocument<128> errorDoc;
        errorDoc["error"] = true;
        errorDoc["reason"] = currentWeatherData.errorReason.c_str();
        String jsonString;
        serializeJson(errorDoc, jsonString);
        request->send(503, "application/json", jsonString);
    }
  });
  
  AsyncCallbackJsonWebHandler* refreshWeatherHandler = new AsyncCallbackJsonWebHandler("/api/weather/refresh", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject obj = json.as<JsonObject>();
    if (obj.containsKey("cityName")) {
        std::string city = obj["cityName"].as<std::string>();
        WeatherTaskParams* params = new WeatherTaskParams;
        params->cityName = city;
        params->forceGeocode = true;
        if (xTaskCreate(forceFetchWeatherDataTask, "forceFetchWeatherDataTask", 16384, params, 1, NULL) == pdPASS) {
            request->send(202, "text/plain", "Weather refresh triggered for new city.");
        } else {
            delete params;
            request->send(500, "text/plain", "Failed to create weather task.");
        }
    } else {
        request->send(400, "text/plain", "Bad Request: Missing cityName");
    }
  });
  server.addHandler(refreshWeatherHandler);

  AsyncCallbackJsonWebHandler* saveSettingsHandler = new AsyncCallbackJsonWebHandler("/api/saveSettings", [](AsyncWebServerRequest *request, JsonVariant &json) {
    // This handler is now asynchronous. It just captures the settings and flags
    // the main loop to perform the save, then immediately responds to the client.
    
    // Re-serialize the parsed JSON from the request back into our global string buffer.
    settingsToSaveJson = ""; // Clear any previous data.
    serializeJson(json, settingsToSaveJson);

    // Set the volatile flag to true. The main loop will see this and handle the save.
    saveSettingsRequested = true;

    // Immediately send a response to the client to unblock the UI.
    request->send(200, "text/plain", "Settings Save Queued!");

    // The actual saving and animation trigger now happens in the main loop.
  }, 8192); // The 8192 buffer size is still needed to receive the large JSON payload.
  server.addHandler(saveSettingsHandler);

  server.on("/api/triggerAnimation", HTTP_POST, [](AsyncWebServerRequest *request){
    startTimeTravelAnimation();
    request->send(200, "text/plain", "Animation triggered!");
  });

  server.on("/api/addPreset", HTTP_POST, [](AsyncWebServerRequest *request){
    preferences.begin(PREFERENCES_NAMESPACE, false);
    String presetsJson = preferences.getString("customPresets", "[]");
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, presetsJson);
    if (error) {
        request->send(500, "text/plain", "Failed to parse presets");
        preferences.end();
        return;
    }
    JsonArray presets = doc.as<JsonArray>();
    JsonObject newPreset = presets.createNestedObject();
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
    DynamicJsonDocument doc(2048);
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
    DynamicJsonDocument doc(2048);
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
    StaticJsonDocument<256> doc;
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
            DynamicJsonDocument doc(256);
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
                DynamicJsonDocument doc(256);
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
            DynamicJsonDocument doc(256);
            doc["action"] = "uploadProgress";
            doc["type"] = "ui";
            doc["filename"] = filename;
            doc["progress"] = 100;
            String jsonString;
            serializeJson(doc, jsonString);
            ws.textAll(jsonString);
        } else {
            DynamicJsonDocument doc(256);
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