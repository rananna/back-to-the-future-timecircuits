#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include <ArduinoJson.h>
#include <string>
#include <PubSubClient.h>
#include "HardwareControl.h"

// HA-ENHANCEMENT: Moved definitions to the header for global visibility.
#define MQTT_UNIQUE_ID "bttf_timecircuits_01"
#define MQTT_DEVICE_TYPE "bttf-clock"
#define MQTT_BASE_TOPIC "homeassistant"

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
extern int currentPageIndex;

// HA-ENHANCEMENT: Extern declarations for new override state
extern bool isMessageOverrideActive;
extern String overrideMessageLine1;
extern String overrideMessageLine2;
extern String overrideMessageLine3;

// HA-MARQUEE: Extern declarations for the dynamic marquee override.
extern bool isMarqueeOverrideActive;
extern String marqueeOverrideMessage;


// ADDED: Extern declaration for the Time Zone data array to make it visible to this file.
extern const TimeZoneEntry TZ_DATA[];

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
void displayOverrideMessage();
// HA-MARQUEE: New function to display the marquee override message.
void displayMarqueeOverride();


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

// --- Home Assistant Integration Functions ---
void publishHaAutoDiscovery();
void updateHaStatus(const char* status);
void publishAllHaStates();
// HA-ERROR-CHECK: New function prototype for the HA entity cleanup tool.
void clearHaEntity(const char* component, const char* unique_id_suffix);


// Utility Functions
String urlEncode(const char* msg);
JsonVariant getJsonVariant(JsonVariant root, const char* path);
void showTemporaryMessage(const char* month, const char* day, const char* year, const char* time, int duration);
const char* getIconForWeatherCode(int code);

#endif // EVENT_MANAGER_H