/**
 * @file MqttManager.cpp
 * @brief Manages all MQTT communication for Home Assistant integration.
 * @details This module handles the connection to an MQTT broker, publishes device
 * status and sensor data, and subscribes to command topics to allow for remote
 * control. It is responsible for generating the Home Assistant MQTT Discovery
 * configuration messages, which allow the device to be automatically recognized
 * by Home Assistant.
 */

#include "DebugLog.h"
#include "MqttManager.h"
#include "EventManager.h"
#include "AnimationManager.h"
#include "DisplayManager.h"
#include "DataManager.h"
#include "web_server.h"
#include "StockManager.h"

extern StockManager stockManager;
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Preferences.h>
#include <LCBUrl.h> 

bool haDiscoveryPublished = false;

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
    JsonDocument doc;
    String topic;
    String payload;

    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"] = MQTT_UNIQUE_ID;

    JsonArray availability = doc["availability"].to<JsonArray>();
    JsonObject availability_topic = availability.add<JsonObject>();
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

    JsonArray options = doc["options"].to<JsonArray>();
    options.add("Einstein's Test (1985)");
    options.add("Marty's First Jump (1985)");
    options.add("Arrival in Past (1955)");
    options.add("Lightning Strike (1955)");

    Preferences prefs;
    prefs.begin(PREFERENCES_NAMESPACE, true);
    String presetsJson = prefs.getString("customPresets", "[]");
    prefs.end();
    JsonDocument presetsDoc;
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
    JsonDocument doc;
    String topic;
    String payload;

    JsonObject device = doc["device"].to<JsonObject>();
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

    doc["type"] = "preset_changed";
    topic = String(MQTT_BASE_TOPIC) + "/device_automation/" + MQTT_UNIQUE_ID + "/preset_changed/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

void publishHaDiagnosticAttributes() {
    if (!mqttClient.connected()) return;
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    JsonDocument doc;

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


/**
 * @brief Publishes the Home Assistant MQTT Discovery configuration messages.
 * @details This function constructs and sends a series of JSON messages to specific
 * MQTT topics. These messages describe the device and its capabilities (sensors,
 * switches, numbers, etc.) to Home Assistant, allowing it to automatically create
 * corresponding entities in the UI. This function is typically called only once
 * upon the first successful connection to the MQTT broker.
 */
void publishHaAutoDiscovery() {
    String device_base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;

    // --- Create a reusable "device" JSON object ---
    JsonDocument device_doc;
    JsonObject device = device_doc.to<JsonObject>();
    device["identifiers"] = MQTT_UNIQUE_ID;
    device["name"] = "Time Circuits Display";
    device["model"] = "BTTF Clock v1";
    device["manufacturer"] = "Doc Brown Industries";
    device["sw_version"] = "2.0";

    JsonDocument availability_doc;
    JsonArray availability = availability_doc.to<JsonArray>();
    JsonObject availability_topic = availability.add<JsonObject>();
    availability_topic["topic"] = device_base_topic + "/status";
    availability_topic["payload_available"] = "online";
    availability_topic["payload_not_available"] = "offline";

    JsonDocument doc;
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

    // --- NEW: Create 12 text entities for direct display control ---
    const char* rows[] = {"dest", "pres", "last"};
    const char* row_names[] = {"Destination", "Present", "Last Departed"};
    const char* segments[] = {"month", "day", "year", "time"};
    const char* segment_names[] = {"Month", "Day", "Year", "Time"};

    for (int r = 0; r < 3; ++r) {
        for (int s = 0; s < 4; ++s) {
            doc.clear();
            String name = String(row_names[r]) + " " + String(segment_names[s]);
            String id_suffix = String(rows[r]) + "_" + String(segments[s]);
            doc["name"] = name;
            doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
            doc["object_id"] = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
            doc["command_topic"] = device_base_topic + "/" + id_suffix + "/command";
            doc["state_topic"] = device_base_topic + "/" + id_suffix + "/state";
            doc["icon"] = "mdi:form-textbox";
            doc["device"] = device;
            doc["availability"] = availability;
            topic = String(MQTT_BASE_TOPIC) + "/text/" + doc["object_id"].as<String>() + "/config";
            serializeJson(doc, payload);
            mqttClient.publish(topic.c_str(), payload.c_str(), true);
        }
    }

    // --- Cleanup obsolete entities ---
    clearHaEntity("sensor", "destination_time");
    clearHaEntity("sensor", "present_time");
    clearHaEntity("sensor", "last_time_departed");
    clearHaEntity("number", "destination_year");
    for (int i=0; i < 5; ++i) {
        String unique_id_suffix = "datapoint_" + String(i) + "_source";
        clearHaEntity("select", unique_id_suffix.c_str());
    }
    clearHaEntity("switch", "stock_ticker_mode");
    clearHaEntity("button", "stock_next");
    clearHaEntity("button", "stock_previous");

    // ADDED: Create Enabled switches for each data point
    for (int i=0; i < 5; ++i) {
        doc.clear();
        doc["name"] = "Data Point " + String(i + 1) + " Enabled";
        String id_suffix = "datapoint_" + String(i) + "_enabled";
        doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        doc["object_id"] = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        doc["command_topic"] = device_base_topic + "/" + id_suffix + "/command";
        doc["state_topic"] = device_base_topic + "/" + id_suffix + "/state";
        doc["icon"] = "mdi:toggle-switch";
        doc["entity_category"] = "config";
        doc["device"] = device;
        doc["availability"] = availability;
        topic = String(MQTT_BASE_TOPIC) + "/switch/" + doc["object_id"].as<String>() + "/config";
        serializeJson(doc, payload);
        mqttClient.publish(topic.c_str(), payload.c_str(), true);
    }

    // ADDED: Create Marquee text inputs for each data point
    for (int i=0; i < 5; ++i) {
        doc.clear();
        doc["name"] = "Data Point " + String(i + 1) + " Marquee";
        String id_suffix = "datapoint_" + String(i) + "_marquee";
        doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        doc["object_id"] = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        doc["command_topic"] = device_base_topic + "/" + id_suffix + "/command";
        doc["state_topic"] = device_base_topic + "/" + id_suffix + "/state";
        doc["icon"] = "mdi:text-box-outline";
        doc["entity_category"] = "config";
        doc["device"] = device;
        doc["availability"] = availability;
        topic = String(MQTT_BASE_TOPIC) + "/text/" + doc["object_id"].as<String>() + "/config";
        serializeJson(doc, payload);
        mqttClient.publish(topic.c_str(), payload.c_str(), true);
    }

    // This sensor has been replaced by the more specific `..._marquee` text entity and `..._enabled` switch.
    // We will now clear any old entities that may exist from previous versions.
    for (int i=0; i < 5; ++i) {
        String unique_id_suffix = "datapoint_" + String(i);
        clearHaEntity("sensor", unique_id_suffix.c_str());
    }


    const char* number_configs[][5] = {
        {"animation_interval", "Animation Interval", "mdi:clock-in", "min", "0,120,1"},
        {"animation_duration", "Animation Duration", "mdi:movie-filter", "ms", "1000,10000,100"},
        {"stock_refresh", "Stock Refresh", "mdi:chart-line", "min", "1,60,1"}
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
    
    const char* button_configs[][3] = {
        {"trigger_animation", "Trigger Animation", "mdi:movie-play"},
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
        doc["payload_press"] = "PRESS";
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
    JsonArray profiles = doc["options"].to<JsonArray>();
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
    

    // --- Notification & Alert Entities ---
    doc.clear();
    doc["name"] = "Override Switch";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_override_switch";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_override_switch";
    doc["command_topic"] = device_base_topic + "/override/command";
    doc["state_topic"] = device_base_topic + "/override/state";
    doc["icon"] = "mdi:message-cog";
    doc["entity_category"] = "config";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/switch/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    doc.clear();
    doc["name"] = "Override Message";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_override_message";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_override_message";
    doc["command_topic"] = device_base_topic + "/override_message/command";
    doc["state_topic"] = device_base_topic + "/override_message/state";
    doc["icon"] = "mdi:message-draw";
    doc["entity_category"] = "config";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/text/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    doc.clear();
    doc["name"] = "Play Sound";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_play_sound";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_play_sound";
    doc["command_topic"] = device_base_topic + "/play_sound/command";
    doc["state_topic"] = device_base_topic + "/play_sound/state";
    JsonArray sounds = doc["options"].to<JsonArray>();
    sounds.add("None");
    sounds.add("ALARM_SOUND");
    sounds.add("ARRIVAL_THUD");
    sounds.add("CONFIRM_ON");
    sounds.add("EASTER_EGG");
    sounds.add("REBOOT_SOUND");
    sounds.add("REMINDER_ALERT");
    sounds.add("TIME_TRAVEL_FAIL");
    doc["icon"] = "mdi:volume-high";
    doc["entity_category"] = "config";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/select/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);


    // --- Live Weather Mode Entities ---
    doc.clear();
    doc["name"] = "Live Weather Mode";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_weather_mode";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_weather_mode";
    doc["command_topic"] = device_base_topic + "/weather_mode/command";
    doc["state_topic"] = device_base_topic + "/weather_mode/state";
    doc["icon"] = "mdi:weather-cloudy";
    doc["entity_category"] = "config";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/switch/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    doc.clear();
    doc["name"] = "Weather City";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_weather_city";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_weather_city";
    doc["command_topic"] = device_base_topic + "/weather_city/command";
    doc["state_topic"] = device_base_topic + "/weather_city/state";
    doc["icon"] = "mdi:city";
    doc["entity_category"] = "config";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/text/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    doc.clear();
    doc["name"] = "Refresh Weather Data";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_weather_refresh";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_weather_refresh";
    doc["command_topic"] = device_base_topic + "/weather_refresh/command";
    doc["payload_press"] = "PRESS";
    doc["icon"] = "mdi:refresh";
    doc["entity_category"] = "config";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/button/" + doc["object_id"].as<String>() + "/config";
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
  
  Log_printf(LOG_LEVEL_INFO, "Attempting to connect to MQTT broker: %s...", currentSettings.mqttBroker.c_str());
  delay(100); 

  String clientId = "TimeCircuits-";
  clientId += String(random(0xffff), HEX);
  String availability_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/status";

  bool connectResult = false;
  if (!currentSettings.mqttUser.empty()) {
      Log_printf(LOG_LEVEL_DEBUG, "Connecting with Client ID: %s and username: %s", clientId.c_str(), currentSettings.mqttUser.c_str());
      connectResult = mqttClient.connect(clientId.c_str(), currentSettings.mqttUser.c_str(), currentSettings.mqttPassword.c_str(), availability_topic.c_str(), 1, true, "offline");
  } else {
      Log_printf(LOG_LEVEL_DEBUG, "Connecting with Client ID: %s (no username)", clientId.c_str());
      connectResult = mqttClient.connect(clientId.c_str(), availability_topic.c_str(), 1, true, "offline");
  }

  if (connectResult) {
    Log_printf(LOG_LEVEL_INFO, "SUCCESS! MQTT client connected.");
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
    Log_printf(LOG_LEVEL_DEBUG, "Subscribed to wildcard command topic: %s", command_topic.c_str());
    
    String audio_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/tts/play";
    mqttClient.subscribe(audio_topic.c_str());
    audio_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/radio/command";
    mqttClient.subscribe(audio_topic.c_str());


    for (int i = 0; i < currentSettings.numDataPoints; i++) {
      if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_MQTT && !currentSettings.dataPoints[i].mqttTopic.empty()) {
        mqttClient.subscribe(currentSettings.dataPoints[i].mqttTopic.c_str());
      }
      // The complex, four-part subscription for HA Push has been removed.
      // All control is now handled via the wildcard command_topic subscription
      // and the `text.time_circuits_display_datapoint_X_marquee` entities.
    }
  } else {
    const char* error_str = "Unknown";
    switch (mqttClient.state()) {
      case -4: error_str = "Connection timeout."; break;
      case -3: error_str = "Connection lost."; break;
      case -2: error_str = "Connect failed."; break;
      case -1: error_str = "Disconnected."; break;
      case 1:  error_str = "Bad protocol version."; break;
      case 2:  error_str = "Client ID rejected."; break;
      case 3:  error_str = "Server unavailable."; break;
      case 4:  error_str = "Bad username or password."; break;
      case 5:  error_str = "Not authorized."; break;
    }
    Log_printf(LOG_LEVEL_ERROR, "MQTT connection FAILED! rc=%d (%s)", mqttClient.state(), error_str);
    delay(100); 
  }
}

/**
 * @brief The callback function that processes all incoming MQTT messages.
 * @details This function is registered with the PubSubClient library and is called
 * whenever a message is received on a subscribed topic. It parses the topic to
 * determine which command is being issued, decodes the payload, and then updates
 * the appropriate setting or triggers the corresponding action.
 * @param topic The MQTT topic the message was received on.
 * @param payload A pointer to the message payload.
 * @param length The length of the payload.
 */
void mqttCallback(char* topic, unsigned char* payload, unsigned int length) {
    // Dynamically allocate a buffer for the message to avoid stack overflow
    // and accommodate larger payloads (e.g., for long marquee text).
    std::string message(reinterpret_cast<char*>(payload), length);

    String topicStr = String(topic);
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/";
    bool stateChanged = false;
    bool settingsChanged = false;
    String component;

    if (topicStr.endsWith("/command")) {
        String component_topic = topicStr.substring(base_topic.length());
        component = component_topic.substring(0, component_topic.indexOf('/'));

        if (component == "power") {
            isDisplayAsleep = (message == "OFF");
            stateChanged = true;
        } else if (component == "brightness") {
            int brightness = std::stoi(message);
            if (brightness >= 0 && brightness <= 7) {
                currentSettings.brightness = brightness;
                settingsChanged = true;
            }
        } else if (component.startsWith("datapoint_")) {
            String id_suffix = component.substring(10);
            int dp_index = id_suffix.substring(0, id_suffix.indexOf('_')).toInt();
            if (dp_index >= 0 && dp_index < 5) {
                if (id_suffix.endsWith("_marquee")) {
                    currentSettings.dataPoints[dp_index].scrollingText = message.c_str();
                    settingsChanged = true;
                } else if (id_suffix.endsWith("_enabled")) {
                    currentSettings.dataPoints[dp_index].enabled = (message == "ON");
                    settingsChanged = true;
                }
            }
        } else if (component == "animation_style") {
            currentSettings.animationStyle = std::stoi(message);
            settingsChanged = true;
        } else if (component == "volume") {
            int vol = std::stoi(message);
            if (vol >= 0 && vol <= 21) {
                currentSettings.notificationVolume = vol;
                audio.setVolume(vol);
                settingsChanged = true;
            }
        } else if (component == "override") {
            isMessageOverrideActive = (message == "ON");
            stateChanged = true;
        } else if (component == "override_message") {
            size_t first_newline = message.find('\n');
            size_t second_newline = message.find('\n', first_newline + 1);
            if (first_newline != std::string::npos) {
                overrideMessageLine1 = message.substr(0, first_newline).c_str();
                if (second_newline != std::string::npos) {
                    overrideMessageLine2 = message.substr(first_newline + 1, second_newline - (first_newline + 1)).c_str();
                    overrideMessageLine3 = message.substr(second_newline + 1).c_str();
                } else {
                    overrideMessageLine2 = message.substr(first_newline + 1).c_str();
                    overrideMessageLine3 = "";
                }
            } else {
                overrideMessageLine1 = message.c_str();
                overrideMessageLine2 = "";
                overrideMessageLine3 = "";
            }
            stateChanged = true;
        } else if (component == "trigger_animation" && message == "PRESS") {
            startTimeTravelAnimation();
        } else if (component.startsWith("dest_") || component.startsWith("pres_") || component.startsWith("last_")) {
            int row = -1, segment = -1;
            if (component.startsWith("dest_")) row = 0;
            else if (component.startsWith("pres_")) row = 1;
            else if (component.startsWith("last_")) row = 2;

            if (component.endsWith("_month")) segment = 0;
            else if (component.endsWith("_day")) segment = 1;
            else if (component.endsWith("_year")) segment = 2;
            else if (component.endsWith("_time")) segment = 3;
            
            if (row != -1 && segment != -1) {
                updateDisplaySegment(row, segment, message.c_str());
                stateChanged = true;
            }
        } else if (component == "trigger_effect") {
            if (message == "Run Boot Sequence") runBootSequence();
            mqttClient.publish((base_topic + "trigger_effect/state").c_str(), "None", true);
        } else if (component == "flash_command") {
            int row = -1, segment = -1;
            if (message.starts_with("dest_")) row = 0;
            else if (message.starts_with("pres_")) row = 1;
            else if (message.starts_with("last_")) row = 2;
            if (message.ends_with("_month")) segment = 0;
            else if (message.ends_with("_day")) segment = 1;
            else if (message.ends_with("_year")) segment = 2;
            else if (message.ends_with("_time")) segment = 3;
            if (row != -1 && segment != -1) {
                triggerFlashEffect(row, segment);
            }
        } else if (component == "sleep_time" || component == "wake_time") {
            size_t colonPos = message.find(':');
            if (colonPos != std::string::npos) {
                int hour = std::stoi(message.substr(0, colonPos));
                int minute = std::stoi(message.substr(colonPos + 1));
                if (component == "sleep_time") {
                    currentSettings.departureHour = hour;
                    currentSettings.departureMinute = minute;
                } else {
                    currentSettings.arrivalHour = hour;
                    currentSettings.arrivalMinute = minute;
                }
                settingsChanged = true;
            }
        } else if (component == "preset_selector") {
            mqttClient.publish((base_topic + "preset_selector/state").c_str(), message.c_str(), true);
        } else if (component == "play_sound") {
            if (message != "None" && hardwareInitialized) {
                playSound(("/" + message + ".mp3").c_str());
            }
            mqttClient.publish((base_topic + "play_sound/state").c_str(), "None", true);
        } else if (component == "sound_toggle") {
            currentSettings.timeTravelSoundToggle = (message == "ON");
            settingsChanged = true;
        } else if (component == "weather_mode") {
            currentSettings.weatherModeEnabled = (message == "ON");
            if (currentSettings.weatherModeEnabled) currentSettings.dataLinkEnabled = false;
            settingsChanged = true;
        } else if (component == "weather_city") {
            if (currentSettings.cityName != message.c_str()) {
                currentSettings.cityName = message.c_str();
                settingsChanged = true;
            }
        } else if (component == "weather_refresh" && message == "PRESS") {
            if (!isFetchingWeather && currentSettings.latitude != 0.0f) {
                isFetchingWeather = true;
                xTaskCreate(fetchWeatherDataTask, "fetchWeatherDataTask", 8192, NULL, 1, NULL);
            }
        } else if (component == "24h_format") {
            currentSettings.displayFormat24h = (message == "ON");
            settingsChanged = true;
        } else if (component == "animation_interval") {
            currentSettings.timeTravelAnimationInterval = std::stoi(message);
            settingsChanged = true;
        } else if (component == "animation_duration") {
            currentSettings.timeTravelAnimationDuration = std::stoi(message);
            settingsChanged = true;
        } else if (component == "stock_refresh") {
            currentSettings.stockRefreshInterval = std::stoi(message);
            settingsChanged = true;
        } else if (component == "reboot_device" && message == "PRESS") {
            ESP.restart();
        } else if (component == "force_ntp_sync" && message == "PRESS") {
            ntpSyncRequested = true;
        } else if (component == "factory_reset" && message == "PRESS") {
            preferences.begin(PREFERENCES_NAMESPACE, false);
            preferences.clear();
            preferences.end();
            ESP.restart();
        } else if (component == "save_all_settings" && message == "PRESS") {
            saveSettings();
        } else if (component == "temporal_echo") {
            isEchoEffectActive = (message == "ON");
            if (isEchoEffectActive) echoEffectStartTime = millis();
            stateChanged = true;
        } else if (component == "profile") {
            if (message == "Standard") {
                currentSettings.brightness = 5;
                currentSettings.notificationVolume = 15;
                currentSettings.timeTravelSoundToggle = true;
            } else if (message == "Cinematic") {
                currentSettings.animationStyle = ANIMATION_TIMELINE_SKIM;
                currentSettings.timeTravelAnimationDuration = 8000;
            } else if (message == "Silent Night") {
                currentSettings.brightness = 1;
                currentSettings.notificationVolume = 0;
                currentSettings.timeTravelSoundToggle = false;
            }
            mqttClient.publish((base_topic + "profile/state").c_str(), message.c_str(), true);
            settingsChanged = true;
        }
    } else if (topicStr == base_topic + "tts/play") {
        JsonDocument doc;
        if (deserializeJson(doc, message) == DeserializationError::Ok) {
            startAudioStream(doc["url"], true, doc["volume"] | -1);
        } else {
            startAudioStream(message.c_str(), true);
        }
    } else if (topicStr == base_topic + "radio/command") {
        if (message == "stop") stopAudioStream();
        else startAudioStream(message.c_str(), false);
    } else {
        for (int i = 0; i < currentSettings.numDataPoints; i++) {
            if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_MQTT && topicStr == currentSettings.dataPoints[i].mqttTopic.c_str()) {
                if (xSemaphoreTake(xDisplayDataMutex, portMAX_DELAY) == pdTRUE) {
                    displayPages[i].year = message.c_str();
                    displayPages[i].month = "";
                    displayPages[i].day = "";
                    displayPages[i].time = "";
                    isMarqueeBufferDirty = true;
                    xSemaphoreGive(xDisplayDataMutex);
                }
                break;
            }
        }
    }

    if (settingsChanged) {
        saveSettings();
        if (component != "profile") {
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

    mqttClient.publish((base_topic + "/override/state").c_str(), isMessageOverrideActive ? "ON" : "OFF", true);
    
    String overrideMessage = overrideMessageLine1;
    if (overrideMessageLine2.length() > 0) overrideMessage += "\n" + overrideMessageLine2;
    if (overrideMessageLine3.length() > 0) overrideMessage += "\n" + overrideMessageLine3;
    mqttClient.publish((base_topic + "/override_message/state").c_str(), overrideMessage.c_str(), true);

    mqttClient.publish((base_topic + "/power/state").c_str(), isDisplayAsleep ? "OFF" : "ON", true);
    
    itoa(currentSettings.brightness, payload, 10);
    mqttClient.publish((base_topic + "/brightness/state").c_str(), payload, true);
    
    itoa(currentSettings.notificationVolume, payload, 10);
    mqttClient.publish((base_topic + "/volume/state").c_str(), payload, true);

    const char* styles[] = {"Sequential Flicker", "Random Flicker", "All Displays Random", "Counting Up", "Wave Flicker", "Tornado Flicker", "Capacitor Charge-Up", "Digital Rain", "Waveform Collapse", "Timeline Skim"};
    if (currentSettings.animationStyle >= 0 && currentSettings.animationStyle < 10) {
        mqttClient.publish((base_topic + "/animation_style/state").c_str(), styles[currentSettings.animationStyle], true);
    }
    
    publishHaDiagnosticAttributes();

    char time_str[6];
    sprintf(time_str, "%02d:%02d", currentSettings.departureHour, currentSettings.departureMinute);
    mqttClient.publish((base_topic + "/sleep_time/state").c_str(), time_str, true);
    sprintf(time_str, "%02d:%02d", currentSettings.arrivalHour, currentSettings.arrivalMinute);
    mqttClient.publish((base_topic + "/wake_time/state").c_str(), time_str, true);

    // Publish the state of the 12 text entities
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
    itoa(currentSettings.stockRefreshInterval, payload, 10);
    mqttClient.publish((base_topic + "/stock_refresh/state").c_str(), payload, true);
    
    mqttClient.publish((base_topic + "/temporal_echo/state").c_str(), isEchoEffectActive ? "ON" : "OFF", true);

    // This state publishing has been removed as the sensor it belongs to was removed.

    mqttClient.publish((base_topic + "/audio/state").c_str(), audio.isRunning() ? "PLAYING" : "IDLE", true);

    for(int i=0; i<5; ++i) {
        String enabled_topic = base_topic + "/datapoint_" + String(i) + "_enabled/state";
        mqttClient.publish(enabled_topic.c_str(), currentSettings.dataPoints[i].enabled ? "ON" : "OFF", true);
        String marquee_topic = base_topic + "/datapoint_" + String(i) + "_marquee/state";
        mqttClient.publish(marquee_topic.c_str(), currentSettings.dataPoints[i].scrollingText.c_str(), true);
    }
}

void updateHaStatus(const char* status) {
	if (!mqttClient.connected()) return;
	String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
	mqttClient.publish((base_topic + "/status/state").c_str(), status, true);
}

void startAudioStream(const char* url, bool is_tts, int volume) {
    Log_printf(LOG_LEVEL_INFO, "Request to start audio stream from URL: %s", url);
    if (!hardwareInitialized) {
        Log_printf(LOG_LEVEL_WARN, "Hardware not initialized, cannot play audio.");
        return;
    }

    if (audio.isRunning()) {
        audio.stopSong();
        Log_printf(LOG_LEVEL_DEBUG, "Stopped existing audio to play new stream.");
    }
    
    digitalWrite(I2S_SD_PIN, HIGH);
    
    if (volume >= 0 && volume <= 100) {
        // Map 0-100 volume from HA to the device's 0-21 scale
        int device_volume = round(volume / 100.0 * 21.0);
        audio.setVolume(device_volume);
        Log_printf(LOG_LEVEL_DEBUG, "Set dynamic volume to %d (%d/100)", device_volume, volume);
    } else {
        audio.setVolume(currentSettings.notificationVolume); // Use default volume
    }

    strncpy(currentSoundFile, url, MAX_FILENAME_LENGTH - 1);
    currentSoundFile[MAX_FILENAME_LENGTH - 1] = '\0';
    
    if (audio.connecttohost(url)) {
        Log_printf(LOG_LEVEL_INFO, "Successfully connected to host for streaming: %s", url);
        if (mqttClient.connected()) {
            mqttClient.publish((String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/audio/state").c_str(), "PLAYING", true);
        }
    } else {
        Log_printf(LOG_LEVEL_ERROR, "Failed to connect to host for streaming: %s", url);
        currentSoundFile[0] = '\0';
        digitalWrite(I2S_SD_PIN, LOW);
    }
}

void stopAudioStream() {
    Log_printf(LOG_LEVEL_INFO, "Request to stop audio stream.");
    if (audio.isRunning()) {
        audio.stopSong();
        currentSoundFile[0] = '\0';
        digitalWrite(I2S_SD_PIN, LOW);
        Log_printf(LOG_LEVEL_INFO, "Audio stream stopped successfully.");
    } else {
        Log_printf(LOG_LEVEL_DEBUG, "No audio stream was running.");
    }
}

void setupMqtt() {
  if (currentSettings.mqttBroker.empty()) {
    Log_printf(LOG_LEVEL_INFO, "No broker configured. MQTT setup skipped.");
    return;
  }
  mqttClient.setServer(currentSettings.mqttBroker.c_str(), currentSettings.mqttPort);
  mqttClient.setCallback(mqttCallback);
  Log_printf(LOG_LEVEL_INFO, "Client configured for broker [%s] on port [%d]", currentSettings.mqttBroker.c_str(), currentSettings.mqttPort);
}