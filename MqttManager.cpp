#include "MqttManager.h"
#include "EventManager.h"
#include "AnimationManager.h"
#include "DisplayManager.h"
#include "DataManager.h"
#include "web_server.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Preferences.h>
#include <LCBUrl.h> 

bool haDiscoveryPublished = false;

void setupMqtt() {
  if (currentSettings.mqttBroker.empty()) {
    Serial.println("MQTT_LOG: No broker configured. MQTT setup skipped.");
    return;
  }
  mqttClient.setServer(currentSettings.mqttBroker.c_str(), currentSettings.mqttPort);
  mqttClient.setCallback(mqttCallback);
  Serial.printf("MQTT_LOG: Client configured for broker [%s] on port [%d]\n", currentSettings.mqttBroker.c_str(), currentSettings.mqttPort);
}

void clearHaEntity(const char* component, const char* unique_id_suffix) {
    String object_id = String(MQTT_UNIQUE_ID) + "_" + unique_id_suffix;
    String topic = String(MQTT_BASE_TOPIC) + "/" + component + "/" + object_id + "/config";
    if (mqttClient.connected()) {
        mqttClient.publish(topic.c_str(), "", true);
    }
}

void publishHaPresetSelector() {
    if (!mqttClient.connected()) return;

    String device_base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    DynamicJsonDocument doc(1024);
    String topic;
    String payload;

    JsonObject device = doc.createNestedObject("device");
    device["identifiers"] = MQTT_UNIQUE_ID;

    JsonArray availability = doc.createNestedArray("availability");
    JsonObject availability_topic = availability.createNestedObject();
    availability_topic["topic"] = device_base_topic + "/status";
    availability_topic["payload_available"] = "online";
    availability_topic["payload_not_available"] = "offline";

    doc["name"] = "Last Departed Preset";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_preset_selector";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_preset_selector";
    doc["command_topic"] = device_base_topic + "/preset_selector/command";
    doc["state_topic"] = device_base_topic + "/preset_selector/state";
    doc["icon"] = "mdi:history";
    doc["entity_category"] = "config";

    JsonArray options = doc.createNestedArray("options");
    options.add("Einstein's Test (1985)");
    options.add("Marty's First Jump (1985)");
    options.add("Arrival in Past (1955)");
    options.add("Lightning Strike (1955)");

    Preferences prefs;
    prefs.begin(PREFERENCES_NAMESPACE, true);
    String presetsJson = prefs.getString("customPresets", "[]");
    prefs.end();
    DynamicJsonDocument presetsDoc(2048);
    if (deserializeJson(presetsDoc, presetsJson) == DeserializationError::Ok) {
        for (JsonObject preset : presetsDoc.as<JsonArray>()) {
            options.add(preset["name"].as<String>());
        }
    }

    topic = String(MQTT_BASE_TOPIC) + "/select/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
}


void publishDeviceTriggers() {
    String device_base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    DynamicJsonDocument doc(1024);
    String topic;
    String payload;

    JsonObject device = doc.createNestedObject("device");
    device["identifiers"] = MQTT_UNIQUE_ID;

    doc["automation_type"] = "trigger";
    doc["topic"] = device_base_topic + "/events";
    doc["type"] = "animation_started";
    doc["subtype"] = "event";
    topic = String(MQTT_BASE_TOPIC) + "/device_automation/" + MQTT_UNIQUE_ID + "/anim_started/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
    
    doc["type"] = "animation_completed";
    topic = String(MQTT_BASE_TOPIC) + "/device_automation/" + MQTT_UNIQUE_ID + "/anim_completed/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    doc["type"] = "sleep_mode_entered";
    topic = String(MQTT_BASE_TOPIC) + "/device_automation/" + MQTT_UNIQUE_ID + "/sleep_entered/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    doc["type"] = "sleep_mode_exited";
    topic = String(MQTT_BASE_TOPIC) + "/device_automation/" + MQTT_UNIQUE_ID + "/sleep_exited/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    doc["type"] = "malfunction_triggered";
    topic = String(MQTT_BASE_TOPIC) + "/device_automation/" + MQTT_UNIQUE_ID + "/malfunction/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    doc["type"] = "preset_changed";
    topic = String(MQTT_BASE_TOPIC) + "/device_automation/" + MQTT_UNIQUE_ID + "/preset_changed/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

void publishHaDiagnosticAttributes() {
    if (!mqttClient.connected()) return;
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    DynamicJsonDocument doc(512);

    doc["free_heap"] = ESP.getFreeHeap();
    doc["uptime_seconds"] = millis() / 1000;
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["ip_address"] = WiFi.localIP().toString();
    doc["animation_style"] = currentSettings.animationStyle;
    doc["is_mqtt_connected"] = mqttClient.connected();
    
    String attributes_payload;
    serializeJson(doc, attributes_payload);
    mqttClient.publish((base_topic + "/status/attributes").c_str(), attributes_payload.c_str(), false);
}


void publishHaAutoDiscovery() {
    String device_base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    StaticJsonDocument<512> device_doc;
    JsonObject device = device_doc.to<JsonObject>();
    device["identifiers"] = MQTT_UNIQUE_ID;
    device["name"] = "Time Circuits Display";
    device["model"] = "BTTF Clock v1";
    device["manufacturer"] = "Doc Brown Industries";
    device["sw_version"] = "2.0";

    StaticJsonDocument<256> availability_doc;
    JsonArray availability = availability_doc.to<JsonArray>();
    JsonObject availability_topic = availability.createNestedObject();
    availability_topic["topic"] = device_base_topic + "/status";
    availability_topic["payload_available"] = "online";
    availability_topic["payload_not_available"] = "offline";

    DynamicJsonDocument doc(2048);
    String topic;
    String payload;
    
    doc.clear();
    doc["name"] = "Status";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_status";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_status";
    doc["state_topic"] = device_base_topic + "/status/state";
    doc["json_attributes_topic"] = device_base_topic + "/status/attributes";
    doc["icon"] = "mdi:clock-outline";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/sensor/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    const char* time_sensors[][3] = {
        {"last_time_departed", "Last Time Departed", "mdi:clock-start"},
        {"present_time", "Present Time", "mdi:clock-time-eight-outline"},
        {"destination_time", "Destination Time", "mdi:clock-end"},
    };
    for (auto const& sensor : time_sensors) {
        doc.clear();
        doc["name"] = sensor[1];
        doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_" + sensor[0];
        doc["object_id"] = String(MQTT_UNIQUE_ID) + "_" + sensor[0];
        doc["state_topic"] = device_base_topic + "/" + sensor[0] + "/state";
        doc["icon"] = sensor[2];
        doc["device_class"] = "timestamp";
        doc["device"] = device;
        doc["availability"] = availability;
        topic = String(MQTT_BASE_TOPIC) + "/sensor/" + doc["object_id"].as<String>() + "/config";
        serializeJson(doc, payload);
        mqttClient.publish(topic.c_str(), payload.c_str(), true);
    }


    const char* number_configs[][5] = {
        {"animation_interval", "Animation Interval", "mdi:clock-in", "min", "0,120,1"},
        {"animation_duration", "Animation Duration", "mdi:movie-filter", "ms", "1000,10000,100"},
        {"datalink_refresh", "DataLink Refresh", "mdi:api", "min", "1,60,1"}
    };
    for (auto const& cfg : number_configs) {
        doc.clear();
        doc["name"] = cfg[1];
        String id_suffix = cfg[0];
        doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        doc["object_id"] = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        doc["command_topic"] = device_base_topic + "/" + id_suffix + "/command";
        doc["state_topic"] = device_base_topic + "/" + id_suffix + "/state";
        doc["icon"] = cfg[2];
        doc["unit_of_measurement"] = cfg[3];
        
        int min_val, max_val, step_val;
        char cfg_copy[20];
        strncpy(cfg_copy, cfg[4], sizeof(cfg_copy) - 1);
        cfg_copy[sizeof(cfg_copy) - 1] = '\0';
        char* token = strtok(cfg_copy, ",");
        min_val = atoi(token);
        token = strtok(NULL, ",");
        max_val = atoi(token);
        token = strtok(NULL, ",");
        step_val = atoi(token);

        doc["min"] = min_val;
        doc["max"] = max_val;
        doc["step"] = step_val;
        
        doc["entity_category"] = "config";
        doc["device"] = device;
        doc["availability"] = availability;
        topic = String(MQTT_BASE_TOPIC) + "/number/" + doc["object_id"].as<String>() + "/config";
        serializeJson(doc, payload);
        mqttClient.publish(topic.c_str(), payload.c_str(), true);
    }

     const char* switch_configs[][3] = {
        {"24h_format", "24-Hour Format", "mdi:clock-time-twelve-outline"}
    };
    for (auto const& cfg : switch_configs) {
        doc.clear();
        doc["name"] = cfg[1];
        String id_suffix = cfg[0];
        doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        doc["object_id"] = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        doc["command_topic"] = device_base_topic + "/" + id_suffix + "/command";
        doc["state_topic"] = device_base_topic + "/" + id_suffix + "/state";
        doc["icon"] = cfg[2];
        doc["entity_category"] = "config";
        doc["device"] = device;
        doc["availability"] = availability;
        topic = String(MQTT_BASE_TOPIC) + "/switch/" + doc["object_id"].as<String>() + "/config";
        serializeJson(doc, payload);
        mqttClient.publish(topic.c_str(), payload.c_str(), true);
    }
    
    doc.clear();
    doc["name"] = "Temporary Marquee Override";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_marquee_temp_override";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_marquee_temp_override";
    doc["command_topic"] = device_base_topic + "/marquee_temp_override/command";
    doc["icon"] = "mdi:label-outline";
    doc["entity_category"] = "config";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/text/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    const char* button_configs[][3] = {
        {"reboot_device", "Reboot Device", "mdi:restart"},
        {"force_ntp_sync", "Force NTP Sync", "mdi:timer-sync-outline"},
        {"factory_reset", "Factory Reset", "mdi:delete-restore"},
        {"save_all_settings", "Save All Settings", "mdi:content-save-all-outline"}
    };
    for (auto const& cfg : button_configs) {
        doc.clear();
        doc["name"] = cfg[1];
        String id_suffix = cfg[0];
        doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        doc["object_id"] = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        doc["command_topic"] = device_base_topic + "/" + id_suffix + "/command";
        doc["icon"] = cfg[2];
        doc["entity_category"] = "config";
        doc["device"] = device;
        doc["availability"] = availability;
        topic = String(MQTT_BASE_TOPIC) + "/button/" + doc["object_id"].as<String>() + "/config";
        serializeJson(doc, payload);
        mqttClient.publish(topic.c_str(), payload.c_str(), true);
    }
    
    doc.clear();
    doc["name"] = "Temporal Echo Effect";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_temporal_echo";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_temporal_echo";
    doc["command_topic"] = device_base_topic + "/temporal_echo/command";
    doc["state_topic"] = device_base_topic + "/temporal_echo/state";
    doc["icon"] = "mdi:ghost";
    doc["entity_category"] = "config";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/switch/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    doc.clear();
    doc["name"] = "Profile";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_profile";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_profile";
    doc["command_topic"] = device_base_topic + "/profile/command";
    doc["state_topic"] = device_base_topic + "/profile/state";
    JsonArray profiles = doc.createNestedArray("options");
    profiles.add("Standard");
    profiles.add("Cinematic");
    profiles.add("Silent Night");
    profiles.add("Unstable");
    profiles.add("Custom");
    doc["icon"] = "mdi:movie-settings";
    doc["entity_category"] = "config";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/select/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    for (int i=0; i < 5; ++i) {
        doc.clear();
        doc["name"] = "Data Point " + String(i + 1) + " Source";
        String id_suffix = "datapoint_" + String(i) + "_source";
        doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        doc["object_id"] = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        doc["command_topic"] = device_base_topic + "/" + id_suffix + "/command";
        doc["state_topic"] = device_base_topic + "/" + id_suffix + "/state";
        JsonArray sources = doc.createNestedArray("options");
        sources.add("API");
        sources.add("MQTT");
        sources.add("Home Assistant Push");
        doc["icon"] = "mdi:database-arrow-down";
        doc["entity_category"] = "config";
        doc["device"] = device;
        doc["availability"] = availability;
        topic = String(MQTT_BASE_TOPIC) + "/select/" + doc["object_id"].as<String>() + "/config";
        serializeJson(doc, payload);
        mqttClient.publish(topic.c_str(), payload.c_str(), true);
    }
    
    doc.clear();
    doc["name"] = "Run Sequence";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_run_sequence";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_run_sequence";
    doc["command_topic"] = device_base_topic + "/run_sequence/command";
    doc["icon"] = "mdi:play-box-multiple-outline";
    doc["entity_category"] = "config";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/text/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    doc.clear();
    doc["name"] = "Stock Ticker Mode";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_stock_ticker_mode";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_stock_ticker_mode";
    doc["command_topic"] = device_base_topic + "/stock_ticker_mode/command";
    doc["state_topic"] = device_base_topic + "/stock_ticker_mode/state";
    doc["icon"] = "mdi:chart-line";
    doc["entity_category"] = "config";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/switch/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    const char* stock_rows[][2] = {
        {"stock_row_1", "Stock Ticker Row 1 (Dest)"},
        {"stock_row_2", "Stock Ticker Row 2 (Pres)"},
        {"stock_row_3", "Stock Ticker Row 3 (Last)"}
    };
    for (auto const& cfg : stock_rows) {
        doc.clear();
        doc["name"] = cfg[1];
        String id_suffix = cfg[0];
        doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        doc["object_id"] = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        doc["command_topic"] = device_base_topic + "/" + id_suffix + "/command";
        doc["state_topic"] = device_base_topic + "/" + id_suffix + "/state";
        doc["icon"] = "mdi:alpha-t-box-outline";
        doc["entity_category"] = "config";
        doc["device"] = device;
        doc["availability"] = availability;
        topic = String(MQTT_BASE_TOPIC) + "/text/" + doc["object_id"].as<String>() + "/config";
        serializeJson(doc, payload);
        mqttClient.publish(topic.c_str(), payload.c_str(), true);
    }

    doc.clear();
    doc["name"] = "Alpha Vantage API Key";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_alpha_vantage_api_key";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_alpha_vantage_api_key";
    doc["command_topic"] = device_base_topic + "/alpha_vantage_api_key/command";
    doc["state_topic"] = device_base_topic + "/alpha_vantage_api_key/state";
    doc["icon"] = "mdi:key-variant";
    doc["entity_category"] = "config";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/text/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    // New Audio sensor for stream state
    doc.clear();
    doc["name"] = "Audio Stream Status";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_audio_status";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_audio_status";
    doc["state_topic"] = device_base_topic + "/audio/state";
    doc["icon"] = "mdi:waveform";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/sensor/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
    
    haDiscoveryPublished = true;
}

void reconnectMqtt() {
  if (currentSettings.mqttBroker.empty()) return;
  
  Serial.println("MQTT_LOG: Attempting to connect...");
  delay(100); 

  String clientId = "BTTF-Clock-";
  clientId += String(random(0xffff), HEX);
  String availability_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/status";

  bool connectResult = false;
  if (!currentSettings.mqttUser.empty()) {
      Serial.printf("MQTT_LOG: Connecting with Client ID: %s and username: %s\n", clientId.c_str(), currentSettings.mqttUser.c_str());
      connectResult = mqttClient.connect(clientId.c_str(), currentSettings.mqttUser.c_str(), currentSettings.mqttPassword.c_str(), availability_topic.c_str(), 1, true, "offline");
  } else {
      Serial.printf("MQTT_LOG: Connecting with Client ID: %s (no username)\n", clientId.c_str());
      connectResult = mqttClient.connect(clientId.c_str(), availability_topic.c_str(), 1, true, "offline");
  }

  if (connectResult) {
    Serial.println("MQTT_LOG: SUCCESS! MQTT client connected.");
    delay(100); 
    
    mqttClient.publish(availability_topic.c_str(), "online", true);

    if (!haDiscoveryPublished) {
        publishHaAutoDiscovery();
    } else {
        publishHaPresetSelector();
    }

    publishAllHaStates();
    String command_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/+/command";
    mqttClient.subscribe(command_topic.c_str());
    
    String audio_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/tts/play";
    mqttClient.subscribe(audio_topic.c_str());
    audio_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/radio/command";
    mqttClient.subscribe(audio_topic.c_str());


    for (int i = 0; i < currentSettings.numDataPoints; i++) {
      if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_MQTT && !currentSettings.dataPoints[i].mqttTopic.empty()) {
        mqttClient.subscribe(currentSettings.dataPoints[i].mqttTopic.c_str());
      }
      if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_HA) {
        String base_dp_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/datapoint/" + String(i);
        mqttClient.subscribe((base_dp_topic + "/month/set").c_str());
        mqttClient.subscribe((base_dp_topic + "/day/set").c_str());
        mqttClient.subscribe((base_dp_topic + "/year/set").c_str());
        mqttClient.subscribe((base_dp_topic + "/time/set").c_str());
      }
    }
  } else {
    Serial.printf("MQTT_LOG: FAILED! rc=%d. ", mqttClient.state());
    switch (mqttClient.state()) {
      case -4: Serial.println("Connection timeout."); break;
      case -3: Serial.println("Connection lost."); break;
      case -2: Serial.println("Connect failed."); break;
      case -1: Serial.println("Disconnected."); break;
      case 1: Serial.println("Bad protocol version."); break;
      case 2: Serial.println("Client ID rejected."); break;
      case 3: Serial.println("Server unavailable."); break;
      case 4: Serial.println("Bad username or password."); break;
      case 5: Serial.println("Not authorized."); break;
      default: Serial.println("Unknown error."); break;
    }
    delay(100); 
  }
}

void mqttCallback(char* topic, unsigned char* payload, unsigned int length) {
    String message = "";
    message.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    String topicStr = String(topic);
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/";
    bool stateChanged = false;
    bool settingsChanged = false;
    
    if (topicStr.endsWith("/command")) {
        String component_topic = topicStr.substring(base_topic.length());
        String component = component_topic.substring(0, component_topic.indexOf('/'));

        char msg_copy[length + 1];
        strncpy(msg_copy, (char*)payload, length);
        msg_copy[length] = '\0';
        
        if (component == "power") {
            isDisplayAsleep = (message == "OFF");
            stateChanged = true;
        } 
        else if (component == "brightness") {
            int brightness = message.toInt();
            if (brightness >= 0 && brightness <= 7) {
                currentSettings.brightness = brightness;
                settingsChanged = true;
                broadcastWsStateUpdate("brightness", brightness);
            }
        }
        else if (component.startsWith("datapoint_")) {
            int dp_index = component.substring(10).toInt();
            if (dp_index >= 0 && dp_index < 5) {
                if (topicStr.endsWith("marquee/command")) {
                    currentSettings.dataPoints[dp_index].scrollingText = message.c_str();
                    settingsChanged = true;
                } else if (topicStr.endsWith("enabled/command")) {
                    settingsChanged = true;
                }
            }
        }
        else if (component == "destination_year") {
            int year = message.toInt();
            if (year >= 1000 && year <= 9999) {
                currentSettings.destinationYear = year;
                settingsChanged = true;
                broadcastWsStateUpdate("destinationYear", year);
            }
        }
        else if (component == "animation_style") {
            currentSettings.animationStyle = message.toInt();
            settingsChanged = true;
            broadcastWsStateUpdate("animationStyleSelect", currentSettings.animationStyle);
        }
        else if (component == "glitch_freq") {
            int freq = message.toInt();
            if (freq >= 0 && freq <= 100) {
                currentSettings.glitchEffectFrequency = freq;
                settingsChanged = true;
                broadcastWsStateUpdate("glitchEffectFrequency", freq);
            }
        }
        else if (component == "malfunction_chance") {
            int chance = message.toInt();
            if (chance >= 1 && chance <= 1000) {
                currentSettings.malfunctionFrequency = chance;
                settingsChanged = true;
                broadcastWsStateUpdate("malfunctionFrequency", chance);
            }
        }
        else if (component == "volume") {
            int vol = message.toInt();
            if (vol >= 0 && vol <= 21) {
                currentSettings.notificationVolume = vol;
                audio.setVolume(vol);
                settingsChanged = true;
                broadcastWsStateUpdate("notificationVolume", vol);
            }
        }
        else if (component == "override") {
            isMessageOverrideActive = (message == "ON");
            stateChanged = true;
        }
        else if (component == "override_text") {
            int first_newline = message.indexOf('\n');
            int second_newline = message.indexOf('\n', first_newline + 1);
            if (first_newline != -1) {
                overrideMessageLine1 = message.substring(0, first_newline);
                if (second_newline != -1) {
                    overrideMessageLine2 = message.substring(first_newline + 1, second_newline);
                    overrideMessageLine3 = message.substring(second_newline + 1);
                } else {
                    overrideMessageLine2 = message.substring(first_newline + 1);
                    overrideMessageLine3 = "";
                }
            } else {
                overrideMessageLine1 = message;
                overrideMessageLine2 = "";
                overrideMessageLine3 = "";
            }
            stateChanged = true;
        }
        else if (component == "animation" && message == "START") {
            startTimeTravelAnimation();
        }
        else if (topicStr.startsWith(base_topic) && topicStr.endsWith("/command") && (topicStr.indexOf("dest_") != -1 || topicStr.indexOf("pres_") != -1 || topicStr.indexOf("last_") != -1)) {
            String comp = topicStr.substring(base_topic.length(), topicStr.length() - 8);
            int row = -1, segment = -1;
            if (comp.startsWith("dest_")) row = 0;
            else if (comp.startsWith("pres_")) row = 1;
            else if (comp.startsWith("last_")) row = 2;

            if (comp.endsWith("_month")) segment = 0;
            else if (comp.endsWith("_day")) segment = 1;
            else if (comp.endsWith("_year")) segment = 2;
            else if (comp.endsWith("_time")) segment = 3;
            
            if (row != -1 && segment != -1) {
                updateDisplaySegment(row, segment, message.c_str());
                stateChanged = true;
            }
        }
        else if (topicStr == base_topic + "trigger_effect/command") {
            if (message == "Trigger Glitch") triggerTemporalGlitch();
            else if (message == "Trigger Malfunction") {
                isMalfunctioning = true;
                malfunctionStartTime = millis();
                currentMalfunctionPhase = MAL_HAYWIRE;
            }
            else if (message == "Run Boot Sequence") runBootSequence();
            mqttClient.publish((base_topic + "trigger_effect/state").c_str(), "None", true);
        }
        else if (topicStr == base_topic + "flash_command/command") {
            int row = -1, segment = -1;
            if (message.startsWith("dest_")) row = 0;
            else if (message.startsWith("pres_")) row = 1;
            else if (message.startsWith("last_")) row = 2;

            if (message.endsWith("_month")) segment = 0;
            else if (message.endsWith("_day")) segment = 1;
            else if (message.endsWith("_year")) segment = 2;
            else if (message.endsWith("_time")) segment = 3;
            
            if (row != -1 && segment != -1) {
                triggerFlashEffect(row, segment);
            }
        }
        else if (topicStr == base_topic + "sleep_time/command" || topicStr == base_topic + "wake_time/command") {
            int colonPos = message.indexOf(':');
            if (colonPos != -1) {
                int hour = message.substring(0, colonPos).toInt();
                int minute = message.substring(colonPos + 1).toInt();
                if (topicStr.indexOf("sleep_time") != -1) {
                    currentSettings.departureHour = hour;
                    currentSettings.departureMinute = minute;
                } else {
                    currentSettings.arrivalHour = hour;
                    currentSettings.arrivalMinute = minute;
                }
                settingsChanged = true;
            }
        }
        else if (topicStr == base_topic + "preset_selector/command") {
            mqttClient.publish((base_topic + "preset_selector/state").c_str(), message.c_str(), true);
        }
        else if (topicStr == base_topic + "play_sound/command") {
            if (message != "None") {
                if (hardwareInitialized) {
                    String filepath = "/" + message + ".mp3";
                    playSound(filepath.c_str());
                }
            }
            mqttClient.publish((base_topic + "play_sound/state").c_str(), "None", true);
        }
        else if (topicStr == base_topic + "sound_toggle/command") {
            currentSettings.timeTravelSoundToggle = (message == "ON");
            settingsChanged = true;
            broadcastWsStateUpdate("timeTravelSoundToggle", currentSettings.timeTravelSoundToggle);
        }
        else if (topicStr == base_topic + "weather_mode/command") {
            bool enabled = (message == "ON");
            currentSettings.weatherModeEnabled = enabled;
            if (enabled) {
                currentSettings.dataLinkEnabled = false;
            }
            settingsChanged = true;
            broadcastWsStateUpdate("weatherModeEnabled", enabled);
            if(enabled) broadcastWsStateUpdate("dataLinkEnabled", false);
        }
        else if (topicStr == base_topic + "weather_city/command") {
            if (currentSettings.cityName != message.c_str()) {
                currentSettings.cityName = message.c_str();
                WeatherTaskParams* params = new WeatherTaskParams{currentSettings.cityName, true};
                xTaskCreatePinnedToCore(forceFetchWeatherDataTask, "forceFetchWeatherDataTask", 8192, params, 1, NULL, 0);
                settingsChanged = true;
            }
        }
        else if (topicStr == base_topic + "weather_refresh/command") {
            if (message == "PRESS") {
                WeatherTaskParams* params = new WeatherTaskParams{currentSettings.cityName, true};
                xTaskCreatePinnedToCore(forceFetchWeatherDataTask, "forceFetchWeatherDataTask", 8192, params, 1, NULL, 0);
            }
        }
        else if (topicStr == base_topic + "24h_format/command") {
            currentSettings.displayFormat24h = (message == "ON");
            settingsChanged = true;
            broadcastWsStateUpdate("displayFormat24h", currentSettings.displayFormat24h);
        } else if (topicStr == base_topic + "animation_interval/command") {
            currentSettings.timeTravelAnimationInterval = message.toInt();
            settingsChanged = true;
            broadcastWsStateUpdate("timeTravelAnimationInterval", currentSettings.timeTravelAnimationInterval);
        } else if (topicStr == base_topic + "animation_duration/command") {
            currentSettings.timeTravelAnimationDuration = message.toInt();
            settingsChanged = true;
            broadcastWsStateUpdate("timeTravelAnimationDuration", currentSettings.timeTravelAnimationDuration);
        } else if (topicStr == base_topic + "datalink_refresh/command") {
            currentSettings.dataLinkRefreshInterval = message.toInt();
            settingsChanged = true;
            broadcastWsStateUpdate("dataLinkRefreshInterval", currentSettings.dataLinkRefreshInterval);
        }
        else if (topicStr == base_topic + "marquee_temp_override/command") {
            DynamicJsonDocument doc(256);
            if (deserializeJson(doc, message) == DeserializationError::Ok) {
                marqueeOverrideMessage = doc["text"].as<String>();
                unsigned long duration = doc["duration"] | 0;
                isMarqueeOverrideActive = true;
                marqueeOverrideEndTime = (duration > 0) ? millis() + (duration * 1000) : 0;
            } else {
                marqueeOverrideMessage = message;
                isMarqueeOverrideActive = true;
                marqueeOverrideEndTime = 0;
            }
            stateChanged = true;
        }
        else if (topicStr == base_topic + "reboot_device/command" && message == "PRESS") {
            ESP.restart();
        } else if (topicStr == base_topic + "force_ntp_sync/command" && message == "PRESS") {
            ntpSyncRequested = true;
        } else if (topicStr == base_topic + "factory_reset/command" && message == "PRESS") {
            preferences.begin(PREFERENCES_NAMESPACE, false);
            preferences.clear();
            preferences.end();
            ESP.restart();
        }
        else if (topicStr == base_topic + "save_all_settings/command" && message == "PRESS") {
            saveSettings();
        }
        else if (topicStr == base_topic + "temporal_echo/command") {
            isEchoEffectActive = (message == "ON");
            if (isEchoEffectActive) {
                echoEffectStartTime = millis();
            }
            stateChanged = true;
        }
        else if (topicStr == base_topic + "profile/command") {
            if (message == "Standard") {
                currentSettings.brightness = 5;
                currentSettings.notificationVolume = 15;
                currentSettings.timeTravelSoundToggle = true;
                currentSettings.glitchEffectFrequency = 0;
                currentSettings.malfunctionFrequency = 25;
            } else if (message == "Cinematic") {
                currentSettings.animationStyle = ANIMATION_TIMELINE_SKIM;
                currentSettings.timeTravelAnimationDuration = 8000;
                currentSettings.glitchEffectFrequency = 10;
            } else if (message == "Silent Night") {
                currentSettings.brightness = 1;
                currentSettings.notificationVolume = 0;
                currentSettings.timeTravelSoundToggle = false;
                currentSettings.glitchEffectFrequency = 0;
            } else if (message == "Unstable") {
                currentSettings.brightness = 7;
                currentSettings.glitchEffectFrequency = 75;
                currentSettings.malfunctionFrequency = 20;
            }
            mqttClient.publish((base_topic + "profile/state").c_str(), message.c_str(), true);
            settingsChanged = true;
        }
        else if (topicStr.indexOf("/datapoint_") != -1 && topicStr.endsWith("_source/command")) {
            String component = topicStr.substring(base_topic.length(), topicStr.length() - 8);
            int dp_index = component.substring(10, component.indexOf('_', 10)).toInt();
            
            if (dp_index >= 0 && dp_index < 5) {
                DataSourceType newSource;
                if (message == "MQTT") newSource = DATA_SOURCE_MQTT;
                else if (message == "Home Assistant Push") newSource = DATA_SOURCE_HA;
                else newSource = DATA_SOURCE_API;

                if (currentSettings.dataPoints[dp_index].dataSourceType != newSource) {
                    currentSettings.dataPoints[dp_index].dataSourceType = newSource;
                    mqttReconnectRequired = true;
                    settingsChanged = true;
                }
            }
        }
        else if (topicStr == base_topic + "run_sequence/command") {
            isSequenceActive = true;
            currentSequenceStep = 0;
            sequenceStepStartTime = millis();
            int stepIndex = 0;
            char script[length + 1];
            message.toCharArray(script, length + 1);

            char* command = strtok(script, ";");
            while (command != NULL && stepIndex < 19) {
                char* p = strchr(command, '(');
                if(p) {
                    *p = 0;
                    char* args = p + 1;
                    p = strchr(args, ')');
                    if(p) *p = 0;

                    if (strcmp(command, "text") == 0) {
                        sequence[stepIndex].command = SEQ_CMD_TEXT;
                        char* target = strtok(args, ",");
                        char* text = strtok(NULL, ",");
                        if (strstr(target, "dest")) sequence[stepIndex].targetRow = 0;
                        else if (strstr(target, "pres")) sequence[stepIndex].targetRow = 1;
                        else if (strstr(target, "last")) sequence[stepIndex].targetRow = 2;
                        if (strstr(target, "month")) sequence[stepIndex].targetSegment = 0;
                        else if (strstr(target, "day")) sequence[stepIndex].targetSegment = 1;
                        else if (strstr(target, "year")) sequence[stepIndex].targetSegment = 2;
                        else if (strstr(target, "time")) sequence[stepIndex].targetSegment = 3;
                        sequence[stepIndex].stringParam = text;
                        stepIndex++;
                    } else if (strcmp(command, "wait") == 0) {
                        sequence[stepIndex].command = SEQ_CMD_WAIT;
                        sequence[stepIndex].intParam = atoi(args);
                        stepIndex++;
                    } else if (strcmp(command, "sound") == 0) {
                        sequence[stepIndex].command = SEQ_CMD_SOUND;
                        sequence[stepIndex].stringParam = args;
                        stepIndex++;
                    } else if (strcmp(command, "flash") == 0) {
                        sequence[stepIndex].command = SEQ_CMD_FLASH;
                        char* target = strtok(args, ",");
                        char* duration = strtok(NULL, ",");
                        if (strstr(target, "dest")) sequence[stepIndex].targetRow = 0;
                        else if (strstr(target, "pres")) sequence[stepIndex].targetRow = 1;
                        else if (strstr(target, "last")) sequence[stepIndex].targetRow = 2;
                        if (strstr(target, "month")) sequence[stepIndex].targetSegment = 0;
                        else if (strstr(target, "day")) sequence[stepIndex].targetSegment = 1;
                        else if (strstr(target, "year")) sequence[stepIndex].targetSegment = 2;
                        else if (strstr(target, "time")) sequence[stepIndex].targetSegment = 3;
                        sequence[stepIndex].intParam = atoi(duration);
                        stepIndex++;
                    }
                }
                command = strtok(NULL, ";");
            }
            sequence[stepIndex].command = SEQ_CMD_END;
        }
        else if (component == "stock_ticker_mode") {
            currentSettings.stockTickerModeEnabled = (message == "ON");
            settingsChanged = true;
            broadcastWsStateUpdate("stockTickerModeEnabled", currentSettings.stockTickerModeEnabled);
        }
        else if (component == "stock_row_1") {
            currentSettings.stockRow1_symbol = message.c_str();
            settingsChanged = true;
        }
        else if (component == "stock_row_2") {
            currentSettings.stockRow2_symbol = message.c_str();
            settingsChanged = true;
        }
        else if (component == "stock_row_3") {
            currentSettings.stockRow3_symbol = message.c_str();
            settingsChanged = true;
        }
        else if (component == "alpha_vantage_api_key") {
            currentSettings.alphaVantageApiKey = message.c_str();
            settingsChanged = true;
        }
        else if (topicStr == base_topic + "tts/play") {
            startAudioStream(message.c_str(), true);
        }
        else if (topicStr == base_topic + "radio/command") {
            if (message == "stop") {
                stopAudioStream();
            } else {
                startAudioStream(message.c_str(), false);
            }
        }
    }
    else {
        for (int i = 0; i < currentSettings.numDataPoints; i++) {
            if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_MQTT && topicStr == currentSettings.dataPoints[i].mqttTopic.c_str()) {
                if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                    displayPages[i].time = message.c_str();
                    xSemaphoreGive(xDisplayDataMutex);
                }
                break;
            }
        }
        if (topicStr.startsWith(base_topic + "datapoint/")) {
            int dp_index = topicStr.substring(base_topic.length() + 10, topicStr.indexOf('/', base_topic.length() + 10)).toInt();
            if (dp_index >= 0 && dp_index < 5) {
                if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                    if (topicStr.endsWith("/month/set")) displayPages[dp_index].month = message.c_str();
                    else if (topicStr.endsWith("/day/set")) displayPages[dp_index].day = message.c_str();
                    else if (topicStr.endsWith("/year/set")) displayPages[dp_index].year = message.c_str();
                    else if (topicStr.endsWith("/time/set")) displayPages[dp_index].time = message.c_str();
                    xSemaphoreGive(xDisplayDataMutex);
                }
            }
        }
    }
    if (settingsChanged) {
        saveSettings();
        if (topicStr != base_topic + "profile/command") {
             mqttClient.publish((base_topic + "profile/state").c_str(), "Custom", true);
        }
        stateChanged = true;
    }
    if (stateChanged) {
        publishAllHaStates();
    }
}


void publishAllHaStates() {
    if (!mqttClient.connected()) return;
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    char payload[20];

    itoa(currentSettings.destinationYear, payload, 10);
    mqttClient.publish((base_topic + "/destination_year/state").c_str(), payload, true);
    
    mqttClient.publish((base_topic + "/override/state").c_str(), isMessageOverrideActive ? "ON" : "OFF", true);
    
    String overrideMessage = overrideMessageLine1;
    if (overrideMessageLine2.length() > 0) overrideMessage += "\n" + overrideMessageLine2;
    if (overrideMessageLine3.length() > 0) overrideMessage += "\n" + overrideMessageLine3;
    mqttClient.publish((base_topic + "/override_text/state").c_str(), overrideMessage.c_str(), true);

    mqttClient.publish((base_topic + "/marquee/state").c_str(), marqueeOverrideMessage.c_str(), true);
    mqttClient.publish((base_topic + "/power/state").c_str(), isDisplayAsleep ? "OFF" : "ON", true);
    
    itoa(currentSettings.brightness, payload, 10);
    mqttClient.publish((base_topic + "/brightness/state").c_str(), payload, true);
    
    itoa(currentSettings.glitchEffectFrequency, payload, 10);
    mqttClient.publish((base_topic + "/glitch_freq/state").c_str(), payload, true);

    itoa(currentSettings.malfunctionFrequency, payload, 10);
    mqttClient.publish((base_topic + "/malfunction_chance/state").c_str(), payload, true);

    itoa(currentSettings.notificationVolume, payload, 10);
    mqttClient.publish((base_topic + "/volume/state").c_str(), payload, true);

    const char* styles[] = {"Sequential Flicker", "Random Flicker", "All Displays Random", "Counting Up", "Wave Flicker", "Tornado Flicker", "Capacitor Charge-Up", "Digital Rain", "Waveform Collapse", "Timeline Skim"};
    if (currentSettings.animationStyle >= 0 && currentSettings.animationStyle < 10) {
        mqttClient.publish((base_topic + "/animation_style/state").c_str(), styles[currentSettings.animationStyle], true);
    }
    
    itoa(WiFi.RSSI(), payload, 10);
    mqttClient.publish((base_topic + "/wifi_rssi/state").c_str(), payload, true);

    itoa(esp_get_free_heap_size(), payload, 10);
    mqttClient.publish((base_topic + "/free_heap/state").c_str(), payload, true);
    
    itoa(millis() / 1000, payload, 10);
    mqttClient.publish((base_topic + "/uptime/state").c_str(), payload, true);

    publishHaDiagnosticAttributes();

    char time_str[6];
    sprintf(time_str, "%02d:%02d", currentSettings.departureHour, currentSettings.departureMinute);
    mqttClient.publish((base_topic + "/sleep_time/state").c_str(), time_str, true);
    sprintf(time_str, "%02d:%02d", currentSettings.arrivalHour, currentSettings.arrivalMinute);
    mqttClient.publish((base_topic + "/wake_time/state").c_str(), time_str, true);

    for(int r=0; r<3; ++r) {
        for(int s=0; s<4; ++s) {
            const char* rows[] = {"dest", "pres", "last"};
            const char* segments[] = {"month", "day", "year", "time"};
            String topic = base_topic + "/" + rows[r] + "_" + segments[s] + "/state";
            mqttClient.publish(topic.c_str(), manualDisplayText[r][s].c_str(), true);
        }
    }
    
    mqttClient.publish((base_topic + "/sound_toggle/state").c_str(), currentSettings.timeTravelSoundToggle ? "ON" : "OFF", true);
    mqttClient.publish((base_topic + "/is_animating/state").c_str(), isAnimating ? "ON" : "OFF", true);
    mqttClient.publish((base_topic + "/is_glitching/state").c_str(), isGlitching ? "ON" : "OFF", true);
    mqttClient.publish((base_topic + "/is_malfunctioning/state").c_str(), isMalfunctioning ? "ON" : "OFF", true);
    mqttClient.publish((base_topic + "/is_asleep/state").c_str(), isDisplayAsleep ? "ON" : "OFF", true);
    itoa(currentPageIndex + 1, payload, 10);
    mqttClient.publish((base_topic + "/marquee_page/state").c_str(), payload, true);
    mqttClient.publish((base_topic + "/weather_mode/state").c_str(), currentSettings.weatherModeEnabled ? "ON" : "OFF", true);
    mqttClient.publish((base_topic + "/weather_city/state").c_str(), currentSettings.cityName.c_str(), true);

    mqttClient.publish((base_topic + "/24h_format/state").c_str(), currentSettings.displayFormat24h ? "ON" : "OFF", true);
    itoa(currentSettings.timeTravelAnimationInterval, payload, 10);
    mqttClient.publish((base_topic + "/animation_interval/state").c_str(), payload, true);
    itoa(currentSettings.timeTravelAnimationDuration, payload, 10);
    mqttClient.publish((base_topic + "/animation_duration/state").c_str(), payload, true);
    itoa(currentSettings.dataLinkRefreshInterval, payload, 10);
    mqttClient.publish((base_topic + "/datalink_refresh/state").c_str(), payload, true);
    
    mqttClient.publish((base_topic + "/temporal_echo/state").c_str(), isEchoEffectActive ? "ON" : "OFF", true);

    const char* sources[] = {"API", "MQTT", "Home Assistant Push"};
    for(int i=0; i<5; ++i) {
        int source_index = (int)currentSettings.dataPoints[i].dataSourceType;
        if (source_index >= 0 && source_index < 3) {
            String topic = base_topic + "/datapoint_" + String(i) + "_source/state";
            mqttClient.publish(topic.c_str(), sources[source_index], true);
        }
    }

    mqttClient.publish((base_topic + "/stock_ticker_mode/state").c_str(), currentSettings.stockTickerModeEnabled ? "ON" : "OFF", true);
    mqttClient.publish((base_topic + "/stock_row_1/state").c_str(), currentSettings.stockRow1_symbol.c_str(), true);
    mqttClient.publish((base_topic + "/stock_row_2/state").c_str(), currentSettings.stockRow2_symbol.c_str(), true);
    mqttClient.publish((base_topic + "/stock_row_3/state").c_str(), currentSettings.stockRow3_symbol.c_str(), true);
    mqttClient.publish((base_topic + "/alpha_vantage_api_key/state").c_str(), currentSettings.alphaVantageApiKey.c_str(), true);
    mqttClient.publish((base_topic + "/audio/state").c_str(), audio.isRunning() ? "PLAYING" : "IDLE", true);

    publishTimeSensors();
}

void updateHaStatus(const char* status) {
	if (!mqttClient.connected()) return;
	String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
	mqttClient.publish((base_topic + "/status/state").c_str(), status, true);
}

void publishTimeSensors() {
    if (!mqttClient.connected() || !timeSynchronized) return;
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    char iso_time[25];

    time_t now;
    time(&now);
    strftime(iso_time, sizeof(iso_time), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    mqttClient.publish((base_topic + "/present_time/state").c_str(), iso_time, true);

    struct tm dest_tm;
    localtime_r(&now, &dest_tm);
    dest_tm.tm_year = currentSettings.destinationYear - 1900;
    time_t dest_time = mktime(&dest_tm);
    strftime(iso_time, sizeof(iso_time), "%Y-%m-%dT%H:%M:%SZ", gmtime(&dest_time));
    mqttClient.publish((base_topic + "/destination_time/state").c_str(), iso_time, true);

    struct tm ltd_tm = {0};
    ltd_tm.tm_year = currentSettings.lastTimeDepartedYear - 1900;
    ltd_tm.tm_mon = currentSettings.lastTimeDepartedMonth - 1;
    ltd_tm.tm_mday = currentSettings.lastTimeDepartedDay;
    ltd_tm.tm_hour = currentSettings.lastTimeDepartedHour;
    ltd_tm.tm_min = currentSettings.lastTimeDepartedMinute;
    time_t ltd_time = mktime(&ltd_tm);
    strftime(iso_time, sizeof(iso_time), "%Y-%m-%dT%H:%M:%SZ", gmtime(&ltd_time));
    mqttClient.publish((base_topic + "/last_time_departed/state").c_str(), iso_time, true);
}

void startAudioStream(const char* url, bool is_tts) {
    Serial.printf("AUDIO_LOG: Request to start audio stream from URL: %s\n", url);
    if (!hardwareInitialized) {
        Serial.println("AUDIO_LOG: Hardware not initialized, cannot play audio.");
        return;
    }

    if (audio.isRunning()) {
        audio.stopSong();
        Serial.println("AUDIO_LOG: Stopped existing audio to play new stream.");
    }
    
    digitalWrite(I2S_SD_PIN, HIGH);
    audio.setVolume(currentSettings.notificationVolume); // Use full volume for streams
    strncpy(currentSoundFile, url, MAX_FILENAME_LENGTH - 1);
    currentSoundFile[MAX_FILENAME_LENGTH - 1] = '\0';
    
    if (audio.connecttohost(url)) {
        Serial.printf("AUDIO_LOG: Successfully connected to host for streaming: %s\n", url);
        if (mqttClient.connected()) {
            mqttClient.publish((String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/audio/state").c_str(), "PLAYING", true);
        }
    } else {
        Serial.printf("AUDIO_LOG: Failed to connect to host for streaming: %s\n", url);
        currentSoundFile[0] = '\0';
        digitalWrite(I2S_SD_PIN, LOW);
    }
}

void stopAudioStream() {
    Serial.println("AUDIO_LOG: Request to stop audio stream.");
    if (audio.isRunning()) {
        audio.stopSong();
        currentSoundFile[0] = '\0';
        digitalWrite(I2S_SD_PIN, LOW);
        Serial.println("AUDIO_LOG: Audio stream stopped successfully.");
    } else {
        Serial.println("AUDIO_LOG: No audio stream was running.");
    }
}