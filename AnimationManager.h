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

// --- Function Declarations for animations and effects ---

/**
 * @brief Initiates the multi-stage time travel animation sequence.
 * @details Sets the system state to 'Animating', plays the initial sound effects,
 * and kicks off the animation state machine handled by handleDisplayAnimation().
 */
void startTimeTravelAnimation();

/**
 * @brief Main state machine for the time travel animation. Called continuously in the main loop.
 * @details Progresses through the phases of the animation (power up, acceleration, arrival, landing)
 * based on elapsed time. This function should be called on every iteration of the main loop.
 */
void handleDisplayAnimation();

/**
 * @brief Handles the "temporal echo" visual effect after a time travel sequence.
 * @details For a short period after an animation, this function creates a subtle, random
 * flickering on the "Present Time" display to simulate residual temporal energy.
 */
void handleTemporalEcho();

/**
 * @brief Handles the random glitch and malfunction effects during normal operation.
 * @details Periodically checks if a random visual glitch or a more dramatic system
 * malfunction sequence should be triggered based on user-configured frequencies.
 */
void handleGlitchEffect();

/**
 * @brief Restores the display to its normal state after a brief glitch effect has completed.
 */
void restoreDisplayAfterGlitch();

/**
 * @brief Main state machine for the system malfunction effect.
 * @details Controls the multi-stage visual sequence for a major system "failure," including
 * haywire flickering, displaying an error message, and simulating a reboot.
 */
void handleMalfunction();

/**
 * @brief Starts the boot-up animation sequence.
 * @details Typically called once at startup to provide a cinematic power-on effect.
 */
void runBootSequence();

/**
 * @brief Main state machine for the boot sequence animation. Called in the main loop.
 * @details Progresses through the boot-up visual phases.
 */
void handleBootSequence();

/**
 * @brief Triggers a brief, intense flicker effect, typically upon the first successful NTP time sync.
 */
void triggerTemporalGlitch();

/**
 * @brief Handles the visual part of the temporal glitch effect after it has been triggered.
 */
void handleTemporalGlitch();

/**
 * @brief Triggers a temporary flashing effect on a specific display segment.
 * @param row The target display row (0: Destination, 1: Present, 2: Last Departed).
 * @param segment The target display segment (0: Month, 1: Day, 2: Year, 3: Time).
 * @param duration The duration of the flash effect in milliseconds.
 */
void triggerFlashEffect(int row, int segment, int duration = 500);

/**
 * @brief Manages the state of active flash effects, turning them off after their duration expires.
 * @details This function should be called on every iteration of the main loop.
 */
void handleFlashEffect();


#endif // ANIMATION_MANAGER_H