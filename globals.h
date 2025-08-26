#ifndef GLOBALS_H
#define GLOBALS_H

#include <WiFi.h>
#include "types.h"
#include <Preferences.h>
#include "logger.h"
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h> // For AsyncWebServer
#include <AudioFileSourceLittleFS.h>
#include <AudioOutputI2S.h>
#include <AudioGeneratorMP3.h>

extern Preferences preferences;
extern Logger logger;
extern WiFiClient espClient;
extern PubSubClient mqttClient;
extern AsyncWebServer server;
extern AudioFileSourceLittleFS *file;
extern AudioOutputI2S *out;
extern AudioGeneratorMP3 *mp3;

// --- GLOBAL VARIABLE EXTERN DECLARATIONS ---
extern bool isPlayingSound;
extern String ttsFile;
extern SemaphoreHandle_t xAudioMutex;
extern bool isStreamingRadio;

extern ClockSettings currentSettings;
extern MarqueeData displayPages[5];
extern MarqueeData lastGoodDisplayPages[5];
extern WeatherData currentWeatherData;
extern StockData stockData[3];
extern unsigned long lastStockDataFetch;
extern std::string lastCityName;
extern unsigned long bootTimestamp;
extern bool hardwareInitialized;
extern bool timeSynchronized;
extern bool ntpSyncRequested;
extern int currentNtpServerIndex;
extern unsigned long lastMqttReconnectAttempt;
extern bool mqttReconnectRequired;
extern bool isAnimating;
extern unsigned long animationStartTime;
extern unsigned long lastAnimationFrameTime;
extern AnimationPhase currentPhase;
extern bool isDisplayAsleep;
extern BootSequenceState bootState;
extern unsigned long bootStateStartTime;
extern unsigned long lastGlitchTime;
extern bool isGlitching;
extern unsigned long glitchStartTime;
extern unsigned long lastPresetCycleTime;
extern bool isEchoEffectActive;
extern unsigned long echoEffectStartTime;
extern unsigned long lastEchoCheckTime;
extern bool isFlickeringNow;
extern unsigned long flickerStartTime;
extern int flickerDisplayIndex;
extern MarqueeState marqueeState;
extern unsigned long lastDataLinkFetch;
extern unsigned long lastMarqueeStateChange;
extern int marqueeScrollPosition;
extern int marqueeScrollPositionYear;
extern volatile bool isFetchingData;
extern int dataPointFetchFailures[5];
extern bool isMalfunctioning;
extern unsigned long malfunctionStartTime;
extern MalfunctionPhase currentMalfunctionPhase;
extern volatile int requestsCompleted;
extern int currentPageIndex;
extern bool isMessageOverrideActive;
extern String overrideMessageLine1;
extern String overrideMessageLine2;
extern String overrideMessageLine3;
extern bool isMarqueeOverrideActive;
extern String marqueeOverrideMessage;
extern unsigned long marqueeOverrideEndTime;
extern SemaphoreHandle_t xDisplayDataMutex;
extern SequenceStep sequence[20];
extern int currentSequenceStep;
extern unsigned long sequenceStepStartTime;
extern bool isSequenceActive;
extern std::string manualDisplayText[3][4];
extern bool isRowInManualMode[3];

// --- CONSTANT EXTERN DECLARATIONS ---
extern const TimeZoneEntry TZ_DATA[];
extern const int NUM_TIMEZONE_OPTIONS;
extern const char TZ_JSON[] PROGMEM;
extern const char *NTP_SERVERS[];
extern const int NUM_NTP_SERVERS;

#endif // GLOBALS_H