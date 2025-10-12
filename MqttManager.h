/**
 * @file MqttManager.h
 * @brief Public interface for managing MQTT connectivity and Home Assistant integration.
 * @details This file declares the functions, enums, and global variables necessary for
 * connecting to an MQTT broker, handling incoming messages, and publishing device state.
 * It is the primary bridge between the device and Home Assistant, managing the entire
 * discovery process and subsequent state updates for all entities.
 */
#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <string>
#include "Audio.h"

/**
 * @name State Machine Enumerations
 * @{
 */

/** @brief Defines the possible states for the internet radio player. */
enum RadioStatus {
  RADIO_STATUS_STOPPED,       /**< The radio is not playing. */
  RADIO_STATUS_CONNECTING,    /**< The radio is attempting to connect to a stream. */
  RADIO_STATUS_PLAYING,       /**< The radio is actively playing a stream. */
  RADIO_STATUS_ERROR          /**< An error occurred while trying to play the radio. */
};

/** @brief Defines the states for the Home Assistant discovery process. */
enum HaDiscoveryState {
    HA_DISCOVERY_IDLE,      /**< Discovery has not started or has been reset. */
    HA_DISCOVERY_RUNNING,   /**< Discovery is in progress, publishing config messages. */
    HA_DISCOVERY_COMPLETE   /**< Discovery has finished successfully. */
};
/** @} */

extern HaDiscoveryState haDiscoveryState;       /**< The current state of the HA discovery state machine. */
extern unsigned long lastHaDiscoveryPublish;    /**< `millis()` timestamp of the last HA discovery message sent. */

/**
 * @name MQTT Configuration
 * @{
 */
#define MQTT_BASE_TOPIC "homeassistant"     /**< The base topic for Home Assistant MQTT discovery. */
#define MQTT_DEVICE_TYPE "bttf_time_circuits" /**< The device type identifier, must match the HA integration's DOMAIN. */
extern char MQTT_UNIQUE_ID[21];             /**< The unique identifier for this device, typically derived from the MAC address. */
/** @} */


/**
 * @name Global State Variables
 * @{
 */
extern String currentProfileName;           /**< The name of the currently active settings profile. */
extern String lastDepartedPreset;           /**< The name of the preset used for the "Last Time Departed" display. */
extern bool isRadioStreaming;               /**< A global flag indicating if an internet radio stream is currently active. */
extern int preAnimationDisplayMode;         /**< A global variable to store the display mode before an animation starts, so it can be restored. */
/** @} */


/**
 * @name Core MQTT Functions
 * @{
 */
void mqttCallback(char* topic, unsigned char* payload, unsigned int length);
void setupMqtt();
void reconnectMqtt();
/** @} */


/**
 * @name Home Assistant Integration Functions
 * @brief Functions specifically for managing the Home Assistant discovery and state updates.
 * @{
 */
void startHaDiscovery();
void handleHaDiscovery();
bool isHaDiscoveryComplete();
void updateHaStatus(const char* status);

// --- NEW Non-Blocking State Publishing ---
/**
 * @brief Defines the states for the non-blocking Home Assistant state publishing process.
 */
enum HaStatePublishState {
    HA_STATE_PUBLISH_IDLE,      /**< The state publisher is not running. */
    HA_STATE_PUBLISH_RUNNING,   /**< The state publisher is actively publishing states in batches. */
    HA_STATE_PUBLISH_COMPLETE   /**< The state publisher has finished its run. */
};
void startHaStatePublishing();
void handleHaStatePublishing();
// --- End Non-Blocking State Publishing ---

void publishAllHaStates();
void clearHaEntity(const char* component, const char* unique_id_suffix);
void publishDeviceTriggers();
/** @} */


/**
 * @name State Publishing Functions
 * @brief Functions that publish the state of a specific device setting or sensor to MQTT.
 * @{
 */
void publishDisplayMode(int mode);
void publishDisplayFormat(bool enabled);
void publishSoundToggle(bool enabled);
void publishBrightness(uint8_t brightness);
void publishAnimationDuration(int duration);
void publishStockRefresh(int interval);
void publishWeatherCity(const std::string& city);
void publishTimeSensors();
void publishRadioMetadata();
/** @} */


/**
 * @name Command and Action Handlers
 * @brief Functions that handle commands received via MQTT or other sources.
 * @{
 */
void startAudioStream(const char* url, bool is_tts, int volume = -1);
void stopAudioStream(bool isTemporary = false);
void handleSequencerCommand(const std::string& payload);
void cleanupAudio(bool isPermanent);
/** @} */


/**
 * @name Generic MQTT and Audio Utilities
 * @{
 */
void publishMqttMessage(const std::string& topic, const std::string& payload);
void subscribeToTopic(const std::string& topic);
void unsubscribeFromTopic(const std::string& topic);
void audio_info(Audio::msg_t m);
/** @} */


#endif // MQTT_MANAGER_H