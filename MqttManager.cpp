#include "MqttManager.h"
#include "EventManager.h"
#include "AnimationManager.h" // For startTimeTravelAnimation
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>


bool haDiscoveryPublished = false;

/**
 * @brief Configures the MQTT client with broker details from settings.
 */
void setupMqtt() {
  if (currentSettings.mqttBroker.empty()) {
    return;
  }
  mqttClient.setServer(currentSettings.mqttBroker.c_str(), currentSettings.mqttPort);
  mqttClient.setCallback(mqttCallback);
}

/**
 * @brief HA-ERROR-CHECK: Publishes an empty retained message to a config topic to clear it in HA.
 */
void clearHaEntity(const char* component, const char* unique_id_suffix) {
    String object_id = String(MQTT_UNIQUE_ID) + "_" + unique_id_suffix;
    String topic = String(MQTT_BASE_TOPIC) + "/" + component + "/" + object_id + "/config";
    if (mqttClient.connected()) {
        mqttClient.publish(topic.c_str(), "", true);
    }
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

/**
 * @brief HA-ENHANCEMENT: Constructs and publishes all MQTT discovery messages for Home Assistant.
 */
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
    
    // --- Entity: Status Sensor with JSON Attributes ---
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

    // --- Diagnostic Sensors ---
    const char* sensors[][4] = { // CORRECTED: Array size changed from 3 to 4
        {"wifi_rssi", "WiFi Signal", "mdi:wifi", "signal_strength"},
        {"uptime", "Uptime", "mdi:timer-sand", "duration"},
        {"free_heap", "Free Memory", "mdi:memory", ""},
    };

    for (auto const& sensor : sensors) {
        doc.clear();
        doc["name"] = sensor[1];
        doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_" + sensor[0];
        doc["object_id"] = String(MQTT_UNIQUE_ID) + "_" + sensor[0];
        doc["state_topic"] = device_base_topic + "/" + sensor[0] + "/state";
        doc["icon"] = sensor[2];
        doc["entity_category"] = "diagnostic";
        if (strlen(sensor[3]) > 0) {
            doc["device_class"] = sensor[3];
        }
        doc["device"] = device;
        doc["availability"] = availability;
        topic = String(MQTT_BASE_TOPIC) + "/sensor/" + doc["object_id"].as<String>() + "/config";
        serializeJson(doc, payload);
        mqttClient.publish(topic.c_str(), payload.c_str(), true);
    }
    
    // --- Timestamp Sensors ---
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


    // --- Configuration Controls ---
    doc.clear();
    doc["name"] = "Destination Year";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_dest_year";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_dest_year";
    doc["command_topic"] = device_base_topic + "/destination_year/command";
    doc["state_topic"] = device_base_topic + "/destination_year/state";
    doc["min"] = 1000;
    doc["max"] = 9999;
    doc["step"] = 1;
    doc["mode"] = "box";
    doc["icon"] = "mdi:calendar-arrow-right";
    doc["entity_category"] = "config";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/number/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    doc.clear();
    doc["name"] = "Animation Style";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_anim_style";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_anim_style";
    doc["command_topic"] = device_base_topic + "/animation_style/command";
    doc["state_topic"] = device_base_topic + "/animation_style/state";
    JsonArray styles = doc.createNestedArray("options");
    styles.add("Sequential Flicker");
    styles.add("Random Flicker");
    styles.add("All Displays Random");
    styles.add("Counting Up");
    styles.add("Wave Flicker");
    styles.add("Tornado Flicker");
    styles.add("Capacitor Charge-Up");
    styles.add("Digital Rain");
    styles.add("Waveform Collapse");
    styles.add("Timeline Skim");
    doc["icon"] = "mdi:animation-play";
    doc["entity_category"] = "config";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/select/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    doc.clear();
    doc["name"] = "Glitch Instability";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_glitch_freq";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_glitch_freq";
    doc["command_topic"] = device_base_topic + "/glitch_freq/command";
    doc["state_topic"] = device_base_topic + "/glitch_freq/state";
    doc["min"] = 0;
    doc["max"] = 100;
    doc["step"] = 1;
    doc["unit_of_measurement"] = "%";
    doc["icon"] = "mdi:flash-alert-outline";
    doc["entity_category"] = "config";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/number/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    doc.clear();
    doc["name"] = "Malfunction Chance";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_malfunction_chance";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_malfunction_chance";
    doc["command_topic"] = device_base_topic + "/malfunction_chance/command";
    doc["state_topic"] = device_base_topic + "/malfunction_chance/state";
    doc["min"] = 1;
    doc["max"] = 200;
    doc["step"] = 1;
    doc["icon"] = "mdi:alert-outline";
    doc["entity_category"] = "config";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/number/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    doc.clear();
    doc["name"] = "Volume";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_volume";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_volume";
    doc["command_topic"] = device_base_topic + "/volume/command";
    doc["state_topic"] = device_base_topic + "/volume/state";
    doc["min"] = 0;
    doc["max"] = 30;
    doc["step"] = 1;
    doc["icon"] = "mdi:volume-high";
    doc["entity_category"] = "config";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/number/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
    
    // --- Data Link Entities ---
    for (int i = 0; i < 5; i++) {
        String dp_id = "datapoint_" + String(i);
        doc.clear();
        doc["name"] = "Data Point " + String(i + 1) + " Marquee";
        doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_" + dp_id + "_marquee";
        doc["object_id"] = String(MQTT_UNIQUE_ID) + "_" + dp_id + "_marquee";
        doc["command_topic"] = device_base_topic + "/" + dp_id + "/marquee/command";
        doc["state_topic"] = device_base_topic + "/" + dp_id + "/marquee/state";
        doc["icon"] = "mdi:text-box-outline";
        doc["entity_category"] = "config";
        doc["device"] = device;
        doc["availability"] = availability;
        topic = String(MQTT_BASE_TOPIC) + "/text/" + doc["object_id"].as<String>() + "/config";
        serializeJson(doc, payload);
        mqttClient.publish(topic.c_str(), payload.c_str(), true);

        doc.clear();
        doc["name"] = "Data Point " + String(i + 1) + " Enabled";
        doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_" + dp_id + "_enabled";
        doc["object_id"] = String(MQTT_UNIQUE_ID) + "_" + dp_id + "_enabled";
        doc["command_topic"] = device_base_topic + "/" + dp_id + "/enabled/command";
        doc["state_topic"] = device_base_topic + "/" + dp_id + "/enabled/state";
        doc["icon"] = "mdi:toggle-switch-outline";
        doc["entity_category"] = "config";
        doc["device"] = device;
        doc["availability"] = availability;
        topic = String(MQTT_BASE_TOPIC) + "/switch/" + doc["object_id"].as<String>() + "/config";
        serializeJson(doc, payload);
        mqttClient.publish(topic.c_str(), payload.c_str(), true);
    }


    publishDeviceTriggers();

    haDiscoveryPublished = true;
}

/**
 * @brief Attempts to reconnect to the MQTT broker if the connection is lost.
 */
void reconnectMqtt() {
  if (currentSettings.mqttBroker.empty()) return;
  if (!mqttClient.connected()) {
    String clientId = "BTTF-Clock-";
    clientId += String(random(0xffff), HEX);
    String availability_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/status";
    
    if (mqttClient.connect(clientId.c_str(), currentSettings.mqttUser.c_str(), currentSettings.mqttPassword.c_str(), availability_topic.c_str(), 1, true, "offline")) {
        mqttClient.publish(availability_topic.c_str(), "online", true);
        
        if (!haDiscoveryPublished) {
            publishHaAutoDiscovery();
        }
        
        publishAllHaStates();

        String command_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/+/command";
        mqttClient.subscribe(command_topic.c_str());

        for (int i = 0; i < currentSettings.numDataPoints; i++) {
          if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_MQTT && !currentSettings.dataPoints[i].mqttTopic.empty()) {
            mqttClient.subscribe(currentSettings.dataPoints[i].mqttTopic.c_str());
          }
        }
    }
  }
}

/**
 * @brief Callback function that is executed when an MQTT message is received.
 */
void mqttCallback(char* topic, unsigned char* payload, unsigned int length) {
    String message = "";
    message.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    String topicStr = String(topic);
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID + "/";
    bool stateChanged = false;
    
    if (topicStr.endsWith("/command")) {
        String component_topic = topicStr.substring(base_topic.length());
        String component = component_topic.substring(0, component_topic.indexOf('/'));

        char msg_copy[length + 1];
        strncpy(msg_copy, (char*)payload, length);
        msg_copy[length] = '\0';
        
        if (component == "power") {
            if (message == "ON") isDisplayAsleep = false;
            else if (message == "OFF") isDisplayAsleep = true;
            stateChanged = true;
        } 
        else if (component == "brightness") {
            int brightness = message.toInt();
            if (brightness >= 0 && brightness <= 7) {
                currentSettings.brightness = brightness;
                stateChanged = true;
            }
        }
        else if (component.startsWith("datapoint_")) {
            int dp_index = component.substring(10).toInt();
            if (dp_index >= 0 && dp_index < 5) {
                if (topicStr.endsWith("marquee/command")) {
                    currentSettings.dataPoints[dp_index].scrollingText = message.c_str();
                    stateChanged = true;
                } else if (topicStr.endsWith("enabled/command")) {
                    // This is a placeholder for the logic to enable/disable a datapoint
                    // You would need to add a boolean flag to the DataPoint struct
                    // currentSettings.dataPoints[dp_index].enabled = (message == "ON");
                    stateChanged = true;
                }
            }
        }
        else if (component == "destination_year") {
            int year = message.toInt();
            if (year >= 1000 && year <= 9999) {
                currentSettings.destinationYear = year;
                stateChanged = true;
            }
        }
        else if (component == "animation_style") {
            if (message == "Sequential Flicker") currentSettings.animationStyle = 0;
            else if (message == "Random Flicker") currentSettings.animationStyle = 1;
            else if (message == "All Displays Random") currentSettings.animationStyle = 2;
            else if (message == "Counting Up") currentSettings.animationStyle = 3;
            else if (message == "Wave Flicker") currentSettings.animationStyle = 4;
            else if (message == "Tornado Flicker") currentSettings.animationStyle = 5;
            else if (message == "Capacitor Charge-Up") currentSettings.animationStyle = 6;
            else if (message == "Digital Rain") currentSettings.animationStyle = 7;
            else if (message == "Waveform Collapse") currentSettings.animationStyle = 8;
            else if (message == "Timeline Skim") currentSettings.animationStyle = 9;
            stateChanged = true;
        }
        else if (component == "glitch_freq") {
            int freq = message.toInt();
            if (freq >= 0 && freq <= 100) {
                currentSettings.glitchEffectFrequency = freq;
                stateChanged = true;
            }
        }
        else if (component == "malfunction_chance") {
            int chance = message.toInt();
            if (chance >= 1 && chance <= 200) {
                currentSettings.malfunctionFrequency = chance;
                stateChanged = true;
            }
        }
        else if (component == "volume") {
            int vol = message.toInt();
            if (vol >= 0 && vol <= 30) {
                currentSettings.notificationVolume = vol;
                #if ENABLE_HARDWARE
                myDFPlayer.volume(vol);
                #endif
                stateChanged = true;
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
    }
    else {
        for (int i = 0; i < currentSettings.numDataPoints; i++) {
            if (currentSettings.dataPoints[i].dataSourceType == DATA_SOURCE_MQTT && topicStr == currentSettings.dataPoints[i].mqttTopic.c_str()) {
                if (xSemaphoreTake(xDisplayDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    displayPages[i].time = message.c_str();
                    xSemaphoreGive(xDisplayDataMutex);
                }
                break;
            }
        }
    }
    if (stateChanged) {
        publishAllHaStates();
    }
}

/**
 * @brief Publishes the current state of all Home Assistant entities to the MQTT broker.
 */
void publishAllHaStates() {
    if (!mqttClient.connected()) return;
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    char payload[20];

    // Publish simple states
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

    // Publish animation style
    const char* styles[] = {"Sequential Flicker", "Random Flicker", "All Displays Random", "Counting Up", "Wave Flicker", "Tornado Flicker", "Capacitor Charge-Up", "Digital Rain", "Waveform Collapse", "Timeline Skim"};
    if (currentSettings.animationStyle >= 0 && currentSettings.animationStyle < 10) {
        mqttClient.publish((base_topic + "/animation_style/state").c_str(), styles[currentSettings.animationStyle], true);
    }
    
    // Publish diagnostic sensors
    itoa(WiFi.RSSI(), payload, 10);
    mqttClient.publish((base_topic + "/wifi_rssi/state").c_str(), payload, true);

    itoa(esp_get_free_heap_size(), payload, 10);
    mqttClient.publish((base_topic + "/free_heap/state").c_str(), payload, true);
    
    itoa(millis() / 1000, payload, 10);
    mqttClient.publish((base_topic + "/uptime/state").c_str(), payload, true);


    // Publish JSON Attributes for the main status sensor
    DynamicJsonDocument doc(512);
    doc["is_animating"] = isAnimating;
    doc["is_asleep"] = isDisplayAsleep;
    doc["is_glitching"] = isGlitching;
    doc["is_malfunctioning"] = isMalfunctioning;
    if (currentSettings.animationStyle >= 0 && currentSettings.animationStyle < 10) {
        doc["animation_style"] = styles[currentSettings.animationStyle];
    }
    String attributes_payload;
    serializeJson(doc, attributes_payload);
    mqttClient.publish((base_topic + "/status/attributes").c_str(), attributes_payload.c_str(), true);

    publishTimeSensors();
}

void publishTimeSensors() {
    if (!mqttClient.connected() || !timeSynchronized) return;
    String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
    char iso_time[25];

    // Present Time
    time_t now;
    time(&now);
    strftime(iso_time, sizeof(iso_time), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    mqttClient.publish((base_topic + "/present_time/state").c_str(), iso_time, true);

    // Destination Time
    struct tm dest_tm;
    localtime_r(&now, &dest_tm);
    dest_tm.tm_year = currentSettings.destinationYear - 1900;
    time_t dest_time = mktime(&dest_tm);
    strftime(iso_time, sizeof(iso_time), "%Y-%m-%dT%H:%M:%SZ", gmtime(&dest_time));
    mqttClient.publish((base_topic + "/destination_time/state").c_str(), iso_time, true);

    // Last Time Departed
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


/**
 * @brief Publishes a status message to the Home Assistant status sensor.
 */
void updateHaStatus(const char* status) {
	if (!mqttClient.connected()) return;
	String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
	mqttClient.publish((base_topic + "/status/state").c_str(), status, true);
}