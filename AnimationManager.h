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
void handleGlitchEffect();
void restoreDisplayAfterGlitch();
void runBootSequence();
void handleBootSequence();
void triggerTemporalGlitch();
void handleTemporalGlitch();
void triggerFlashEffect(int row, int segment, int duration = 500);
void handleFlashEffect();
void broadcastAnimationComplete();

#endif // ANIMATION_MANAGER_H