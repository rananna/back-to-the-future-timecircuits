/**
 * @file AnimationManager.cpp
 * @brief Implements the logic for managing all visual animations and the animation sequencer.
 * @details This file contains the implementation for the animation-related functions declared
 * in `AnimationManager.h`. This includes the complex state machine for the cinematic boot
 * sequence, the handler for the multi-track animation sequencer, and various helper functions
 * for triggering and cleaning up animations.
 */
#include "AnimationManager.h"
#include <ESPmDNS.h>

// --- NEW: Global flag to prevent display updates until boot sequence is complete ---
bool bootSequenceCompleted = false;

#include "EventManager.h"
#include "HardwareControl.h"
#include "DebugLog.h"
#include "AnimationSequences.h"

// --- NEW: A global timeout for any single animation sequence track ---
#define MAX_SEQUENCE_DURATION 15000 // 15 seconds
#include "DisplayManager.h"
#include "MqttManager.h"

// --- NEW: Track the currently running animation type for logging ---
AnimationType currentAnimationType = ANIMATION_TYPE_MAX; // Initialize to a known invalid state

// --- NEW: Extern declaration to access the pre-animation display mode ---
extern int preAnimationDisplayMode;
#include <WiFi.h>
#include "web_server.h"
#include <ArduinoJson.h>

// --- Add these extern declarations for the new state variables ---
extern bool isStyledAnimating;
extern unsigned long styledAnimationStartTime;
extern AnimationPhase currentStyledPhase;
char old_dest_str[17], old_pres_str[17], old_last_str[17];

static BootSequenceState nextStateAfterSound = BOOT_INACTIVE;
static AnimationPhase nextPhaseAfterSound = ANIM_INACTIVE;


// --- Static flag to prevent boot loop ---
static bool infoMessageSet = false;

// --- Extern variable/function declarations ---
extern void setOverrideMessage(const char* line1, const char* line2, const char* line3);
extern bool isMessageOverrideActive;
extern unsigned long bootStateStartTime;
volatile bool isTransitioningAnimation = false;

// Helper function prototypes
void playReconfiguringSound();
void resetDisplayToNormal();
static void comprehensiveAnimationCleanup();
void doFinalAnimationCleanup();

extern int speedometerValue;

// Extern variables should be within the conditional block
#if ENABLE_HARDWARE
extern DisplayRow destRow, presRow, lastRow;
#endif

void broadcastAnimationComplete() {
    if (ws.count() > 0) {
        JsonDocument doc;
        doc["action"] = "animationComplete";
        String jsonString;
        serializeJson(doc, jsonString);
        ws.textAll(jsonString);
        Serial.println("SERVER_DEBUG: Broadcasted animationComplete message.");
    }
}

// Effects are now handled inside the sequencer

void triggerFlashEffect(int row, int segment, int duration) {
    if (row < 0 || row > 2 || segment < 0 || segment > 3) {
        Log_printf(LOG_LEVEL_WARN, "SEQ: Invalid parameters for triggerFlashEffect (row: %d, seg: %d)", row, segment);
        return;
    }

    // If a sequence is already active on this row, don't override it.
    if (sequencerTracks[row].isActive) {
        Log_printf(LOG_LEVEL_INFO, "SEQ: Ignoring flash effect on row %d, sequence already active.", row);
        return;
    }

    Log_printf(LOG_LEVEL_INFO, "SEQ: Triggering flash effect on row %d, segment %d for %dms.", row, segment, duration);

    // Reset the track to ensure it's in a clean state
    sequencerTracks[row].reset();

    // Configure the track for the flash effect
    sequencerTracks[row].isActive = true;
    sequencerTracks[row].stepStartTime = millis();
    sequencerTracks[row].trackStartTime = millis();
    sequencerTracks[row].originalBrightness = currentSettings.brightness;

    // Step 1: Flash the specified segment for the given duration
    sequencerTracks[row].steps[0] = {SEQ_CMD_FLASH, row, segment, duration, 0, "", ""};

    // Step 2: End the sequence
    sequencerTracks[row].steps[1] = {SEQ_CMD_END, 0, 0, 0, 0, "", ""};
}

// File-scoped variable to hold the chosen animation style for a single run
static int randomAnimationStyle = -1;

// --- START: NEW FADE AND PULSE IMPLEMENTATIONS ---
// Global effect handlers are no longer needed; this logic is now inside handleSequencer.

// --- TIME TRAVEL ANIMATION ---
/**
 * @brief DEPRECATED. Helper function for legacy sound-driven state transitions.
 */
void playSoundAndSetNextPhase(const char* filename, AnimationPhase nextPhase) {
    if (hardwareInitialized && currentSettings.timeTravelSoundToggle) {
        playSound(filename, false, -1);
    }
    nextPhaseAfterSound = nextPhase;
    currentPhase = ANIM_WAIT_FOR_SOUND;
    animationStartTime = millis();
}

void startTimeTravelAnimation() {
    Log_printf(LOG_LEVEL_INFO, "DIAG: startTimeTravelAnimation() called.");
    // Attempt to take the mutex. If we can't get it, another task is trying
    // to start an animation, so we should just exit.
    if (xSemaphoreTake(xAnimationStartMutex, (TickType_t)10) != pdTRUE) {
        return;
    }

    // We have the mutex, now we can safely check the animation flag.
    if (isAnimating) {
        xSemaphoreGive(xAnimationStartMutex); // Release the mutex before returning.
        return;
    }

    // Set the animation flag to prevent other tasks from starting another animation.
    isAnimating = true;

    // The critical section is over, release the mutex.
    xSemaphoreGive(xAnimationStartMutex);

    animationStartTime = millis();
    // Set the initial phase; the state machine will handle the rest.
    currentPhase = ANIM_POWER_UP;
    updateHaStatus("Animating");

    // The Last Time Departed is now exclusively set via the UI.
    // This animation is purely visual.
}

void handleDisplayAnimation() {
    if (!isAnimating || !hardwareInitialized) return;
#if ENABLE_HARDWARE
    unsigned long elapsed = millis() - animationStartTime;

    // --- FIX: Add a global timeout to prevent the animation from hanging indefinitely ---
    const unsigned long MAX_ANIMATION_DURATION = 8000; // 8 seconds
    if (elapsed > MAX_ANIMATION_DURATION) {
        Serial.println(F("ANIMATION_ERROR: Time travel animation timed out. Forcing exit."));
        comprehensiveAnimationCleanup();
        isAnimating = false;
        currentPhase = ANIM_INACTIVE;
        updateHaStatus("Idle");
        broadcastAnimationComplete();
        return;
    }
    static AnimationPhase lastPhase = ANIM_INACTIVE;

    // Detect when the animation phase changes to trigger the sound for the new phase.
    if (currentPhase != lastPhase) {
        if (currentSettings.timeTravelSoundToggle) {
            switch (currentPhase) {
                case ANIM_POWER_UP:
                    playSound("engine_rev.mp3", false, -1);
                    break;
                case ANIM_ARRIVAL:
                    playSound("time_travel.mp3", false, -1);
                    break;
                default:
                    break; // No sound for other states
            }
        }
        lastPhase = currentPhase;
    }

    switch (currentPhase) {
        case ANIM_POWER_UP:
            if (elapsed < 2000) {
                animateTornadoFlicker();
            } else {
                currentPhase = ANIM_TIME_ACCELERATION;
                animationStartTime = millis(); // Reset timer for the next phase
            }
            break;

        case ANIM_TIME_ACCELERATION:
             if (elapsed < 10000) { // Duration is 10 seconds
                float progress = (float)elapsed / 10000.0f;
                float easedProgress = progress * (2.0f - progress);
                int speed = 88 * easedProgress;
                
                displaySpeed(speed);
                animateDisplayRowRandomly(destRow);
                animateDisplayRowRandomly(presRow);
                animateDisplayRowRandomly(lastRow);
            } else {
                displaySpeed(88);
                flashAllDisplays();
                currentPhase = ANIM_ARRIVAL;
                animationStartTime = millis();
            }
            break;

        case ANIM_ARRIVAL:
            if (elapsed < currentSettings.timeTravelAnimationDuration) {
                // The main cinematic animation always uses the "Timeline Skim" effect.
                animateAllRowsTimelineSkim(elapsed, currentSettings.timeTravelAnimationDuration, currentSettings.destinationYear, false);
            } else {
                currentPhase = ANIM_LANDING;
                animationStartTime = millis();
            }
            break;

        case ANIM_LANDING:
             if (elapsed < 1000) {
                animateTornadoFlicker();
            } else {
                comprehensiveAnimationCleanup();
                isAnimating = false;
                currentPhase = ANIM_INACTIVE;
                lastPhase = ANIM_INACTIVE; // Reset for next run
                updateHaStatus("Idle");
                isEchoEffectActive = true;
                echoEffectStartTime = millis();
                broadcastAnimationComplete();
            }
            break;
        default:
            // Failsafe to prevent getting stuck in an unknown state.
            // A crash was observed when animations would enter an undefined state.
            // Calling the comprehensive cleanup ensures the system returns to a
            // stable state, preventing memory corruption or unexpected behavior.
            comprehensiveAnimationCleanup();
            isAnimating = false;
            currentPhase = ANIM_INACTIVE;
            lastPhase = ANIM_INACTIVE;
            broadcastAnimationComplete();
            break;
    }
#endif
}




// --- OTHER EFFECTS ---

void handleTemporalEcho() {
    // --- FIX: Do not run this effect if the boot sequence hasn't finished ---
    if (!bootSequenceCompleted) return;
    if (!isEchoEffectActive || isAnimating || isStyledAnimating || !hardwareInitialized) return;
#if ENABLE_HARDWARE
    if (millis() - echoEffectStartTime > 3000) { // Effect lasts for 3 seconds
        isEchoEffectActive = false;
        // --- FIX: Force a redraw of the present time row to clear the flicker effect ---
        updateNormalClockDisplay(false, true, false);
        return;
    }

    // Randomly flicker the "Present Time" display
    if (random(100) < 10) {
        animateDisplayRowRandomly(presRow);
    }
#endif
}

// --- BOOT SEQUENCE ---

void runBootSequence() {
    Serial.println("BOOT_LOG: runBootSequence() called.");
    if (bootState == BOOT_INACTIVE) {
        blankAllDisplays(); // Immediately clear the display to prevent showing old data
        bootState = BOOT_AWAIT_HUM;
        bootStateStartTime = millis();
        Serial.println("BOOT_LOG: Boot sequence initiated.");
    } else {
        Serial.println("BOOT_LOG: Boot sequence already in progress. Call ignored.");
    }
}



void handleBootSequence() {
    if (bootState == BOOT_INACTIVE) return;

    static bool stateActionCompleted = false;
    unsigned long elapsed = millis() - bootStateStartTime;
    static BootSequenceState lastLoggedState = BOOT_INACTIVE;
    static int lastDiagSecond = -1;
    static bool typingStarted = false;

    if (bootState != lastLoggedState) {
        Serial.printf("BOOT_LOG: Entering state %d. Elapsed: %lu ms.\n", bootState, elapsed);
        lastLoggedState = bootState;
        stateActionCompleted = false;
        typingStarted = false;
    }

    switch (bootState) {
        case BOOT_AWAIT_HUM:
            if (!stateActionCompleted) {
                blankAllDisplays();
                printToDisplay(presRow.month, " SYS");
                printToDisplay(presRow.day, " IS");
                printToDisplay(presRow.year, "BOOT");
                printToDisplay(presRow.time, "ING ");
                // Only write the present time row to the display
                if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
                    presRow.month.writeDisplay();
                    presRow.day.writeDisplay();
                    presRow.year.writeDisplay();
                    presRow.time.writeDisplay();
                    xSemaphoreGive(xDisplayHardwareMutex);
                }
                playSound("/hum.mp3", false, 15);
                stateActionCompleted = true;
            }
            if (elapsed > BOOT_AWAIT_HUM_DURATION) {
                bootState = BOOT_START;
                bootStateStartTime = millis();
            }
            break;
        case BOOT_START:
            if (!stateActionCompleted) {
                // Text display logic moved to BOOT_AWAIT_HUM
                stateActionCompleted = true;
            }
            if (elapsed > 1000) {
                bootState = BOOT_SYSTEM_ACTIVATE;
                bootStateStartTime = millis();
            }
            break;
        case BOOT_SYSTEM_ACTIVATE:
            {
                const int holdDuration = 5000; // Keep the message on screen for 5 seconds

                if (!stateActionCompleted) {
                    playSound("/relay_activation.mp3", false, -1);
                    blankAllDisplays();
                    // Instantly display the text instead of typing it out
                    printToDisplay(destRow.month, " TIM");
                    printToDisplay(destRow.day, " E  ");
                    printToDisplay(destRow.year, "CIRC");
                    printToDisplay(destRow.time, "UITS");
                    printToDisplay(presRow.month, "");
                    printToDisplay(presRow.day, "");
                    printToDisplay(presRow.year, "ACTI");
                    printToDisplay(presRow.time, "VATE");
                    // Explicitly write to the display hardware
                    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
                        destRow.month.writeDisplay();
                        destRow.day.writeDisplay();
                        destRow.year.writeDisplay();
                        destRow.time.writeDisplay();
                        presRow.month.writeDisplay();
                        presRow.day.writeDisplay();
                        presRow.year.writeDisplay();
                        presRow.time.writeDisplay();
                        xSemaphoreGive(xDisplayHardwareMutex);
                    }

                    stateActionCompleted = true;
                }

                if (elapsed > holdDuration) {
                    bootState = BOOT_FLUX_CAPACITOR_IGNITION;
                    bootStateStartTime = millis();
                }
            }
            break;

        // --- FIX START: Decouple audio from animation ---
        case BOOT_FLUX_CAPACITOR_IGNITION:
            // This state now ONLY starts the sound and waits for it to begin playing.
            if (!stateActionCompleted) {
                playSound("/flux_capacitor_power_on.mp3", false, -1);
                stateActionCompleted = true;
            }
            // Once the audio is confirmed to be running, move to the animation state.
            if (elapsed > 2000) { // Failsafe timeout of 2s
                bootState = BOOT_FLUX_CAPACITOR_ANIMATION;
                bootStateStartTime = millis(); // Reset the timer for the animation phase
            }
            break;

        case BOOT_FLUX_CAPACITOR_ANIMATION:
            // This new state handles all the visuals. The sound is guaranteed to be playing.
            if (elapsed < BOOT_FLUX_CAPACITOR_IGNITION_DURATION) {
                 if (elapsed < 3000) { // First 3 seconds: flash
                    if ((elapsed / 250) % 2 == 0) {
                        flashAllDisplays();
                    } else {
                        blankAllDisplays();
                    }
                } else { // The rest of the animation
                    animateDisplayRowRandomly(destRow);
                    animateDisplayRowRandomly(lastRow);
                    if ((elapsed / 250) % 2 == 0) {
                        displayStaticFluxText();
                    } else {
                        printToDisplay(presRow.month, "");
                        printToDisplay(presRow.day, "");
                        printToDisplay(presRow.year, "");
                        printToDisplay(presRow.time, "");
                        if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
                            presRow.month.writeDisplay();
                            presRow.day.writeDisplay();
                            presRow.year.writeDisplay();
                            presRow.time.writeDisplay();
                            xSemaphoreGive(xDisplayHardwareMutex);
                        }
                    }
                }
            } else {
                // When the animation duration is over, transition to the next major step.
                bootState = BOOT_DIAGNOSTICS;
                bootStateStartTime = millis();
            }
            break;
        // --- FIX END ---

        case BOOT_DIAGNOSTICS:
            {
                if (!stateActionCompleted) {
                    playSound("/keypad_beeps.mp3", false, -1);
                    stateActionCompleted = true;
                }

                int currentSecond = elapsed / 2000;

                if (currentSecond != lastDiagSecond) {
                    blankAllDisplays();

                    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
                        if (currentSecond == 0) {
                            printToDisplay(destRow.month, " CPU");
                            printToDisplay(destRow.day, " OK");
                            destRow.month.writeDisplay();
                            destRow.day.writeDisplay();
                        } else if (currentSecond == 1) {
                            printToDisplay(presRow.month, " MEM");
                            printToDisplay(presRow.day, " OK");
                            presRow.month.writeDisplay();
                            presRow.day.writeDisplay();
                        } else if (currentSecond == 2) {
                            printToDisplay(lastRow.month, " WFI");
                            printToDisplay(lastRow.day, " OK");
                            lastRow.month.writeDisplay();
                            lastRow.day.writeDisplay();
                        } else if (currentSecond == 3) {
                            // For variety, put the last message back on the top row
                            printToDisplay(destRow.month, " MQT");
                            printToDisplay(destRow.day, " OK");
                            destRow.month.writeDisplay();
                            destRow.day.writeDisplay();
                        }
                        xSemaphoreGive(xDisplayHardwareMutex);
                    }
                    lastDiagSecond = currentSecond;
                }

                if (elapsed > BOOT_DIAGNOSTICS_DURATION) {
                    bootState = BOOT_FINAL_CHECKS;
                    bootStateStartTime = millis();
                }
            }
            break;
        case BOOT_FINAL_CHECKS:
            {
                if (!stateActionCompleted) {
                    playSound("/engine_rev.mp3", false, -1);
                    stateActionCompleted = true;
                }
                //if (audio.isRunning()) {
                    if (!stateActionCompleted) {
                        blankAllDisplays();
                        stateActionCompleted = true;
                    }

                    float progress = (float)elapsed / BOOT_FINAL_CHECKS_DURATION;
                    if (progress > 1.0) progress = 1.0;
                    float easedProgress = 1 - pow(1 - progress, 3);
                    int speed = 1 + (easedProgress * 87);
                    if (speed > 88) speed = 88;

                    displaySpeedRamp(speed);

                    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
                        printToDisplay(destRow.month, " SYS");
                        printToDisplay(destRow.day, " IS");
                        printToDisplay(presRow.year, "LIVE");
                        printToDisplay(presRow.time, " NOW");
                        destRow.year.writeDisplay();
                        destRow.time.writeDisplay();
                        xSemaphoreGive(xDisplayHardwareMutex);
                    }
                //}
                if (elapsed > BOOT_FINAL_CHECKS_DURATION) {
                    bootState = BOOT_TEMPORAL_DISPLACEMENT;
                    bootStateStartTime = millis();
                }
            }
            break;
        case BOOT_TEMPORAL_DISPLACEMENT:
            {
                if (!stateActionCompleted) {
                    playSound("/time_travel.mp3", false, -1);
                    stateActionCompleted = true;
                }
                //if (audio.isRunning()) {
                    animateRandomRealTimes();
                //}
                if (elapsed > BOOT_TEMPORAL_DISPLACEMENT_DURATION) {
                    bootState = BOOT_ARRIVAL;
                    bootStateStartTime = millis();
                }
            }
            break;
        case BOOT_ARRIVAL:
            {
                if (!stateActionCompleted) {
                    playSound("/arrival_chime.mp3", false, -1);
                    stateActionCompleted = true;
                }
                if (elapsed > 2000) { // Failsafe timeout of 2s
                    bootState = BOOT_ARRIVAL_ANIMATION;
                    bootStateStartTime = millis(); // Reset the timer for the animation phase
                }
            }
            break;
        case BOOT_ARRIVAL_ANIMATION:
            {
                if (!stateActionCompleted) {
                    // --- FIX: Display text immediately without waiting for audio ---
                    blankAllDisplays();
                    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
                        printToDisplay(destRow.year, "ARRI");
                        printToDisplay(destRow.time, "VAL");
                        printToDisplay(presRow.year, "OUTA");
                        printToDisplay(presRow.time, "TIME");

                        destRow.year.writeDisplay();
                        destRow.time.writeDisplay();
                        presRow.year.writeDisplay();
                        presRow.time.writeDisplay();
                        xSemaphoreGive(xDisplayHardwareMutex);
                    }
                    stateActionCompleted = true; // Mark that the initial action is done
                }

                // Display the "WELCOME" message after a delay
                if (elapsed > 2000) { // Show "WELCOME" after 2 seconds
                    if (xSemaphoreTake(xDisplayHardwareMutex, portMAX_DELAY) == pdTRUE) {
                        printToDisplay(lastRow.year, " WEL");
                        printToDisplay(lastRow.time, "COME");
                        lastRow.year.writeDisplay();
                        lastRow.time.writeDisplay();
                        xSemaphoreGive(xDisplayHardwareMutex);
                    }
                }

                // Transition to the next state after the full duration
                if (elapsed > BOOT_ARRIVAL_DURATION) {
                    bootState = BOOT_COOL_DOWN;
                    bootStateStartTime = millis();
                }
            }
            break;
        case BOOT_COOL_DOWN:
            {
                if (!stateActionCompleted) {
                    // Manually fade out the audio
                    // for (int i = currentSettings.notificationVolume; i >= 0; i--) {
                    //     audio.setVolume(i);
                    //     delay(50);
                    // }
                    // audio.stopSong();
                    stateActionCompleted = true;
                }

                float progress = (float)elapsed / BOOT_COOL_DOWN_DURATION;
                if (progress > 1.0) progress = 1.0;
                uint8_t brightness = 7 * (1.0 - progress);

                destRow.month.setBrightness(brightness);
                destRow.day.setBrightness(brightness);
                destRow.year.setBrightness(brightness);
                destRow.time.setBrightness(brightness);
                presRow.month.setBrightness(brightness);
                presRow.day.setBrightness(brightness);
                presRow.year.setBrightness(brightness);
                presRow.time.setBrightness(brightness);
                lastRow.month.setBrightness(brightness);
                lastRow.day.setBrightness(brightness);
                lastRow.year.setBrightness(brightness);
                lastRow.time.setBrightness(brightness);

                if (elapsed > BOOT_COOL_DOWN_DURATION) {
                    bootState = BOOT_COMPLETE;
                    bootStateStartTime = millis();
                }
            }
            break;
        case BOOT_COMPLETE:
            {
                if (elapsed > 500) {
                    comprehensiveAnimationCleanup(); // Resets manual modes without forcing clock display
                    updateNormalClockDisplay(true, true, true); // Force a redraw of the clock
                    bootSequenceCompleted = true; // --- NEW: Signal that the boot sequence is done.
                    bootState = BOOT_INACTIVE;
                    // The main display loop will now handle updating the display correctly
                    // according to the restored displayMode (e.g., Clock, Weather, Stocks).
                    Serial.println("BOOT_LOG: Boot sequence finished. Restoring previous display mode.");
                }
            }
            break;
        default:
            {
                Serial.printf("BOOT_LOG: Unknown boot state %d. Resetting to INACTIVE.\n", bootState);
                bootState = BOOT_INACTIVE;
            }
            break;
    }
}
/**
 * @brief Resets all display rows and flags to the normal clock view.
 * @details This function is a robust way to exit any special display mode
 * (like an override message or manual text) and ensure the correct time is
 * shown. It clears all relevant state flags before forcing a full redraw
 * of all three time circuit rows.
 */
#include <string.h>

void resetDisplayToNormal() {
    // Clear any active override message flags
    isMessageOverrideActive = false;

    // Reset manual text override for all display segments
    for (int r = 0; r < NUM_SEQUENCER_TRACKS; ++r) {
        isRowInManualMode[r] = false;
        for (int s = 0; s < 4; ++s) {
            strcpy(manualDisplayText[r][s], "");
        }
    }

    // Force a full redraw of all three rows to the current time
    updateNormalClockDisplay(true, true, true);
}

/**
 * @brief Restores the display to a clean state after an animation.
 * @details This function resets all key state flags (manual mode, overrides)
 * and restores the default brightness to all display segments. It's a comprehensive
 * cleanup designed to be called at the end of any animation sequence to ensure
 * the display returns to normal operation without artifacts or getting stuck
 * in a previous state. It does NOT force a display redraw.
 */
static void comprehensiveAnimationCleanup() {
    // Reset all override and manual mode flags
    isMessageOverrideActive = false;
    for (int r = 0; r < NUM_SEQUENCER_TRACKS; ++r) {
        isRowInManualMode[r] = false;
        for (int s = 0; s < 4; ++s) {
            strcpy(manualDisplayText[r][s], "");
        }
    }

    // Restore brightness
    uint8_t saved_brightness = currentSettings.brightness;
    destRow.month.setBrightness(saved_brightness);
    destRow.day.setBrightness(saved_brightness);
    destRow.year.setBrightness(saved_brightness);
    destRow.time.setBrightness(saved_brightness);
    presRow.month.setBrightness(saved_brightness);
    presRow.day.setBrightness(saved_brightness);
    presRow.year.setBrightness(saved_brightness);
    presRow.time.setBrightness(saved_brightness);
    lastRow.month.setBrightness(saved_brightness);
    lastRow.day.setBrightness(saved_brightness);
    lastRow.year.setBrightness(saved_brightness);
    lastRow.time.setBrightness(saved_brightness);
}

/**
 * @brief Retrieves the full 13-character text for a given row.
 * @details This is used by effects like `RANDOM_FLICKER_TEXT` when no string
 * parameter is provided, allowing the effect to operate on the currently
 * displayed text.
 * @param row The display row index (0-2).
 * @param buffer A character buffer to write the resulting string into. Must be at least 14 bytes.
 */
void getFullRowText(int row, char* buffer) {
    if (row < 0 || row > 2 || buffer == nullptr) {
        if (buffer) buffer[0] = '\0';
        return;
    }
    // Safely concatenate the text from all four segments into the buffer.
    buffer[0] = '\0'; // Start with an empty string
    strcat(buffer, manualDisplayText[row][0]);
    strcat(buffer, manualDisplayText[row][1]);
    strcat(buffer, manualDisplayText[row][2]);
    strcat(buffer, manualDisplayText[row][3]);
}

void handleSequencer() {
    bool needsDisplayUpdate = false;
    DisplayRow* rows[] = {&destRow, &presRow, &lastRow};

    for (int i = 0; i < NUM_SEQUENCER_TRACKS; i++) {
        SequencerTrack& track = sequencerTracks[i];
        DisplayRow& row = *rows[i];

        // --- NEW: Check for track timeout ---
        if (track.isActive && (millis() - track.trackStartTime > MAX_SEQUENCE_DURATION)) {
            Log_printf(LOG_LEVEL_WARN, "SEQ: Track %d timed out after %d ms. Aborting ALL tracks.", i, MAX_SEQUENCE_DURATION);
            stopAllSequences();
            // --- FIX: DO NOT call doFinalAnimationCleanup() here. ---
            // The main state machine at the end of this function will detect that the animation
            // has stopped and will call the cleanup function correctly and safely.
            // Calling it here would cause a double-call and a crash.
            needsDisplayUpdate = true;
            break;
        }

        // --- Handle active effects for this track ---

        // Handle Fade Effect
        if (track.isFading) {
            unsigned long fadeElapsed = millis() - track.fadeStartTime;
            if (fadeElapsed >= (unsigned long)track.fadeDuration) {
                track.isFading = false;
                uint8_t finalBrightness = track.isFadeIn ? track.originalBrightness : 0;
                row.month.setBrightness(finalBrightness);
                row.day.setBrightness(finalBrightness);
                row.year.setBrightness(finalBrightness);
                row.time.setBrightness(finalBrightness);
                needsDisplayUpdate = true;
            } else {
                float progress = (float)fadeElapsed / (float)track.fadeDuration;
                uint8_t newBrightness = track.isFadeIn ?
                    (uint8_t)(progress * (float)track.originalBrightness) :
                    (uint8_t)((1.0f - progress) * (float)track.originalBrightness);
                row.month.setBrightness(newBrightness);
                row.day.setBrightness(newBrightness);
                row.year.setBrightness(newBrightness);
                row.time.setBrightness(newBrightness);
                needsDisplayUpdate = true;
            }
        }

        // Handle Pulse and Flash Effects
        for (int s = 0; s < 4; s++) {
            if (track.isPulsing[s]) {
                if (millis() > track.pulseEndTimes[s]) {
                    track.isPulsing[s] = false;
                    needsDisplayUpdate = true;
                } else if (millis() - track.lastPulseToggle[s] > track.pulseInterval) {
                    track.pulseStates[s] = !track.pulseStates[s];
                    track.lastPulseToggle[s] = millis();
                    needsDisplayUpdate = true;
                }
            }
            if (track.isFlashing[s]) {
                if (track.flashEndTimes[s] != 0 && millis() > track.flashEndTimes[s]) {
                    track.isFlashing[s] = false;
                    needsDisplayUpdate = true;
                } else if (millis() - track.lastFlashToggle[s] > 75) {
                    track.flashStates[s] = !track.flashStates[s];
                    track.lastFlashToggle[s] = millis();
                    needsDisplayUpdate = true;
                }
            }
        }

        if (!track.isActive) {
            continue;
        }

        // --- Process Commands ---
        bool advance_step = false;
        SequenceStep& step = track.steps[track.currentStep];
        unsigned long commandElapsed = millis() - track.stepStartTime;

        switch (step.command) {
            // --- Display Control ---
            case SEQ_CMD_SET_TEXT:
                if (!track.stepInitialized) {
                    updateDisplaySegment(step.targetRow, step.targetSegment, step.stringParam);
                    track.stepInitialized = true;
                    advance_step = true;
                }
                break;

            case SEQ_CMD_CLEAR_SEGMENT:
                if (!track.stepInitialized) {
                    updateDisplaySegment(step.targetRow, step.targetSegment, "");
                    track.stepInitialized = true;
                    advance_step = true;
                }
                break;

            case SEQ_CMD_SET_BRIGHTNESS:
                if (!track.stepInitialized) {
                    uint8_t brightness = (uint8_t)constrain(step.intParam, 0, 7);
                    row.month.setBrightness(brightness);
                    row.day.setBrightness(brightness);
                    row.year.setBrightness(brightness);
                    row.time.setBrightness(brightness);
                    needsDisplayUpdate = true;
                    track.stepInitialized = true;
                    advance_step = true;
                }
                break;

            case SEQ_CMD_RESTORE_ROW:
                if (!track.stepInitialized) {
                    restoreDisplayRow(step.targetRow);
                    track.stepInitialized = true;
                    advance_step = true;
                }
                break;

            case SEQ_CMD_RESTORE_ALL_ROWS:
                if (!track.stepInitialized) {
                    for (int j = 0; j < NUM_SEQUENCER_TRACKS; ++j) {
                        restoreDisplayRow(j);
                    }
                    track.stepInitialized = true;
                    advance_step = true;
                }
                break;

            // --- Basic Commands ---
            case SEQ_CMD_WAIT:
                if (!track.stepInitialized) {
                    track.stepInitialized = true;
                }
                if (commandElapsed >= (unsigned long)step.intParam) {
                    advance_step = true;
                }
                break;

            case SEQ_CMD_SOUND:
                if (!track.stepInitialized) {
                    playSound(step.stringParam, false, -1);
                    track.stepInitialized = true;
                    advance_step = true;
                }
                break;

            // --- Logic Commands ---
            case SEQ_CMD_LOOP_START:
                if (!track.stepInitialized) {
                    track.stepInitialized = true;
                    if (step.intParam > 0) {
                        // A normal loop of 1 or more iterations.
                        track.loopStartStep = track.currentStep;
                        track.loopCounter = step.intParam;
                    } else {
                        // A loop with 0 or fewer iterations should be skipped entirely.
                        Log_printf(LOG_LEVEL_INFO, "SEQ: Track %d skipping loop with count %d.", i, step.intParam);
                        int openLoops = 1;
                        int seekStep = track.currentStep + 1;
                        while (seekStep < MAX_SEQUENCE_STEPS) {
                            if (track.steps[seekStep].command == SEQ_CMD_LOOP_START) {
                                openLoops++;
                            } else if (track.steps[seekStep].command == SEQ_CMD_LOOP_END) {
                                openLoops--;
                            }
                            if (openLoops == 0) {
                                break;
                            }
                            seekStep++;
                        }

                        if (openLoops == 0) {
                            // Jump PC to the LOOP_END. The main loop will advance it to the next step, skipping the loop body.
                            track.currentStep = seekStep;
                        } else {
                            // No matching LOOP_END found, this is a sequence error.
                            Log_printf(LOG_LEVEL_WARN, "SEQ: Track %d has an unclosed loop at step %d. Aborting.", i, track.currentStep);
                            track.reset(); // Stop this track
                            break;
                        }
                    }
                }
                advance_step = true;
                break;

            case SEQ_CMD_LOOP_END:
                if (!track.stepInitialized) {
                    track.stepInitialized = true;
                    if (track.loopStartStep < 0) {
                        // This is a rogue LOOP_END without a matching LOOP_START.
                        Log_printf(LOG_LEVEL_WARN, "SEQ: Track %d found rogue LOOP_END at step %d. Aborting.", i, track.currentStep);
                        track.reset();
                    } else {
                        // This is a valid loop end. Decrement counter and check if we need to loop again.
                        track.loopCounter--;
                        if (track.loopCounter > 0) {
                            // Jump back to the step *after* LOOP_START.
                            track.currentStep = track.loopStartStep;
                        } else {
                            // Loop is finished.
                            track.loopStartStep = -1;
                            track.loopCounter = 0;
                        }
                    }
                }
                advance_step = true;
                break;

            // --- Effects ---
            case SEQ_CMD_FADE_IN:
            case SEQ_CMD_FADE_OUT:
                if (!track.stepInitialized) {
                    if (track.isFading) break; // Don't start a new fade if one is already active on this track
                    track.isFading = true;
                    track.isFadeIn = (step.command == SEQ_CMD_FADE_IN);
                    track.fadeDuration = step.intParam;
                    track.fadeStartTime = millis();
                    track.originalBrightness = currentSettings.brightness;
                    track.stepInitialized = true;
                }
                // --- FIX: This is now a blocking command. We wait for the fade to complete. ---
                if (!track.isFading) {
                    advance_step = true;
                }
                break;

            case SEQ_CMD_PULSE:
                if (step.targetSegment < -1 || step.targetSegment >= 4) {
                    Log_printf(LOG_LEVEL_WARN, "SEQ: Invalid segment %d for PULSE on track %d. Skipping.", step.targetSegment, i);
                    advance_step = true;
                    break;
                }
                if (!track.stepInitialized) {
                    // If a stringParam is provided, set the text before starting the pulse.
                    if (step.stringParam[0] != '\0') {
                        updateDisplaySegment(step.targetRow, step.targetSegment, step.stringParam);
                    }

                    // Use intParam for interval, intParam2 for total duration.
                    track.pulseInterval = (step.intParam > 0) ? step.intParam : 750;
                    unsigned long duration = (step.intParam2 > 0) ? step.intParam2 : 5000; // Default 5s duration

                    if (step.targetSegment == -1) { // Apply to all segments
                        for (int s = 0; s < 4; s++) {
                            track.isPulsing[s] = true;
                            track.pulseEndTimes[s] = millis() + duration;
                            track.pulseStates[s] = true;
                            track.lastPulseToggle[s] = millis();
                        }
                    } else { // Apply to a single segment
                        track.isPulsing[step.targetSegment] = true;
                        track.pulseEndTimes[step.targetSegment] = millis() + duration;
                        track.pulseStates[step.targetSegment] = true;
                        track.lastPulseToggle[step.targetSegment] = millis();
                    }
                    track.stepInitialized = true;
                } else {
                    // --- FIX: This is now a blocking command. Check for completion. ---
                    bool stillPulsing = false;
                    if (step.targetSegment == -1) {
                        for (int s = 0; s < 4; s++) {
                            if (track.isPulsing[s]) {
                                stillPulsing = true;
                                break;
                            }
                        }
                    } else {
                        stillPulsing = track.isPulsing[step.targetSegment];
                    }

                    if (!stillPulsing) {
                        advance_step = true;
                    }
                }
                break;

            case SEQ_CMD_FLASH:
                if (step.targetSegment < -1 || step.targetSegment >= 4) {
                    Log_printf(LOG_LEVEL_WARN, "SEQ: Invalid segment %d for FLASH on track %d. Skipping.", step.targetSegment, i);
                    advance_step = true;
                    break;
                }
                if (!track.stepInitialized) {
                    if (step.targetSegment == -1) { // Apply to all segments
                        for (int s = 0; s < 4; s++) {
                            track.isFlashing[s] = true;
                            track.flashEndTimes[s] = (step.intParam == 0) ? 0 : millis() + step.intParam;
                            track.flashStates[s] = true;
                            track.lastFlashToggle[s] = millis();
                        }
                    } else { // Apply to a single segment
                        track.isFlashing[step.targetSegment] = true;
                        track.flashEndTimes[step.targetSegment] = (step.intParam == 0) ? 0 : millis() + step.intParam;
                        track.flashStates[step.targetSegment] = true;
                        track.lastFlashToggle[step.targetSegment] = millis();
                    }
                    track.stepInitialized = true;
                } else {
                    // --- FIX: This is now a blocking command. Check for completion. ---
                    bool stillFlashing = false;
                    if (step.targetSegment == -1) {
                        for (int s = 0; s < 4; s++) {
                            if (track.isFlashing[s]) {
                                stillFlashing = true;
                                break;
                            }
                        }
                    } else {
                        stillFlashing = track.isFlashing[step.targetSegment];
                    }

                    if (!stillFlashing) {
                        advance_step = true;
                    }
                }
                break;

            case SEQ_CMD_MARQUEE:
                if (!track.stepInitialized) {
                    // This now correctly calls the marquee initialization function
                    startSequencerMarquee(track, step.stringParam);
                    track.stepInitialized = true;
                }
                // The marquee effect runs in the background via handleAllSequencerMarquees().
                // We check the isMarqueeActive flag to know when it's done.
                if (!track.isMarqueeActive) {
                    advance_step = true;
                }
                break;

            case SEQ_CMD_COUNTDOWN:
                { // Scope for local variables
                    // Helper lambda to get the string for the countdown value
                    auto get_countdown_string = [](int value, char* buffer, size_t buffer_size) {
                        const char* str = "";
                        if (value > 20) {
                            snprintf(buffer, buffer_size, "%d", value);
                            return;
                        }
                        switch (value) {
                            case 0: str = "ZERO"; break;
                            case 1: str = "ONE"; break;
                            case 2: str = "TWO"; break;
                            case 3: str = "THREE"; break;
                            case 4: str = "FOUR"; break;
                            case 5: str = "FIVE"; break;
                            case 6: str = "SIX"; break;
                            case 7: str = "SEVEN"; break;
                            case 8: str = "EIGHT"; break;
                            case 9: str = "NINE"; break;
                            case 10: str = "TEN"; break;
                            case 11: str = "ELEVEN"; break;
                            case 12: str = "TWELVE"; break;
                            case 13: str = "THIRTEEN"; break;
                            case 14: str = "FOURTEEN"; break;
                            case 15: str = "FIFTEEN"; break;
                            case 16: str = "SIXTEEN"; break;
                            case 17: str = "SEVENTEEN"; break;
                            case 18: str = "EIGHTEEN"; break;
                            case 19: str = "NINETEEN"; break;
                            case 20: str = "TWENTY"; break;
                            default: str = ""; break;
                        }
                        strncpy(buffer, str, buffer_size);
                        buffer[buffer_size - 1] = '\0';
                    };

                    // Helper lambda to format and display the countdown string
                    auto display_countdown = [&](int value) {
                        char text_buffer[14];
                        get_countdown_string(value, text_buffer, sizeof(text_buffer));

                        char display_buffer[14];
                        memset(display_buffer, ' ', 13);
                        display_buffer[13] = '\0';

                        int text_len = strlen(text_buffer);
                        int padding = (13 - text_len) / 2;
                        if (padding < 0) padding = 0;

                        strncpy(display_buffer + padding, text_buffer, 13 - padding);

                        // This command is assumed to always target the full row
                        updateDisplaySegment(step.targetRow, -1, display_buffer);
                    };

                    // Validate the delay parameter. If it's invalid (e.g., negative), default to 1000ms.
                    unsigned long countdown_delay = (step.intParam2 > 0) ? (unsigned long)step.intParam2 : 1000;

                    if (!track.stepInitialized) {
                        // --- FIX: Add a failsafe to prevent hangs from huge numbers ---
                        const int MAX_COUNTDOWN_VALUE = 3600; // Cap at 1 hour
                        track.countdownValue = (step.intParam > MAX_COUNTDOWN_VALUE) ? MAX_COUNTDOWN_VALUE : step.intParam;
                        if (step.intParam > MAX_COUNTDOWN_VALUE) {
                            Log_printf(LOG_LEVEL_WARN, "SEQ: COUNTDOWN value %d capped at %d.", step.intParam, MAX_COUNTDOWN_VALUE);
                        }

                        // Initialization: Set the starting value and display it immediately.
                        track.countdownLastUpdate = millis();
                        display_countdown(track.countdownValue);
                        track.stepInitialized = true;
                    } else if (millis() - track.countdownLastUpdate >= countdown_delay) {
                        // Update: Decrement the value after the delay has passed.
                        track.countdownValue--;
                        track.countdownLastUpdate = millis(); // Reset timer for the next interval

                        // Display the new value as long as the countdown is not finished.
                        if (track.countdownValue >= 0) {
                            display_countdown(track.countdownValue);
                        }
                    }

                    // Completion Check: Advance to the next step when the countdown finishes.
                    if (track.countdownValue < 0) {
                        advance_step = true;
                    }
                }
                break;

            case SEQ_CMD_SCANNER:
                if (!track.stepInitialized) {
                    track.scannerPosition = 0;
                    track.scannerDirection = true;
                    track.lastScannerUpdate = millis();
                    track.stepInitialized = true;
                }
                if (commandElapsed >= (unsigned long)step.intParam) {
                    advance_step = true;
                } else {
                    if (millis() - track.lastScannerUpdate > (unsigned long)step.intParam2) {
                        const char* visual = (step.stringParam[0] != '\0') ? step.stringParam : "#";
                        int visual_len = strlen(visual);

                        // --- FIX: Use a static buffer to prevent heap fragmentation ---
                        static char scan_buffer[14];
                        memset(scan_buffer, ' ', 13);
                        scan_buffer[13] = '\0';

                        // Correctly place the visual without going out of bounds
                        if (track.scannerPosition + visual_len <= 13) {
                            memcpy(scan_buffer + track.scannerPosition, visual, visual_len);
                        }
                        updateDisplaySegment(step.targetRow, -1, scan_buffer);

                        if (track.scannerDirection) { // moving right
                            track.scannerPosition++;
                            if (track.scannerPosition + visual_len > 13) {
                                track.scannerPosition = 13 - visual_len;
                                track.scannerDirection = false;
                                track.scannerPosition--;
                            }
                        } else { // moving left
                            track.scannerPosition--;
                            if (track.scannerPosition < 0) {
                                track.scannerPosition = 0;
                                track.scannerDirection = true;
                                track.scannerPosition++;
                            }
                        }
                        track.lastScannerUpdate = millis();
                    }
                }
                vTaskDelay(1); // Yield to other tasks
                break;

            case SEQ_CMD_TYPEWRITER:
                if (!track.stepInitialized) {
                    track.typewriterIndex = 0;
                    track.lastTypewriterUpdate = millis();
                    updateDisplaySegment(step.targetRow, step.targetSegment, ""); // Clear segment first
                    track.stepInitialized = true;
                }
                if ((unsigned)track.typewriterIndex >= strlen(step.stringParam)) {
                    advance_step = true;
                } else {
                    if (millis() - track.lastTypewriterUpdate > (unsigned long)step.intParam) {
                        track.typewriterIndex++;
                        // --- FIX: Use a static buffer and strncpy to prevent heap fragmentation ---
                        static char typewriter_buffer[14];
                        strncpy(typewriter_buffer, step.stringParam, track.typewriterIndex);
                        typewriter_buffer[track.typewriterIndex] = '\0';
                        updateDisplaySegment(step.targetRow, step.targetSegment, typewriter_buffer);
                        track.lastTypewriterUpdate = millis();
                    }
                }
                vTaskDelay(1); // Yield to other tasks
                break;

            case SEQ_CMD_WIPE:
                 if (!track.stepInitialized) {
                    track.wipeSegment = 0;
                    track.lastWipeUpdate = millis();
                    track.stepInitialized = true;
                }
                if (track.wipeSegment >= 13) {
                    advance_step = true;
                } else {
                    if (millis() - track.lastWipeUpdate > (unsigned long)step.intParam) {
                        // --- FIX: Use a static buffer to prevent heap fragmentation ---
                        static char wipe_buffer[14];
                        memset(wipe_buffer, ' ', 13);
                        wipe_buffer[13] = '\0';

                        int len_to_copy = min((int)strlen(step.stringParam), track.wipeSegment);
                        strncpy(wipe_buffer, step.stringParam, len_to_copy);

                        updateDisplaySegment(step.targetRow, -1, wipe_buffer);
                        track.wipeSegment++;
                        track.lastWipeUpdate = millis();
                    }
                }
                vTaskDelay(1); // Yield to other tasks
                break;

            case SEQ_CMD_BAR_GRAPH:
                {
                    // intParam:  Starting percentage (0-100)
                    // intParam2: Duration in milliseconds
                    if (!track.stepInitialized) {
                        track.barGraphStartTime = millis(); // Use a dedicated timer
                        track.stepInitialized = true;
                    }

                    unsigned long animElapsed = millis() - track.barGraphStartTime;
                    unsigned long totalDuration = (unsigned long)step.intParam2;

                    // Check for animation completion
                    if (animElapsed >= totalDuration) {
                        // Ensure the bar is 100% full at the end
                        char final_bar[14];
                        memset(final_bar, '|', 13);
                        final_bar[13] = '\0';

                        if (step.stringParam[0] != '\0') {
                            int text_len = strlen(step.stringParam);
                            int start_pos = (13 - text_len) / 2;
                            if (start_pos < 0) start_pos = 0;
                            strncpy(final_bar + start_pos, step.stringParam, 13 - start_pos);
                        }
                        updateDisplaySegment(step.targetRow, -1, final_bar);
                        advance_step = true;
                    } else {
                        // Calculate animation progress
                        float startPercent = (float)constrain(step.intParam, 0, 100) / 100.0f;
                        float animProgress = (totalDuration > 0) ? ((float)animElapsed / (float)totalDuration) : 1.0f;
                        if (animProgress > 1.0f) animProgress = 1.0f;

                        // Calculate the current state of the bar
                        float currentProgress = startPercent + ((1.0f - startPercent) * animProgress);
                        int lit_count = (int)(currentProgress * 13.0f);
                        if (lit_count > 13) lit_count = 13;

                        char bar[14];
                        memset(bar, ' ', 13);
                        bar[13] = '\0';
                        for (int j = 0; j < lit_count; j++) {
                            bar[j] = '|';
                        }

                        // Overlay the text if it exists
                        if (step.stringParam[0] != '\0') {
                            int text_len = strlen(step.stringParam);
                            int start_pos = (13 - text_len) / 2;
                            if (start_pos < 0) start_pos = 0;
                            strncpy(bar + start_pos, step.stringParam, 13 - start_pos);
                        }

                        updateDisplaySegment(step.targetRow, -1, bar);
                    }
                }
                break;

            case SEQ_CMD_RANDOM_FLICKER_TEXT:
                if (!track.stepInitialized) {
                    if (step.stringParam[0] == '\0') {
                        char currentManualText[14];
                        getFullRowText(step.targetRow, currentManualText);

                        bool is_blank = true;
                        for(int j=0; j<13; j++) {
                            if(currentManualText[j] != ' ') {
                                is_blank = false;
                                break;
                            }
                        }

                        if (is_blank) {
                            char dest_str[17], pres_str[17], last_str[17];
                            getFormattedTimeStrings(dest_str, pres_str, last_str);
                            const char* time_strings[] = {dest_str, pres_str, last_str};
                            strncpy(track.flickerOriginalText, time_strings[step.targetRow], 13);
                        } else {
                            strncpy(track.flickerOriginalText, currentManualText, 13);
                        }
                    } else {
                        strncpy(track.flickerOriginalText, step.stringParam, 13);
                    }
                    track.flickerOriginalText[13] = '\0'; // Ensure null termination
                    track.lastFlickerUpdate = millis();
                    track.stepInitialized = true;
                }
                if (commandElapsed >= (unsigned long)step.intParam2) {
                    updateDisplaySegment(step.targetRow, step.targetSegment, track.flickerOriginalText);
                    advance_step = true;
                } else {
                    if (millis() - track.lastFlickerUpdate > (unsigned long)step.intParam) {
                        static char flicker_buffer[14];
                        strncpy(flicker_buffer, track.flickerOriginalText, 13);
                        flicker_buffer[13] = '\0';

                        for (int j = 0; j < 13; ++j) {
                            if (flicker_buffer[j] == '\0') break;
                            if (random(100) < 30) {
                                flicker_buffer[j] = (char)random(33, 126);
                            }
                        }
                        updateDisplaySegment(step.targetRow, step.targetSegment, flicker_buffer);
                        track.lastFlickerUpdate = millis();
                    }
                }
                break;

            case SEQ_CMD_SCRAMBLE_TEXT:
                if (!track.stepInitialized) {
                    // Initialize the buffer with spaces.
                    memset(track.scrambleBuffer, ' ', 13);
                    track.scrambleBuffer[13] = '\0';
                    track.scrambleCharIndex = 0;
                    track.lastScrambleUpdate = millis();
                    track.lastScrambleLockInTime = millis();
                    track.stepInitialized = true;
                }

                if ((unsigned)track.scrambleCharIndex >= strlen(step.stringParam)) {
                    // Animation is complete, ensure final text is displayed and advance.
                    updateDisplaySegment(step.targetRow, step.targetSegment, step.stringParam);
                    advance_step = true;
                } else {
                    // Check if it's time to lock in the next character.
                    if (millis() - track.lastScrambleLockInTime >= (unsigned long)step.intParam2) {
                        if ((unsigned)track.scrambleCharIndex < strlen(step.stringParam)) {
                            track.scrambleBuffer[track.scrambleCharIndex] = step.stringParam[track.scrambleCharIndex];
                        }
                        track.scrambleCharIndex++;
                        track.lastScrambleLockInTime = millis();
                    }

                    // Check if it's time to update the flickering characters.
                    if (millis() - track.lastScrambleUpdate >= (unsigned long)step.intParam) {
                        // --- FIX: Use a temporary buffer on the stack to prevent heap fragmentation ---
                        char temp_scramble_buffer[14];
                        strcpy(temp_scramble_buffer, track.scrambleBuffer);

                        for (size_t j = track.scrambleCharIndex; j < strlen(step.stringParam); ++j) {
                            temp_scramble_buffer[j] = (char)random(33, 126);
                        }
                        updateDisplaySegment(step.targetRow, step.targetSegment, temp_scramble_buffer);
                        track.lastScrambleUpdate = millis();
                    }
                }
                break;

            case SEQ_CMD_SCROLL_IN:
                // This is a simplified implementation. A more robust one would handle different directions.
                if (!track.stepInitialized) {
                    track.typewriterIndex = 0; // Re-using typewriter state for simplicity
                    track.lastTypewriterUpdate = millis();
                    track.stepInitialized = true;
                }
                if ((unsigned)track.typewriterIndex >= strlen(step.stringParam)) {
                    advance_step = true;
                } else {
                    if (millis() - track.lastTypewriterUpdate > (unsigned long)step.intParam) {
                        track.typewriterIndex++;

                        char text_buffer[14];
                        memset(text_buffer, ' ', 13);
                        text_buffer[13] = '\0';

                        int len_to_copy = min(track.typewriterIndex, (int)strlen(step.stringParam));
                        int start_pos = 13 - len_to_copy;
                        strncpy(text_buffer + start_pos, step.stringParam, len_to_copy);

                        updateDisplaySegment(step.targetRow, -1, text_buffer);
                        track.lastTypewriterUpdate = millis();
                    }
                }
                break;

            case SEQ_CMD_CROSSFADE_TEXT:
                // --- FIX: Use if/else if to prevent race condition ---
                if (!track.stepInitialized) {
                    // Phase 1: Start fade out.
                    track.crossfadePhase = 1;
                    track.isFading = true;
                    track.isFadeIn = false;
                    track.fadeDuration = step.intParam / 2;
                    track.fadeStartTime = millis();
                    track.stepInitialized = true; // Mark that we have started.
                } else if (track.crossfadePhase == 1 && !track.isFading) {
                    // Phase 2: Fade out is complete. Change text and start fade in.
                    track.crossfadePhase = 2;
                    updateDisplaySegment(step.targetRow, step.targetSegment, step.stringParam);
                    track.isFading = true;
                    track.isFadeIn = true;
                    track.fadeDuration = step.intParam / 2;
                    track.fadeStartTime = millis();
                } else if (track.crossfadePhase == 2 && !track.isFading) {
                    // Phase 3: Fade in is complete. End the command.
                    track.crossfadePhase = 0; // Reset for any future use.
                    advance_step = true;
                }
                break;

            case SEQ_CMD_TRIGGER_ANIMATION:
                if (!track.stepInitialized) {
                    // The animation to trigger is passed in intParam.
                    // This is a full takeover, so the original row is ignored.
                    triggerAnimation((AnimationType)step.intParam);
                    track.stepInitialized = true;
                    advance_step = true;
                }
                break;

            case SEQ_CMD_MQTT_PUBLISH:
                if (!track.stepInitialized) {
                    publishMqttMessage(step.stringParam, step.stringParam2);
                    track.stepInitialized = true;
                    advance_step = true;
                }
                break;

            case SEQ_CMD_DISPLAY_HA_SENSOR:
                if (!track.stepInitialized) {
                    track.isWaitingForHAState = true;
                    track.haStateReceived = false;
                    strncpy(track.haSensorTopic, step.stringParam, MAX_SEQ_STRING_LEN -1);
                    track.haSensorTopic[MAX_SEQ_STRING_LEN - 1] = '\0';
                    subscribeToTopic(track.haSensorTopic);
                    track.stepInitialized = true;
                }
                // Check if we've received the state or if we've timed out
                if (track.haStateReceived || commandElapsed > 5000) { // 5-second timeout
                    unsubscribeFromTopic(track.haSensorTopic);
                    track.isWaitingForHAState = false;
                    advance_step = true;
                }
                break;

            // --- End of Sequence ---
            case SEQ_CMD_END:
            case SEQ_CMD_NONE:
                // --- FIX: This is now just a simple cleanup for this track. ---
                // The global cleanup is handled by the new logic at the end of handleSequencer().
                track.reset();
                needsDisplayUpdate = true;
                break;

            default:
                Log_printf(LOG_LEVEL_WARN, "SEQ: Track %d entered unknown command state %d. Aborting.", i, step.command);
                track.reset();
                needsDisplayUpdate = true;
                break;
        }

        if (advance_step) {
            track.currentStep++;
            track.stepStartTime = millis();
            track.stepInitialized = false;
        }
    }

    // --- FIX: Robust End-of-Animation Detection ---
    // This logic reliably detects when the last active track has finished.
    static bool wasAnimatingLastCycle = false;
    bool isAnimatingThisCycle = false;

    for (int i = 0; i < NUM_SEQUENCER_TRACKS; ++i) {
        if (sequencerTracks[i].isActive) {
            isAnimatingThisCycle = true;
            break; // An active track was found, no need to check further.
        }
    }

    // If we were animating on the previous cycle but are not animating on the current one,
    // it means the animation has just completed.
    if (wasAnimatingLastCycle && !isAnimatingThisCycle && !isTransitioningAnimation) {
        doFinalAnimationCleanup();
    }

    // Update the state for the next iteration of the loop.
    wasAnimatingLastCycle = isAnimatingThisCycle;


    if (needsDisplayUpdate) {
        updateNormalClockDisplay();
    }

    // --- FIX: Yield CPU time to prevent task starvation ---
    // A delay of 1 tick is enough to allow other tasks (like the network stack)
    // to run, preventing crashes from memory allocation failures in mDNS, etc.
    vTaskDelay(1);
}

/**
 * @brief Stops all effects on a specific track, restores its brightness, and resets its state.
 * @details This function is a critical cleanup utility. It ensures that when a sequence ends,
 * is aborted, or times out, the corresponding display row is returned to a neutral, visible
 * state. It cancels any ongoing effects and calls the track's `reset()` method to clear all
 * state variables, preventing them from interfering with subsequent animations. It also handles
 * restoring the main display mode after the very last track has finished.
 * @param trackIndex The index of the track (0-2) to clean up.
 */
/**
 * @brief Performs the final, global cleanup after any animation concludes.
 * @details This function is the single source of truth for post-animation cleanup.
 * It restores the display mode, restarts the mDNS service, and notifies the UI.
 * A static boolean guard prevents it from running more than once in the event of
 * overlapping calls, ensuring stability.
 */
void doFinalAnimationCleanup() {
    // Use a static boolean to ensure this cleanup logic only runs once,
    // even if called multiple times in quick succession from different paths.
    static bool cleanupInProgress = false;
    if (cleanupInProgress) {
        return;
    }
    cleanupInProgress = true;

    if (currentAnimationType != ANIMATION_TYPE_MAX) {
        Log_printf(LOG_LEVEL_INFO, "SEQ: Animation %d (%s) completed.", (int)currentAnimationType, animationTypeToString(currentAnimationType));
        currentAnimationType = ANIMATION_TYPE_MAX;
    }

    Log_printf(LOG_LEVEL_INFO, "SEQ: All tracks finished. Cleaning up and restoring pre-animation display mode: %d", preAnimationDisplayMode);
    comprehensiveAnimationCleanup();
    currentSettings.displayMode = preAnimationDisplayMode;
    justFinishedAnimation = true;

    // mDNS service is no longer stopped/restarted during animations.

    // Broadcast completion to the UI now that we are certain the entire animation is done.
    broadcastAnimationComplete();
    cleanupInProgress = false;
}

void stopAllSequences() {
    Log_printf(LOG_LEVEL_INFO, "SEQ: Stopping all active sequences.");
    for (int i = 0; i < NUM_SEQUENCER_TRACKS; i++) {
        if (sequencerTracks[i].isActive) {
            sequencerTracks[i].reset();
            Log_printf(LOG_LEVEL_INFO, "SEQ: Forcibly stopped track %d.", i);
        }
    }
}

/**
 * @brief Safely clears a sequence step without allocating a temporary object on the stack.
 * @details This function manually resets each member of the SequenceStep struct to its
 * default value. This avoids the `step = SequenceStep()` assignment which would
 * create a large temporary object on the stack and cause a crash.
 * @param step The SequenceStep object to clear.
 */
void clearSequenceStep(SequenceStep& step) {
    step.command = SEQ_CMD_NONE;
    step.targetRow = 0;
    step.targetSegment = 0;
    step.intParam = 0;
    step.intParam2 = 0;
    step.stringParam[0] = '\0';
    step.stringParam2[0] = '\0';
}

/**
 * @brief Safely clears a sequencer track without allocating a temporary object on the stack.
 * @details This function is a critical fix for a stack overflow bug. The `SequencerTrack`
 * struct is too large to be created as a temporary object on the stack. Instead of doing
 * `track = SequencerTrack()`, this function manually resets each member of the struct
 * to its default value, which has a negligible impact on the stack. It uses the
 * `clearSequenceStep` helper to safely clear the large `steps` array.
 * @param track The SequencerTrack object to clear.
 */

void runCrossfadeTest() {
    Log_printf(LOG_LEVEL_INFO, "SEQ_TEST: --- Running Crossfade Fix Test ---");

    // Reset track 0 for a clean test
    sequencerTracks[0].reset();

    int i = 0;
    sequencerTracks[0].steps[i++] = {SEQ_CMD_SET_TEXT, 0, 0, 0, 0, "Initial", ""};
    sequencerTracks[0].steps[i++] = {SEQ_CMD_WAIT, 0, 0, 2000, 0, "", ""};
    sequencerTracks[0].steps[i++] = {SEQ_CMD_CROSSFADE_TEXT, 0, 0, 2000, 0, "Faded", ""};
    sequencerTracks[0].steps[i++] = {SEQ_CMD_SET_TEXT, 0, 0, 0, 0, "SUCCESS", ""}; // This should appear if the fix works
    sequencerTracks[0].steps[i++] = {SEQ_CMD_END, 0, 0, 0, 0, "", ""};

    sequencerTracks[0].isActive = true;
    sequencerTracks[0].stepStartTime = millis();
    sequencerTracks[0].trackStartTime = millis();
    sequencerTracks[0].originalBrightness = currentSettings.brightness;

    Log_printf(LOG_LEVEL_INFO, "SEQ_TEST: --- Crossfade Fix Test Started ---");
}


void runSequencerTest() {
    Log_printf(LOG_LEVEL_INFO, "SEQ_TEST: --- Running Comprehensive Sequencer Test ---");

    // Reset all tracks to ensure a clean slate
    for (int i = 0; i < NUM_SEQUENCER_TRACKS; ++i) {
        sequencerTracks[i].reset();
    }

    // --- Track 0: Top Row - Demonstrates text, countdown, and MQTT ---
    int i = 0;
    sequencerTracks[0].steps[i++] = {SEQ_CMD_SET_BRIGHTNESS, 0, -1, 7, 0, "", ""};
    sequencerTracks[0].steps[i++] = {SEQ_CMD_SET_TEXT, 0, 0, 0, 0, "SEQ", ""};
    sequencerTracks[0].steps[i++] = {SEQ_CMD_COUNTDOWN, 0, 1, 3, 1000, "", ""}; // Countdown 3..2..1 in day segment
    sequencerTracks[0].steps[i++] = {SEQ_CMD_TYPEWRITER, 0, 2, 100, 0, "TEST", ""}; // Type "TEST" in year segment
    sequencerTracks[0].steps[i++] = {SEQ_CMD_WAIT, 0, 0, 1000, 0, "", ""};
    sequencerTracks[0].steps[i++] = {SEQ_CMD_MQTT_PUBLISH, 0, 0, 0, 0, "timecircuits/test", "Track 0 Finished"};
    sequencerTracks[0].steps[i++] = {SEQ_CMD_RESTORE_ROW, 0, 0, 0, 0, "", ""};
    sequencerTracks[0].steps[i++] = {SEQ_CMD_END, 0, 0, 0, 0, "", ""};
    sequencerTracks[0].isActive = true;
    sequencerTracks[0].stepStartTime = millis();
    sequencerTracks[0].trackStartTime = millis();

    // --- Track 1: Middle Row - Demonstrates loops and high-level effects ---
    i = 0;
    sequencerTracks[1].steps[i++] = {SEQ_CMD_LOOP_START, 1, 0, 2, 0, "", ""}; // Loop twice
    sequencerTracks[1].steps[i++] = {SEQ_CMD_SCANNER, 1, -1, 2000, 80, "", ""}; // Scan for 2s, 80ms step
    sequencerTracks[1].steps[i++] = {SEQ_CMD_LOOP_END, 1, 0, 0, 0, "", ""};
    sequencerTracks[1].steps[i++] = {SEQ_CMD_BAR_GRAPH, 1, -1, 0, 150, "", ""}; // Bar graph, 150ms step
    sequencerTracks[1].steps[i++] = {SEQ_CMD_WAIT, 1, 0, 1000, 0, "", ""};
    sequencerTracks[1].steps[i++] = {SEQ_CMD_TRIGGER_ANIMATION, 1, 0, (int)ANIMATION_WAVE_FLICKER, 0, "", ""}; // Trigger a global animation
    sequencerTracks[1].steps[i++] = {SEQ_CMD_END, 1, 0, 0, 0, "", ""};
    sequencerTracks[1].isActive = true;
    sequencerTracks[1].stepStartTime = millis();
    sequencerTracks[1].trackStartTime = millis();

    // --- Track 2: Bottom Row - Demonstrates more visual effects and HA integration ---
    i = 0;
    sequencerTracks[2].steps[i++] = {SEQ_CMD_WIPE, 2, -1, 100, 0, "WIPE TEST", ""};
    sequencerTracks[2].steps[i++] = {SEQ_CMD_RANDOM_FLICKER_TEXT, 2, 2, 2000, 100, "FLICKER", ""};
    sequencerTracks[2].steps[i++] = {SEQ_CMD_SET_TEXT, 2, 0, 0, 0, "GET", ""};
    sequencerTracks[2].steps[i++] = {SEQ_CMD_SET_TEXT, 2, 1, 0, 0, "HA", ""};
    sequencerTracks[2].steps[i++] = {SEQ_CMD_DISPLAY_HA_SENSOR, 2, 2, 0, 0, "homeassistant/sensor/test/state", ""};
    sequencerTracks[2].steps[i++] = {SEQ_CMD_WAIT, 2, 0, 2000, 0, "", ""};
    sequencerTracks[2].steps[i++] = {SEQ_CMD_FADE_OUT, 2, -1, 1000, 0, "", ""};
    sequencerTracks[2].steps[i++] = {SEQ_CMD_END, 2, 0, 0, 0, "", ""};
    sequencerTracks[2].isActive = true;
    sequencerTracks[2].stepStartTime = millis();
    sequencerTracks[2].trackStartTime = millis();

    Log_printf(LOG_LEVEL_INFO, "SEQ_TEST: --- Comprehensive Sequencer Test Started ---");
}

void triggerAnimation(AnimationType animType) {
    // This function is a full takeover. It replaces all running tracks
    // with the new animation.
    Log_printf(LOG_LEVEL_INFO, "SEQ: Triggering new animation %d (%s). All current tracks will be replaced.", (int)animType, animationTypeToString(animType));

    // --- FIX: Set the transition flag to prevent premature cleanup ---
    isTransitioningAnimation = true;

    // --- FIX: Save the current display mode so it can be restored after the animation. ---
    preAnimationDisplayMode = currentSettings.displayMode;
    currentSettings.displayMode = -1; // Pause the main display loop

    // --- NEW: Store the current animation type for logging completion ---
    currentAnimationType = animType;

    // --- FIX: Use a static buffer to prevent heap fragmentation from frequent allocations ---
    // The SequencerTrack struct is very large (~5.5KB), so creating an array on the stack
    // would cause a stack overflow. Using a static buffer allocates this memory once at
    // compile time, avoiding both stack overflow and runtime heap fragmentation issues
    // that were causing crashes during rapid animation changes (e.g., preset cycling).
    static SequencerTrack temp_tracks[NUM_SEQUENCER_TRACKS];

    // --- FIX: Generate the animation into the temporary buffer *before* stopping the old one. ---
    // This prevents use-after-free issues where stopAllSequences() might clear a string
    // that is still referenced by a track that is about to be replaced.

    // Generate the requested animation into the temporary heap-allocated tracks.
    generateAnimationSequence(animType, temp_tracks);

    // Now that the new animation is prepared, stop all currently running tracks.
    stopAllSequences();

    // Copy the steps from ALL generated tracks to the main sequencer tracks.
    for (int j = 0; j < NUM_SEQUENCER_TRACKS; ++j) {
        // We don't need to call reset() here because stopAllSequences() already did.
        for (int i = 0; i < MAX_SEQUENCE_STEPS; ++i) {
            sequencerTracks[j].steps[i] = temp_tracks[j].steps[i];
            if (temp_tracks[j].steps[i].command == SEQ_CMD_END) {
                break; // Stop copying after the END command for this track.
            }
        }

        // Activate the track if it has any commands.
        if (sequencerTracks[j].steps[0].command != SEQ_CMD_NONE) {
             sequencerTracks[j].isActive = true;
             sequencerTracks[j].trackStartTime = millis();
             sequencerTracks[j].stepStartTime = millis();
             sequencerTracks[j].originalBrightness = currentSettings.brightness;
        }
    }

    // Static memory does not need to be manually deallocated.

    // --- FIX: Clear the transition flag now that the new animation is active ---
    isTransitioningAnimation = false;
}

void startSequencerMarquee(SequencerTrack& track, const char* text) {
    if (text == nullptr || text[0] == '\0') {
        track.isMarqueeActive = false;
        return;
    }
    track.isMarqueeActive = true;
    // Safely construct the padded string for scrolling
    snprintf(track.marqueeText, sizeof(track.marqueeText), "             %s             ", text);
    track.marqueeScrollPosition = 0;
    track.lastMarqueeScrollTime = millis();
    Log_printf(LOG_LEVEL_INFO, "SEQ: Marquee started on track %d", track.steps[track.currentStep].targetRow);
}

void handleAllSequencerMarquees() {
    for (int i = 0; i < 3; ++i) {
        SequencerTrack& track = sequencerTracks[i];

        if (track.isActive && track.isMarqueeActive) {
            if (millis() - track.lastMarqueeScrollTime > 120) {
                track.marqueeScrollPosition++;

                if ((unsigned)track.marqueeScrollPosition > strlen(track.marqueeText) - 13) {
                    track.isMarqueeActive = false;
                    Log_printf(LOG_LEVEL_INFO, "SEQ: Marquee finished on track %d.", i);

                    SequenceStep& step = track.steps[track.currentStep];
                    char centeredText[14];
                    memset(centeredText, ' ', 13);
                    centeredText[13] = '\0';
                    int text_len = strlen(step.stringParam);
                    int padding = (13 - text_len) / 2;
                    if (padding < 0) padding = 0;
                    strncpy(centeredText + padding, step.stringParam, 13 - padding);

                    updateDisplaySegment(i, -1, centeredText);

                } else {
                    char displayText[14];
                    strncpy(displayText, track.marqueeText + track.marqueeScrollPosition, 13);
                    displayText[13] = '\0';
                    updateDisplaySegment(i, -1, displayText);
                }
                track.lastMarqueeScrollTime = millis();
            }
        }
    }
}