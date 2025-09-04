#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <string>

// --- MQTT Configuration ---
#define MQTT_BASE_TOPIC "homeassistant"
#define MQTT_DEVICE_TYPE "timecircuits"
extern char MQTT_UNIQUE_ID[19];


void mqttCallback(char* topic, unsigned char* payload, unsigned int length);
void setupMqtt();
void reconnectMqtt();
void publishHaAutoDiscovery();
void updateHaStatus(const char* status);
void publishAllHaStates();
void clearHaEntity(const char* component, const char* unique_id_suffix);
void publishDeviceTriggers();
void publishTimeSensors();
void startAudioStream(const char* url, bool is_tts);
void stopAudioStream();


#endif // MQTT_MANAGER_H