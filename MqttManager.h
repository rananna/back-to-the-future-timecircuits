#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <string>
#include <AudioFileSourceHTTPStream.h>
#include <AudioFileSourceICYStream.h>
#include <AudioGeneratorMP3.h>

extern AudioOutputI2S *out;
extern AudioGeneratorMP3 *audioGenerator;
extern AudioFileSourceHTTPStream *fileSourceHttp;
extern AudioFileSourceICYStream *fileSourceIcy;

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