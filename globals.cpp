#include "globals.h"
#include "config.h"
#include "types.h"
#include <ArduinoJson.h>
#include <vector>
#include <string>

// --- GLOBALS DEFINITIONS ---
// Variables are defined here, and declared as 'extern' in globals.h to be accessible from other files.

// --- NTP Servers ---
const char* NTP_SERVERS[] = {
    "pool.ntp.org",
    "time.nist.gov",
    "ntp.ubuntu.com"
};
const int NUM_NTP_SERVERS = sizeof(NTP_SERVERS) / sizeof(NTP_SERVERS[0]);

// --- Timezone Data ---
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
  {"tzString":"BRT3BRST,M10.3.0/0,M2.3.0/0","name":"Sao Paulo","ianaTzName":"America/Sao_Paulo", "South America"},
  {"tzString":"ART3","name":"Buenos Aires","ianaTzName":"America/Buenos_Aires","region":"South America"},
  {"tzString":"COT5","name":"Bogota","ianaTzName":"America/Bogota","region":"South America"},
  {"tzString":"VET4","name":"Caracas","ianaTzName":"America/Caracas","region":"South America"},
  {"tzString":"CLT4CLST,M9.2.6/24,M4.2.6/24","name":"Santiago","ianaTzName":"America/Santiago","region":"South America"},
  {"tzString":"PET5","name":"Lima","ianaTzName":"America/Lima","region":"South America"},
  {"tzString":"GMT0BST,M3.5.0/1,M10.5.0", "name":"London Time","ianaTzName":"Europe/London","region":"Europe"},
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


// --- DATA & STATE ---
ClockSettings currentSettings;
ClockSettings factorySettings;

bool isAnimating = false;
unsigned long animationStartTime = 0;
AnimationPhase currentPhase = ANIM_INACTIVE;

bool isEchoEffectActive = false;
unsigned long echoEffectStartTime = 0;

bool isDisplayAsleep = false;
bool isMalfunctioning = false;
unsigned long lastGlitchTime = 0;
unsigned long malfunctionStartTime = 0;
MalfunctionPhase currentMalfunctionPhase = MAL_INACTIVE;
bool isGlitching = false;
unsigned long glitchStartTime = 0;

BootSequenceState bootState = BOOT_INACTIVE;
unsigned long bootStateStartTime = 0;

bool hardwareInitialized = false;
String manualDisplayText[3][4];
bool isRowInManualMode[3];

// --- DATA FETCHING ---
volatile int requestsCompleted = 0;
SemaphoreHandle_t xDisplayDataMutex = NULL;
volatile bool isFetchingData = false;
unsigned long lastDataLinkFetch = 0;
StockData stockData[3];
WeatherData currentWeatherData;
std::string lastCityName = "";
DisplayPage displayPages[NUM_PAGES];
DisplayPage lastGoodDisplayPages[NUM_PAGES];
int dataPointFetchFailures[5] = {0};
bool timeSynchronized = false;

// --- AUDIO ---
SemaphoreHandle_t xAudioMutex = NULL;
bool isStreamingRadio = false;
String ttsFile = "";
AudioOutputI2S* out = nullptr;
AudioGeneratorMP3* mp3 = nullptr;
bool isPlayingSound = false;
AudioFileSourceLittleFS* file = nullptr;


// --- MAIN LOOP & WEB SERVER ---
unsigned long lastPresetCycleTime = 0;
AsyncWebServer server(80);
unsigned long bootTimestamp = 0;
unsigned long lastMqttReconnectAttempt = 0;
bool isFlickeringNow = false;
unsigned long lastStockDataFetch = 0;
int currentNtpServerIndex = 0;
Preferences preferences;
Logger logger;
AsyncWebSocket ws("/ws");

// --- MQTT ---
WiFiClient espClient;
PubSubClient mqttClient(espClient);
bool isMessageOverrideActive = false;
String overrideMessageLine1 = "";
String overrideMessageLine2 = "";
String overrideMessageLine3 = "";
String marqueeOverrideMessage = "";
bool isMarqueeOverrideActive = false;
unsigned long marqueeOverrideEndTime = 0;
bool mqttReconnectRequired = false;
bool isSequenceActive = false;
int currentSequenceStep = 0;
unsigned long sequenceStepStartTime = 0;
SequenceStep sequence[20];
int currentPageIndex = 0;