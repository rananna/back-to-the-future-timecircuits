#include "web_server.h"
#include "api_templates.h"
#include "DataManager.h"
#include "EventManager.h"
#include <AsyncJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <string>
#include <WiFi.h>
#include <AudioOutputI2S.h>
#include <Update.h>
#include "MqttManager.h"
#include "globals.h"
#include <LittleFS.h>
#include "HardwareControl.h" // For playSound
#include "AnimationManager.h" // For startTimeTravelAnimation
#include "config.h"
#include "logger.h"
#include "wifimanagernew.h" // Added for the new WifiManager functions


// Make the global 'out' object from the main .ino file available here
extern AudioOutputI2S *out;
extern bool clientLoggingEnabled;
extern Logger logger; // FIX: Changed 'log' to 'logger' to match the declaration in globals.h

const char* OTA_USER = "admin";
const char* OTA_PASS = "bttf-ota";

const char apiTemplates[] PROGMEM = "{}"; // Abridged for brevity

AsyncWebSocket ws("/ws");

// Declare the new function that will be defined in the .ino file
void forceFetchWeatherDataTask(void* p);

// Forward declaration for WiFi provisioning
void handleSaveCredentials(AsyncWebServerRequest *request);
void handleRootPage(AsyncWebServerRequest *request);
const char* WIFI_PROV_PAGE = "/wifimanager.html";

void broadcastLog(const char* message) {
    if (ws.count() > 0 && clientLoggingEnabled) {
        DynamicJsonDocument doc(512);
        doc["action"] = "log";
        doc["payload"] = message;
        String jsonString;
        serializeJson(doc, jsonString);
        ws.textAll(jsonString);
    }
}


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

void broadcastWsStateUpdate(const char* key, int value) {
    StaticJsonDocument<32> doc;
    doc.set(value);
    broadcastWsStateUpdate(key, doc.as<JsonVariant>());
}

void broadcastWsStateUpdate(const char* key, bool value) {
    StaticJsonDocument<32> doc;
    doc.set(value);
    broadcastWsStateUpdate(key, doc.as<JsonVariant>());
}

void makeApiRequestTask(void* p) {
    ApiTestParams* params = (ApiTestParams*)p;
    String urlStr = params->url;
    String authKey = params->authKey;
    String authValue = params->authValue;
    uint32_t clientId = params->clientId;
    String action = params->action; // <-- FIX: 'action' member added to ApiTestParams in types.h
    int rowIndex = params->rowIndex;
    delete params; 

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
    }

    vTaskDelete(NULL); 
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WEB_LOG: WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WEB_LOG: WebSocket client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, data, len);

            if (error) {
                Serial.print(F("WEB_LOG: deserializeJson() failed: "));
                Serial.println(error.c_str());
                return;
            }

            String action = doc["action"];
             if (action == "testStock") {
                Serial.println("SERVER_DEBUG: 'testStock' action received.");
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
                Serial.printf("SERVER_DEBUG: Stock URL created: %s\n", url.c_str());

                ApiTestParams* params = new ApiTestParams{url, "", "", client->id(), "stockTestResult", rowIndex};
                BaseType_t taskCreated = xTaskCreate(makeApiRequestTask, "apiTestTask", 8192, params, 1, NULL);
                if (taskCreated != pdPASS) {
                    delete params;
                    Serial.println("SERVER_DEBUG: ERROR - Failed to create stock test task!");
                } else {
                    Serial.println("SERVER_DEBUG: Stock test task created successfully.");
                }
            } else if (action == "testApi") {
                Serial.println("SERVER_DEBUG: 'testApi' action received.");
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
                 Serial.printf("SERVER_DEBUG: API Test URL: %s\n", url.c_str());

                ApiTestParams* params = new ApiTestParams{url, authKey, authValue, client->id(), "apiResult", 0};

                BaseType_t taskCreated = xTaskCreate(makeApiRequestTask, "apiTestTask", 8192, params, 1, NULL);
                if (taskCreated != pdPASS) {
                    delete params;
                     Serial.println("SERVER_DEBUG: ERROR - Failed to create API test task!");
                }
            } else if (action == "setLogging") {
                clientLoggingEnabled = doc["payload"];
                Serial.printf("WEB_LOG: Logging enabled status set to: %s\n", clientLoggingEnabled ? "true" : "false");
            }
        }
    }
}


void handleScanNetworks(AsyncWebServerRequest *request) {
    int n = WiFi.scanNetworks();
    DynamicJsonDocument doc(1024);
    JsonArray networks = doc.createNestedArray("networks");
    for (int i = 0; i < n; ++i) {
        JsonObject network = networks.createNestedObject();
        network["ssid"] = WiFi.SSID(i);
        network["rssi"] = WiFi.RSSI(i);
    }
    String jsonString;
    serializeJson(doc, jsonString);
    request->send(200, "application/json", jsonString);
}


void handleRootPage(AsyncWebServerRequest *request) {
    request->send(LittleFS, WIFI_PROV_PAGE, "text/html");
}

void handleSaveCredentials(AsyncWebServerRequest *request) {
    String ssid = request->getParam("ssid", true)->value();
    String password = request->getParam("password", true)->value();
    saveWiFiCredentials(ssid.c_str(), password.c_str());
    request->send(200, "text/plain", "Credentials saved. Rebooting...");
    delay(100);
    ESP.restart();
}


void setupWebRoutes() {
  Serial.println(F("WEB_LOG: Inside setupWebRoutes(). Attaching handlers..."));
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Check if we are in AP mode and serve the provisioning page if so
  if (WiFi.getMode() == WIFI_MODE_AP) {
      server.on("/", HTTP_GET, handleRootPage);
      server.on("/save-credentials", HTTP_POST, handleSaveCredentials);
      server.on("/scan_networks", HTTP_GET, handleScanNetworks); // New endpoint handler
      
      // New endpoints for robust connection handling
      server.on("/test_credentials", HTTP_POST, [](AsyncWebServerRequest *request) {
          if (request->hasParam("ssid", true) && request->hasParam("pass", true)) {
              String ssid = request->getParam("ssid", true)->value();
              String password = request->getParam("pass", true)->value();
              
              if (testWifiAndConnect(ssid.c_str(), password.c_str())) {
                  request->send(200, "text/plain", "SUCCESS");
              } else {
                  request->send(400, "text/plain", "FAILURE");
              }
          } else {
              request->send(400, "text/plain", "Bad Request: Missing SSID or password");
          }
      });
      
      server.on("/forget_network", HTTP_POST, [](AsyncWebServerRequest *request) {
          preferences.begin(PREFERENCES_NAMESPACE, false); // FIX: Changed WIFI_PREF_NAMESPACE to PREFERENCES_NAMESPACE
          preferences.clear();
          preferences.end();
          request->send(200, "text/plain", "Credentials cleared. Rebooting.");
          delay(100);
          ESP.restart();
      });

      server.onNotFound([](AsyncWebServerRequest *request){
        request->send(404, "text/plain", "File Not Found. Redirecting to configuration portal.");
        request->redirect("/");
      });
  } else {
      // Normal handlers for when we are connected to a WiFi network
      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ 
        Serial.println(F("WEB_LOG: Client requested /index.html"));
        request->send(LittleFS, "/index.html", "text/html"); 
      });
      server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){ 
        Serial.println(F("WEB_LOG: Client requested /style.css"));
        request->send(LittleFS, "/style.css", "text/css"); 
      });
      server.on("/data_handling.js", HTTP_GET, [](AsyncWebServerRequest *request){ 
        Serial.println(F("WEB_LOG: Client requested /data_handling.js"));
        request->send(LittleFS, "/data_handling.js", "application/javascript"); 
      });
      server.on("/main_ui.js", HTTP_GET, [](AsyncWebServerRequest *request){ 
        Serial.println(F("WEB_LOG: Client requested /main_ui.js"));
        request->send(LittleFS, "/main_ui.js", "application/javascript"); 
      });
      server.on("/api/isReady", HTTP_GET, [](AsyncWebServerRequest *request){ 
        Serial.println(F("WEB_LOG: Client requested /api/isReady"));
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
        StaticJsonDocument<512> doc;
        doc["departureHour"] = currentSettings.departureHour;
        doc["departureMinute"] = currentSettings.departureMinute;
        doc["arrivalHour"] = currentSettings.arrivalHour;
        doc["arrivalMinute"] = currentSettings.arrivalMinute;
        doc["brightness"] = currentSettings.brightness;
        doc["notificationVolume"] = currentSettings.notificationVolume;
        doc["timeTravelAnimationDuration"] = currentSettings.timeTravelAnimationDuration;
        doc["timeTravelAnimationInterval"] = currentSettings.timeTravelAnimationInterval;
        doc["animationStyle"] = currentSettings.animationStyle;
        doc["glitchEffectFrequency"] = currentSettings.glitchEffectFrequency;
        doc["malfunctionFrequency"] = currentSettings.malfunctionFrequency;
        doc["timeTravelSoundToggle"] = currentSettings.timeTravelSoundToggle;
        doc["timeTravelVolumeFade"] = currentSettings.timeTravelVolumeFade;
        doc["presetCycleInterval"] = currentSettings.presetCycleInterval;
        doc["displayFormat24h"] = currentSettings.displayFormat24h;
        doc["loggingEnabled"] = currentSettings.loggingEnabled;
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
        doc["alphaVantageApiKey"] = currentSettings.alphaVantageApiKey.c_str();
        doc["stockRow1_symbol"] = currentSettings.stockRow1_symbol.c_str();
        doc["stockRow2_symbol"] = currentSettings.stockRow2_symbol.c_str();
        doc["stockRow3_symbol"] = currentSettings.stockRow3_symbol.c_str();

        JsonArray dataPoints = doc.createNestedArray("dataPoints");
        for (int i = 0; i < 5; i++) {
            if (i < currentSettings.numDataPoints) {
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
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
      });

      server.on("/api/timezones", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "application/json", TZ_JSON);
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
            doc["temperature"] = currentWeatherData.temp;
            doc["apparentTemperature"] = currentWeatherData.feels_like;
            doc["windSpeed"] = currentWeatherData.wind_speed;
            doc["humidity"] = currentWeatherData.humidity;
            doc["weatherCode"] = currentWeatherData.icon;
            doc["dailyHigh"] = currentWeatherData.temp_max;
            doc["dailyLow"] = currentWeatherData.temp_min;

            String jsonString;
            serializeJson(doc, jsonString);
            request->send(200, "application/json", jsonString);
        } else {
            request->send(503, "application/json", "{\"error\":\"Weather data not available\"}");
        }
      });
      
      AsyncCallbackJsonWebHandler* refreshWeatherHandler = new AsyncCallbackJsonWebHandler("/api/weather/refresh", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject obj = json.as<JsonObject>();
        if (obj.containsKey("cityName")) {
            std::string city = obj["cityName"].as<std::string>();
            WeatherTaskParams* params = new WeatherTaskParams{city, true};
            if (xTaskCreate(forceFetchWeatherDataTask, "forceFetchWeatherDataTask", 8192, params, 1, NULL) == pdPASS) {
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
        JsonObject obj = json.as<JsonObject>();
        
        Serial.println("SERVER_DEBUG: Received request to /api/saveSettings");
        String receivedJson;
        serializeJson(obj, receivedJson);
        Serial.println(receivedJson);

        std::string oldMqttBroker = currentSettings.mqttBroker.c_str();
        int oldMqttPort = currentSettings.mqttPort;
        std::string oldCityName = currentSettings.cityName.c_str();

        currentSettings.destinationYear = obj["destinationYear"] | currentSettings.destinationYear;
        currentSettings.destinationTimezoneIndex = obj["destinationTimezoneIndex"] | currentSettings.destinationTimezoneIndex;
        currentSettings.lastTimeDepartedYear = obj["lastTimeDepartedYear"] | currentSettings.lastTimeDepartedYear;
        currentSettings.lastTimeDepartedMonth = obj["lastTimeDepartedMonth"] | currentSettings.lastTimeDepartedMonth;
        currentSettings.lastTimeDepartedDay = obj["lastTimeDepartedDay"] | currentSettings.lastTimeDepartedDay;
        currentSettings.lastTimeDepartedHour = obj["lastTimeDepartedHour"] | currentSettings.lastTimeDepartedHour;
        currentSettings.lastTimeDepartedMinute = obj["lastTimeDepartedMinute"] | currentSettings.lastTimeDepartedMinute;
        currentSettings.presetCycleInterval = obj["presetCycleInterval"] | currentSettings.presetCycleInterval;
        currentSettings.departureHour = obj["departureHour"] | currentSettings.departureHour;
        currentSettings.departureMinute = obj["departureMinute"] | currentSettings.departureMinute;
        currentSettings.arrivalHour = obj["arrivalHour"] | currentSettings.arrivalHour;
        currentSettings.arrivalMinute = obj["arrivalMinute"] | currentSettings.arrivalMinute;
        currentSettings.brightness = obj["brightness"] | currentSettings.brightness;
        currentSettings.timeTravelAnimationDuration = obj["timeTravelAnimationDuration"] | currentSettings.timeTravelAnimationDuration;
        currentSettings.timeTravelAnimationInterval = obj["timeTravelAnimationInterval"] | currentSettings.timeTravelAnimationInterval;
        currentSettings.animationStyle = obj["animationStyle"] | currentSettings.animationStyle;
        currentSettings.glitchEffectFrequency = obj["glitchEffectFrequency"] | currentSettings.glitchEffectFrequency;
        currentSettings.malfunctionFrequency = obj["malfunctionFrequency"] | currentSettings.malfunctionFrequency;
        currentSettings.notificationVolume = obj["notificationVolume"] | currentSettings.notificationVolume;
        currentSettings.timeTravelSoundToggle = obj["timeTravelSoundToggle"] | currentSettings.timeTravelSoundToggle;
        currentSettings.timeTravelVolumeFade = obj["timeTravelVolumeFade"] | currentSettings.timeTravelVolumeFade;
        currentSettings.presentTimezoneIndex = obj["presentTimezoneIndex"] | currentSettings.presentTimezoneIndex;
        currentSettings.displayFormat24h = obj["displayFormat24h"] | currentSettings.displayFormat24h;
        currentSettings.loggingEnabled = obj["loggingEnabled"] | currentSettings.loggingEnabled;

        currentSettings.dataLinkEnabled = obj["dataLinkEnabled"] | currentSettings.dataLinkEnabled;
        currentSettings.dataLinkRefreshInterval = obj["dataLinkRefreshInterval"] | currentSettings.dataLinkRefreshInterval;
        if (obj.containsKey("mqttBroker")) currentSettings.mqttBroker = obj["mqttBroker"].as<std::string>().c_str();
        currentSettings.mqttPort = obj["mqttPort"] | 1883;
        if (obj.containsKey("mqttUser")) currentSettings.mqttUser = obj["mqttUser"].as<std::string>().c_str();
        if (obj.containsKey("mqttPassword")) currentSettings.mqttPassword = obj["mqttPassword"].as<std::string>().c_str();
        currentSettings.weatherModeEnabled = obj["weatherModeEnabled"] | currentSettings.weatherModeEnabled;
        if (obj.containsKey("cityName")) {
            std::string newCityName = obj["cityName"].as<std::string>();
            if (newCityName != oldCityName) {
                lastCityName = "";
                if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                    currentWeatherData.dataValid = false;
                    xSemaphoreGive(xDisplayDataMutex);
                }
            }
            currentSettings.cityName = newCityName.c_str();
        }
        currentSettings.useMetricUnits = obj["useMetricUnits"] | currentSettings.useMetricUnits;
        currentSettings.stockTickerModeEnabled = obj["stockTickerModeEnabled"] | currentSettings.stockTickerModeEnabled;
        if (obj.containsKey("alphaVantageApiKey")) {
            currentSettings.alphaVantageApiKey = obj["alphaVantageApiKey"].as<std::string>().c_str();
            Serial.printf("SERVER_DEBUG: Saving Alpha Vantage Key: %s\n", currentSettings.alphaVantageApiKey.c_str());
        }
        if (obj.containsKey("stockRow1_symbol")) currentSettings.stockRow1_symbol = obj["stockRow1_symbol"].as<std::string>().c_str();
        if (obj.containsKey("stockRow2_symbol")) currentSettings.stockRow2_symbol = obj["stockRow2_symbol"].as<std::string>().c_str();
        if (obj.containsKey("stockRow3_symbol")) currentSettings.stockRow3_symbol = obj["stockRow3_symbol"].as<std::string>().c_str();

        currentSettings.numDataPoints = obj["numDataPoints"] | 0;
        if (obj.containsKey("dataPoints")) {
            JsonArray dataPoints = obj["dataPoints"];
            for (int i = 0; i < 5; i++) {
                if (i < currentSettings.numDataPoints && i < dataPoints.size()) {
                    JsonObject dp = dataPoints[i];
                    if (dp.containsKey("dataSourceType")) currentSettings.dataPoints[i].dataSourceType = (DataSourceType)(dp["dataSourceType"].as<int>());
                    if (dp.containsKey("url")) currentSettings.dataPoints[i].url = dp["url"].as<std::string>().c_str();
                    if (dp.containsKey("monthPath")) currentSettings.dataPoints[i].monthPath = dp["monthPath"].as<std::string>().c_str();
                    if (dp.containsKey("dayPath")) currentSettings.dataPoints[i].dayPath = dp["dayPath"].as<std::string>().c_str();
                    if (dp.containsKey("yearPath")) currentSettings.dataPoints[i].yearPath = dp["yearPath"].as<std::string>().c_str();
                    if (dp.containsKey("timePath")) currentSettings.dataPoints[i].timePath = dp["timePath"].as<std::string>().c_str();
                    if (dp.containsKey("prefix")) currentSettings.dataPoints[i].prefix = dp["prefix"].as<std::string>().c_str();
                    if (dp.containsKey("suffix")) currentSettings.dataPoints[i].suffix = dp["suffix"].as<std::string>().c_str();
                    if (dp.containsKey("icon")) currentSettings.dataPoints[i].icon = dp["icon"].as<std::string>().c_str();
                    currentSettings.dataPoints[i].scrollSpeed = dp["scrollSpeed"] | 150;
                    if (dp.containsKey("mqttTopic")) currentSettings.dataPoints[i].mqttTopic = dp["mqttTopic"].as<std::string>().c_str();
                    if (dp.containsKey("yearPrefix")) currentSettings.dataPoints[i].yearPrefix = dp["yearPrefix"].as<std::string>().c_str();
                    if (dp.containsKey("yearSuffix")) currentSettings.dataPoints[i].yearSuffix = dp["yearSuffix"].as<std::string>().c_str();
                    if (dp.containsKey("displayMode")) currentSettings.dataPoints[i].displayMode = (DisplayMode)(dp["displayMode"].as<int>());
                    if (dp.containsKey("scrollingText")) currentSettings.dataPoints[i].scrollingText = dp["scrollingText"].as<std::string>().c_str();
                    if (dp.containsKey("authHeaderKey")) currentSettings.dataPoints[i].authHeaderKey = dp["authHeaderKey"].as<std::string>().c_str();
                    if (dp.containsKey("authHeaderValue")) currentSettings.dataPoints[i].authHeaderValue = dp["authHeaderValue"].as<std::string>().c_str();
                    if (dp.containsKey("apiExampleKey")) currentSettings.dataPoints[i].apiExampleKey = dp["apiExampleKey"].as<std::string>().c_str();
                } else {
                    currentSettings.dataPoints[i] = {}; // Clear unused data points
                }
            }
        }

        if (mqttClient.connected()) {
            mqttClient.disconnect();
        }
        saveSettings();
        delay(100);
        mqttReconnectRequired = true;

        if (hardwareInitialized && out) {
            out->SetGain((float)currentSettings.notificationVolume / 30.0f);
        }

        request->send(200, "text/plain", "Settings Saved!");
      });
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
        request->send_P(200, "application/json", apiTemplates);
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
        Serial.printf("WEB_LOG: 404 Not Found: %s\n", request->url().c_str());
        request->send(404, "text/plain", "Not found");
      });

      server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!request->authenticate(OTA_USER, OTA_PASS)) {
            return request->requestAuthentication();
        }
        request->send(LittleFS, "/update.html", "text/html");
      });

      server.on("/update", HTTP_POST, 
        [](AsyncWebServerRequest *request){
            // This is the success response
            AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", !Update.hasError() ? "Update successful! Rebooting..." : "UPDATE FAILED");
            response->addHeader("Connection", "close");
            request->send(response);
            if (!Update.hasError()) {
                delay(100);
                ESP.restart();
            }
        }, 
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
            // This is the upload handler
            if (!request->authenticate(OTA_USER, OTA_PASS)) {
                return request->requestAuthentication();
            }
            
            if(!index){
                Serial.printf("Update Start: %s\n", filename.c_str());
                if(!Update.begin(UPDATE_SIZE_UNKNOWN)){
                    Update.printError(Serial);
                }
            }
            if(len){
                if(Update.write(data, len) != len){
                    Update.printError(Serial);
                }
            }
            if(final){
                if(!Update.end(true)){
                    Update.printError(Serial);
                } else {
                    Serial.println("Update complete!");
                }
            }
        });

        // New endpoint for data file uploads
        server.on("/upload-data", HTTP_POST,
            [](AsyncWebServerRequest *request) {
                request->send(200, "text/plain", "File uploaded successfully!");
            },
            [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
                if (!index) {
                    String path = "/";
                    if (!filename.startsWith("/")) {
                        path += filename;
                    } else {
                        path = filename;
                    }
                    request->_tempFile = LittleFS.open(path, "w");
                    Serial.printf("WEB_LOG: Uploading file: %s\n", path.c_str());
                }

                if (len) {
                    request->_tempFile.write(data, len);
                }

                if (final) {
                    request->_tempFile.close();
                    Serial.printf("WEB_LOG: File upload complete: %s, %u bytes\n", filename.c_str(), index + len);
                }
            });
  }

  server.begin();
  logger.push(LOG_INFO, "WEB", "HTTP server started."); // FIX: Changed 'log.push' to 'logger.push'
  logger.push(LOG_INFO, "WEB", "Web server started... OK"); // FIX: Changed 'log.push' to 'logger.push'
}