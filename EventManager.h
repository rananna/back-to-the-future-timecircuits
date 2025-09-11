#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include <ArduinoJson.h>
#include <string>
#include <PubSubClient.h>
#include "SettingsManager.h"
#include "AnimationManager.h"
#include "Audio.h"

// --- EXTERN DECLARATIONS for global variables in the main .ino file ---
extern SettingsManager settingsManager;
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
extern const int MAX_FETCH_FAILURES;
extern bool isMalfunctioning;
extern unsigned long malfunctionStartTime;
extern MalfunctionPhase currentMalfunctionPhase;
extern volatile int requestsCompleted;
extern PubSubClient mqttClient;
extern bool timeSynchronized;
extern int currentPageIndex;
extern char currentSoundFile[MAX_FILENAME_LENGTH];


extern bool isMessageOverrideActive;
extern String overrideMessageLine1;
extern String overrideMessageLine2;
extern String overrideMessageLine3;

extern bool isMarqueeOverrideActive;
extern String marqueeOverrideMessage;
extern unsigned long marqueeOverrideEndTime;

extern const TimeZoneEntry TZ_DATA[];
extern SemaphoreHandle_t xDisplayDataMutex;
extern std::string lastCityName;

// --- START: SEQUENCER GLOBALS ---
extern SequenceStep sequence[20];
extern int currentSequenceStep;
extern unsigned long sequenceStepStartTime;
extern bool isSequenceActive;
// --- END: SEQUENCER GLOBALS ---

// --- NEW GLOBAL AUDIO DECLARATIONS ---
extern Audio audio;
extern bool hardwareInitialized;
// Event types for inter-task communication
enum EventType {
    EVENT_NONE,
    EVENT_WIFI_CONNECTED,
    EVENT_WIFI_DISCONNECTED,
    EVENT_MQTT_RECEIVED,
    EVENT_BUTTON_PRESS,
    EVENT_TIME_SYNC,
    EVENT_SETTINGS_UPDATED
};

// Function Prototypes
void initializeEventManager();
void postEvent(EventType type, const JsonDocument& payload);
void handleEvents();
void playSound(const char* filename);


#endif // EVENT_MANAGER_H