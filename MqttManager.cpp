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
#include <LittleFS.h>

extern StockManager stockManager;
#include "Audio.h"
extern Audio audio;
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Preferences.h>
#include <LCBUrl.h> 

String currentProfileName = "Standard";
String lastDepartedPreset = "None";

// --- HA Discovery State ---
HaDiscoveryState haDiscoveryState = HA_DISCOVERY_IDLE;
unsigned long lastHaDiscoveryPublish = 0;
const unsigned int HA_DISCOVERY_DELAY = 100; // Milliseconds between each discovery message
static JsonDocument discoveryDoc;
static String device_base_topic;

// --- Radio Metadata Globals ---
String radioStationName = "";
String radioSongTitle = "";
bool isRadioStreaming = false;

// --- NEW: Global state for saving display mode before animations ---
int preAnimationDisplayMode = DMS_NORMAL_CLOCK; // Default to normal clock

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
    size_t payload_size = serializeJson(doc, payload);

    // Check for serialization errors (e.g., buffer overflow)
    if (payload_size == 0) {
        Log_printf(LOG_LEVEL_ERROR, "HA Discovery: JSON serialization failed for %s. Payload buffer may be too small.", object_id.c_str());
        return; // Stop processing this message
    }

    // Log the discovery message details for debugging and verification
    Log_printf(LOG_LEVEL_INFO, "HA Discovery: Publishing to topic [%s]", topic.c_str());
    Log_printf(LOG_LEVEL_INFO, "HA Discovery: Payload: %s", payload.c_str());

    if (mqttClient.connected()) {
        // This is a blocking loop that will continue until the message is successfully published.
        // It now includes a timeout to prevent the device from hanging if the broker is unresponsive.
        const unsigned long publish_timeout = 5000; // 5-second timeout
        unsigned long start_time = millis();
        bool published = false;

        while (mqttClient.connected() && (millis() - start_time < publish_timeout)) {
            if (mqttClient.publish(topic.c_str(), payload.c_str(), true)) {
                published = true;
                break; // Exit loop on success
            }
            // Log a warning that the buffer is full and we are retrying.
            Log_printf(LOG_LEVEL_WARN, "HA Discovery: Publish buffer for %s is full. Retrying in 100ms...", object_id.c_str());
            delay(100);
            mqttClient.loop(); // Allow the client to process outgoing messages
        }

        if (published) {
            // After a successful publish, give the client time to send the message.
            mqttClient.loop();
            delay(75); // A short delay to help ensure message delivery
        } else {
            // Log a critical error if the message could not be sent within the timeout.
            Log_printf(LOG_LEVEL_ERROR, "CRITICAL: HA Discovery for %s failed after %lums. Broker unresponsive?", object_id.c_str(), publish_timeout);
        }
    } else {
        Log_printf(LOG_LEVEL_WARN, "HA Discovery: Cannot publish, MQTT client not connected.");
    }
}

void publishHaPresetSelector() {
    if (!mqttClient.connected()) return;

    String device_base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    JsonDocument doc; // Use a single document

    // --- Start with the standard device and availability info ---
    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"] = MQTT_UNIQUE_ID;
    device["name"] = "Time Circuits";
    device["model"] = "BTTF Clock v1";
    device["manufacturer"] = "Doc Brown Industries";
    device["sw_version"] = "2.0";

    JsonObject availability = doc["availability"].to<JsonObject>();
    availability["topic"] = device_base_topic + "/status";
    availability["payload_available"] = "online";
    availability["payload_not_available"] = "offline";

    // --- Add entity-specific config ---
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

    // Add custom presets from preferences
    Preferences prefs;
    prefs.begin(PREFERENCES_NAMESPACE, true);
    String presetsJson = "[]";
    if (prefs.isKey("customPresets")) {
        presetsJson = prefs.getString("customPresets", "[]");
    }
    prefs.end();
    JsonDocument presetsDoc;
    if (deserializeJson(presetsDoc, presetsJson) == DeserializationError::Ok) {
        for (JsonObject preset : presetsDoc.as<JsonArray>()) {
            if (preset["name"].is<const char*>()) {
                options.add(preset["name"].as<String>());
            }
        }
    }

    // --- Publish ---
    publishDiscoveryMessage(doc, "select");
}

void publishDeviceTriggers() {
    String device_base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    JsonDocument doc; // Use a single, larger doc

    // --- Base Device Info (Identifier only is sufficient for triggers) ---
    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"] = MQTT_UNIQUE_ID;

    // --- Base Trigger Info ---
    doc["automation_type"] = "trigger";
    doc["topic"] = device_base_topic + "/events";

    // --- Loop through triggers, adding/removing specific fields ---
    const char* trigger_types[] = {"animation_started", "animation_completed", "sleep_mode_entered", "sleep_mode_exited", "preset_changed"};
    const char* trigger_subtypes[] = {"anim_started", "anim_completed", "sleep_entered", "sleep_exited", "preset_changed"};

    for(int i = 0; i < sizeof(trigger_types)/sizeof(trigger_types[0]); ++i) {
        doc["type"] = trigger_types[i];
        String object_id = String(MQTT_UNIQUE_ID) + "_" + trigger_subtypes[i];
        doc["object_id"] = object_id;
        doc["unique_id"] = object_id;
        // No "name" needed for device triggers, HA uses "type" and "subtype"

        publishDiscoveryMessage(doc, "device_automation");

        // No need to remove fields here since they are just overwritten in the next loop iteration
    }
}

void publishDisplayMode(int mode) {
    if (!mqttClient.connected()) return;
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    const char* modes[] = {"Normal Clock", "Stock Ticker", "Weather", "Data Link"};
    if (mode >= 0 && mode < 4) {
        mqttClient.publish((base_topic + "/display_mode/state").c_str(), modes[mode], true);
    }
}

void publishHaDiagnosticAttributes() {
    if (!mqttClient.connected()) return;
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    JsonDocument doc; // StaticJsonDocument is fine here, small payload

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


// --- START OF NEW DISCOVERY FUNCTIONS ---

// Forward declarations for all discovery functions
void publishStatusSensorDiscovery();
void publishTimeDisplayEntitiesDiscovery();
void cleanupOldEntities();
void publishDataPointSwitchesDiscovery();
void publishDataPointMarqueesDiscovery();
void cleanupOldDataPointSensors();
void publishNumberConfigsDiscovery();
void publishSwitchConfigsDiscovery();
void publishButtonConfigsDiscovery();
void publishSequencerButtonDiscovery();
void publishTemporalEchoSwitchDiscovery();
void publishProfileSelectorDiscovery();
void publishOverrideSwitchDiscovery();
void cleanupOldOverrideMessageEntity();
void publishOverrideLineTextEntitiesDiscovery();
void cleanupOldMediaPlayerEntities();
void publishDisplayModeSelectorDiscovery();
void publishWeatherEntitiesDiscovery();
void publishAudioStatusSensorDiscovery();
void publishRadioSensorsDiscovery();
void publishPresetSelectorDiscovery();


/**
 * @brief Prepares the shared JSON document for HA discovery messages.
 */
void prepareHaDiscovery() {
    device_base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    discoveryDoc.clear();
    JsonObject device = discoveryDoc["device"].to<JsonObject>();
    device["identifiers"] = MQTT_UNIQUE_ID;
    device["name"] = "Time Circuits";
    device["model"] = "BTTF Clock v1";
    device["manufacturer"] = "Doc Brown Industries";
    device["sw_version"] = "2.0";
    device["icon"] = "mdi:car-clock";
    JsonObject availability = discoveryDoc["availability"].to<JsonObject>();
    availability["topic"] = device_base_topic + "/status";
    availability["payload_available"] = "online";
    availability["payload_not_available"] = "offline";
}

void publishStatusSensorDiscovery() {
    discoveryDoc["name"] = "Status";
    String status_id = String(MQTT_UNIQUE_ID) + "_status";
    discoveryDoc["unique_id"] = status_id;
    discoveryDoc["object_id"] = status_id;
    discoveryDoc["state_topic"] = device_base_topic + "/status/state";
    discoveryDoc["json_attributes_topic"] = device_base_topic + "/status/attributes";
    discoveryDoc["icon"] = "mdi:clock-outline";
    publishDiscoveryMessage(discoveryDoc, "sensor");
    discoveryDoc.remove("name"); discoveryDoc.remove("unique_id"); discoveryDoc.remove("object_id"); discoveryDoc.remove("state_topic"); discoveryDoc.remove("json_attributes_topic"); discoveryDoc.remove("icon");
}

void publishTimeDisplayEntitiesDiscovery() {
    const char* rows[] = {"dest", "pres", "last"};
    const char* row_names[] = {"Destination", "Present", "Last Departed"};
    const char* segments[] = {"month", "day", "year", "time"};
    const char* segment_names[] = {"Month", "Day", "Year", "Time"};
    for (int r = 0; r < 3; ++r) {
        for (int s = 0; s < 4; ++s) {
            String name = String(row_names[r]) + " " + String(segment_names[s]);
            String id_suffix = String(rows[r]) + "_" + String(segments[s]);
            String entity_id = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
            discoveryDoc["name"] = name;
            discoveryDoc["unique_id"] = entity_id;
            discoveryDoc["object_id"] = entity_id;
            discoveryDoc["command_topic"] = device_base_topic + "/" + id_suffix + "/command";
            discoveryDoc["state_topic"] = device_base_topic + "/" + id_suffix + "/state";
            discoveryDoc["icon"] = "mdi:form-textbox";
            publishDiscoveryMessage(discoveryDoc, "text");
            discoveryDoc.remove("name"); discoveryDoc.remove("unique_id"); discoveryDoc.remove("object_id"); discoveryDoc.remove("command_topic"); discoveryDoc.remove("state_topic"); discoveryDoc.remove("icon");
        }
    }
}

void cleanupOldEntities() {
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
}

void publishDataPointSwitchesDiscovery() {
    for (int i=0; i < 5; ++i) {
        discoveryDoc["name"] = "Data Point " + String(i + 1) + " Enabled";
        String id_suffix = "datapoint_" + String(i) + "_enabled";
        String entity_id = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        discoveryDoc["unique_id"] = entity_id;
        discoveryDoc["object_id"] = entity_id;
        discoveryDoc["command_topic"] = device_base_topic + "/" + id_suffix + "/command";
        discoveryDoc["state_topic"] = device_base_topic + "/" + id_suffix + "/state";
        discoveryDoc["icon"] = "mdi:toggle-switch";
        discoveryDoc["entity_category"] = "config";
        publishDiscoveryMessage(discoveryDoc, "switch");
        discoveryDoc.remove("name"); discoveryDoc.remove("unique_id"); discoveryDoc.remove("object_id"); discoveryDoc.remove("command_topic"); discoveryDoc.remove("state_topic"); discoveryDoc.remove("icon"); discoveryDoc.remove("entity_category");
    }
}

void publishDataPointMarqueesDiscovery() {
    for (int i=0; i < 5; ++i) {
        discoveryDoc["name"] = "Data Point " + String(i + 1) + " Marquee";
        String id_suffix = "datapoint_" + String(i) + "_marquee";
        String entity_id = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        discoveryDoc["unique_id"] = entity_id;
        discoveryDoc["object_id"] = entity_id;
        discoveryDoc["command_topic"] = device_base_topic + "/" + id_suffix + "/command";
        discoveryDoc["state_topic"] = device_base_topic + "/" + id_suffix + "/state";
        discoveryDoc["icon"] = "mdi:text-box-outline";
        discoveryDoc["entity_category"] = "config";
        publishDiscoveryMessage(discoveryDoc, "text");
        discoveryDoc.remove("name"); discoveryDoc.remove("unique_id"); discoveryDoc.remove("object_id"); discoveryDoc.remove("command_topic"); discoveryDoc.remove("state_topic"); discoveryDoc.remove("icon"); discoveryDoc.remove("entity_category");
    }
}

void cleanupOldDataPointSensors() {
    for (int i=0; i < 5; ++i) {
        String unique_id_suffix = "datapoint_" + String(i);
        clearHaEntity("sensor", unique_id_suffix.c_str());
    }
}

void publishNumberConfigsDiscovery() {
    const char* number_configs[][5] = {
        {"animation_interval", "Animation Interval", "mdi:clock-in", "min", "0,120,1"},
        {"animation_duration", "Animation Duration", "mdi:movie-filter", "ms", "1000,10000,100"},
        {"stock_refresh", "Stock Refresh", "mdi:chart-line", "min", "1,60,1"}
    };
    for (auto const& cfg : number_configs) {
        discoveryDoc["name"] = cfg[1];
        String entity_id = String(MQTT_UNIQUE_ID) + "_" + cfg[0];
        discoveryDoc["unique_id"] = entity_id;
        discoveryDoc["object_id"] = entity_id;
        discoveryDoc["command_topic"] = device_base_topic + "/" + cfg[0] + "/command";
        discoveryDoc["state_topic"] = device_base_topic + "/" + cfg[0] + "/state";
        discoveryDoc["icon"] = cfg[2];
        discoveryDoc["unit_of_measurement"] = cfg[3];
        int min_val, max_val, step_val;
        // Use sscanf for safer parsing than strtok
        if (sscanf(cfg[4], "%d,%d,%d", &min_val, &max_val, &step_val) == 3) {
            discoveryDoc["min"] = min_val;
            discoveryDoc["max"] = max_val;
            discoveryDoc["step"] = step_val;
        }
        discoveryDoc["entity_category"] = "config";
        publishDiscoveryMessage(discoveryDoc, "number");
        discoveryDoc.remove("name"); discoveryDoc.remove("unique_id"); discoveryDoc.remove("object_id"); discoveryDoc.remove("command_topic"); discoveryDoc.remove("state_topic"); discoveryDoc.remove("icon"); discoveryDoc.remove("unit_of_measurement"); discoveryDoc.remove("min"); discoveryDoc.remove("max"); discoveryDoc.remove("step"); discoveryDoc.remove("entity_category");
    }
}

void publishSwitchConfigsDiscovery() {
     const char* switch_configs[][3] = {
        {"24h_format", "24-Hour Format", "mdi:clock-time-twelve-outline"}
    };
    for (auto const& cfg : switch_configs) {
        discoveryDoc["name"] = cfg[1];
        String entity_id = String(MQTT_UNIQUE_ID) + "_" + cfg[0];
        discoveryDoc["unique_id"] = entity_id;
        discoveryDoc["object_id"] = entity_id;
        discoveryDoc["command_topic"] = device_base_topic + "/" + cfg[0] + "/command";
        discoveryDoc["state_topic"] = device_base_topic + "/" + cfg[0] + "/state";
        discoveryDoc["icon"] = cfg[2];
        discoveryDoc["entity_category"] = "config";
        publishDiscoveryMessage(discoveryDoc, "switch");
        discoveryDoc.remove("name"); discoveryDoc.remove("unique_id"); discoveryDoc.remove("object_id"); discoveryDoc.remove("command_topic"); discoveryDoc.remove("state_topic"); discoveryDoc.remove("icon"); discoveryDoc.remove("entity_category");
    }
}
    
void publishButtonConfigsDiscovery() {
    const char* button_configs[][3] = {
        {"trigger_animation", "Trigger Animation", "mdi:movie-play"},
        {"reboot_device", "Reboot Device", "mdi:restart"},
        {"force_ntp_sync", "Force NTP Sync", "mdi:timer-sync-outline"},
        {"factory_reset", "Factory Reset", "mdi:delete-restore"},
        {"save_all_settings", "Save All Settings", "mdi:content-save-all-outline"}
    };
    for (auto const& cfg : button_configs) {
        discoveryDoc["name"] = cfg[1];
        String entity_id = String(MQTT_UNIQUE_ID) + "_" + cfg[0];
        discoveryDoc["unique_id"] = entity_id;
        discoveryDoc["object_id"] = entity_id;
        discoveryDoc["command_topic"] = device_base_topic + "/" + cfg[0] + "/command";
        discoveryDoc["payload_press"] = "PRESS";
        discoveryDoc["icon"] = cfg[2];
        discoveryDoc["entity_category"] = "config";
        publishDiscoveryMessage(discoveryDoc, "button");
        discoveryDoc.remove("name"); discoveryDoc.remove("unique_id"); discoveryDoc.remove("object_id"); discoveryDoc.remove("command_topic"); discoveryDoc.remove("payload_press"); discoveryDoc.remove("icon"); discoveryDoc.remove("entity_category");
    }
}

void publishSequencerButtonDiscovery() {
    discoveryDoc["name"] = "Trigger Sequence";
    String sequencer_id = String(MQTT_UNIQUE_ID) + "_sequencer";
    discoveryDoc["unique_id"] = sequencer_id;
    discoveryDoc["object_id"] = sequencer_id;
    discoveryDoc["command_topic"] = device_base_topic + "/sequencer/command";
    discoveryDoc["payload_press"] = "PRESS";
    discoveryDoc["icon"] = "mdi:movie-play-outline";
    discoveryDoc["entity_category"] = "config";
    publishDiscoveryMessage(discoveryDoc, "button");
    discoveryDoc.remove("name"); discoveryDoc.remove("unique_id"); discoveryDoc.remove("object_id"); discoveryDoc.remove("command_topic"); discoveryDoc.remove("payload_press"); discoveryDoc.remove("icon"); discoveryDoc.remove("entity_category");
}
    
void publishTemporalEchoSwitchDiscovery() {
    discoveryDoc["name"] = "Temporal Echo Effect";
    String temporal_echo_id = String(MQTT_UNIQUE_ID) + "_temporal_echo";
    discoveryDoc["unique_id"] = temporal_echo_id;
    discoveryDoc["object_id"] = temporal_echo_id;
    discoveryDoc["command_topic"] = device_base_topic + "/temporal_echo/command";
    discoveryDoc["state_topic"] = device_base_topic + "/temporal_echo/state";
    discoveryDoc["icon"] = "mdi:ghost";
    discoveryDoc["entity_category"] = "config";
    publishDiscoveryMessage(discoveryDoc, "switch");
    discoveryDoc.remove("name"); discoveryDoc.remove("unique_id"); discoveryDoc.remove("object_id"); discoveryDoc.remove("command_topic"); discoveryDoc.remove("state_topic"); discoveryDoc.remove("icon"); discoveryDoc.remove("entity_category");
}

void publishProfileSelectorDiscovery() {
    discoveryDoc["name"] = "Profile";
    String profile_id = String(MQTT_UNIQUE_ID) + "_profile";
    discoveryDoc["unique_id"] = profile_id;
    discoveryDoc["object_id"] = profile_id;
    discoveryDoc["command_topic"] = device_base_topic + "/profile/command";
    discoveryDoc["state_topic"] = device_base_topic + "/profile/state";
    JsonArray profiles = discoveryDoc["options"].to<JsonArray>();
    profiles.add("Standard");
    profiles.add("Cinematic");
    profiles.add("Silent Night");
    profiles.add("Unstable");
    profiles.add("Custom");
    discoveryDoc["icon"] = "mdi:movie-settings";
    discoveryDoc["entity_category"] = "config";
    publishDiscoveryMessage(discoveryDoc, "select");
    discoveryDoc.remove("name"); discoveryDoc.remove("unique_id"); discoveryDoc.remove("object_id"); discoveryDoc.remove("command_topic"); discoveryDoc.remove("state_topic"); discoveryDoc.remove("options"); discoveryDoc.remove("icon"); discoveryDoc.remove("entity_category");
}
    
void publishOverrideSwitchDiscovery() {
    discoveryDoc["name"] = "Override Switch";
    String override_switch_id = String(MQTT_UNIQUE_ID) + "_override_switch";
    discoveryDoc["unique_id"] = override_switch_id;
    discoveryDoc["object_id"] = override_switch_id;
    discoveryDoc["command_topic"] = device_base_topic + "/override_switch/command";
    discoveryDoc["state_topic"] = device_base_topic + "/override_switch/state";
    discoveryDoc["icon"] = "mdi:message-cog";
    discoveryDoc["entity_category"] = "config";
    publishDiscoveryMessage(discoveryDoc, "switch");
    discoveryDoc.remove("name"); discoveryDoc.remove("unique_id"); discoveryDoc.remove("object_id"); discoveryDoc.remove("command_topic"); discoveryDoc.remove("state_topic"); discoveryDoc.remove("icon"); discoveryDoc.remove("entity_category");
}

void cleanupOldOverrideMessageEntity() {
    clearHaEntity("text", "override_message");
}

void publishOverrideLineTextEntitiesDiscovery() {
    for (int i = 1; i <= 3; i++) {
        discoveryDoc["name"] = "Override Message Line " + String(i);
        String id_suffix = "override_line_" + String(i);
        String entity_id = String(MQTT_UNIQUE_ID) + "_" + id_suffix;
        discoveryDoc["unique_id"] = entity_id;
        discoveryDoc["object_id"] = entity_id;
        discoveryDoc["command_topic"] = device_base_topic + "/" + id_suffix + "/command";
        discoveryDoc["state_topic"] = device_base_topic + "/" + id_suffix + "/state";
        discoveryDoc["icon"] = "mdi:message-draw";
        discoveryDoc["entity_category"] = "config";
        publishDiscoveryMessage(discoveryDoc, "text");
        discoveryDoc.remove("name"); discoveryDoc.remove("unique_id"); discoveryDoc.remove("object_id"); discoveryDoc.remove("command_topic"); discoveryDoc.remove("state_topic"); discoveryDoc.remove("icon"); discoveryDoc.remove("entity_category");
    }
}

void cleanupOldMediaPlayerEntities() {
    clearHaEntity("select", "play_sound");
    clearHaEntity("text", "tts_text");
    clearHaEntity("sensor", "audio_status");
}

void publishDisplayModeSelectorDiscovery() {
    discoveryDoc["name"] = "Display Mode";
    String display_mode_id = String(MQTT_UNIQUE_ID) + "_display_mode";
    discoveryDoc["unique_id"] = display_mode_id;
    discoveryDoc["object_id"] = display_mode_id;
    discoveryDoc["command_topic"] = device_base_topic + "/display_mode/command";
    discoveryDoc["state_topic"] = device_base_topic + "/display_mode/state";
    JsonArray modes = discoveryDoc["options"].to<JsonArray>();
    modes.add("Normal Clock");
    modes.add("Stock Ticker");
    modes.add("Weather");
    modes.add("Data Link");
    discoveryDoc["icon"] = "mdi:television-classic";
    discoveryDoc["entity_category"] = "config";
    publishDiscoveryMessage(discoveryDoc, "select");
    discoveryDoc.remove("name"); discoveryDoc.remove("unique_id"); discoveryDoc.remove("object_id"); discoveryDoc.remove("command_topic"); discoveryDoc.remove("state_topic"); discoveryDoc.remove("options"); discoveryDoc.remove("icon"); discoveryDoc.remove("entity_category");
}

void publishWeatherEntitiesDiscovery() {
    discoveryDoc["name"] = "Weather City";
    String weather_city_id = String(MQTT_UNIQUE_ID) + "_weather_city";
    discoveryDoc["unique_id"] = weather_city_id;
    discoveryDoc["object_id"] = weather_city_id;
    discoveryDoc["command_topic"] = device_base_topic + "/weather_city/command";
    discoveryDoc["state_topic"] = device_base_topic + "/weather_city/state";
    discoveryDoc["icon"] = "mdi:city";
    discoveryDoc["entity_category"] = "config";
    publishDiscoveryMessage(discoveryDoc, "text");
    discoveryDoc.remove("name"); discoveryDoc.remove("unique_id"); discoveryDoc.remove("object_id"); discoveryDoc.remove("command_topic"); discoveryDoc.remove("state_topic"); discoveryDoc.remove("icon"); discoveryDoc.remove("entity_category");

    discoveryDoc["name"] = "Refresh Weather Data";
    String weather_refresh_id = String(MQTT_UNIQUE_ID) + "_weather_refresh";
    discoveryDoc["unique_id"] = weather_refresh_id;
    discoveryDoc["object_id"] = weather_refresh_id;
    discoveryDoc["command_topic"] = device_base_topic + "/weather_refresh/command";
    discoveryDoc["payload_press"] = "PRESS";
    discoveryDoc["icon"] = "mdi:refresh";
    discoveryDoc["entity_category"] = "config";
    publishDiscoveryMessage(discoveryDoc, "button");
    discoveryDoc.remove("name"); discoveryDoc.remove("unique_id"); discoveryDoc.remove("object_id"); discoveryDoc.remove("command_topic"); discoveryDoc.remove("payload_press"); discoveryDoc.remove("icon"); discoveryDoc.remove("entity_category");
}

void publishAudioStatusSensorDiscovery() {
    discoveryDoc["name"] = "Audio Stream Status";
    String audio_status_id = String(MQTT_UNIQUE_ID) + "_audio_status";
    discoveryDoc["unique_id"] = audio_status_id;
    discoveryDoc["object_id"] = audio_status_id;
    discoveryDoc["state_topic"] = device_base_topic + "/audio/state";
    discoveryDoc["icon"] = "mdi:waveform";
    publishDiscoveryMessage(discoveryDoc, "sensor");
    discoveryDoc.remove("name"); discoveryDoc.remove("unique_id"); discoveryDoc.remove("object_id"); discoveryDoc.remove("state_topic"); discoveryDoc.remove("icon");
}

void publishRadioSensorsDiscovery() {
    discoveryDoc["name"] = "Radio Station";
    String radio_station_id = String(MQTT_UNIQUE_ID) + "_radio_station_name";
    discoveryDoc["unique_id"] = radio_station_id;
    discoveryDoc["object_id"] = radio_station_id;
    discoveryDoc["state_topic"] = device_base_topic + "/radio_station_name/state";
    discoveryDoc["icon"] = "mdi:radio-tower";
    publishDiscoveryMessage(discoveryDoc, "sensor");
    discoveryDoc.remove("name"); discoveryDoc.remove("unique_id"); discoveryDoc.remove("object_id"); discoveryDoc.remove("state_topic"); discoveryDoc.remove("icon");

    discoveryDoc["name"] = "Radio Song";
    String radio_song_id = String(MQTT_UNIQUE_ID) + "_radio_song_title";
    discoveryDoc["unique_id"] = radio_song_id;
    discoveryDoc["object_id"] = radio_song_id;
    discoveryDoc["state_topic"] = device_base_topic + "/radio_song_title/state";
    discoveryDoc["icon"] = "mdi:music-note";
    publishDiscoveryMessage(discoveryDoc, "sensor");
    discoveryDoc.remove("name"); discoveryDoc.remove("unique_id"); discoveryDoc.remove("object_id"); discoveryDoc.remove("state_topic"); discoveryDoc.remove("icon");
}

void publishPresetSelectorDiscovery() {
    // This function is a wrapper around the existing preset selector publisher
    publishHaPresetSelector();
}

// --- END OF NEW DISCOVERY FUNCTIONS ---

// Array of discovery function pointers for the state machine
void (*discovery_functions[])() = {
    prepareHaDiscovery,
    publishStatusSensorDiscovery,
    publishTimeDisplayEntitiesDiscovery,
    cleanupOldEntities,
    publishDataPointSwitchesDiscovery,
    publishDataPointMarqueesDiscovery,
    cleanupOldDataPointSensors,
    publishNumberConfigsDiscovery,
    publishSwitchConfigsDiscovery,
    publishButtonConfigsDiscovery,
    publishSequencerButtonDiscovery,
    publishTemporalEchoSwitchDiscovery,
    publishProfileSelectorDiscovery,
    publishOverrideSwitchDiscovery,
    cleanupOldOverrideMessageEntity,
    publishOverrideLineTextEntitiesDiscovery,
    cleanupOldMediaPlayerEntities,
    publishDisplayModeSelectorDiscovery,
    publishWeatherEntitiesDiscovery,
    publishAudioStatusSensorDiscovery,
    publishRadioSensorsDiscovery,
    publishPresetSelectorDiscovery
};
const int num_discovery_functions = sizeof(discovery_functions) / sizeof(discovery_functions[0]);
static int discovery_function_index = 0;

/**
 * @brief Kicks off the HA discovery process.
 */
void startHaDiscovery() {
    Log_printf(LOG_LEVEL_INFO, "Starting Home Assistant discovery process...");
    haDiscoveryState = HA_DISCOVERY_RUNNING;
    discovery_function_index = 0;
    lastHaDiscoveryPublish = 0; // Allow the first message to be sent immediately
}

/**
 * @brief Manages the non-blocking HA discovery state machine.
 * @details This function should be called in the main loop. It publishes one
 * discovery message per call, with a delay between messages.
 */
void handleHaDiscovery() {
    // --- FIX: Prevent discovery from running if MQTT is not connected ---
    // This is a critical stability fix. The discovery process allocates significant
    // memory for JSON documents. If the client is disconnected, attempting to publish
    // these messages can fail, but the loop continues, leading to rapid memory
    // exhaustion and a device crash (ESP_ERR_NO_MEM).
    if (!mqttClient.connected()) {
        // Log this event, as it indicates a potential issue if it happens frequently.
        Log_printf(LOG_LEVEL_WARN, "HA Discovery: Paused. MQTT client is not connected.");
        // If discovery was running, reset it to idle so it can restart on reconnect.
        // If it was already complete, do nothing. This prevents the state from being
        // reset during the intentional disconnect/reconnect for mDNS startup.
        if (haDiscoveryState == HA_DISCOVERY_RUNNING) {
            haDiscoveryState = HA_DISCOVERY_IDLE;
        }
        return;
    }

    if (haDiscoveryState != HA_DISCOVERY_RUNNING) {
        return;
    }

    if (millis() - lastHaDiscoveryPublish >= HA_DISCOVERY_DELAY) {
        if (discovery_function_index < num_discovery_functions) {
            Log_printf(LOG_LEVEL_INFO, "HA Discovery: Publishing step %d of %d", discovery_function_index + 1, num_discovery_functions);
            discovery_functions[discovery_function_index]();
            discovery_function_index++;
            lastHaDiscoveryPublish = millis();
        } else {
            Log_printf(LOG_LEVEL_INFO, "Home Assistant discovery complete.");
            haDiscoveryState = HA_DISCOVERY_COMPLETE;
        }
    }
}

/**
 * @brief Checks if the Home Assistant discovery process has finished.
 * @return True if discovery is complete, false otherwise.
 */
bool isHaDiscoveryComplete() {
    return haDiscoveryState == HA_DISCOVERY_COMPLETE;
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

    // --- FIX: Only run HA discovery if it has not been completed before. ---
    // This prevents memory-intensive discovery from running on every reconnect,
    // which can cause instability on flaky networks.
    if (haDiscoveryState != HA_DISCOVERY_COMPLETE) {
        Log_printf(LOG_LEVEL_INFO, "HA discovery has not been completed. Starting process.");
        startHaDiscovery();
    } else {
        Log_printf(LOG_LEVEL_INFO, "HA discovery already completed. Skipping.");
    }

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

    String radio_stations_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/radio_stations/command";
    mqttClient.subscribe(radio_stations_topic.c_str());
    Log_printf(LOG_LEVEL_DEBUG, "Subscribed to radio stations command topic: %s", radio_stations_topic.c_str());

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
                applyBrightness();
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
            currentSettings.animationStyle = (AnimationType)std::stoi(message);
            settingsChanged = true;
        } else if (component == "volume") {
            int vol = std::stoi(message);
            if (vol >= 0 && vol <= 21) {
                currentSettings.notificationVolume = vol;
                audio.setVolume(vol);
                settingsChanged = true;
            }
        } else if (component == "override_switch") {
            isMessageOverrideActive = (message == "ON");
            String state_topic = base_topic + "override_switch/state";
            mqttClient.publish(state_topic.c_str(), isMessageOverrideActive ? "ON" : "OFF", true);
            stateChanged = true;
        } else if (component == "override_line_1") {
            overrideMessageLine1 = message.c_str();
            String state_topic = base_topic + "override_line_1/state";
            mqttClient.publish(state_topic.c_str(), message.c_str(), true);
            stateChanged = true;
        } else if (component == "override_line_2") {
            overrideMessageLine2 = message.c_str();
            String state_topic = base_topic + "override_line_2/state";
            mqttClient.publish(state_topic.c_str(), message.c_str(), true);
            stateChanged = true;
        } else if (component == "override_line_3") {
            overrideMessageLine3 = message.c_str();
            String state_topic = base_topic + "override_line_3/state";
            mqttClient.publish(state_topic.c_str(), message.c_str(), true);
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
            lastDepartedPreset = message.c_str();
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
            int oldMode = currentSettings.displayMode;
            if (message == "Normal Clock") {
                currentSettings.displayMode = DMS_NORMAL_CLOCK;
            } else if (message == "Stock Ticker") {
                currentSettings.displayMode = DMS_STOCK_TICKER;
            } else if (message == "Data Link") {
                currentSettings.displayMode = DMS_DATA_LINK;
            } else if (message == "Weather") {
                currentSettings.displayMode = DMS_WEATHER;
            }

            if (currentSettings.displayMode != oldMode) {
                publishDisplayMode(currentSettings.displayMode);
                settingsChanged = true;
            }
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
            startHaDiscovery();
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
            currentProfileName = message.c_str();
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
                Log_printf(LOG_LEVEL_INFO, "Stopping audio stream via user command.");
                stopAudioStream(false); // false = not a temporary stop
            } else {
                Log_printf(LOG_LEVEL_INFO, "Starting radio stream.");
                startAudioStream(message.c_str(), false);
            }
        } else if (component == "radio_stations") {
            Log_printf(LOG_LEVEL_INFO, "Received radio stations list. Saving to LittleFS.");
            File file = LittleFS.open("/radio_stations.json", "w");
            if (!file) {
                Log_printf(LOG_LEVEL_ERROR, "Failed to open radio_stations.json for writing");
                return;
            }
            if (file.print(message.c_str())) {
                Log_printf(LOG_LEVEL_INFO, "Successfully wrote radio stations to LittleFS.");
                broadcastRadioStationsUpdated();
            } else {
                Log_printf(LOG_LEVEL_ERROR, "Failed to write radio stations to LittleFS.");
            }
            file.close();
        }
    } else {
    // --- START: New logic for HA Sensor command in Sequencer ---
    for (int i = 0; i < 3; ++i) {
        if (sequencerTracks[i].isActive && sequencerTracks[i].isWaitingForHAState) {
            if (topicStr == sequencerTracks[i].haSensorTopic.c_str()) {
                Log_printf(LOG_LEVEL_INFO, "MQTT: Received state for track %d. Payload: %s", i, message.c_str());
                int segment = sequencerTracks[i].steps[sequencerTracks[i].currentStep].targetSegment;
                manualDisplayText[i][segment] = message; // Update the display text directly
                sequencerTracks[i].haStateReceived = true; // Signal that we got the data
                break; // Assume only one track can wait for a topic at a time
            }
        }
    }
    // --- END: New logic for HA Sensor command ---

        // This handles incoming data for any of the 5 data points that are configured
        // with a `dataSourceType` of `DATA_SOURCE_MQTT`.
        for (int i = 0; i < currentSettings.numDataPoints; i++) {
            // Check if the topic matches and the data source is MQTT
            if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_MQTT &&
                topicStr == currentSettings.dataPoints[i].mqttTopic.c_str()) {
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

void subscribeToTopic(const std::string& topic) {
    if (mqttClient.connected()) {
        mqttClient.subscribe(topic.c_str());
        Log_printf(LOG_LEVEL_INFO, "MQTT: Subscribed to topic [%s]", topic.c_str());
    }
}

void unsubscribeFromTopic(const std::string& topic) {
    if (mqttClient.connected()) {
        mqttClient.unsubscribe(topic.c_str());
        Log_printf(LOG_LEVEL_INFO, "MQTT: Unsubscribed from topic [%s]", topic.c_str());
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

    // --- ADDED: Publish states for the select entities that were being missed ---
    mqttClient.publish((base_topic + "/profile/state").c_str(), currentProfileName.c_str(), true);
    mqttClient.publish((base_topic + "/preset_selector/state").c_str(), lastDepartedPreset.c_str(), true);

    mqttClient.publish((base_topic + "/audio/state").c_str(), audio.isRunning() ? "PLAYING" : "IDLE", true);

    // --- ADDED: Publish radio metadata states ---
    mqttClient.publish((base_topic + "/radio_station_name/state").c_str(), radioStationName.c_str(), true);
    mqttClient.publish((base_topic + "/radio_song_title/state").c_str(), radioSongTitle.c_str(), true);

    for(int i=0; i<5; ++i) {
        String enabled_topic = base_topic + "/datapoint_" + String(i) + "_enabled/state";
        mqttClient.publish(enabled_topic.c_str(), currentSettings.dataPoints[i].enabled ? "ON" : "OFF", true);
        String marquee_topic = base_topic + "/datapoint_" + String(i) + "_marquee/state";
        mqttClient.publish(marquee_topic.c_str(), currentSettings.dataPoints[i].scrollingText.c_str(), true);
    }
}

void publishMqttMessage(const std::string& topic, const std::string& payload) {
    if (mqttClient.connected()) {
        mqttClient.publish(topic.c_str(), payload.c_str(), false); // Not retained
        Log_printf(LOG_LEVEL_INFO, "MQTT: Published to topic [%s] with payload [%s]", topic.c_str(), payload.c_str());
    } else {
        Log_printf(LOG_LEVEL_WARN, "MQTT: Cannot publish, client not connected.");
    }
}

void handleSequencerCommand(const std::string& payload) {
    // --- NEW: Save the current display mode before starting any sequence ---
    preAnimationDisplayMode = currentSettings.displayMode;

    // --- FIX: Stop any and all currently running sequences before starting a new one ---
    // This acts as a "master reset" to prevent state corruption or conflicts when
    // a new sequence is triggered while another is still active.
    stopAllSequences();

    // --- NEW: Handle special, non-JSON-based named sequences ---
    if (payload == "Debug") {
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence 'Debug'");
        generateAnimationSequence(ANIMATION_DEBUG, sequencerTracks);
        return; // Bypass JSON parsing
    } else if (payload == "DebugEffects") {
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence 'DebugEffects'");
        generateAnimationSequence(ANIMATION_DEBUG_EFFECTS, sequencerTracks);
        return; // Bypass JSON parsing
    } else if (payload == "DebugLogic") {
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence 'DebugLogic'");
        generateAnimationSequence(ANIMATION_DEBUG_LOGIC, sequencerTracks);
        return; // Bypass JSON parsing
    } else if (payload == "DebugParallelLogic") {
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence 'DebugParallelLogic'");
        generateAnimationSequence(ANIMATION_DEBUG_PARALLEL_LOGIC, sequencerTracks);
        return; // Bypass JSON parsing
    } else if (payload == "DebugStress") {
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence 'DebugStress'");
        generateAnimationSequence(ANIMATION_DEBUG_STRESS, sequencerTracks);
        return; // Bypass JSON parsing
    } else if (payload == "CrossfadeTest") {
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence 'CrossfadeTest'");
        runCrossfadeTest();
        return; // Bypass JSON parsing
    } else if (payload == "DebugWipe") {
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence 'DebugWipe'");
        runWipeTest();
        return; // Bypass JSON parsing
    }

    JsonDocument doc;
    std::string json_to_parse;

    // First, check if the payload is a named sequence string
    if (payload == "Intruder Alert") {
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence 'Intruder Alert'");
        json_to_parse = R"([
            {"targetRow":0, "commands":[{"command":"MARQUEE", "stringParam":"INTRUDER ALERT"}, {"command":"SOUND", "stringParam":"electric_sparks.mp3"}, {"command":"PULSE", "targetSegment":-1, "intParam":5000}]},
            {"targetRow":1, "commands":[{"command":"SCRAMBLE_TEXT", "stringParam":"BREACH DETECTED", "intParam":100, "intParam2":400}]},
            {"targetRow":2, "commands":[{"command":"MARQUEE", "stringParam":"LOCKDOWN INITIATED"}, {"command":"PULSE", "targetSegment":-1, "intParam":5000}]}
        ])";
    } else if (payload == "Time Travel") {
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence 'Time Travel'");
        json_to_parse = R"([
            {"targetRow": 0, "commands": [{"command": "SOUND", "stringParam": "time_travel.mp3"}, {"command": "BAR_GRAPH", "stringParam":"GO", "intParam":0, "intParam2":8000}]},
            {"targetRow": 1, "commands": [{"command": "MARQUEE", "stringParam": "TIME TRAVEL ACTIVATED"}, {"command": "WAIT", "intParam": 1000}, {"command": "MARQUEE", "stringParam": "88 MPH"}]},
            {"targetRow": 2, "commands": [{"command": "FLASH", "targetSegment": -1, "intParam": 8000}]}
        ])";
    } else if (payload == "Party Mode") {
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence 'Party Mode'");
        json_to_parse = R"([
            {"targetRow":0, "commands":[{"command":"MARQUEE", "stringParam":"PARTY TIME!"}, {"command":"LOOP_START"}, {"command":"PULSE", "targetSegment":-1, "intParam":1000}, {"command":"WAIT", "intParam":1000}, {"command":"LOOP_END"}]},
            {"targetRow":1, "commands":[{"command":"LOOP_START"}, {"command":"MARQUEE", "stringParam":"DANCE"}, {"command":"WAIT", "intParam":2000}, {"command":"MARQUEE", "stringParam":"PARTY"}, {"command":"WAIT", "intParam":2000}, {"command":"LOOP_END"}]},
            {"targetRow":2, "commands":[{"command":"LOOP_START"}, {"command":"MARQUEE", "stringParam":"WOOHOO"}, {"command":"WAIT", "intParam":5000}, {"command":"LOOP_END"}]}
        ])";
    } else if (payload == "Countdown") {
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence 'Countdown'");
        json_to_parse = R"([
            {"targetRow":1, "commands":[{"command":"COUNTDOWN", "intParam":10, "intParam2":1000}, {"command":"MARQUEE", "stringParam":"LIFTOFF!"}, {"command":"SOUND", "stringParam":"engine_rev.mp3"}]}
        ])";
    } else if (payload == "Knight Rider") {
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence 'Knight Rider'");
        json_to_parse = R"([
            {"targetRow":2, "commands":[{"command":"SCANNER", "intParam":10000, "intParam2":100}]}
        ])";
    } else if (payload == "Cylon") {
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence 'Cylon'");
        json_to_parse = R"([
            {"targetRow":1, "commands":[{"command":"SCANNER", "intParam":10000, "intParam2":200}]}
        ])";
    } else if (payload == "Lightning") {
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence 'Lightning'");
        json_to_parse = R"([
            {"targetRow":0, "commands":[
                {"command":"SOUND", "stringParam":"/lightning.mp3"},
                {"command":"LOOP_START", "intParam":10},
                {"command":"RANDOM_FLICKER_TEXT", "stringParam":" ", "intParam":200, "intParam2":50},
                {"command":"WAIT", "intParam":200},
                {"command":"FLASH", "targetSegment":-1, "intParam":150},
                {"command":"WAIT", "intParam":150},
                {"command":"LOOP_END"}
            ]},
            {"targetRow":1, "commands":[
                {"command":"LOOP_START", "intParam":10},
                {"command":"RANDOM_FLICKER_TEXT", "stringParam":" ", "intParam":200, "intParam2":50},
                {"command":"WAIT", "intParam":200},
                {"command":"FLASH", "targetSegment":-1, "intParam":150},
                {"command":"WAIT", "intParam":150},
                {"command":"LOOP_END"}
            ]},
            {"targetRow":2, "commands":[
                {"command":"LOOP_START", "intParam":10},
                {"command":"RANDOM_FLICKER_TEXT", "stringParam":" ", "intParam":200, "intParam2":50},
                {"command":"WAIT", "intParam":200},
                {"command":"FLASH", "targetSegment":-1, "intParam":150},
                {"command":"WAIT", "intParam":150},
                {"command":"LOOP_END"}
            ]}
        ])";
    } else if (payload == "Loading") {
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence 'Loading'");
        json_to_parse = R"([
            {"targetRow":0, "commands":[
                {"command":"SET_TEXT", "stringParam":"FLUX CAPACITOR"},
                {"command":"WAIT", "intParam":1500}
            ]},
            {"targetRow":1, "commands":[
                {"command":"WAIT", "intParam":1500},
                {"command":"SET_TEXT", "stringParam":"TIME CIRCUITS"},
                {"command":"WAIT", "intParam":1500}
            ]},
            {"targetRow":2, "commands":[
                {"command":"WAIT", "intParam":3000},
                {"command":"SET_TEXT", "stringParam":"SYSTEMS ONLINE"}
            ]}
        ])";
    } else if (payload == "Error") {
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence 'Error'");
        json_to_parse = R"([
            {"targetRow":0, "commands":[
                {"command":"SCRAMBLE_TEXT", "stringParam":"ERROR", "intParam":100, "intParam2":200},
                {"command":"SET_TEXT", "stringParam":"ERROR"}
            ]},
            {"targetRow":1, "commands":[{"command":"MARQUEE", "stringParam":"SYSTEM MALFUNCTION"}]},
            {"targetRow":2, "commands":[{"command":"SOUND", "stringParam":"/error_beeps.mp3"}]}
        ])";
    } else if (payload == "Flux Capacitor Charge-Up") {
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence 'Flux Capacitor Charge-Up'");
        json_to_parse = R"([
            {"targetRow":2, "commands":[{"command":"SOUND", "stringParam":"flux_capacitor_power_on.mp3"}, {"command":"BAR_GRAPH", "stringParam":"CHARGING", "intParam":0, "intParam2":5000}]},
            {"targetRow":0, "commands":[{"command":"WAIT", "intParam":5000}, {"command":"FLASH", "targetSegment":-1, "intParam":500}]},
            {"targetRow":1, "commands":[{"command":"WAIT", "intParam":5000}, {"command":"FLASH", "targetSegment":-1, "intParam":500}]}
        ])";
    } else if (payload == "Tachyons Detected") {
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence 'Tachyons Detected'");
        json_to_parse = R"([
            {"targetRow":1, "commands":[{"command":"SCRAMBLE_TEXT", "stringParam":"TACHYONS DETECTED", "intParam":150, "intParam2":5000}, {"command":"SOUND", "stringParam":"hum.mp3"}]}
        ])";
    } else if (payload == "Data Stream") {
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence 'Data Stream'");
        json_to_parse = R"([
            {"targetRow":0, "commands":[{"command":"RANDOM_FLICKER_TEXT", "stringParam":" ", "intParam":50, "intParam2":10000}]},
            {"targetRow":1, "commands":[{"command":"RANDOM_FLICKER_TEXT", "stringParam":" ", "intParam":50, "intParam2":10000}]},
            {"targetRow":2, "commands":[{"command":"RANDOM_FLICKER_TEXT", "stringParam":" ", "intParam":50, "intParam2":10000}]}
        ])";
    } else if (payload == "Wormhole Collapse") {
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence 'Wormhole Collapse'");
        json_to_parse = R"([
            {"targetRow":0, "commands":[{"command":"SOUND", "stringParam":"electric_sparks.mp3"}, {"command":"RANDOM_FLICKER_TEXT", "stringParam":" ", "intParam":100, "intParam2":3000}, {"command":"WAIT", "intParam":3000}, {"command":"FADE_OUT", "intParam":1000}]},
            {"targetRow":1, "commands":[{"command":"RANDOM_FLICKER_TEXT", "stringParam":" ", "intParam":100, "intParam2":3000}, {"command":"WAIT", "intParam":3500}, {"command":"FADE_OUT", "intParam":1000}]},
            {"targetRow":2, "commands":[{"command":"RANDOM_FLICKER_TEXT", "stringParam":" ", "intParam":100, "intParam2":3000}, {"command":"WAIT", "intParam":4000}, {"command":"FADE_OUT", "intParam":1000}]}
        ])";
    } else {
        // Assume it's a JSON payload
        json_to_parse = payload;
    }

    DeserializationError error = deserializeJson(doc, json_to_parse);

    if (error) {
        // If we are here, it means the payload was not a known named sequence, AND it's not valid JSON.
        Log_printf(LOG_LEVEL_ERROR, "Failed to parse sequencer JSON: %s. Payload was: %s", error.c_str(), payload.c_str());
        return;
    }

    JsonArray track_definitions = doc.as<JsonArray>();
    if (track_definitions.isNull()) {
        Log_printf(LOG_LEVEL_ERROR, "Sequencer payload is not a JSON array of track definitions.");
        return;
    }

    for (JsonObject track_def : track_definitions) {
        int targetRow = track_def["targetRow"] | -1;

        if (targetRow < 0 || targetRow > 2) {
            Log_printf(LOG_LEVEL_WARN, "Skipping track with invalid targetRow: %d", targetRow);
            continue;
        }

        if (sequencerTracks[targetRow].isActive) {
            Log_printf(LOG_LEVEL_WARN, "Ignoring new sequence for row %d, one is already active.", targetRow);
            continue;
        }

        JsonArray commands = track_def["commands"].as<JsonArray>();
        if (commands.isNull()) {
            Log_printf(LOG_LEVEL_WARN, "Skipping track for row %d: missing 'commands' array.", targetRow);
            continue;
        }

        int step_index = 0;
        for (JsonObject command : commands) {
            if (step_index >= 19) {
                Log_printf(LOG_LEVEL_WARN, "Command limit reached for track %d. Ignoring further commands.", targetRow);
                break;
            }

            const char* cmd = command["command"];
            if (!cmd) {
                Log_printf(LOG_LEVEL_WARN, "Skipping invalid command in track %d: missing 'command' key.", targetRow);
                continue;
            }

            SequenceStep& current_step = sequencerTracks[targetRow].steps[step_index];
            current_step.targetRow = targetRow; // Assign the row to the step for context

            if (strcmp(cmd, "MARQUEE") == 0) {
                current_step.command = SEQ_CMD_MARQUEE;
                current_step.stringParam = command["stringParam"] | "";
                current_step.targetSegment = command["targetSegment"] | -1;
            } else if (strcmp(cmd, "FADE_IN") == 0) {
                current_step.command = SEQ_CMD_FADE_IN;
                current_step.intParam = command["intParam"] | 1000;
            } else if (strcmp(cmd, "FADE_OUT") == 0) {
                current_step.command = SEQ_CMD_FADE_OUT;
                current_step.intParam = command["intParam"] | 1000;
            } else if (strcmp(cmd, "PULSE") == 0) {
                current_step.command = SEQ_CMD_PULSE;
                current_step.targetSegment = command["targetSegment"] | -1;
                current_step.intParam = command["intParam"] | 1000;
            } else if (strcmp(cmd, "FLASH") == 0) {
                current_step.command = SEQ_CMD_FLASH;
                current_step.targetSegment = command["targetSegment"] | -1;
                current_step.intParam = command["intParam"] | 500;
            } else if (strcmp(cmd, "SOUND") == 0) {
                current_step.command = SEQ_CMD_SOUND;
                current_step.stringParam = command["stringParam"] | "";
            } else if (strcmp(cmd, "WAIT") == 0) {
                current_step.command = SEQ_CMD_WAIT;
                current_step.intParam = command["intParam"] | 1000;
            } else if (strcmp(cmd, "RANDOM_FLICKER_TEXT") == 0) {
                current_step.command = SEQ_CMD_RANDOM_FLICKER_TEXT;
                current_step.stringParam = command["stringParam"] | "";
                current_step.intParam = command["intParam"] | 2000;
                current_step.intParam2 = command["intParam2"] | 100;
            } else if (strcmp(cmd, "BAR_GRAPH") == 0) {
                current_step.command = SEQ_CMD_BAR_GRAPH;
                current_step.stringParam = command["stringParam"] | "";
                current_step.intParam = command["intParam"] | 5000;
                current_step.intParam2 = command["intParam2"] | 250;
            } else if (strcmp(cmd, "SCRAMBLE_TEXT") == 0) {
                current_step.command = SEQ_CMD_SCRAMBLE_TEXT;
                current_step.stringParam = command["stringParam"] | "";
                current_step.targetSegment = command["targetSegment"] | -1;
                current_step.intParam = command["intParam"] | 50;
                current_step.intParam2 = command["intParam2"] | 100;
            } else if (strcmp(cmd, "SCANNER") == 0) {
                current_step.command = SEQ_CMD_SCANNER;
                current_step.stringParam = command["stringParam"] | "---";
                current_step.intParam = command["intParam"] | 5000;
                current_step.intParam2 = command["intParam2"] | 50;
            } else if (strcmp(cmd, "COUNTDOWN") == 0) {
                current_step.command = SEQ_CMD_COUNTDOWN;
                current_step.targetSegment = command["targetSegment"] | -1;
                current_step.intParam = command["intParam"] | 10;
                current_step.intParam2 = command["intParam2"] | 1000;
            } else if (strcmp(cmd, "LOOP_START") == 0) {
                current_step.command = SEQ_CMD_LOOP_START;
                current_step.intParam = command["intParam"] | 1;
            } else if (strcmp(cmd, "LOOP_END") == 0) {
                current_step.command = SEQ_CMD_LOOP_END;
            } else if (strcmp(cmd, "SET_TEXT") == 0) {
                current_step.command = SEQ_CMD_SET_TEXT;
                current_step.stringParam = command["stringParam"] | "";
                current_step.targetSegment = command["targetSegment"] | -1;
            } else if (strcmp(cmd, "CLEAR_SEGMENT") == 0) {
                current_step.command = SEQ_CMD_CLEAR_SEGMENT;
                current_step.targetSegment = command["targetSegment"] | 0;
            } else if (strcmp(cmd, "SET_BRIGHTNESS") == 0) {
                current_step.command = SEQ_CMD_SET_BRIGHTNESS;
                current_step.intParam = command["intParam"] | 7; // Default to max brightness
            } else if (strcmp(cmd, "RESTORE_ROW") == 0) {
                current_step.command = SEQ_CMD_RESTORE_ROW;
            } else if (strcmp(cmd, "TYPEWRITER") == 0) {
                current_step.command = SEQ_CMD_TYPEWRITER;
                current_step.stringParam = command["stringParam"] | "";
                current_step.targetSegment = command["targetSegment"] | -1;
                current_step.intParam = command["intParam"] | 50; // Delay
            } else if (strcmp(cmd, "WIPE") == 0) {
                current_step.command = SEQ_CMD_WIPE;
                current_step.stringParam = command["stringParam"] | "";
                current_step.intParam = command["intParam"] | 50; // Delay
            } else if (strcmp(cmd, "SCROLL_IN") == 0) {
                current_step.command = SEQ_CMD_SCROLL_IN;
                current_step.stringParam = command["stringParam"] | "";
                current_step.intParam = command["intParam"] | 50; // Delay
            } else if (strcmp(cmd, "CROSSFADE_TEXT") == 0) {
                current_step.command = SEQ_CMD_CROSSFADE_TEXT;
                current_step.stringParam = command["stringParam"] | "";
                current_step.targetSegment = command["targetSegment"] | -1;
                current_step.intParam = command["intParam"] | 1000; // Duration
            } else if (strcmp(cmd, "TRIGGER_ANIMATION") == 0) {
                current_step.command = SEQ_CMD_TRIGGER_ANIMATION;
                current_step.intParam = command["intParam"] | 0; // Animation type
            } else if (strcmp(cmd, "MQTT_PUBLISH") == 0) {
                current_step.command = SEQ_CMD_MQTT_PUBLISH;
                current_step.stringParam = command["stringParam"] | ""; // Topic
                current_step.stringParam2 = command["stringParam2"] | ""; // Payload
            } else if (strcmp(cmd, "DISPLAY_HA_SENSOR") == 0) {
                current_step.command = SEQ_CMD_DISPLAY_HA_SENSOR;
                current_step.stringParam = command["stringParam"] | ""; // Topic (sensor entity_id)
                current_step.targetSegment = command["targetSegment"] | 0;
            } else if (strcmp(cmd, "RESTORE_ALL_ROWS") == 0) {
                current_step.command = SEQ_CMD_RESTORE_ALL_ROWS;
            } else {
                Log_printf(LOG_LEVEL_WARN, "Unknown sequencer command '%s' in track %d.", cmd, targetRow);
                continue;
            }
            step_index++;
        }

        sequencerTracks[targetRow].steps[step_index].command = SEQ_CMD_END;
        sequencerTracks[targetRow].currentStep = 0;
        sequencerTracks[targetRow].stepStartTime = millis();
        sequencerTracks[targetRow].isActive = true;
        sequencerTracks[targetRow].trackStartTime = millis(); // --- NEW: Set track start time for timeout ---
        sequencerTracks[targetRow].stepInitialized = false; // --- FIX: Reset the initialization flag for the new sequence ---
        Log_printf(LOG_LEVEL_INFO, "Sequencer track %d activated with %d steps.", targetRow, step_index);
    }
}

void updateHaStatus(const char* status) {
	if (!mqttClient.connected()) return;
	String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
	mqttClient.publish((base_topic + "/status/state").c_str(), status, true);
}

/**
 * @brief Publishes the current radio metadata to the corresponding MQTT topics.
 * @details This function is called whenever the station name or song title changes.
 * It sends the data to Home Assistant and broadcasts it to the web UI.
 */
void publishRadioMetadata() {
    if (!mqttClient.connected()) return;
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;

    // Publish to Home Assistant topics
    mqttClient.publish((base_topic + "/radio_station_name/state").c_str(), radioStationName.c_str(), true);
    mqttClient.publish((base_topic + "/radio_song_title/state").c_str(), radioSongTitle.c_str(), true);

    // Broadcast to Web UI
    broadcastRadioMetadata(radioStationName.c_str(), radioSongTitle.c_str());
}

void audio_info(Audio::msg_t m) {
    switch(m.e) {
        case Audio::evt_streamtitle:
            Log_printf(LOG_LEVEL_INFO, "ICY METADATA: %s", m.msg);
            radioSongTitle = m.msg;
            // Clean up common garbage text from titles
            radioSongTitle.replace(" - ", " ");
            radioSongTitle.replace("Now Playing: ", "");
            radioSongTitle.trim();
            if (radioSongTitle.length() <= 1) {
                radioSongTitle = "Currently Playing";
            }
            publishRadioMetadata();
            break;

        case Audio::evt_name:
            Log_printf(LOG_LEVEL_INFO, "STATION NAME: %s", m.msg);
            radioStationName = m.msg;
            publishRadioMetadata();
            break;

        case Audio::evt_eof:
            Log_printf(LOG_LEVEL_INFO, "Stream ended unexpectedly. Info: %s.", m.msg);
            if (isRadioStreaming) {
                Log_printf(LOG_LEVEL_INFO, "Radio stream dropped. Performing permanent cleanup.");
                cleanupAudio(true);
            } else {
                Log_printf(LOG_LEVEL_INFO, "TTS or other temporary stream ended. Performing temporary cleanup.");
                cleanupAudio(false);
            }
            break;

        default:
            // You can add other cases here if needed, e.g., for logging
            // Log_printf(LOG_LEVEL_DEBUG, "Audio Event: %s", m.msg);
            break;
    }
}

/**
 * @brief Centralized function to stop audio playback and reset all related states.
 * @details This function is the single source of truth for halting any audio.
 * It stops the player, powers down the DAC, clears all state variables
 * (isRadioStreaming, metadata, etc.), unregisters callbacks, and notifies all
 * clients (HA and Web UI) that playback has stopped. This ensures the system
s
 * state is always consistent.
 */
void cleanupAudio(bool isPermanent) {
    Log_printf(LOG_LEVEL_INFO, "--- Centralized Audio Cleanup (Permanent: %s) ---", isPermanent ? "true" : "false");

    if (audio.isRunning()) {
        audio.stopSong();
    }

    digitalWrite(I2S_SD_PIN, LOW);
    currentSoundFile[0] = '\0';

    // If the stop is permanent (i.e., user-commanded), and the radio was playing,
    // then we must reset all the radio-specific states.
    if (isPermanent && isRadioStreaming) {
        isRadioStreaming = false;
        radioStationName = "";
        radioSongTitle = "";
        publishRadioMetadata(); // Send cleared metadata
        broadcastRadioStatus(RADIO_STATUS_STOPPED);
    }

    // The new API uses a single static callback, so we don't unregister it.

    // Update HA state to IDLE
    if (mqttClient.connected()) {
        mqttClient.publish((String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/audio/state").c_str(), "IDLE", true);
    }

    Log_printf(LOG_LEVEL_INFO, "--- Audio cleanup complete ---");
}

void startAudioStream(const char* url, bool is_tts, int volume) {
    Log_printf(LOG_LEVEL_INFO, "Request to start audio stream from URL: %s", url);
    if (!hardwareInitialized) {
        Log_printf(LOG_LEVEL_WARN, "Hardware not initialized, cannot play audio.");
        if (!is_tts) broadcastRadioStatus(RADIO_STATUS_ERROR, "Hardware not ready");
        return;
    }

    if (audio.isRunning()) {
        Log_printf(LOG_LEVEL_DEBUG, "Stopping existing audio to play new stream.");
        // If the new stream is TTS, the current one is stopped temporarily.
        // If the new stream is Radio, the current one is stopped permanently.
        stopAudioStream(is_tts);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // If it's a radio stream, set the callback. Otherwise, ensure it's null.
    if (!is_tts) {
        isRadioStreaming = true; // It's a radio stream
        broadcastRadioStatus(RADIO_STATUS_CONNECTING);
    } else {
        isRadioStreaming = false; // It's a TTS stream
    }
    
    digitalWrite(I2S_SD_PIN, HIGH);
    
    if (volume >= 0 && volume <= 100) {
        int device_volume = round(volume / 100.0 * 21.0);
        audio.setVolume(device_volume);
        Log_printf(LOG_LEVEL_DEBUG, "Set dynamic volume to %d (%d/100)", device_volume, volume);
    } else {
        audio.setVolume(currentSettings.notificationVolume);
    }

    strncpy(currentSoundFile, url, MAX_FILENAME_LENGTH - 1);
    currentSoundFile[MAX_FILENAME_LENGTH - 1] = '\0';
    
    if (audio.connecttohost(currentSoundFile)) {
        Log_printf(LOG_LEVEL_INFO, "Successfully connected to host for streaming: %s", currentSoundFile);
        if (mqttClient.connected()) {
            mqttClient.publish((String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/audio/state").c_str(), "PLAYING", true);
        }
        if (!is_tts) {
            broadcastRadioStatus(RADIO_STATUS_PLAYING);
            // The station name is now handled by the evt_name case in the audio_info callback
            // radioStationName = audio.getStationName();
            // publishRadioMetadata();
        }
    } else {
        Log_printf(LOG_LEVEL_ERROR, "Failed to connect to host for streaming: %s", url);
        // Broadcast a specific error to the UI before running the generic cleanup.
        if (!is_tts) {
            broadcastRadioStatus(RADIO_STATUS_ERROR, "Failed to connect to host");
        }
        // A failed connection attempt means the stream should be considered permanently stopped.
        cleanupAudio(true);
    }
}

void stopAudioStream(bool isTemporary) {
    Log_printf(LOG_LEVEL_INFO, "Request to stop audio stream (isTemporary: %s)", isTemporary ? "true" : "false");
    // A temporary stop is NOT permanent. A non-temporary stop IS permanent.
    cleanupAudio(!isTemporary);
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