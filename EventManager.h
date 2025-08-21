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
extern const int MAX_FETCH_FAILURES;
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
extern SemaphoreHandle_t xDisplayDataMutex;
extern std::string lastCityName;

#endif // EVENT_MANAGER_H