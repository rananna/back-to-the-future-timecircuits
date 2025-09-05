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
#define BOOT_ANIMATION_FRAME_INTERVAL 50
#define BOOT_POWER_ON_DURATION 250
#define BOOT_SYSTEM_CHECK_DURATION 1000
#define BOOT_TEMPORAL_LOCK_DURATION 3000
#define BOOT_SPEEDOMETER_DURATION 30000
#define BOOT_FADE_DURATION 2000

// THIS IS NOW THE ONLY DEFINITION OF THIS ENUM
enum BootSequenceState {
  BOOT_INACTIVE,
  BOOT_START,
  BOOT_SHOW_SYS_CHECK,
  BOOT_PLAY_HUM_SOUND, 
  BOOT_WAIT_FOR_SOUND,
  BOOT_POWER_ON_DEST,
  BOOT_POWER_ON_PRES,
  BOOT_POWER_ON_LAST,
  BOOT_SYSTEM_CHECK_PHASE1,
  BOOT_SYSTEM_CHECK_PHASE2,
  BOOT_SYSTEM_CHECK_PHASE3,
  BOOT_SPEEDOMETER,
  BOOT_FADE_TO_CLOCK,
  BOOT_COMPLETE
};
// --- END MODIFICATION ---

// --- Function Declarations for animations and effects ---
void startTimeTravelAnimation();
void handleDisplayAnimation();
void handleTemporalEcho();
void handleGlitchEffect();
void restoreDisplayAfterGlitch();
void handleMalfunction();
void runBootSequence();
void handleBootSequence();
void triggerTemporalGlitch();
void handleTemporalGlitch();
void triggerFlashEffect(int row, int segment, int duration = 500);
void handleFlashEffect();

#endif // ANIMATION_MANAGER_H