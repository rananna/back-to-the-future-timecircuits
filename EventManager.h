#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include <ArduinoJson.h>
#include <string>
#include <PubSubClient.h>
#include "HardwareControl.h"

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
extern const int MAX_FETCH_FAILURES;
extern bool isMalfunctioning;
extern unsigned long malfunctionStartTime;
extern MalfunctionPhase currentMalfunctionPhase;
extern volatile int requestsCompleted;
extern PubSubClient mqttClient;
extern bool timeSynchronized;
extern int currentPageIndex;

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


#endif // EVENT_MANAGER_H