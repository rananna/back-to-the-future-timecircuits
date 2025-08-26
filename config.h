#ifndef CONFIG_H
#define CONFIG_H

// MQTT Settings
#define MQTT_UNIQUE_ID "bttf_time_circuits"
#define MQTT_BASE_TOPIC "homeassistant"
#define MQTT_DEVICE_TYPE "bttf_tc"

// Animation Styles
#define ANIMATION_TIMELINE_SKIM 5

// Sequence Commands
#define SEQ_CMD_TEXT 1
#define SEQ_CMD_WAIT 2
#define SEQ_CMD_SOUND 3
#define SEQ_CMD_FLASH 4
#define SEQ_CMD_END 5

// Preferences Namespace
#define PREFERENCES_NAMESPACE "bttf-config"

// Hardware Pins
#define I2S_BCLK_PIN 26
#define I2S_LRC_PIN 25
#define I2S_DIN_PIN 22
#define I2S_SD_PIN 5

// Theme Definitions
#define THEME_TIME_CIRCUITS 0
#define THEME_OUTATIME 1
#define THEME_88MPH 2
#define THEME_PLUTONIUM_GLOW 3
#define THEME_MR_FUSION 4
#define THEME_CLOCK_TOWER 5

#define THEME_PREF_KEY "theme"

// Forward declarations for global variables
extern bool ntpSyncRequested;
extern int currentSequenceStep;

// Forward declarations for functions
void updateDisplaySegment(int row, int segment, const char* text);
void fetchWeatherDataTask(void* parameter);
void playSound(const char* filename);
void startTimeTravelAnimation();
void broadcastWsStateUpdate(const String& key, const String& value);

#endif  // CONFIG_H