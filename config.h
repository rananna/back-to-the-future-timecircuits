#ifndef CONFIG_H
#define CONFIG_H

// Hardware Configuration
#define ENABLE_HARDWARE 1

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

// NTP Servers
const char* NTP_SERVERS[] = {
    "pool.ntp.org",
    "time.nist.gov",
    "ntp.ubuntu.com"
};
const int NUM_NTP_SERVERS = sizeof(NTP_SERVERS) / sizeof(NTP_SERVERS[0]);


// Timezone Data
const TimeZoneEntry TZ_DATA[] = {
  // North America
  { "Eastern Time", "EST5EDT,M3.2.0,M11.1.0", "America/New_York", "North America" },
  { "Central Time", "CST6CDT,M3.2.0,M11.1.0", "America/Chicago", "North America" },
  { "Mountain Time", "MST7MDT,M3.2.0,M11.1.0", "America/Denver", "North America" },
  { "Pacific Time", "PST8PDT,M3.2.0,M11.1.0", "America/Los_Angeles", "North America" },
  { "Alaska Time", "AKST9AKDT,M3.2.0,M11.1.0", "America/Anchorage", "North America" },
  { "Hawaii Time", "HST10", "Pacific/Honolulu", "North America" },
  { "Atlantic Time (Canada)", "AST4ADT,M3.2.0,M11.1.0", "America/Halifax", "North America" },
  { "Newfoundland Time", "NST3:30NDT,M3.2.0,M11.1.0", "America/St_Johns", "North America" },
  { "Mexico City", "CST6CDT,M4.1.0,M10.5.0", "America/Mexico_City", "North America" },
  { "Vancouver", "PST8PDT,M3.2.0,M11.1.0", "America/Vancouver", "North America" },
  
  // South America
  { "Sao Paulo", "BRT3BRST,M10.3.0/0,M2.3.0/0", "America/Sao_Paulo", "South America" },
  { "Buenos Aires", "ART3", "America/Buenos_Aires", "South America" },
  { "Bogota", "COT5", "America/Bogota", "South America" },
  { "Caracas", "VET4", "America/Caracas", "South America" },
  { "Santiago", "CLT4CLST,M9.2.6/24,M4.2.6/24", "America/Santiago", "South America" },
  { "Lima", "PET5", "America/Lima", "South America" },
  
  // Europe
  { "London Time", "GMT0BST,M3.5.0/1,M10.5.0", "Europe/London", "Europe" },
  { "Paris Time", "CET-1CEST,M3.5.0,M10.5.0/3", "Europe/Paris", "Europe" },
  { "Moscow Time", "MSK-3", "Europe/Moscow", "Europe" },
  { "Rome Time", "CET-1CEST,M3.5.0,M10.5.0/3", "Europe/Rome", "Europe" },
  { "Madrid Time", "CET-1CEST,M3.5.0,M10.5.0/3", "Europe/Madrid", "Europe" },
  { "Berlin Time", "CET-1CEST,M3.5.0,M10.5.0/3", "Europe/Berlin", "Europe" },
  { "Athens Time", "EET-2EEST,M3.5.0/3,M10.5.0/4", "Europe/Athens", "Europe" },

  // Asia
  { "Tokyo Time", "JST-9", "Asia/Tokyo", "Asia" },
  { "Shanghai", "CST-8", "Asia/Shanghai", "Asia" },
  { "Dubai", "GST-4", "Asia/Dubai", "Asia" },
  { "Seoul", "KST-9", "Asia/Seoul", "Asia" },
  { "Jerusalem", "IST-2IDT,M3.4.4/26,M10.5.0", "Asia/Jerusalem", "Asia" },
  { "Kolkata", "IST-5:30", "Asia/Kolkata", "Asia" },

  // Australia/Oceania
  { "Sydney Time", "AEST-10AEDT,M10.1.0,M4.1.0/3", "Australia/Sydney", "Australia" },
  { "Melbourne", "AEST-10AEDT,M10.1.0,M4.1.0/3", "Australia/Melbourne", "Australia" },
  { "Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3", "Pacific/Auckland", "Oceania" },
  { "Fiji", "FJT-12FJST,M11.1.0,M1.2.0", "Pacific/Fiji", "Oceania" },
  { "Brisbane", "AEST-10", "Australia/Brisbane", "Australia" },
  { "Perth", "AWST-8", "Australia/Perth", "Australia" }
};
const int NUM_TIMEZONE_OPTIONS = sizeof(TZ_DATA) / sizeof(TZ_DATA[0]);

const char TZ_JSON[] PROGMEM = R"=====(
[
  {"tzString":"EST5EDT,M3.2.0,M11.1.0","name":"Eastern Time","ianaTzName":"America/New_York","region":"North America"},
  {"tzString":"CST6CDT,M3.2.0,M11.1.0","name":"Central Time","ianaTzName":"America/Chicago","region":"North America"},
  {"tzString":"MST7MDT,M3.2.0,M11.1.0","name":"Mountain Time","ianaTzName":"America/Denver","region":"North America"},
  {"tzString":"PST8PDT,M3.2.0,M11.1.0","name":"Pacific Time","ianaTzName":"America/Los_Angeles","region":"North America"},
  {"tzString":"AKST9AKDT,M3.2.0,M11.1.0","name":"Alaska Time","ianaTzName":"America/Anchorage","region":"North America"},
  {"tzString":"HST10","name":"Hawaii Time","ianaTzName":"Pacific/Honolulu","region":"North America"},
  {"tzString":"AST4ADT,M3.2.0,M11.1.0","name":"Atlantic Time (Canada)","ianaTzName":"America/Halifax","region":"North America"},
  {"tzString":"NST3:30NDT,M3.2.0,M11.1.0","name":"Newfoundland Time","ianaTzName":"America/St_Johns","region":"North America"},
  {"tzString":"CST6CDT,M4.1.0,M10.5.0","name":"Mexico City","ianaTzName":"America/Mexico_City","region":"North America"},
  {"tzString":"PST8PDT,M3.2.0,M11.1.0","name":"Vancouver","ianaTzName":"America/Vancouver","region":"North America"},
  {"tzString":"BRT3BRST,M10.3.0/0,M2.3.0/0","name":"Sao Paulo","ianaTzName":"America/Sao_Paulo","region":"South America"},
  {"tzString":"ART3","name":"Buenos Aires","ianaTzName":"America/Buenos_Aires","region":"South America"},
  {"tzString":"COT5","name":"Bogota","ianaTzName":"America/Bogota","region":"South America"},
  {"tzString":"VET4","name":"Caracas","ianaTzName":"America/Caracas","region":"South America"},
  {"tzString":"CLT4CLST,M9.2.6/24,M4.2.6/24","name":"Santiago","ianaTzName":"America/Santiago","region":"South America"},
  {"tzString":"PET5","name":"Lima","ianaTzName":"America/Lima","region":"South America"},
  {"tzString":"GMT0BST,M3.5.0/1,M10.5.0","name":"London Time","ianaTzName":"Europe/London","region":"Europe"},
  {"tzString":"CET-1CEST,M3.5.0,M10.5.0/3","name":"Paris Time","ianaTzName":"Europe/Paris","region":"Europe"},
  {"tzString":"MSK-3","name":"Moscow Time","ianaTzName":"Europe/Moscow","region":"Europe"},
  {"tzString":"CET-1CEST,M3.5.0,M10.5.0/3","name":"Rome Time","ianaTzName":"Europe/Rome","region":"Europe"},
  {"tzString":"CET-1CEST,M3.5.0,M10.5.0/3","name":"Madrid Time","ianaTzName":"Europe/Madrid","region":"Europe"},
  {"tzString":"CET-1CEST,M3.5.0,M10.5.0/3","name":"Berlin Time","ianaTzName":"Europe/Berlin","region":"Europe"},
  {"tzString":"EET-2EEST,M3.5.0/3,M10.5.0/4","name":"Athens Time","ianaTzName":"Europe/Athens","region":"Europe"},
  {"tzString":"JST-9","name":"Tokyo Time","ianaTzName":"Asia/Tokyo","region":"Asia"},
  {"tzString":"CST-8","name":"Shanghai","ianaTzName":"Asia/Shanghai","region":"Asia"},
  {"tzString":"GST-4","name":"Dubai","ianaTzName":"Asia/Dubai","region":"Asia"},
  {"tzString":"KST-9","name":"Seoul","ianaTzName":"Asia/Seoul","region":"Asia"},
  {"tzString":"IST-2IDT,M3.4.4/26,M10.5.0","name":"Jerusalem","ianaTzName":"Asia/Jerusalem","region":"Asia"},
  {"tzString":"IST-5:30","name":"Kolkata","ianaTzName":"Asia/Kolkata","region":"Asia"},
  {"tzString":"AEST-10AEDT,M10.1.0,M4.1.0/3","name":"Sydney Time","ianaTzName":"Australia/Sydney","region":"Australia"},
  {"tzString":"AEST-10AEDT,M10.1.0,M4.1.0/3","name":"Melbourne","ianaTzName":"Australia/Melbourne","region":"Australia"},
  {"tzString":"NZST-12NZDT,M9.5.0,M4.1.0/3","name":"Auckland","ianaTzName":"Pacific/Auckland","region":"Oceania"},
  {"tzString":"FJT-12FJST,M11.1.0,M1.2.0","name":"Fiji","ianaTzName":"Pacific/Fiji","region":"Oceania"},
  {"tzString":"AEST-10","name":"Brisbane","ianaTzName":"Australia/Brisbane","region":"Australia"},
  {"tzString":"AWST-8","name":"Perth","ianaTzName":"Australia/Perth","region":"Australia"}
]
)=====";

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