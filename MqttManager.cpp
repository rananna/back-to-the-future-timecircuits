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

void clearHaEntity(const char* component, const char* unique_id_suffix) {
    String object_id = String(MQTT_UNIQUE_ID) + "_" + unique_id_suffix;
    String topic = String(MQTT_BASE_TOPIC) + "/" + component + "/" + object_id + "/config";
    if (mqttClient.connected()) {
        mqttClient.publish(topic.c_str(), "", true);
    }
}

/**
 * @brief Centralized helper to publish a Home Assistant discovery message.
 * @details This function constructs the topic, serializes the JSON payload,
 * and publishes the configuration message to the appropriate MQTT discovery topic.
 * It now includes enhanced logging to help debug discovery issues.
 * @param doc The JsonDocument containing the entity's configuration.
 * @param component The Home Assistant component type (e.g., "sensor", "switch").
 */
void publishDiscoveryMessage(JsonDocument& doc, const char* component) {
    String payload;
    String object_id = doc["object_id"].as<String>();
    String topic = String(MQTT_BASE_TOPIC) + "/" + component + "/" + object_id + "/config";

    // Serialize the JSON document to a string
    serializeJson(doc, payload);

    // Log the discovery message details for debugging and verification
    Log_printf(LOG_LEVEL_INFO, "HA Discovery: Publishing to topic [%s]", topic.c_str());
    Log_printf(LOG_LEVEL_INFO, "HA Discovery: Payload: %s", payload.c_str());

    if (mqttClient.connected()) {
        // This is a blocking loop that will continue until the message is successfully published.
        // It checks mqttClient.connected() in each iteration to prevent an infinite loop if the connection is lost.
        while (mqttClient.connected() && !mqttClient.publish(topic.c_str(), payload.c_str(), true)) {
            Log_printf(LOG_LEVEL_WARN, "HA Discovery: Publish buffer for %s is full. Retrying in 100ms...", object_id.c_str());
            // Wait and allow the MQTT client to process its outgoing messages
            delay(100);
            mqttClient.loop();
        }
        // After a successful publish, give the client time to send the message from the buffer.
        delay(75);
        mqttClient.loop();
    } else {
        Log_printf(LOG_LEVEL_WARN, "HA Discovery: Cannot publish, MQTT client not connected.");
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

    JsonObject availability = doc["availability"].to<JsonObject>();
    availability["topic"] = device_base_topic + "/status";
    availability["payload_available"] = "online";
    availability["payload_not_available"] = "offline";

    doc["name"] = "Last Departed Preset";
    String preset_selector_id = String(MQTT_UNIQUE_ID) + "_preset_selector";
    doc["unique_id"] = preset_selector_id;
    doc["object_id"] = preset_selector_id;
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

    publishDiscoveryMessage(doc, "select");
}


void publishDeviceTriggers() {
    String device_base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    JsonDocument doc;

    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"] = MQTT_UNIQUE_ID;

    doc["automation_type"] = "trigger";
    doc["topic"] = device_base_topic + "/events";

    // Define triggers in a loop to reduce repetition
    const char* trigger_types[] = {"animation_started", "animation_completed", "sleep_mode_entered", "sleep_mode_exited", "preset_changed"};
    const char* trigger_subtypes[] = {"anim_started", "anim_completed", "sleep_entered", "sleep_exited", "preset_changed"};

    for(int i = 0; i < sizeof(trigger_types)/sizeof(trigger_types[0]); ++i) {
        doc["type"] = trigger_types[i];
        String object_id = String(MQTT_UNIQUE_ID) + "_" + trigger_subtypes[i];
        doc["object_id"] = object_id;
        doc["unique_id"] = object_id;
        // No need to set a "name" for device triggers, HA uses "type" and "subtype"
        publishDiscoveryMessage(doc, "device_automation");
    }
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
    device["name"] = "Time Circuits";
    device["model"] = "BTTF Clock v1";
    device["manufacturer"] = "Doc Brown Industries";
    device["sw_version"] = "2.0";

    JsonDocument availability_doc;
    JsonObject availability = availability_doc.to<JsonObject>();
    availability["topic"] = device_base_topic + "/status";
    availability["payload_available"] = "online";
    availability["payload_not_available"] = "offline";

    JsonDocument doc;
    
    doc.clear();
    doc["name"] = "Status";
    String status_id = String(MQTT_UNIQUE_ID) + "_status";
    doc["unique_id"] = status_id;
    doc["object_id"] = status_id;
    doc["state_topic"] = device_base_topic + "/status/state";
    doc["json_attributes_topic"] = device_base_topic + "/status/attributes";
    doc["icon"] = "mdi:clock-outline";
    doc["device"].set(device);
    doc["availability"].set(availability);
    publishDiscoveryMessage(doc, "sensor");

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
            String entity_id = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
            doc["name"] = name;
            doc["unique_id"] = entity_id;
            doc["object_id"] = entity_id;
            doc["command_topic"] = device_base_topic + "/" + id_suffix + "/command";
            doc["state_topic"] = device_base_topic + "/" + id_suffix + "/state";
            doc["icon"] = "mdi:form-textbox";
            doc["device"].set(device);
            doc["availability"].set(availability);
            publishDiscoveryMessage(doc, "text");
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
    clearHaEntity("switch", "live_weather_mode");
    clearHaEntity("button", "stock_next");
    clearHaEntity("button", "stock_previous");

    // ADDED: Create Enabled switches for each data point
    for (int i=0; i < 5; ++i) {
        doc.clear();
        doc["name"] = "Data Point " + String(i + 1) + " Enabled";
        String id_suffix = "datapoint_" + String(i) + "_enabled";
        String entity_id = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        doc["unique_id"] = entity_id;
        doc["object_id"] = entity_id;
        doc["command_topic"] = device_base_topic + "/" + id_suffix + "/command";
        doc["state_topic"] = device_base_topic + "/" + id_suffix + "/state";
        doc["icon"] = "mdi:toggle-switch";
        doc["entity_category"] = "config";
        doc["device"].set(device);
        doc["availability"].set(availability);
        publishDiscoveryMessage(doc, "switch");
    }

    // ADDED: Create Marquee text inputs for each data point
    for (int i=0; i < 5; ++i) {
        doc.clear();
        doc["name"] = "Data Point " + String(i + 1) + " Marquee";
        String id_suffix = "datapoint_" + String(i) + "_marquee";
        String entity_id = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        doc["unique_id"] = entity_id;
        doc["object_id"] = entity_id;
        doc["command_topic"] = device_base_topic + "/" + id_suffix + "/command";
        doc["state_topic"] = device_base_topic + "/" + id_suffix + "/state";
        doc["icon"] = "mdi:text-box-outline";
        doc["entity_category"] = "config";
        doc["device"].set(device);
        doc["availability"].set(availability);
        publishDiscoveryMessage(doc, "text");
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
        String entity_id = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        doc["unique_id"] = entity_id;
        doc["object_id"] = entity_id;
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
        doc["device"].set(device);
        doc["availability"].set(availability);
        publishDiscoveryMessage(doc, "number");
    }

     const char* switch_configs[][3] = {
        {"24h_format", "24-Hour Format", "mdi:clock-time-twelve-outline"}
    };
    for (auto const& cfg : switch_configs) {
        doc.clear();
        doc["name"] = cfg[1];
        String id_suffix = cfg[0];
        String entity_id = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        doc["unique_id"] = entity_id;
        doc["object_id"] = entity_id;
        doc["command_topic"] = device_base_topic + "/" + id_suffix + "/command";
        doc["state_topic"] = device_base_topic + "/" + id_suffix + "/state";
        doc["icon"] = cfg[2];
        doc["entity_category"] = "config";
        doc["device"].set(device);
        doc["availability"].set(availability);
        publishDiscoveryMessage(doc, "switch");
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
        String entity_id = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        doc["unique_id"] = entity_id;
        doc["object_id"] = entity_id;
        doc["command_topic"] = device_base_topic + "/" + id_suffix + "/command";
        doc["payload_press"] = "PRESS";
        doc["icon"] = cfg[2];
        doc["entity_category"] = "config";
        doc["device"].set(device);
        doc["availability"].set(availability);
        publishDiscoveryMessage(doc, "button");
    }

    // --- Sequencer Button ---
    doc.clear();
    doc["name"] = "Trigger Sequence";
    String sequencer_id = String(MQTT_UNIQUE_ID) + "_sequencer";
    doc["unique_id"] = sequencer_id;
    doc["object_id"] = sequencer_id;
    doc["command_topic"] = device_base_topic + "/sequencer/command";
    doc["payload_press"] = "PRESS";
    doc["icon"] = "mdi:movie-play-outline";
    doc["entity_category"] = "config";
    doc["device"].set(device);
    doc["availability"].set(availability);
    publishDiscoveryMessage(doc, "button");
    
    doc.clear();
    doc["name"] = "Temporal Echo Effect";
    String temporal_echo_id = String(MQTT_UNIQUE_ID) + "_temporal_echo";
    doc["unique_id"] = temporal_echo_id;
    doc["object_id"] = temporal_echo_id;
    doc["command_topic"] = device_base_topic + "/temporal_echo/command";
    doc["state_topic"] = device_base_topic + "/temporal_echo/state";
    doc["icon"] = "mdi:ghost";
    doc["entity_category"] = "config";
    doc["device"].set(device);
    doc["availability"].set(availability);
    publishDiscoveryMessage(doc, "switch");

    doc.clear();
    doc["name"] = "Profile";
    String profile_id = String(MQTT_UNIQUE_ID) + "_profile";
    doc["unique_id"] = profile_id;
    doc["object_id"] = profile_id;
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
    doc["device"].set(device);
    doc["availability"].set(availability);
    publishDiscoveryMessage(doc, "select");
    

    // --- Notification & Alert Entities ---
    doc.clear();
    doc["name"] = "Override Switch";
    String override_switch_id = String(MQTT_UNIQUE_ID) + "_override_switch";
    doc["unique_id"] = override_switch_id;
    doc["object_id"] = override_switch_id;
    doc["command_topic"] = device_base_topic + "/override/command";
    doc["state_topic"] = device_base_topic + "/override/state";
    doc["icon"] = "mdi:message-cog";
    doc["entity_category"] = "config";
    doc["device"].set(device);
    doc["availability"].set(availability);
    publishDiscoveryMessage(doc, "switch");

    // --- Remove the old single override message entity ---
    clearHaEntity("text", "override_message");

    // --- Create three separate text entities for each override line ---
    for (int i = 1; i <= 3; i++) {
        doc.clear();
        doc["name"] = "Override Message Line " + String(i);
        String id_suffix = "override_line_" + String(i);
        String entity_id = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        doc["unique_id"] = entity_id;
        doc["object_id"] = entity_id;
        doc["command_topic"] = device_base_topic + "/" + id_suffix + "/command";
        doc["state_topic"] = device_base_topic + "/" + id_suffix + "/state";
        doc["icon"] = "mdi:message-draw";
        doc["entity_category"] = "config";
        doc["device"].set(device);
        doc["availability"].set(availability);
        publishDiscoveryMessage(doc, "text");
    }

    // --- Media Player Entity (replaces select.play_sound, text.tts_text, sensor.audio_status) ---
    // doc.clear();
    // doc["name"] = "Speaker";
    // String media_player_id = String(MQTT_UNIQUE_ID) + "_media_player";
    // doc["unique_id"] = media_player_id;
    // doc["object_id"] = media_player_id;
    // doc["device_class"] = "speaker";

    // // State topics
    // doc["state_topic"] = device_base_topic + "/audio/state";
    // doc["volume_state_topic"] = device_base_topic + "/volume/state";

    // // Command topics that the custom media_player.py entity uses
    // doc["command_topic"] = device_base_topic + "/radio/command";
    // doc["volume_command_topic"] = device_base_topic + "/volume/command";
    // doc["json_commands_topic"] = device_base_topic + "/tts/command";

    // // Supported Features
    // JsonArray features = doc["supported_features"].to<JsonArray>();
    // features.add("play_media");
    // features.add("stop");
    // features.add("volume_set");
    // features.add("select_source");

    // doc["device"].set(device);
    // doc["availability"].set(availability);
    // publishDiscoveryMessage(doc, "media_player");

    // --- Cleanup old entities replaced by the media_player ---
    clearHaEntity("select", "play_sound");
    clearHaEntity("text", "tts_text");
    clearHaEntity("sensor", "audio_status");


    // --- Display Mode Selection ---
    doc.clear();
    doc["name"] = "Display Mode";
    String display_mode_id = String(MQTT_UNIQUE_ID) + "_display_mode";
    doc["unique_id"] = display_mode_id;
    doc["object_id"] = display_mode_id;
    doc["command_topic"] = device_base_topic + "/display_mode/command";
    doc["state_topic"] = device_base_topic + "/display_mode/state";
    JsonArray modes = doc["options"].to<JsonArray>();
    modes.add("Normal Clock");
    modes.add("Stock Ticker");
    modes.add("Weather");
    modes.add("Data Link");
    doc["icon"] = "mdi:television-classic";
    doc["entity_category"] = "config";
    doc["device"].set(device);
    doc["availability"].set(availability);
    publishDiscoveryMessage(doc, "select");

    // --- Live Weather Mode Entities ---
    doc.clear();
    doc["name"] = "Weather City";
    String weather_city_id = String(MQTT_UNIQUE_ID) + "_weather_city";
    doc["unique_id"] = weather_city_id;
    doc["object_id"] = weather_city_id;
    doc["command_topic"] = device_base_topic + "/weather_city/command";
    doc["state_topic"] = device_base_topic + "/weather_city/state";
    doc["icon"] = "mdi:city";
    doc["entity_category"] = "config";
    doc["device"].set(device);
    doc["availability"].set(availability);
    publishDiscoveryMessage(doc, "text");

    doc.clear();
    doc["name"] = "Refresh Weather Data";
    String weather_refresh_id = String(MQTT_UNIQUE_ID) + "_weather_refresh";
    doc["unique_id"] = weather_refresh_id;
    doc["object_id"] = weather_refresh_id;
    doc["command_topic"] = device_base_topic + "/weather_refresh/command";
    doc["payload_press"] = "PRESS";
    doc["icon"] = "mdi:refresh";
    doc["entity_category"] = "config";
    doc["device"].set(device);
    doc["availability"].set(availability);
    publishDiscoveryMessage(doc, "button");

    // New Audio sensor for stream state
    doc.clear();
    doc["name"] = "Audio Stream Status";
    String audio_status_id = String(MQTT_UNIQUE_ID) + "_audio_status";
    doc["unique_id"] = audio_status_id;
    doc["object_id"] = audio_status_id;
    doc["state_topic"] = device_base_topic + "/audio/state";
    doc["icon"] = "mdi:waveform";
    doc["device"].set(device);
    doc["availability"].set(availability);
    publishDiscoveryMessage(doc, "sensor");
}

void reconnectMqtt() {
  Log_printf(LOG_LEVEL_DEBUG, "Entering reconnectMqtt function.");
  if (currentSettings.mqttBroker.empty()) return;
  
  Log_printf(LOG_LEVEL_INFO, "Attempting to connect to MQTT broker: %s...", currentSettings.mqttBroker.c_str());
  delay(100); 

  String clientId = MQTT_UNIQUE_ID;
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
    // It's crucial to delay and call loop() here to allow the client to process the CONNACK from the broker.
    // Without this, the first publish will likely fail as the client is not yet ready.
    delay(250);
    mqttClient.loop();
    
    mqttClient.publish(availability_topic.c_str(), "online", true);
    mqttClient.loop(); // Allow time for the availability message to be sent.

    // Force HA discovery on every reconnect to ensure capabilities are always up-to-date.
    publishHaAutoDiscovery();
    publishHaPresetSelector();

    publishAllHaStates();
    String command_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/+/command";
    mqttClient.subscribe(command_topic.c_str());
    Log_printf(LOG_LEVEL_DEBUG, "Subscribed to wildcard command topic: %s", command_topic.c_str());
    
    String audio_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/tts/command";
    mqttClient.subscribe(audio_topic.c_str());
    audio_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/radio/command";
    mqttClient.subscribe(audio_topic.c_str());
    audio_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/play_sound/command";
    mqttClient.subscribe(audio_topic.c_str());

    String sequencer_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/sequencer/command";
    mqttClient.subscribe(sequencer_topic.c_str());
    Log_printf(LOG_LEVEL_DEBUG, "Subscribed to sequencer command topic: %s", sequencer_topic.c_str());

    String discover_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/discover/command";
    mqttClient.subscribe(discover_topic.c_str());
    Log_printf(LOG_LEVEL_DEBUG, "Subscribed to discover command topic: %s", discover_topic.c_str());


    for (int i = 0; i < currentSettings.numDataPoints; i++) {
      if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_MQTT && !currentSettings.dataPoints[i].mqttTopic.empty()) {
        mqttClient.subscribe(currentSettings.dataPoints[i].mqttTopic.c_str());
      }
      // The complex, four-part subscription for HA Push has been removed.
      // All control is now handled via the wildcard command_topic subscription
      // and the datapoint marquee text entities.
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
    Log_printf(LOG_LEVEL_INFO, "MQTT message received. Topic: [%s]", topic);
    // Use a string to safely handle the payload, which might not be null-terminated.
    std::string message(reinterpret_cast<char*>(payload), length);
    Log_printf(LOG_LEVEL_INFO, "MQTT Payload (raw): %s", message.c_str());


    // Dynamically allocate a buffer for the message to avoid stack overflow
    // and accommodate larger payloads (e.g., for long marquee text).

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
                    if (currentSettings.dataPoints[dp_index].scrollingText != message.c_str()) {
                        currentSettings.dataPoints[dp_index].scrollingText = message.c_str();
                        settingsChanged = true;
                    }
                    isMarqueeBufferDirty = true; // Force display to re-render the marquee
                } else if (id_suffix.endsWith("_enabled")) {
                    bool is_on = (message == "ON");
                    currentSettings.dataPoints[dp_index].enabled = is_on;

                    // Immediately publish the state back to HA to fix momentary toggle issue
                    String enabled_topic = base_topic + component + "/state";
                    mqttClient.publish(enabled_topic.c_str(), is_on ? "ON" : "OFF", true);

                    if (is_on) {
                        // Automation: When a data point is enabled, switch to Data Link mode
                        currentSettings.displayMode = DMS_DATA_LINK;
                        // And ensure the number of data points is high enough
                        if (dp_index + 1 > currentSettings.numDataPoints) {
                            currentSettings.numDataPoints = dp_index + 1;
                        }
                        // And set the source to Home Assistant
                        currentSettings.dataPoints[dp_index].dataSourceType = DATA_SOURCE_HA;

                        // Force a refresh of the marquee to start scrolling immediately
                        isMarqueeBufferDirty = true;
                    } else {
                        // Automation: If the last data point is disabled, revert to Normal Clock mode
                        bool any_other_enabled = false;
                        for (int i = 0; i < 5; i++) {
                            if (currentSettings.dataPoints[i].enabled) {
                                any_other_enabled = true;
                                break;
                            }
                        }
                        if (!any_other_enabled) {
                            currentSettings.displayMode = DMS_NORMAL_CLOCK;
                        }
                    }
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
        } else if (component == "override_line_1") {
            overrideMessageLine1 = message.c_str();
            stateChanged = true;
        } else if (component == "override_line_2") {
            overrideMessageLine2 = message.c_str();
            stateChanged = true;
        } else if (component == "override_line_3") {
            overrideMessageLine3 = message.c_str();
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
                playSound((message + ".mp3").c_str(), true);
            }
            mqttClient.publish((base_topic + "play_sound/state").c_str(), "None", true);
        } else if (component == "sound_toggle") {
            currentSettings.timeTravelSoundToggle = (message == "ON");
            settingsChanged = true;
        } else if (component == "display_mode") {
            if (message == "Normal Clock") {
                currentSettings.displayMode = DMS_NORMAL_CLOCK;
            } else if (message == "Stock Ticker") {
                currentSettings.displayMode = DMS_STOCK_TICKER;
            } else if (message == "Weather") {
                currentSettings.displayMode = DMS_WEATHER;
            } else if (message == "Data Link") {
                currentSettings.displayMode = DMS_DATA_LINK;
            }
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
        } else if (component == "discover" && message == "ON") {
            Log_printf(LOG_LEVEL_INFO, "HA discovery command received. Republishing all entities.");
            publishHaAutoDiscovery();
            publishHaPresetSelector();
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
        } else if (component == "sequencer") {
            handleSequencerCommand(message);
        } else if (component == "tts_text") {
            // When text is received on the command topic, just publish it right back to the state topic.
            // This acts as a trigger for the blueprint automation in Home Assistant, which will then
            // call the real TTS service and send the audio URL back to the .../tts/command topic.
            String state_topic = base_topic + "tts_text/state";
            mqttClient.publish(state_topic.c_str(), message.c_str(), true);
        } else if (component == "tts") {
            Log_printf(LOG_LEVEL_INFO, "Handling media player command (tts topic). Payload: %s", message.c_str());
            JsonDocument doc;
            if (deserializeJson(doc, message) == DeserializationError::Ok) {
                // HA's play_media service sends 'media_id', but we also check for 'url' for direct calls.
                const char* url = doc["media_id"] | doc["url"];
                if (url) {
                    int volume = doc["volume"] | -1; // Use dynamic volume if provided, else -1
                    Log_printf(LOG_LEVEL_INFO, "Parsed media JSON. URL: %s, Volume: %d", url, volume);
                    startAudioStream(url, true, volume);
                } else {
                    Log_printf(LOG_LEVEL_ERROR, "Media JSON received, but 'media_id' or 'url' key is missing.");
                }
            } else {
                Log_printf(LOG_LEVEL_WARN, "Media command payload is not valid JSON. Treating as raw URL.");
                startAudioStream(message.c_str(), true);
            }
        } else if (component == "radio") {
            Log_printf(LOG_LEVEL_INFO, "Handling radio command. Payload: %s", message.c_str());
            if (message == "stop") {
                Log_printf(LOG_LEVEL_INFO, "Stopping audio stream.");
                stopAudioStream();
            } else {
                Log_printf(LOG_LEVEL_INFO, "Starting radio stream.");
                startAudioStream(message.c_str(), false);
            }
        }
    } else {
        // This handles incoming data for any of the 5 data points that are configured
        // with a `dataSourceType` of `DATA_SOURCE_MQTT`.
        for (int i = 0; i < currentSettings.numDataPoints; i++) {
            // Check if the topic matches and the data source is MQTT
            if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_MQTT &&
                topicStr == currentSettings.dataPoints[i].mqttTopic.c_str()) {

                // --- FIX ---
                // The original code was writing to the `displayPages` buffer, which is only
                // used by the `DATA_SOURCE_HA` type. The fix is to write directly to the
                // `scrollingText` field in the settings, which is what the DisplayManager
                // actually reads for this data source type.
                currentSettings.dataPoints[i].scrollingText = message.c_str();
                isMarqueeBufferDirty = true; // Set the dirty flag to force a re-render
                saveSettings(); // Persist the new text

                break; // Exit the loop since we found the matching topic
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
    
    // Publish the state of the new, separate override line entities
    mqttClient.publish((base_topic + "/override_line_1/state").c_str(), overrideMessageLine1.c_str(), true);
    mqttClient.publish((base_topic + "/override_line_2/state").c_str(), overrideMessageLine2.c_str(), true);
    mqttClient.publish((base_topic + "/override_line_3/state").c_str(), overrideMessageLine3.c_str(), true);

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

    // Publish the state of the new display mode selector
    const char* modes[] = {"Normal Clock", "Stock Ticker", "Weather", "Data Link"};
    if (currentSettings.displayMode >= 0 && currentSettings.displayMode < 4) {
        mqttClient.publish((base_topic + "/display_mode/state").c_str(), modes[currentSettings.displayMode], true);
    }

    mqttClient.publish((base_topic + "/audio/state").c_str(), audio.isRunning() ? "PLAYING" : "IDLE", true);

    for(int i=0; i<5; ++i) {
        String enabled_topic = base_topic + "/datapoint_" + String(i) + "_enabled/state";
        mqttClient.publish(enabled_topic.c_str(), currentSettings.dataPoints[i].enabled ? "ON" : "OFF", true);
        String marquee_topic = base_topic + "/datapoint_" + String(i) + "_marquee/state";
        mqttClient.publish(marquee_topic.c_str(), currentSettings.dataPoints[i].scrollingText.c_str(), true);
    }
}

void handleSequencerCommand(const std::string& payload) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        Log_printf(LOG_LEVEL_ERROR, "Failed to parse sequencer JSON: %s", error.c_str());
        return;
    }

    JsonArray sequence = doc.as<JsonArray>();
    if (sequence.isNull()) {
        Log_printf(LOG_LEVEL_ERROR, "Sequencer payload is not a JSON array.");
        return;
    }

    for (JsonObject command : sequence) {
        const char* cmd = command["command"];
        if (!cmd) {
            Log_printf(LOG_LEVEL_WARN, "Skipping invalid sequencer command: missing 'command' key.");
            continue;
        }

        if (strcmp(cmd, "flash") == 0) {
            const char* segment = command["segment"];
            if (segment) {
                int row = -1, seg = -1;
                if (strstr(segment, "dest_")) row = 0;
                else if (strstr(segment, "pres_")) row = 1;
                else if (strstr(segment, "last_")) row = 2;

                if (strstr(segment, "_month")) seg = 0;
                else if (strstr(segment, "_day")) seg = 1;
                else if (strstr(segment, "_year")) seg = 2;
                else if (strstr(segment, "_time")) seg = 3;

                if (row != -1 && seg != -1) {
                    triggerFlashEffect(row, seg, command["duration"] | 500);
                } else {
                    Log_printf(LOG_LEVEL_WARN, "Invalid segment for flash command: %s", segment);
                }
            } else {
                Log_printf(LOG_LEVEL_WARN, "Missing 'segment' for flash command.");
            }
        } else if (strcmp(cmd, "sound") == 0) {
            const char* effect = command["effect"];
            if (effect) {
                playSound((String(effect) + ".mp3").c_str(), true);
            } else {
                Log_printf(LOG_LEVEL_WARN, "Missing 'effect' for sound command.");
            }
        } else if (strcmp(cmd, "message") == 0) {
            const char* display = command["display"];
            if (display) {
                int row = -1;
                if (strcmp(display, "destination") == 0) row = 0;
                else if (strcmp(display, "present") == 0) row = 1;
                else if (strcmp(display, "last_departed") == 0) row = 2;

                if (row != -1) {
                    updateDisplaySegment(row, 0, command["month"] | "");
                    updateDisplaySegment(row, 1, command["day"] | "");
                    updateDisplaySegment(row, 2, command["year"] | "");
                    updateDisplaySegment(row, 3, command["time"] | "");
                } else {
                    Log_printf(LOG_LEVEL_WARN, "Invalid display for message command: %s", display);
                }
            } else {
                Log_printf(LOG_LEVEL_WARN, "Missing 'display' for message command.");
            }
        } else if (strcmp(cmd, "delay") == 0) {
            if (command["duration"].is<int>()) {
                delay(command["duration"].as<int>());
            } else {
                Log_printf(LOG_LEVEL_WARN, "Missing 'duration' for delay command.");
            }
        } else {
            Log_printf(LOG_LEVEL_WARN, "Unknown sequencer command: %s", cmd);
        }
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
    
    // --- FIX: Pass the persistent global buffer, not the temporary 'url' pointer ---
    // The 'url' pointer can become invalid after this function returns, causing a crash
    // in the audio library's background task. 'currentSoundFile' is a global buffer
    // that will persist for the duration of the stream.
    if (audio.connecttohost(currentSoundFile)) {
        Log_printf(LOG_LEVEL_INFO, "Successfully connected to host for streaming: %s", currentSoundFile);
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
  // Programmatically set the buffer size to ensure it's large enough for HA discovery payloads.
  // This is more reliable than using the #define directive.
  if (!mqttClient.setBufferSize(1500)) {
    Log_printf(LOG_LEVEL_ERROR, "CRITICAL: Failed to allocate MQTT buffer. Discovery will fail.");
  }
  mqttClient.setServer(currentSettings.mqttBroker.c_str(), currentSettings.mqttPort);
  mqttClient.setCallback(mqttCallback);
  Log_printf(LOG_LEVEL_INFO, "Client configured for broker [%s] on port [%d]", currentSettings.mqttBroker.c_str(), currentSettings.mqttPort);
}