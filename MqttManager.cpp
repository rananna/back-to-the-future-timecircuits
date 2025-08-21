#include "MqttManager.h"
#include "EventManager.h"
#include "AnimationManager.h" // For startTimeTravelAnimation
#include <PubSubClient.h>
#include <ArduinoJson.h>

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

    DynamicJsonDocument doc(1024);
    String topic;
    String payload;
    
    // --- Entity: Status Sensor ---
    doc.clear();
    doc["name"] = "Status";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_status";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_status";
    doc["state_topic"] = device_base_topic + "/status/state";
    doc["icon"] = "mdi:clock-outline";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/sensor/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    // --- Entity: Destination Year (Number Input) ---
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
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/number/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    // --- Entity: Message Override Switch ---
    doc.clear();
    doc["name"] = "Message Override";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_override_switch";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_override_switch";
    doc["command_topic"] = device_base_topic + "/override/command";
    doc["state_topic"] = device_base_topic + "/override/state";
    doc["icon"] = "mdi:message-alert-outline";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/switch/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    // --- Entity: Override Message Text Input ---
    doc.clear();
    doc["name"] = "Override Message";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_override_text";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_override_text";
    doc["command_topic"] = device_base_topic + "/override_text/command";
    doc["state_topic"] = device_base_topic + "/override_text/state";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/text/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
    
    // --- Device Triggers for Automations ---
    doc.clear();
    doc["automation_type"] = "trigger";
    doc["topic"] = device_base_topic + "/events";
    doc["type"] = "animation_started";
    doc["subtype"] = "event";
    doc["device"] = device;
    topic = String(MQTT_BASE_TOPIC) + "/device_automation/" + MQTT_UNIQUE_ID + "/anim_started/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
    
    doc.clear();
    doc["automation_type"] = "trigger";
    doc["topic"] = device_base_topic + "/events";
    doc["type"] = "animation_completed";
    doc["subtype"] = "event";
    doc["device"] = device;
    topic = String(MQTT_BASE_TOPIC) + "/device_automation/" + MQTT_UNIQUE_ID + "/anim_completed/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    // --- HA-MARQUEE: Add Text Entity for Dynamic Marquee Control ---
    doc.clear();
    doc["name"] = "Marquee Message";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_marquee_message";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_marquee_message";
    doc["command_topic"] = device_base_topic + "/marquee/command";
    doc["state_topic"] = device_base_topic + "/marquee/state";
    doc["icon"] = "mdi:sign-text";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/text/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    // --- Entity: Power Switch ---
    doc.clear();
    doc["name"] = "Power";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_power";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_power";
    doc["command_topic"] = device_base_topic + "/power/command";
    doc["state_topic"] = device_base_topic + "/power/state";
    doc["icon"] = "mdi:power";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/switch/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    // --- Entity: Brightness (Number Input) ---
    doc.clear();
    doc["name"] = "Brightness";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_brightness";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_brightness";
    doc["command_topic"] = device_base_topic + "/brightness/command";
    doc["state_topic"] = device_base_topic + "/brightness/state";
    doc["min"] = 0;
    doc["max"] = 7;
    doc["step"] = 1;
    doc["mode"] = "slider";
    doc["icon"] = "mdi:brightness-6";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/number/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);

    // --- Entity: Animation Trigger (Button) ---
    doc.clear();
    doc["name"] = "Trigger Animation";
    doc["unique_id"] = String(MQTT_UNIQUE_ID) + "_trigger_animation";
    doc["object_id"] = String(MQTT_UNIQUE_ID) + "_trigger_animation";
    doc["command_topic"] = device_base_topic + "/animation/command";
    doc["payload_press"] = "START";
    doc["icon"] = "mdi:movie-play-outline";
    doc["device"] = device;
    doc["availability"] = availability;
    topic = String(MQTT_BASE_TOPIC) + "/button/" + doc["object_id"].as<String>() + "/config";
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
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
            haDiscoveryPublished = true;
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
            char* endptr;
            long val = strtol(msg_copy, &endptr, 10);
            if (*endptr == '\0') {
                int brightness = (int)val;
                if (brightness >= 0 && brightness <= 7) {
                    currentSettings.brightness = brightness;
                    stateChanged = true;
                }
            }
        }
        else if (component == "marquee") {
            isMarqueeOverrideActive = (message.length() > 0);
            marqueeOverrideMessage = message;
            stateChanged = true;
        }
        else if (component == "destination_year") {
            char* endptr;
            long val = strtol(msg_copy, &endptr, 10);
            if (*endptr == '\0') {
                int year = (int)val;
                if (year >= 1000 && year <= 9999) {
                    currentSettings.destinationYear = year;
                    stateChanged = true;
                }
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
                // Future logic for handling data point updates
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
}

/**
 * @brief Publishes a status message to the Home Assistant status sensor.
 */
void updateHaStatus(const char* status) {
	if (!mqttClient.connected()) return;
	String base_topic = String(MQTT_DEVICE_TYPE) + "/" + MQTT_UNIQUE_ID;
	mqttClient.publish((base_topic + "/status/state").c_str(), status, true);
}