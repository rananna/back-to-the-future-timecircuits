#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include <ArduinoJson.h>
#include <string>
#include <PubSubClient.h>
#include "HardwareControl.h"
#include "DisplayManager.h"
#include "AnimationManager.h"
#include "Audio.h"

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
extern volatile bool isFetchingWeather;
extern int dataPointFetchFailures[5];
extern const int MAX_FETCH_FAILURES;
extern volatile int requestsCompleted;
extern PubSubClient mqttClient;
extern bool timeSynchronized;
extern int currentPageIndex;
extern char currentSoundFile[MAX_FILENAME_LENGTH];


extern bool isMessageOverrideActive;
extern String overrideMessageLine1;
extern String overrideMessageLine2;
extern String overrideMessageLine3;

extern SemaphoreHandle_t xDisplayDataMutex;
extern std::string lastCityName;

// --- START: SEQUENCER GLOBALS ---
extern SequencerTrack sequencerTracks[3]; // One track for each display row
extern bool isFading;
extern bool isSequencerMarqueeActive;
// --- END: SEQUENCER GLOBALS ---

// --- NEW GLOBAL AUDIO DECLARATIONS ---
extern Audio audio;
extern bool hardwareInitialized;
// Function Prototypes



#endif // EVENT_MANAGER_H