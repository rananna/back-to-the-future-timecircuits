/**
 * @file AnimationManager.h
 * @brief Public interface for managing all visual animations, the boot sequence, and the animation sequencer.
 * @details This file declares the functions, enums, and constants required to control the device's
 * visual effects. It manages the state for the cinematic boot sequence, provides entry points
 * for triggering animations, and declares the core handlers for the multi-track animation sequencer.
 * It also defines global flags and mutexes for managing system state and concurrency.
 */
#ifndef ANIMATION_MANAGER_H
#define ANIMATION_MANAGER_H

#include "AnimationSequences.h"
#include "HardwareControl.h"

/**
 * @name Boot Sequence Constants
 * @brief Defines the duration for each major phase of the cinematic boot sequence.
 * @{
 */
#define BOOT_AWAIT_HUM_DURATION 10000                   /**< Duration (ms) for the initial humming sound before visuals start. */
#define BOOT_FLUX_CAPACITOR_IGNITION_DURATION 10000     /**< Duration (ms) for the flux capacitor ignition phase. */
#define BOOT_DIAGNOSTICS_DURATION 8000                  /**< Duration (ms) for the system diagnostics display phase. */
#define BOOT_FINAL_CHECKS_DURATION 15000                /**< Duration (ms) for the final checks and keypad entry phase. */
#define BOOT_TEMPORAL_DISPLACEMENT_DURATION 6000        /**< Duration (ms) for the time travel displacement effect. */
#define BOOT_ARRIVAL_DURATION 5000                      /**< Duration (ms) for the arrival sequence. */
#define BOOT_COOL_DOWN_DURATION 2000                    /**< Duration (ms) for the final cool-down phase before normal operation. */
/** @} */

/**
 * @brief Defines the states for the cinematic boot sequence state machine.
 * @details This enum controls the flow of the multi-stage boot animation, ensuring
 * sounds and visuals are synchronized correctly.
 */
enum BootSequenceState {
  BOOT_INACTIVE,                  /**< The boot sequence is not running. */
  BOOT_AWAIT_HUM,                 /**< Waiting for the initial hum sound to play. */
  BOOT_START,                     /**< The first visual state, activating the system. */
  BOOT_SYSTEM_ACTIVATE,           /**< "TIME CIRCUITS ON" text is displayed. */
  BOOT_FLUX_CAPACITOR_IGNITION,   /**< Flux capacitor sound and initial flicker effects. */
  BOOT_FLUX_CAPACITOR_ANIMATION,  /**< Main flux capacitor visual animation. */
  BOOT_DIAGNOSTICS,               /**< System status (CPU, MEM, WIFI) is displayed. */
  BOOT_FINAL_CHECKS,              /**< Simulates keypad entry and final system checks. */
  BOOT_TEMPORAL_DISPLACEMENT,     /**< The main time travel visual effect. */
  BOOT_ARRIVAL,                   /**< Arrival chime and display stabilization. */
  BOOT_ARRIVAL_ANIMATION,         /**< Post-arrival visual effects. */
  BOOT_COOL_DOWN,                 /**< Final stabilization before handing off to normal operation. */
  BOOT_COMPLETE                   /**< The boot sequence has finished successfully. */
};

#include "freertos/semphr.h"

/**
 * @brief A FreeRTOS mutex to prevent multiple tasks from starting animations simultaneously.
 * @details This ensures that a new animation cannot start while another is in the process of
 * being initialized, preventing race conditions.
 */
extern SemaphoreHandle_t xAnimationStartMutex;

/**
 * @brief A global flag indicating whether the boot sequence has completed.
 * @details This is critical for preventing normal operations (like clock updates) from
 * running before the system is fully initialized and the boot animation is finished.
 */
extern bool bootSequenceCompleted;

/**
 * @brief A volatile flag to signal that an animation has just finished.
 * @details This helps prevent race conditions where the main loop might try to render a normal
 * display state in the same cycle that an animation is cleaning up, which could cause
 * visual artifacts. The main loop checks this flag and can skip a render cycle to let the
 * system stabilize.
 */
extern volatile bool justFinishedAnimation;

/**
 * @brief Stores the display mode that was active before an animation started.
 * @details This is used by the cleanup functions to restore the correct display
 * (e.g., Clock, Weather, Stocks) after an animation sequence completes.
 */
extern int preAnimationDisplayMode;

/**
 * @brief Stores the `AnimationType` of the currently running sequence.
 * @details Used primarily for logging and debugging to identify which animation is active.
 */
extern AnimationType currentAnimationType;


/**
 * @name Legacy Animation and Effect Functions
 * @brief Functions for older, non-sequencer-based animations.
 * @{
 */
void startTimeTravelAnimation();
void handleDisplayAnimation();
void handleTemporalEcho();
/** @} */

/**
 * @name Boot Sequence Functions
 * @{
 */
void runBootSequence();
void handleBootSequence();
/** @} */

/**
 * @name General Animation Utilities
 * @{
 */
void triggerFlashEffect(int row, int segment, int duration = 500);
void broadcastAnimationComplete();
/** @} */

#include "Sequencer.h"

/**
 * @brief Global array of sequencer tracks, one for each of the 3 display rows.
 * @details This is the core data structure for the animation sequencer, holding the state
 * and command lists for all concurrently running animations.
 */
extern SequencerTrack sequencerTracks[3];

/**
 * @name Animation Sequencer Core Functions
 * @brief The main functions for controlling the animation sequencer.
 * @{
 */

/**
 * @brief The main handler for the animation sequencer, called repeatedly from the main loop.
 * @details This function iterates through the active tracks in `sequencerTracks`, processes
 * the current command for each, and updates their state. This is the engine that drives all
 * modern animations.
 */
void handleSequencer();

/**
 * @brief Stops all running animations on a specific track and cleans up its state.
 * @param trackIndex The index of the track to stop (0-2).
 */
void stopAndCleanupTrack(int trackIndex);

/**
 * @brief Immediately stops all animations on all tracks and resets the sequencer to an idle state.
 * @details This is a master reset function, crucial for interrupting an animation safely.
 */
void stopAllSequences();

/**
 * @brief Triggers a new animation sequence globally.
 * @details This is the primary entry point for starting any built-in animation. It safely
 * stops any currently running animation and starts the new one.
 * @param animType The `AnimationType` of the sequence to start.
 */
void triggerAnimation(AnimationType animType);

/**
 * @brief Initializes and starts a marquee (scrolling text) effect on a given track.
 * @param track The `SequencerTrack` to run the marquee on.
 * @param text The text to scroll.
 */
void startSequencerMarquee(SequencerTrack& track, const std::string& text);

/**
 * @brief Handles the continuous update of all active marquee effects.
 * @details This function is called from the main loop to advance the scroll position of
 * any active marquees, making the text appear to move.
 */
void handleAllSequencerMarquees();

/** @} */


/**
 * @name Debug and Test Functions
 * @brief Functions used for development and debugging purposes.
 * @{
 */
void runSequencerTest();
void runCrossfadeTest();
/** @} */


/**
 * @name Utility Functions
 * @{
 */
/**
 * @brief Retrieves the full 13-character text currently displayed on a row.
 * @param row The index of the row to read from (0-2).
 * @return A std::string containing the text from the specified row.
 */
std::string getFullRowText(int row);
/** @} */

#endif // ANIMATION_MANAGER_H