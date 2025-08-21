#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h> 
#include <string>

// Corrected 'byte*' to 'unsigned char*' to match the library's expectation
void mqttCallback(char* topic, unsigned char* payload, unsigned int length);
void setupMqtt();
void reconnectMqtt();
void publishHaAutoDiscovery();
void updateHaStatus(const char* status);
void publishAllHaStates();
void clearHaEntity(const char* component, const char* unique_id_suffix);

#endif // MQTT_MANAGER_H