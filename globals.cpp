#include "config.h"
#include "globals.h"
#include "types.h"
#include "logger.h"

// --- AUDIO ---
SemaphoreHandle_t xAudioMutex = xSemaphoreCreateMutex();
bool isStreamingRadio = false;
String ttsFile = "";
AudioOutputI2S* out = NULL;
AudioGeneratorMP3* mp3 = NULL;
bool isPlayingSound = false;
AudioFileSourceLittleFS* file = NULL;

// --- TIME AND NTP ---
bool ntpSyncRequested = false;
bool timeSynchronized = false;

// --- DISPLAY & ANIMATION ---
ClockSettings currentSettings;
ClockSettings factorySettings;

bool isAnimating = false;
unsigned long animationStartTime = 0;
AnimationPhase currentPhase = ANIMATION_IDLE;

bool isEchoEffectActive = false;
unsigned long echoEffectStartTime = 0;

bool isDisplayAsleep = false;
bool isMalfunctioning = false;
unsigned long lastGlitchTime = 0;
unsigned long malfunctionStartTime = 0;
MalfunctionPhase currentMalfunctionPhase = MALFUNCTION_IDLE;
bool isGlitching = false;
unsigned long glitchStartTime = 0;

BootSequenceState bootState = BOOT_SEQUENCE_IDLE;
unsigned long bootStateStartTime = 0;

bool hardwareInitialized = false;
String manualDisplayText[3][4];
bool isRowInManualMode[3] = {false, false, false};

// --- DATA ---
volatile int requestsCompleted = 0;
SemaphoreHandle_t xDisplayDataMutex = xSemaphoreCreateMutex();
volatile bool isFetchingData = false;
unsigned long lastDataLinkFetch = 0;
StockData stockData[3];
WeatherData currentWeatherData;
std::string lastCityName = "";
DisplayPage displayPages[NUM_PAGES]; // Added definition
DisplayPage lastGoodDisplayPages[NUM_PAGES]; // Added definition
int dataPointFetchFailures[5] = {0, 0, 0, 0, 0};
int currentPageIndex = 0; // Added definition

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

// --- WEB & APP ---
AsyncWebServer server(80);
unsigned long bootTimestamp = 0;
unsigned long lastMqttReconnectAttempt = 0;
bool isFlickeringNow = false;
unsigned long lastStockDataFetch = 0;
int currentNtpServerIndex = 0;
Preferences preferences;
unsigned long lastPresetCycleTime = 0;
Logger logger;
AsyncWebSocket ws("/ws");

// Added definitions for marquee scrolling
int marqueeScrollPosition = 0;
int marqueeScrollPositionYear = 0;
unsigned long lastMarqueeStateChange = 0;

const char* NTP_SERVERS[] = {
    "pool.ntp.org",
    "time.nist.gov",
    "time.google.com"
};

const int NUM_NTP_SERVERS = sizeof(NTP_SERVERS) / sizeof(NTP_SERVERS[0]);

const TimeZoneEntry TZ_DATA[] = {
    {"UTC", "UTC0"},
    {"EST5EDT", "EST5EDT,M3.2.0,M11.1.0"},
    {"CST6CDT", "CST6CDT,M3.2.0,M11.1.0"},
    {"MST7MDT", "MST7MDT,M3.2.0,M11.1.0"},
    {"PST8PDT", "PST8PDT,M3.2.0,M11.1.0"},
    {"JST", "JST-9"},
    {"CET", "CET-1CEST,M3.5.0,M10.5.0/3"}
};

const int NUM_TIMEZONE_OPTIONS = sizeof(TZ_DATA) / sizeof(TZ_DATA[0]);

const char TZ_JSON[] PROGMEM = R"(
[{"key":"UTC", "value":"UTC0"},
{"key":"EST5EDT", "value":"EST5EDT,M3.2.0,M11.1.0"},
{"key":"CST6CDT", "value":"CST6CDT,M3.2.0,M11.1.0"},
{"key":"MST7MDT", "value":"MST7MDT,M3.2.0,M11.1.0"},
{"key":"PST8PDT", "value":"PST8PDT,M3.2.0,M11.1.0"},
{"key":"JST", "value":"JST-9"},
{"key":"CET", "value":"CET-1CEST,M3.5.0,M10.5.0/3"}]
)";