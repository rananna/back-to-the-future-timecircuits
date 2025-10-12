/**
 * @file MqttManager.cpp
 * @brief Manages all MQTT communication for Home Assistant integration.
 * @details This module handles the connection to an MQTT broker, publishes device
 * status and sensor data, and subscribes to command topics to allow for remote
 * control. It is responsible for generating the Home Assistant MQTT Discovery
 * configuration messages, which allow the device to be automatically recognized
 * by Home Assistant.
 */

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
#include "Sequencer.h"
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

// Forward declaration for the function defined later in the file.
void ensureBaseDiscoveryConfig();

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

/**
 * @brief Clears a Home Assistant entity's discovery configuration.
 * @details This is used to remove old or deprecated entities from Home Assistant by
 * publishing an empty payload to their configuration topic with the retain flag set.
 * @param component The HA component type (e.g., "sensor", "switch").
 * @param unique_id_suffix The unique part of the entity's ID.
 */
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

/**
 * @brief Publishes the discovery configuration for the "Last Departed Preset" selector.
 * @details This creates a dropdown in Home Assistant containing all the movie presets
 * and any user-defined custom presets, allowing the user to select a "Last Time Departed"
 * value from a predefined list.
 */
void publishHaPresetSelector() {
    ensureBaseDiscoveryConfig();
    if (!mqttClient.connected()) return;
    String device_base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;

    // --- Add entity-specific config to the global discoveryDoc ---
    discoveryDoc["name"] = "Last Departed Preset";
    String preset_selector_id = String(MQTT_UNIQUE_ID) + "_preset_selector";
    discoveryDoc["unique_id"] = preset_selector_id;
    discoveryDoc["object_id"] = preset_selector_id;
    discoveryDoc["command_topic"] = device_base_topic + "/preset_selector/command";
    discoveryDoc["state_topic"] = device_base_topic + "/preset_selector/state";
    discoveryDoc["icon"] = "mdi:history";
    discoveryDoc["entity_category"] = "config";

    JsonArray options = discoveryDoc["options"].to<JsonArray>();
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
    publishDiscoveryMessage(discoveryDoc, "select");

    // --- Cleanup ---
    discoveryDoc.remove("name");
    discoveryDoc.remove("unique_id");
    discoveryDoc.remove("object_id");
    discoveryDoc.remove("command_topic");
    discoveryDoc.remove("state_topic");
    discoveryDoc.remove("icon");
    discoveryDoc.remove("entity_category");
    discoveryDoc.remove("options");
}

/**
 * @brief Publishes the discovery configurations for all device triggers.
 * @details This allows Home Assistant automations to be triggered by specific events
 * occurring on the device, such as an animation starting or finishing.
 */
void publishDeviceTriggers() {
    ensureBaseDiscoveryConfig();
    String device_base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;

    // --- Base Trigger Info (using global discoveryDoc) ---
    discoveryDoc["automation_type"] = "trigger";
    discoveryDoc["topic"] = device_base_topic + "/events";

    // --- Loop through triggers, adding/removing specific fields ---
    const char* trigger_types[] = {"animation_started", "animation_completed", "sleep_mode_entered", "sleep_mode_exited", "preset_changed"};
    const char* trigger_subtypes[] = {"anim_started", "anim_completed", "sleep_entered", "sleep_exited", "preset_changed"};

    for(int i = 0; i < sizeof(trigger_types)/sizeof(trigger_types[0]); ++i) {
        discoveryDoc["type"] = trigger_types[i];
        discoveryDoc["subtype"] = trigger_subtypes[i];
        String object_id = String(MQTT_UNIQUE_ID) + "_" + trigger_subtypes[i];
        discoveryDoc["object_id"] = object_id;
        discoveryDoc["unique_id"] = object_id;

        publishDiscoveryMessage(discoveryDoc, "device_automation");

        // Clean up entity-specific fields for the next iteration/function
        discoveryDoc.remove("type");
        discoveryDoc.remove("subtype");
        discoveryDoc.remove("object_id");
        discoveryDoc.remove("unique_id");
    }

    // Clean up fields common to all triggers in this function
    discoveryDoc.remove("automation_type");
    discoveryDoc.remove("topic");
}

/**
 * @brief Publishes the current display mode state to MQTT.
 * @param mode The integer representing the current display mode.
 */
void publishDisplayMode(int mode) {
    if (!mqttClient.connected()) return;
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    const char* modes[] = {"Normal Clock", "Stock Ticker", "Weather", "Data Link"};
    if (mode >= 0 && mode < 4) {
        mqttClient.publish((base_topic + "/display_mode/state").c_str(), modes[mode], true);
    }
}

/**
 * @brief Publishes the current state of the 24h format setting.
 * @param enabled True if 24-hour format is enabled, false otherwise.
 */
void publishDisplayFormat(bool enabled) {
    if (!mqttClient.connected()) return;
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    String topic = base_topic + "/24h_format/state";
    mqttClient.publish(topic.c_str(), enabled ? "ON" : "OFF", true);
}

/**
 * @brief Publishes the current state of the time travel sound toggle setting.
 * @param enabled True if sounds are enabled, false otherwise.
 */
void publishSoundToggle(bool enabled) {
    if (!mqttClient.connected()) return;
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    String topic = base_topic + "/sound_toggle/state";
    mqttClient.publish(topic.c_str(), enabled ? "ON" : "OFF", true);
}

/**
 * @brief Publishes the current brightness level.
 * @param brightness The brightness level (0-7).
 */
void publishBrightness(uint8_t brightness) {
    if (!mqttClient.connected()) return;
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    String topic = base_topic + "/brightness/state";
    char payload[4];
    snprintf(payload, sizeof(payload), "%d", brightness);
    mqttClient.publish(topic.c_str(), payload, true);
}

/**
 * @brief Publishes the current animation interval.
 * @param interval The interval in minutes.
 */
void publishAnimationInterval(int interval) {
    if (!mqttClient.connected()) return;
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    String topic = base_topic + "/animation_interval/state";
    char payload[8];
    snprintf(payload, sizeof(payload), "%d", interval);
    mqttClient.publish(topic.c_str(), payload, true);
}

/**
 * @brief Publishes the current stock refresh interval.
 * @param interval The interval in minutes.
 */
void publishStockRefresh(int interval) {
    if (!mqttClient.connected()) return;
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    String topic = base_topic + "/stock_refresh/state";
    char payload[8];
    snprintf(payload, sizeof(payload), "%d", interval);
    mqttClient.publish(topic.c_str(), payload, true);
}

/**
 * @brief Publishes the current weather city.
 * @param city The name of the city.
 */
void publishWeatherCity(const std::string& city) {
    if (!mqttClient.connected()) return;
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    String topic = base_topic + "/weather_city/state";
    mqttClient.publish(topic.c_str(), city.c_str(), true);
}

/**
 * @brief Publishes various diagnostic attributes for the status sensor.
 * @details This includes data like free heap memory, uptime, and Wi-Fi signal strength,
 * which are useful for monitoring the device's health from Home Assistant.
 */
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
void publishMediaPlayerDiscovery();
void publishDisplayModeSelectorDiscovery();
void publishWeatherEntitiesDiscovery();
void publishAudioStatusSensorDiscovery();
void publishRadioSensorsDiscovery();
void publishPresetSelectorDiscovery();


/**
 * @brief Ensures the base device and availability config is present in the discovery document.
 * @details This function is designed to be called before publishing any discovery
 * message. It adds the common 'device' and 'availability' objects to the global
 * discoveryDoc without clearing it, making it safe to use repeatedly.
 */
void ensureBaseDiscoveryConfig() {
    // Set the base topic string, as it's used in the availability topic.
    device_base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;

    // Add or overwrite the 'device' object.
    JsonObject device = discoveryDoc["device"].to<JsonObject>();
    device["identifiers"] = MQTT_UNIQUE_ID;
    device["name"] = "Time Circuits";
    device["model"] = "BTTF Clock v1";
    device["manufacturer"] = "Doc Brown Industries";
    device["sw_version"] = "2.0";
    String ip_addr = WiFi.localIP().toString();
    if (ip_addr != "0.0.0.0") {
        device["configuration_url"] = "http://" + ip_addr + "/";
    }
    device["icon"] = "mdi:car-clock";

    // Add or overwrite the 'availability' object.
    JsonObject availability = discoveryDoc["availability"].to<JsonObject>();
    availability["topic"] = device_base_topic + "/status";
    availability["payload_available"] = "online";
    availability["payload_not_available"] = "offline";
}

/**
 * @brief Prepares the shared JSON document for HA discovery messages.
 */
void prepareHaDiscovery() {
    discoveryDoc.clear();
    ensureBaseDiscoveryConfig();
}

/**
 * @brief Publishes the discovery config for the main device status sensor.
 */
void publishStatusSensorDiscovery() {
    ensureBaseDiscoveryConfig();
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

/**
 * @brief Publishes discovery configs for the 12 text entities that control the display segments.
 * @details This creates a 3x4 grid of text input entities in Home Assistant, giving
 * fine-grained control over what is displayed on each of the 12 segments.
 */
void publishTimeDisplayEntitiesDiscovery() {
    ensureBaseDiscoveryConfig();
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

/**
 * @brief Clears old, deprecated entities from Home Assistant.
 * @details This function is run once during discovery to remove entities that have been
 * replaced by newer ones, preventing clutter in the user's Home Assistant instance.
 */
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

/**
 * @brief Publishes discovery configs for the five "Data Point Enabled" switches.
 */
void publishDataPointSwitchesDiscovery() {
    ensureBaseDiscoveryConfig();
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

/**
 * @brief Publishes discovery configs for the five "Data Point Marquee" text entities.
 */
void publishDataPointMarqueesDiscovery() {
    ensureBaseDiscoveryConfig();
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

/**
 * @brief Clears old, deprecated data point sensor entities.
 */
void cleanupOldDataPointSensors() {
    for (int i=0; i < 5; ++i) {
        String unique_id_suffix = "datapoint_" + String(i);
        clearHaEntity("sensor", unique_id_suffix.c_str());
    }
}

/**
 * @brief Publishes discovery configs for various number input (slider) entities.
 */
void publishNumberConfigsDiscovery() {
    ensureBaseDiscoveryConfig();
    const char* number_configs[][5] = {
        {"animation_interval", "Animation Interval", "mdi:clock-in", "min", "0,120,1"},
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

/**
 * @brief Publishes discovery configs for various switch entities.
 */
void publishSwitchConfigsDiscovery() {
    ensureBaseDiscoveryConfig();
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
    
/**
 * @brief Publishes discovery configs for various button entities.
 */
void publishButtonConfigsDiscovery() {
    ensureBaseDiscoveryConfig();
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

/**
 * @brief Publishes the discovery config for the "Trigger Sequence" button.
 */
void publishSequencerButtonDiscovery() {
    ensureBaseDiscoveryConfig();
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
    
/**
 * @brief Publishes the discovery config for the "Temporal Echo Effect" switch.
 */
void publishTemporalEchoSwitchDiscovery() {
    ensureBaseDiscoveryConfig();
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

/**
 * @brief Publishes the discovery config for the "Profile" selector.
 */
void publishProfileSelectorDiscovery() {
    ensureBaseDiscoveryConfig();
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
    
/**
 * @brief Publishes the discovery config for the "Override Switch".
 */
void publishOverrideSwitchDiscovery() {
    ensureBaseDiscoveryConfig();
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

/**
 * @brief Clears the old, single-line override message entity.
 */
void cleanupOldOverrideMessageEntity() {
    clearHaEntity("text", "override_message");
}

/**
 * @brief Publishes discovery configs for the three override message line text entities.
 */
void publishOverrideLineTextEntitiesDiscovery() {
    ensureBaseDiscoveryConfig();
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

/**
 * @brief Clears old, deprecated entities related to the media player.
 */
void cleanupOldMediaPlayerEntities() {
    clearHaEntity("select", "play_sound");
    clearHaEntity("text", "tts_text");
    clearHaEntity("sensor", "audio_status");
}

/**
 * @brief Publishes the discovery message for the media_player entity.
 * @details This creates a media_player in Home Assistant that can be used to
 * control audio playback on the device, specifically for playing the favorite
 * radio station. It includes the full device object to ensure it's correctly
 * associated with the main Time Circuits device.
 */
void publishMediaPlayerDiscovery() {
    // This is the core fix. By calling this function here, we guarantee that the
    // discovery payload for the media_player will always contain the essential
    // 'device' and 'availability' objects, just like every other entity.
    ensureBaseDiscoveryConfig();

    discoveryDoc["name"] = "Time Circuits Radio";
    String media_player_id = String(MQTT_UNIQUE_ID) + "_media_player";
    discoveryDoc["unique_id"] = media_player_id;
    discoveryDoc["object_id"] = media_player_id;

    // Command topic to receive play/pause/stop commands
    discoveryDoc["command_topic"] = device_base_topic + "/radio/command";
    // State topic to report the current status (playing, paused, idle)
    discoveryDoc["state_topic"] = device_base_topic + "/audio/state";

    // Supported commands
    JsonArray supported_commands = discoveryDoc["supported_features"].to<JsonArray>();
    supported_commands.add("play");
    supported_commands.add("stop");
    supported_commands.add("play_media");
    supported_commands.add("volume_set");
    supported_commands.add("select_source");

    // Mapping HA commands to firmware commands
    discoveryDoc["payload_play"] = "play_favorite_radio";
    discoveryDoc["payload_stop"] = "stop";

    // Define the list of available sources (radio stations)
    JsonArray sources = discoveryDoc["source_list"].to<JsonArray>();
    sources.add("Favorite Radio Station");

    discoveryDoc["icon"] = "mdi:radio";
    discoveryDoc["entity_category"] = "config";

    publishDiscoveryMessage(discoveryDoc, "media_player");

    // Cleanup for the next function. We only remove the keys specific to this entity.
    // The 'device' and 'availability' objects are left untouched for the next function.
    discoveryDoc.remove("name");
    discoveryDoc.remove("unique_id");
    discoveryDoc.remove("object_id");
    discoveryDoc.remove("command_topic");
    discoveryDoc.remove("state_topic");
    discoveryDoc.remove("supported_features");
    discoveryDoc.remove("payload_play");
    discoveryDoc.remove("payload_stop");
    discoveryDoc.remove("source_list");
    discoveryDoc.remove("icon");
    discoveryDoc.remove("entity_category");
}

/**
 * @brief Publishes the discovery config for the "Display Mode" selector.
 */
void publishDisplayModeSelectorDiscovery() {
    ensureBaseDiscoveryConfig();
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

/**
 * @brief Publishes discovery configs for weather-related entities.
 */
void publishWeatherEntitiesDiscovery() {
    ensureBaseDiscoveryConfig();
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

/**
 * @brief Publishes the discovery config for the "Audio Stream Status" sensor.
 */
void publishAudioStatusSensorDiscovery() {
    ensureBaseDiscoveryConfig();
    discoveryDoc["name"] = "Audio Stream Status";
    String audio_status_id = String(MQTT_UNIQUE_ID) + "_audio_status";
    discoveryDoc["unique_id"] = audio_status_id;
    discoveryDoc["object_id"] = audio_status_id;
    discoveryDoc["state_topic"] = device_base_topic + "/audio/state";
    discoveryDoc["icon"] = "mdi:waveform";
    publishDiscoveryMessage(discoveryDoc, "sensor");
    discoveryDoc.remove("name"); discoveryDoc.remove("unique_id"); discoveryDoc.remove("object_id"); discoveryDoc.remove("state_topic"); discoveryDoc.remove("icon");
}

/**
 * @brief Publishes discovery configs for the radio metadata sensors.
 */
void publishRadioSensorsDiscovery() {
    ensureBaseDiscoveryConfig();
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

/**
 * @brief A wrapper function to publish the preset selector discovery.
 */
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
    publishMediaPlayerDiscovery,
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
    // --- FIX: Immediately exit if discovery is not actively running ---
    // This prevents any further processing, including the MQTT connection check below,
    // unless the discovery process is in the specific 'RUNNING' state. This stops
    // the log spam and potential crashes when the MQTT client is disconnected and
    // discovery is idle.
    if (haDiscoveryState != HA_DISCOVERY_RUNNING) {
        return;
    }

    // This is a critical stability fix. The discovery process allocates significant
    // memory for JSON documents. If the client is disconnected, attempting to publish
    // these messages can fail, but the loop continues, leading to rapid memory
    // exhaustion and a device crash (ESP_ERR_NO_MEM).
    if (!mqttClient.connected()) {
        // Log this event, as it indicates a potential issue if it happens frequently.
        Log_printf(LOG_LEVEL_WARN, "HA Discovery: Paused. MQTT client is not connected.");
        // If discovery was running, reset it to idle so it can restart on reconnect.
        haDiscoveryState = HA_DISCOVERY_IDLE;
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

/**
 * @brief Handles the logic for connecting or reconnecting to the MQTT broker.
 * @details This function is called when a connection is needed. It sets the "last will
 * and testament" (LWT) to "offline", attempts the connection with or without credentials,
 * and upon success, subscribes to all necessary command topics and starts the HA
 * discovery process if needed.
 */
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

    // --- FIX: Add a delay before publishing all states ---
    // This gives the client time to process incoming subscription ACK messages from the broker
    // before we flood the outgoing buffer with all of the state messages. This prevents the
    // client from blocking and causing a keep-alive timeout (ERR: 128).
    Log_printf(LOG_LEVEL_INFO, "MQTT: All topics subscribed. Pausing for 1 second before publishing all states...");
    delay(1000);
    mqttClient.loop(); // Process incoming ACKs during the delay

    startHaStatePublishing();
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
        } else if (component == "animation_sequence") {
            AnimationType newSequence = animationTypeFromString(message.c_str());
            if (newSequence != currentSettings.animationSequence) {
                currentSettings.animationSequence = newSequence;
                broadcastWsStateUpdate("animationSequence", message.c_str());
                settingsChanged = true;
            }
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
                // --- FIX: Stop any existing audio before playing a new sound from MQTT ---
                // This ensures that MQTT commands to play a specific sound effect
                // will correctly interrupt any currently playing audio (e.g., radio).
                stopAudioStream(false); // false = This is a permanent stop, not a temporary one.
                playSound((message + ".mp3").c_str(), true, -1);
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
                // Publish the change back to HA to confirm state
                publishDisplayMode(currentSettings.displayMode);

                // --- NEW: Broadcast the change to all connected WebSocket clients ---
                broadcastWsStateUpdate("displayMode", currentSettings.displayMode);

                // Persist the change to NVS
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
            if (message == "stop" || message == "STOP" || message == "PAUSE") {
                Log_printf(LOG_LEVEL_INFO, "Stopping audio stream via MQTT user command.");
                stopAudioStream(false); // false = not a temporary stop
            } else if (message == "PLAY" || message == "play_favorite_radio") {
                Log_printf(LOG_LEVEL_INFO, "MQTT: Play favorite radio command received.");
                if (!currentSettings.favoriteRadioUrl.empty()) {
                    startAudioStream(currentSettings.favoriteRadioUrl.c_str(), false);
                } else {
                    Log_printf(LOG_LEVEL_WARN, "Favorite radio URL is not set. Cannot play.");
                }
            }
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
        startHaStatePublishing();
    }
}

/**
 * @brief Subscribes to a specified MQTT topic.
 * @param topic The topic to subscribe to.
 */
void subscribeToTopic(const std::string& topic) {
    if (mqttClient.connected()) {
        mqttClient.subscribe(topic.c_str());
        Log_printf(LOG_LEVEL_INFO, "MQTT: Subscribed to topic [%s]", topic.c_str());
    }
}

/**
 * @brief Unsubscribes from a specified MQTT topic.
 * @param topic The topic to unsubscribe from.
 */
void unsubscribeFromTopic(const std::string& topic) {
    if (mqttClient.connected()) {
        mqttClient.unsubscribe(topic.c_str());
        Log_printf(LOG_LEVEL_INFO, "MQTT: Unsubscribed from topic [%s]", topic.c_str());
    }
}


// --- START: New Non-Blocking State Publishing Implementation ---

// State variables for the publishing machine
static HaStatePublishState haStatePublishStatus = HA_STATE_PUBLISH_IDLE;
static int haStatePublishIndex = 0;
static unsigned long lastHaStatePublishTime = 0;
const unsigned int HA_STATE_PUBLISH_DELAY = 50; // 50ms delay between each publish chunk

// Forward declarations for the chunked publishing functions
void publishHaStatesChunk1();
void publishHaStatesChunk2();
void publishHaStatesChunk3();
void publishHaStatesChunk4();
void publishHaStatesChunk5();
void publishHaStatesChunk6();

// Array of function pointers to the chunked publishing functions
void (*ha_state_publish_functions[])() = {
    publishHaStatesChunk1,
    publishHaStatesChunk2,
    publishHaStatesChunk3,
    publishHaStatesChunk4,
    publishHaStatesChunk5,
    publishHaStatesChunk6
};
const int num_ha_state_publish_functions = sizeof(ha_state_publish_functions) / sizeof(ha_state_publish_functions[0]);

/**
 * @brief Kicks off the non-blocking state publishing process.
 * @details This function resets the state machine to its initial state, allowing the
 * `handleHaStatePublishing` function to begin sending state updates in chunks. It's
 * safe to call this at any time to re-publish all states.
 */
void startHaStatePublishing() {
    Log_printf(LOG_LEVEL_INFO, "MQTT: Starting non-blocking publication of all HA states.");
    haStatePublishStatus = HA_STATE_PUBLISH_RUNNING;
    haStatePublishIndex = 0;
    lastHaStatePublishTime = 0; // Ensures the first chunk is published immediately
}

/**
 * @brief Manages the non-blocking HA state publishing state machine.
 * @details This function should be called in the main loop. It publishes one chunk of
 * state messages per call, with a configured delay between chunks. This prevents
 * overwhelming the MQTT client's output buffer and starving the network task, which
 * was the cause of the previous watchdog timeouts and crashes.
 */
void handleHaStatePublishing() {
    if (haStatePublishStatus != HA_STATE_PUBLISH_RUNNING) {
        return;
    }

    // Ensure the client is connected before attempting to publish
    if (!mqttClient.connected()) {
        Log_printf(LOG_LEVEL_WARN, "HA State Publishing: Paused. MQTT client is not connected.");
        haStatePublishStatus = HA_STATE_PUBLISH_IDLE; // Reset so it can be restarted
        return;
    }

    if (millis() - lastHaStatePublishTime >= HA_STATE_PUBLISH_DELAY) {
        if (haStatePublishIndex < num_ha_state_publish_functions) {
            Log_printf(LOG_LEVEL_DEBUG, "HA State Publishing: Publishing chunk %d of %d", haStatePublishIndex + 1, num_ha_state_publish_functions);
            ha_state_publish_functions[haStatePublishIndex]();
            haStatePublishIndex++;
            lastHaStatePublishTime = millis();
        } else {
            Log_printf(LOG_LEVEL_INFO, "HA State Publishing: All chunks published successfully.");
            haStatePublishStatus = HA_STATE_PUBLISH_IDLE; // Go back to idle
        }
    }
}


// --- Chunked Publishing Functions ---

void publishHaStatesChunk1() { // Core settings and overrides
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    char payload[20];
    mqttClient.publish((base_topic + "/override_switch/state").c_str(), isMessageOverrideActive ? "ON" : "OFF", true);
    mqttClient.publish((base_topic + "/override_line_1/state").c_str(), overrideMessageLine1.c_str(), true);
    mqttClient.publish((base_topic + "/override_line_2/state").c_str(), overrideMessageLine2.c_str(), true);
    mqttClient.publish((base_topic + "/override_line_3/state").c_str(), overrideMessageLine3.c_str(), true);
    mqttClient.publish((base_topic + "/power/state").c_str(), isDisplayAsleep ? "OFF" : "ON", true);
    itoa(currentSettings.brightness, payload, 10);
    mqttClient.publish((base_topic + "/brightness/state").c_str(), payload, true);
    itoa(currentSettings.notificationVolume, payload, 10);
    mqttClient.publish((base_topic + "/volume/state").c_str(), payload, true);
}

void publishHaStatesChunk2() { // Time and display text (rows 1 & 2)
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    char time_str[6];
    sprintf(time_str, "%02d:%02d", currentSettings.departureHour, currentSettings.departureMinute);
    mqttClient.publish((base_topic + "/sleep_time/state").c_str(), time_str, true);
    sprintf(time_str, "%02d:%02d", currentSettings.arrivalHour, currentSettings.arrivalMinute);
    mqttClient.publish((base_topic + "/wake_time/state").c_str(), time_str, true);

    // Rows 0 and 1
    for(int r=0; r<2; ++r) {
        for(int s=0; s<4; ++s) {
            const char* rows[] = {"dest", "pres", "last"};
            const char* segments[] = {"month", "day", "year", "time"};
            String topic = base_topic + "/" + rows[r] + "_" + segments[s] + "/state";
            mqttClient.publish(topic.c_str(), manualDisplayText[r][s].c_str(), true);
        }
    }
}

void publishHaStatesChunk3() { // Display text (row 3) and various toggles
    char topic_buffer[128]; // Buffer for constructing topic strings
    String base_topic_str = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    const char* base_topic = base_topic_str.c_str();

    // Row 2
    const char* segments[] = {"month", "day", "year", "time"};
    for(int s=0; s<4; ++s) {
        snprintf(topic_buffer, sizeof(topic_buffer), "%s/last_%s/state", base_topic, segments[s]);
        mqttClient.publish(topic_buffer, manualDisplayText[2][s].c_str(), true);
    }

    snprintf(topic_buffer, sizeof(topic_buffer), "%s/sound_toggle/state", base_topic);
    mqttClient.publish(topic_buffer, currentSettings.timeTravelSoundToggle ? "ON" : "OFF", true);

    snprintf(topic_buffer, sizeof(topic_buffer), "%s/is_animating/state", base_topic);
    mqttClient.publish(topic_buffer, isAnimating ? "ON" : "OFF", true);

    snprintf(topic_buffer, sizeof(topic_buffer), "%s/is_asleep/state", base_topic);
    mqttClient.publish(topic_buffer, isDisplayAsleep ? "ON" : "OFF", true);

    snprintf(topic_buffer, sizeof(topic_buffer), "%s/24h_format/state", base_topic);
    mqttClient.publish(topic_buffer, currentSettings.displayFormat24h ? "ON" : "OFF", true);

    snprintf(topic_buffer, sizeof(topic_buffer), "%s/temporal_echo/state", base_topic);
    mqttClient.publish(topic_buffer, isEchoEffectActive ? "ON" : "OFF", true);
}

void publishHaStatesChunk4() { // Intervals, selectors, and status
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    char payload[20];
    itoa(currentSettings.timeTravelAnimationInterval, payload, 10);
    mqttClient.publish((base_topic + "/animation_interval/state").c_str(), payload, true);
    itoa(currentSettings.stockRefreshInterval, payload, 10);
    mqttClient.publish((base_topic + "/stock_refresh/state").c_str(), payload, true);
    
    const char* modes[] = {"Normal Clock", "Stock Ticker", "Weather", "Data Link"};
    if (currentSettings.displayMode >= 0 && currentSettings.displayMode < 4) {
        mqttClient.publish((base_topic + "/display_mode/state").c_str(), modes[currentSettings.displayMode], true);
    }
    mqttClient.publish((base_topic + "/profile/state").c_str(), currentProfileName.c_str(), true);
    mqttClient.publish((base_topic + "/preset_selector/state").c_str(), lastDepartedPreset.c_str(), true);
    publishHaDiagnosticAttributes();
}

void publishHaStatesChunk5() { // Audio and Data Link points 1-3
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    mqttClient.publish((base_topic + "/audio/state").c_str(), audio.isRunning() ? "PLAYING" : "IDLE", true);
    mqttClient.publish((base_topic + "/radio_station_name/state").c_str(), radioStationName.c_str(), true);
    mqttClient.publish((base_topic + "/radio_song_title/state").c_str(), radioSongTitle.c_str(), true);
    mqttClient.publish((base_topic + "/weather_city/state").c_str(), currentSettings.cityName.c_str(), true);

    for(int i=0; i<3; ++i) {
        String enabled_topic = base_topic + "/datapoint_" + String(i) + "_enabled/state";
        mqttClient.publish(enabled_topic.c_str(), currentSettings.dataPoints[i].enabled ? "ON" : "OFF", true);
        String marquee_topic = base_topic + "/datapoint_" + String(i) + "_marquee/state";
        mqttClient.publish(marquee_topic.c_str(), currentSettings.dataPoints[i].scrollingText.c_str(), true);
    }
}

void publishHaStatesChunk6() { // Data Link points 4-5
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    for(int i=3; i<5; ++i) {
        String enabled_topic = base_topic + "/datapoint_" + String(i) + "_enabled/state";
        mqttClient.publish(enabled_topic.c_str(), currentSettings.dataPoints[i].enabled ? "ON" : "OFF", true);
        String marquee_topic = base_topic + "/datapoint_" + String(i) + "_marquee/state";
        mqttClient.publish(marquee_topic.c_str(), currentSettings.dataPoints[i].scrollingText.c_str(), true);
    }
}

/**
 * @brief A wrapper function that calls the new non-blocking state publisher.
 * @details This function is kept for backward compatibility in case any part of the code
 * still calls it directly. It now simply triggers the new non-blocking state machine.
 */
void publishAllHaStates() {
    startHaStatePublishing();
}

// --- END: New Non-Blocking State Publishing Implementation ---

void publishMqttMessage(const std::string& topic, const std::string& payload) {
    if (mqttClient.connected()) {
        mqttClient.publish(topic.c_str(), payload.c_str(), false); // Not retained
        Log_printf(LOG_LEVEL_INFO, "MQTT: Published to topic [%s] with payload [%s]", topic.c_str(), payload.c_str());
    } else {
        Log_printf(LOG_LEVEL_WARN, "MQTT: Cannot publish, client not connected.");
    }
}

/**
 * @brief Processes an incoming animation sequencer command.
 * @details This function is the entry point for all sequencer commands, whether from
 * MQTT or the web UI. It first attempts to parse the payload as a JSON object for
 * custom sequences. If that fails, it treats the payload as the string name of a
 * built-in animation and triggers it accordingly.
 * @param payload The JSON string or name of the sequence to run.
 */
void handleSequencerCommand(const std::string& payload) {
    // --- FIX: Allocate tracks statically to prevent stack overflow ---
    // The SequencerTrack struct is very large (~11.5KB). Allocating an array of them
    // on the stack causes an immediate overflow and crash when this function is called.
    // Making it static moves the allocation to the heap, which is much larger.
    static SequencerTrack tracks[NUM_SEQUENCER_TRACKS];

    // --- FIX: Clear the temporary tracks buffer BEFORE parsing. ---
    // This is a critical stability fix. If `parseSequenceFromJson` fails, this
    // static buffer would otherwise retain the data from the last *successful*
    // sequence generation. This would cause the device to seemingly run the wrong
    // animation (the previous one) when a malformed command is received.
    // By clearing it here, a failed parse results in an empty sequence, which
    // will simply do nothing and time out safely.
    for (int i = 0; i < NUM_SEQUENCER_TRACKS; i++) {
        tracks[i] = SequencerTrack(); // Reset to default state
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error == DeserializationError::Ok) {
        // It's a valid JSON string. Now, determine its structure.
        std::string tracks_payload;
        if (doc.is<JsonArray>()) {
            // This is the correct, preferred format: a direct array of tracks.
            tracks_payload = payload;
            Log_printf(LOG_LEVEL_INFO, "Sequencer: Processing direct JSON payload (Array format).");
        } else if (doc.is<JsonObject>() && !doc["tracks"].isNull() && doc["tracks"].is<JsonArray>()) {
            // This is for backward compatibility with older tools or blueprints that
            // might wrap the array in an object like: {"tracks": [...]}.
            serializeJson(doc["tracks"], tracks_payload);
            Log_printf(LOG_LEVEL_INFO, "Sequencer: Processing direct JSON payload (Object wrapper format).");
        } else {
            // The payload is valid JSON, but not in a structure we can use.
            Log_printf(LOG_LEVEL_ERROR, "Sequencer: JSON payload is not a track array or a {'tracks':...} object. Aborting.");
            return; // Exit without starting any animation.
        }

        // Pause the main display loop
        preAnimationDisplayMode = currentSettings.displayMode;
        currentSettings.displayMode = -1;

        // 1. Generate: Parse the validated and extracted JSON payload.
        parseSequenceFromJson(tracks, tracks_payload);
        Log_printf(LOG_LEVEL_DEBUG, "Sequencer: Parsing of JSON payload complete.");

        // 2. Stop: Halt all currently running animations.
        stopAllSequences();
        Log_printf(LOG_LEVEL_DEBUG, "Sequencer: Stopped all current sequences.");

        // 3. Copy: Transfer the new sequence from the temporary buffer to the main one.
        for (int i = 0; i < NUM_SEQUENCER_TRACKS; i++) {
            sequencerTracks[i] = tracks[i];
        }
        Log_printf(LOG_LEVEL_DEBUG, "Sequencer: Copied new sequence into active tracks.");

        // 4. Activate: Mark the tracks as active so the sequencer will run them.
        for (int i = 0; i < NUM_SEQUENCER_TRACKS; i++) {
            if (sequencerTracks[i].steps[0].command != SEQ_CMD_NONE) {
                sequencerTracks[i].isActive = true;
                sequencerTracks[i].trackStartTime = millis();
                sequencerTracks[i].stepStartTime = millis();
                sequencerTracks[i].originalBrightness = currentSettings.brightness;
                Log_printf(LOG_LEVEL_DEBUG, "Sequencer: Activated track %d.", i);
            }
        }

    } else {
        // It's not JSON, so treat it as a named sequence.
        AnimationType animType = animationTypeFromString(payload.c_str());
        Log_printf(LOG_LEVEL_INFO, "Sequencer: Activating named sequence '%s' (Enum: %d)", payload.c_str(), (int)animType);
        triggerAnimation(animType);
    }
}

/**
 * @brief Publishes a status update to the main device status sensor in Home Assistant.
 * @param status The status string to publish (e.g., "Idle", "Animating").
 */
void updateHaStatus(const char* status) {
	if (!mqttClient.connected()) return;
	String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
	mqttClient.publish((base_topic + "/status/state").c_str(), status, true);
}

void publishRadioMetadata() {
    if (!mqttClient.connected()) return;
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;

    // Publish to Home Assistant topics
    mqttClient.publish((base_topic + "/radio_station_name/state").c_str(), radioStationName.c_str(), true);
    mqttClient.publish((base_topic + "/radio_song_title/state").c_str(), radioSongTitle.c_str(), true);

    // Broadcast to Web UI
    broadcastRadioMetadata(radioStationName.c_str(), radioSongTitle.c_str());
}

/**
 * @brief The callback function that processes events from the audio library.
 * @details This function is registered with the ESP32-audioI2S library. It is called
 * for events like receiving new ICY metadata (song titles), station names, or when
 * a stream ends (EOF).
 * @param m A message struct from the audio library containing the event type and data.
 */
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
/**
 * @brief Centralized function to stop audio playback and reset all related states.
 * @details This function is the single source of truth for halting any audio.
 * It stops the player, powers down the DAC, clears all state variables
 * (isRadioStreaming, metadata, etc.), unregisters callbacks, and notifies all
 * clients (HA and Web UI) that playback has stopped. This ensures the system
 * state is always consistent.
 * @param isPermanent If `true`, the stop is considered permanent (e.g., user-commanded).
 * If `false`, it's temporary (e.g., for TTS), and the radio stream state is preserved.
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

/**
 * @brief Starts playing an audio stream from a URL.
 * @details This function handles requests to play audio, either from an internet radio
 * station or a Text-to-Speech (TTS) service. It stops any currently playing audio,
 * sets the volume, and connects to the specified host.
 * @param url The URL of the audio stream.
 * @param is_tts `true` if the stream is for TTS, which affects state handling.
 * @param volume The volume to play at (0-100), or -1 to use the default setting.
 */
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

/**
 * @brief Stops the current audio stream.
 * @param isTemporary If `true`, the stop is considered temporary (e.g., for TTS),
 * and the radio stream state is preserved. If `false`, it's a permanent stop.
 */
void stopAudioStream(bool isTemporary) {
    Log_printf(LOG_LEVEL_INFO, "Request to stop audio stream (isTemporary: %s)", isTemporary ? "true" : "false");
    // A temporary stop is NOT permanent. A non-temporary stop IS permanent.
    cleanupAudio(!isTemporary);
}

/**
 * @brief Initializes the MQTT client with server and callback information.
 * @details This function is called once during setup. It configures the MQTT client
 * with the broker address and port from settings and registers the main `mqttCallback`
 * function to handle incoming messages. It also increases the client's internal buffer
 * size to handle large Home Assistant discovery payloads.
 */
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