#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include <ArduinoJson.h>
#include <string>
#include <PubSubClient.h> // <-- FIX IS HERE
#include "HardwareControl.h"

// --- EXTERN DECLARATIONS for global variables in the main .ino file ---
extern ClockSettings currentSettings;
extern MarqueeData displayPages[5];
extern MarqueeData lastGoodDisplayPages[5];
extern WeatherData currentWeatherData;
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
extern PubSubClient mqttClient;
extern bool timeSynchronized;

// --- FUNCTION DECLARATIONS ---

// Animation & Effects
void startTimeTravelAnimation();
void handleDisplayAnimation();
void handleTemporalEcho();
void handleGlitchEffect();
void restoreDisplayAfterGlitch();
void handleMalfunction();

// Display Updates
void updateNormalClockDisplay();
void updateMarqueeDisplay();
void handleWeatherDisplay();

// System & State Handlers
void runBootSequence();
void handleBootSequence();
void handlePresetCycling();
void handleSleepSchedule();

// Data & Networking
void fetchDataLink();
void fetchWeatherData(struct WeatherTaskParams* params);
void fetchWeatherDataTask(void* p);
void forceFetchWeatherDataTask(void* p);
void mqttCallback(char* topic, byte* payload, unsigned int length);
void setupMqtt();
void reconnectMqtt();

// Utility Functions
String urlEncode(const char* msg);
JsonVariant getJsonVariant(JsonVariant root, const char* path);
void showTemporaryMessage(const char* month, const char* day, const char* year, const char* time, int duration);
const char* getIconForWeatherCode(int code);

#endif // EVENT_MANAGER_H