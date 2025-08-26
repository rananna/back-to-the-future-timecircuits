#include "globals.h"
#include <AudioFileSourceLittleFS.h>
#include <AudioOutputI2S.h>
#include <AudioGeneratorMP3.h>

// --- GLOBAL VARIABLE DEFINITIONS ---

// Audio
AudioFileSourceLittleFS *file = nullptr;
AudioOutputI2S *out = nullptr;
AudioGeneratorMP3 *mp3 = nullptr;
bool isPlayingSound = false;
String ttsFile = "";
SemaphoreHandle_t xAudioMutex;
bool isStreamingRadio = false;

// Settings & Data
ClockSettings currentSettings;
MarqueeData displayPages[5];
MarqueeData lastGoodDisplayPages[5];
WeatherData currentWeatherData;
StockData stockData[3];
unsigned long lastStockDataFetch = 0;
std::string lastCityName = "";
unsigned long bootTimestamp = 0;
bool hardwareInitialized = false;

// Time
bool timeSynchronized = false;
bool ntpSyncRequested = false;
int currentNtpServerIndex = 0;

// Network & Services
AsyncWebServer server(80);
Preferences preferences;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long lastMqttReconnectAttempt = 0;
bool mqttReconnectRequired = false;

// Animation & Display State
bool isAnimating = false;
unsigned long animationStartTime = 0;
unsigned long lastAnimationFrameTime = 0;
AnimationPhase currentPhase = ANIM_INACTIVE;
bool isDisplayAsleep = false;
BootSequenceState bootState = BOOT_INACTIVE;
unsigned long bootStateStartTime = 0;
unsigned long lastGlitchTime = 0;
bool isGlitching = false;
unsigned long glitchStartTime = 0;
unsigned long lastPresetCycleTime = 0;
bool isEchoEffectActive = false;
unsigned long echoEffectStartTime = 0;
unsigned long lastEchoCheckTime = 0;
bool isFlickeringNow = false;
unsigned long flickerStartTime = 0;
int flickerDisplayIndex = -1;
bool isMarqueeOverrideActive = false;
String marqueeOverrideMessage = "";
unsigned long marqueeOverrideEndTime = 0;

// Marquee & DataLink
MarqueeState marqueeState = M_IDLE;
unsigned long lastDataLinkFetch = 0;
unsigned long lastMarqueeStateChange = 0;
int marqueeScrollPosition = 0;
int marqueeScrollPositionYear = 0;
volatile bool isFetchingData = false;
int dataPointFetchFailures[5] = {0, 0, 0, 0, 0};
bool isMalfunctioning = false;
unsigned long malfunctionStartTime = 0;
MalfunctionPhase currentMalfunctionPhase = MAL_INACTIVE;
volatile int requestsCompleted = 0;
int currentPageIndex = 0;
std::string manualDisplayText[3][4];
bool isRowInManualMode[3] = {false, false, false};


// Message Overrides
bool isMessageOverrideActive = false;
String overrideMessageLine1 = "";
String overrideMessageLine2 = "";
String overrideMessageLine3 = "";

// Mutex
SemaphoreHandle_t xDisplayDataMutex;

// Sequencer
SequenceStep sequence[20];
int currentSequenceStep = 0;
unsigned long sequenceStepStartTime = 0;
bool isSequenceActive = false;

// Logging
Logger logger;
bool clientLoggingEnabled = false;


// --- CONSTANT DEFINITIONS ---
const TimeZoneEntry TZ_DATA[] = {
	{ "UTC0", "UTC", "Etc/UTC", "Global" },
	{ "NST3:30NDT,M3.2.0,M11.1.0", "Newfoundland (St. John's)", "America/St_Johns", "Americas" },
	{ "AST4ADT,M3.2.0,M11.1.0", "Atlantic (Halifax)", "America/Halifax", "Americas" },
	{ "EST5EDT,M3.2.0,M11.1.0", "Eastern (New York)", "America/New_York", "Americas" },
	{ "CST6CDT,M3.2.0,M11.1.0", "Central (Chicago)", "America/Chicago", "Americas" },
	{ "MST7MDT,M3.2.0,M11.1.0", "Mountain (Denver)", "America/Denver", "Americas" },
	{ "PST8PDT,M3.2.0,M11.1.0", "Pacific (Los Angeles)", "America/Los_Angeles", "Americas" },
	{ "AKST9AKDT,M3.2.0,M11.1.0", "Alaska (Anchorage)", "America/Anchorage", "Americas" },
	{ "MST7", "Mountain (Phoenix, No DST)", "America/Phoenix", "Americas" },
	{ "HST10", "Hawaii (Honolulu, No DST)", "Pacific/Honolulu", "Americas" },
	{ "GMT0BST,M3.5.0/1,M10.5.0", "GMT/BST (London)", "Europe" },
	{ "CET-1CEST,M3.5.0,M10.5.0", "CET/CEST (Berlin)", "Europe" },
	{ "EET-2EEST,M3.5.0/3,M10.5.0/4", "EET/EEST (Athens)", "Europe" },
	{ "<+03>-3", "Moscow Standard Time", "Europe" },
	{ "<+03>-3", "Turkey Time (Istanbul)", "Europe" },
	{ "IST-5:30", "Indian Standard Time (Kolkata)", "Asia" },
	{ "<+08>-8", "Singapore Standard Time", "Asia" },
	{ "CST-8", "China Standard Time (Shanghai)", "Asia" },
	{ "KST-9", "Korea Standard Time (Seoul)", "Asia" },
	{ "JST-9", "Japan Standard Time (Tokyo)", "Asia" },
	{ "<+04>-4", "Gulf Standard Time (Dubai)", "Asia" },
	{ "AWST-8", "AWST (Perth)", "Australia & Oceania" },
	{ "AEST-10AEDT,M10.1.0,M4.1.0/3", "AEST/AEDT (Sydney)", "Australia & Oceania" },
	{ "NZST-12NZDT,M9.5.0,M4.1.0/3", "NZST/NZDT (Auckland)", "Australia & Oceania" },
	{ "ChST-10", "Chamorro Time (Guam)", "Australia & Oceania" },
	{ "WAT-1", "West Africa Time (Lagos)", "Africa" },
	{ "SAST-2", "South Africa Standard Time", "Africa" },
	{ "EET-2", "EET (Cairo)", "Africa" },
	{ "EAT-3", "East Africa Time (Nairobi)", "Africa" },
	{ "<-03>3", "Brasilia Time (Sao Paulo)", "South America" },
	{ "<-03>3", "Argentina Time (Buenos Aires)", "South America" }
};
const int NUM_TIMEZONE_OPTIONS = sizeof(TZ_DATA) / sizeof(TZ_DATA[0]);

const char TZ_JSON[] PROGMEM = "{\"Global\":[{\"value\":0,\"text\":\"UTC\",\"ianaTzName\":\"Etc/UTC\"}],\"Americas\":[{\"value\":1,\"text\":\"Newfoundland (St. John's)\",\"ianaTzName\":\"America/St_Johns\"},{\"value\":2,\"text\":\"Atlantic (Halifax)\",\"ianaTzName\":\"America/Halifax\"},{\"value\":3,\"text\":\"Eastern (New York)\",\"ianaTzName\":\"America/New_York\"},{\"value\":4,\"text\":\"Central (Chicago)\",\"ianaTzName\":\"America/Chicago\"},{\"value\":5,\"text\":\"Mountain (Denver)\",\"ianaTzName\":\"America/Denver\"},{\"value\":6,\"text\":\"Pacific (Los Angeles)\",\"ianaTzName\":\"America/Los_Angeles\"},{\"value\":7,\"text\":\"Alaska (Anchorage)\",\"ianaTzName\":\"America/Anchorage\"},{\"value\":8,\"text\":\"Mountain (Phoenix, No DST)\",\"ianaTzName\":\"America/Phoenix\"},{\"value\":9,\"text\":\"Hawaii (Honolulu, No DST)\",\"ianaTzName\":\"Pacific/Honolulu\"}],\"Europe\":[{\"value\":10,\"text\":\"GMT/BST (London)\",\"ianaTzName\":\"Europe/London\"},{\"value\":11,\"text\":\"CET/CEST (Berlin)\",\"ianaTzName\":\"Europe/Berlin\"},{\"value\":12,\"text\":\"EET/EEST (Athens)\",\"ianaTzName\":\"Europe/Athens\"},{\"value\":13,\"text\":\"Moscow Standard Time\",\"ianaTzName\":\"Europe/Moscow\"},{\"value\":14,\"text\":\"Turkey Time (Istanbul)\",\"ianaTzName\":\"Europe/Istanbul\"}],\"Asia\":[{\"value\":15,\"text\":\"Indian Standard Time (Kolkata)\",\"ianaTzName\":\"Asia/Kolkata\"},{\"value\":16,\"text\":\"Singapore Standard Time\",\"ianaTzName\":\"Asia/Singapore\"},{\"value\":17,\"text\":\"China Standard Time (Shanghai)\",\"ianaTzName\":\"Asia/Shanghai\"},{\"value\":18,\"text\":\"Korea Standard Time (Seoul)\",\"ianaTzName\":\"Asia/Seoul\"},{\"value\":19,\"text\":\"Japan Standard Time (Tokyo)\",\"ianaTzName\":\"Asia/Tokyo\"},{\"value\":20,\"text\":\"Gulf Standard Time (Dubai)\",\"ianaTzName\":\"Asia/Dubai\"}],\"Australia & Oceania\":[{\"value\":21,\"text\":\"AWST (Perth)\",\"ianaTzName\":\"Australia/Perth\"},{\"value\":23,\"text\":\"NZST-12NZDT,M9.5.0,M4.1.0/3\", \"text\":\"NZST/NZDT (Auckland)\",\"ianaTzName\":\"Pacific/Auckland\"},{\"value\":22,\"text\":\"AEST-10AEDT,M10.1.0,M4.1.0/3\", \"text\":\"AEST/AEDT (Sydney)\",\"ianaTzName\":\"Australia/Sydney\"},{\"value\":24,\"text\":\"Chamorro Time (Guam)\",\"ianaTzName\":\"Pacific/Guam\"}],\"Africa\":[{\"value\":25,\"text\":\"West Africa Time (Lagos)\",\"ianaTzName\":\"Africa/Lagos\"},{\"value\":26,\"text\":\"South Africa Standard Time\",\"ianaTzName\":\"Africa/Johannesburg\"},{\"value\":27,\"text\":\"EET (Cairo)\",\"ianaTzName\":\"Africa/Cairo\"},{\"value\":28,\"text\":\"East Africa Time (Nairobi)\",\"ianaTzName\":\"Africa/Nairobi\"}],\"South America\":[{\"value\":29,\"text\":\"Brasilia Time (Sao Paulo)\",\"ianaTzName\":\"America/Sao_Paulo\"},{\"value\":30,\"text\":\"Argentina Time (Buenos Aires)\",\"ianaTzName\":\"America/Argentina/Buenos_Aires\"}]}";

const char *NTP_SERVERS[] = { "pool.ntp.org", "time.google.com", "time.nist.gov" };
const int NUM_NTP_SERVERS = sizeof(NTP_SERVERS) / sizeof(NTP_SERVERS[0]);