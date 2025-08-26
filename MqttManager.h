#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <PubSubClient.h>

// Function prototypes
void setupMqtt();
void handleMqtt();
void reconnectMqtt();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishAllHaStates();
void updateHaStatus(const char* status);
void publishTimeSensors();
void playTtsFromUrl(const char* url);
void playRadioStream(const char* url);
void stopRadioStream();

#endif // MQTT_MANAGER_H