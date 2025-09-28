#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <string>
#include "Audio.h"

enum RadioStatus {
  RADIO_STATUS_STOPPED,
  RADIO_STATUS_CONNECTING,
  RADIO_STATUS_PLAYING,
  RADIO_STATUS_ERROR
};

// --- MQTT Configuration ---
#define MQTT_BASE_TOPIC "homeassistant"
// IMPORTANT: This must match the DOMAIN constant in the Home Assistant integration.
#define MQTT_DEVICE_TYPE "bttf_time_circuits"
extern char MQTT_UNIQUE_ID[21];
extern String currentProfileName;
extern String lastDepartedPreset;
extern bool isRadioStreaming;


void mqttCallback(char* topic, unsigned char* payload, unsigned int length);
void setupMqtt();
void reconnectMqtt();
void publishHaAutoDiscovery();
void updateHaStatus(const char* status);
void publishAllHaStates();
void clearHaEntity(const char* component, const char* unique_id_suffix);
void publishDeviceTriggers();
void publishTimeSensors();
void startAudioStream(const char* url, bool is_tts, int volume = -1);
void stopAudioStream(bool isTemporary = false);
void handleSequencerCommand(const std::string& payload);
void cleanupAudio(bool isPermanent);

// --- Radio Metadata ---
void audio_info(Audio::msg_t m);
void publishRadioMetadata();


#endif // MQTT_MANAGER_H