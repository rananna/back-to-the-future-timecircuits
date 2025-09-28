/**
 * @file AnimationManager.h
 * @brief Manages all visual animations and special effects for the display.
 * @details This module contains the state machines and handlers for complex visual sequences
 * such as the time travel animation, boot sequence, and random glitch effects. It is designed
 * to be non-blocking to ensure smooth visual performance.
 */

#ifndef ANIMATION_MANAGER_H
#define ANIMATION_MANAGER_H
#include "HardwareControl.h"

// --- MODIFIED: New constants and states for the cinematic boot sequence ---
#define BOOT_AWAIT_HUM_DURATION 10000
#define BOOT_WARM_UP_DURATION 1000
#define BOOT_COLD_START_DURATION 5000
#define BOOT_FLUX_CAPACITOR_IGNITION_DURATION 10000
#define BOOT_DIAGNOSTICS_DURATION 10000
#define BOOT_FINAL_CHECKS_DURATION 15000
#define BOOT_TEMPORAL_DISPLACEMENT_DURATION 6000
#define BOOT_ARRIVAL_DURATION 5000
#define BOOT_COOL_DOWN_DURATION 2000


// THIS IS NOW THE ONLY DEFINITION OF THIS ENUM
enum BootSequenceState {
  BOOT_INACTIVE,
  BOOT_AWAIT_HUM,
  BOOT_START,
  BOOT_WARM_UP,
  BOOT_COLD_START,
  BOOT_FLUX_CAPACITOR_IGNITION,
  BOOT_FLUX_CAPACITOR_ANIMATION, // <-- FIX: New state added for animation
  BOOT_DIAGNOSTICS,
  BOOT_FINAL_CHECKS,
  BOOT_TEMPORAL_DISPLACEMENT,
  BOOT_ARRIVAL,
  BOOT_ARRIVAL_ANIMATION,
  BOOT_COOL_DOWN,
  BOOT_COMPLETE
};
// --- END MODIFICATION ---

#include "freertos/semphr.h"

// Externally declared mutex for controlling animation start
extern SemaphoreHandle_t xAnimationStartMutex;

// --- Function Declarations for animations and effects ---
void startTimeTravelAnimation();
void handleDisplayAnimation();
void startStyledAnimation();
void handleStyledAnimation();
void handleTemporalEcho();
void runBootSequence();
void handleBootSequence();
void triggerFlashEffect(int row, int segment, int duration = 500);
void handleFlashEffect();
void broadcastAnimationComplete();
void startPulseEffect(int row, int segment, int duration);
void handlePulseEffect();
void startFadeEffect(int duration, bool isFadeIn);
void handleFadeEffect();

// --- START: NEW Sequencer Data Structures ---
// Maximum number of steps in a single sequence track
#define MAX_SEQUENCE_STEPS 20

// Defines the available commands for the sequencer
enum SequenceCommand {
    SEQ_CMD_NONE,
    SEQ_CMD_WAIT,
    SEQ_CMD_MARQUEE,
    SEQ_CMD_FADE_IN,
    SEQ_CMD_FADE_OUT,
    SEQ_CMD_PULSE,
    SEQ_CMD_FLASH,
    SEQ_CMD_SOUND,
    SEQ_CMD_END // Marks the end of a sequence
};

// Represents a single step in a sequence
struct SequenceStep {
    SequenceCommand command;
    int targetRow;
    int targetSegment;
    int intParam;
    String stringParam;
};

// Represents a single track of commands for one display row
struct SequencerTrack {
    bool isActive;
    int currentStep;
    unsigned long stepStartTime;
    bool stepInitialized;

    // --- NEW: Local state for marquee commands ---
    bool isMarqueeActive = false;
    std::string marqueeText;
    int marqueeScrollPosition = 0;
    unsigned long lastMarqueeScrollTime = 0;

    // --- NEW: Local state for fade, pulse, and flash effects ---
    bool isFading = false;
    unsigned long fadeStartTime = 0;
    int fadeDuration = 0;
    bool isFadeIn = false;
    uint8_t originalBrightness = 0;

    // Pulse and flash states are per-segment (4 segments per row)
    bool isPulsing[4] = {false};
    unsigned long pulseEndTimes[4] = {0};
    bool pulseStates[4] = {false};
    unsigned long lastPulseToggle[4] = {0};

    bool isFlashing[4] = {false};
    unsigned long flashEndTimes[4] = {0};
    bool flashStates[4] = {false};
    unsigned long lastFlashToggle[4] = {0};

    SequenceStep steps[MAX_SEQUENCE_STEPS];
};

// Global array of sequencer tracks, one for each of the 3 display rows
extern SequencerTrack sequencerTracks[3];

// Function declaration for the new sequencer handler
void handleSequencer();

// --- NEW: Function declaration for the startup test ---
void runSequencerTest();
// --- END: NEW Sequencer Data Structures ---

#endif // ANIMATION_MANAGER_H