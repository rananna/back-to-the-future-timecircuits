#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <AudioFileSourceLittleFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "types.h"
#include "logger.h"
#include <vector>

// Forward declarations to break circular dependencies
class AsyncWebServer;
class Logger;
class AsyncWebSocket;
class AsyncMqttClient;

// --- AUDIO ---
extern SemaphoreHandle_t xAudioMutex;
extern bool isStreamingRadio;
extern String ttsFile; // Corrected type to String
extern AudioOutputI2S* out;
extern AudioGeneratorMP3* mp3;
extern bool isPlayingSound;
extern AudioFileSourceLittleFS* file;


// --- TIME AND NTP ---
extern bool ntpSyncRequested;
extern bool timeSynchronized;

// --- DISPLAY & ANIMATION ---
extern ClockSettings currentSettings;
extern ClockSettings factorySettings;

extern bool isAnimating;
extern unsigned long animationStartTime;
extern AnimationPhase currentPhase;

extern bool isEchoEffectActive;
extern unsigned long echoEffectStartTime;

extern bool isDisplayAsleep;
extern bool isMalfunctioning;
extern unsigned long lastGlitchTime;
extern unsigned long malfunctionStartTime;
extern MalfunctionPhase currentMalfunctionPhase;
extern bool isGlitching;
extern unsigned long glitchStartTime;

extern BootSequenceState bootState;
extern unsigned long bootStateStartTime;

extern bool hardwareInitialized;
extern String manualDisplayText[3][4];
extern bool isRowInManualMode[3];


// --- DATA ---
extern volatile int requestsCompleted;
extern SemaphoreHandle_t xDisplayDataMutex;
extern volatile bool isFetchingData;
extern unsigned long lastDataLinkFetch;
extern StockData stockData[3];
extern WeatherData currentWeatherData;
extern std::string lastCityName;
extern DisplayPage displayPages[NUM_PAGES];
extern DisplayPage lastGoodDisplayPages[NUM_PAGES];
extern int dataPointFetchFailures[5];

// --- MQTT ---
extern WiFiClient espClient;
extern PubSubClient mqttClient;
extern bool isMessageOverrideActive;
extern String overrideMessageLine1;
extern String overrideMessageLine2;
extern String overrideMessageLine3;
extern String marqueeOverrideMessage;
extern bool isMarqueeOverrideActive;
extern unsigned long marqueeOverrideEndTime;
extern bool mqttReconnectRequired;
extern bool isSequenceActive;
extern int currentSequenceStep;
extern unsigned long sequenceStepStartTime;
extern SequenceStep sequence[20];

// --- WEB & APP ---
extern AsyncWebServer server;
extern unsigned long bootTimestamp;
extern unsigned long lastMqttReconnectAttempt;
extern bool isFlickeringNow;
extern unsigned long lastStockDataFetch;
extern int currentNtpServerIndex;
extern Preferences preferences;
extern unsigned long lastPresetCycleTime;
extern Logger logger;
extern AsyncWebSocket ws;

#endif //GLOBALS_H