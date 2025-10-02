#include "AnimationManager.h"

// --- NEW: Global flag to prevent display updates until boot sequence is complete ---
bool bootSequenceCompleted = false;

#include "EventManager.h"
#include "HardwareControl.h"
#include "DebugLog.h"
#include "AnimationSequences.h"

// --- NEW: A global timeout for any single animation sequence track ---
#define MAX_SEQUENCE_DURATION 60000 // 60 seconds
#include "DisplayManager.h"
#include "MqttManager.h"

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

// Helper function prototypes
void playReconfiguringSound();
void resetDisplayToNormal();
static void comprehensiveAnimationCleanup();

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

/**
 * @brief Triggers a short flash effect on a specific display segment using the sequencer.
 * @param row The display row (0-2) to flash.
 * @param segment The segment within the row (0-3) to flash.
 * @param duration The total duration of the flash effect in milliseconds.
 */
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
void playSoundAndSetNextPhase(const char* filename, AnimationPhase nextPhase) {
    if (hardwareInitialized && currentSettings.timeTravelSoundToggle) {
        playSound(filename);
    }
    nextPhaseAfterSound = nextPhase;
    currentPhase = ANIM_WAIT_FOR_SOUND;
    animationStartTime = millis();
}

/**
 * @brief Initiates the multi-stage time travel animation sequence.
 */
/**
 * @brief Initiates the multi-stage time travel animation sequence.
 * @details This function acts as the entry point for the main cinematic time travel
 * animation. It sets the global `isAnimating` flag to true, which prevents other
 * display modes from interfering, and sets the initial animation phase. The actual
 * animation is handled by the `handleDisplayAnimation` state machine, which is
 * called on each iteration of the main loop.
 */
void startTimeTravelAnimation() {
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

/**
 * @brief The main state machine for the CINEMATIC time travel animation. Called in the main loop.
 */
/**
 * @brief Manages the state machine for the main cinematic time travel animation.
 * @details This function is called on every loop iteration while `isAnimating` is true.
 * It uses a `switch` statement to progress through the different phases of the
 * animation (e.g., power up, time acceleration, arrival). It handles the timing for
 * each phase and calls the appropriate low-level animation functions from HardwareControl.cpp.
 */
void handleDisplayAnimation() {
    if (!isAnimating || !hardwareInitialized) return;
#if ENABLE_HARDWARE
    unsigned long elapsed = millis() - animationStartTime;

    // --- FIX: Add a global timeout to prevent the animation from hanging indefinitely ---
    const unsigned long MAX_ANIMATION_DURATION = 30000; // 30 seconds
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
                    playSound("engine_rev.mp3");
                    break;
                case ANIM_ARRIVAL:
                    playSound("time_travel.mp3");
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
            // Failsafe to prevent getting stuck in an unknown state
            isAnimating = false;
            currentPhase = ANIM_INACTIVE;
            lastPhase = ANIM_INACTIVE;
            break;
    }
#endif
}


// --- NEW FUNCTIONS FOR STYLED ANIMATION ---

/**
 * @brief Initiates the styled animation sequence for scheduled events.
 */
void startStyledAnimation() {
    // Attempt to take the mutex. If we can't get it, another task is trying
    // to start an animation, so we should just exit.
    if (xSemaphoreTake(xAnimationStartMutex, (TickType_t)10) != pdTRUE) {
        Serial.println("ANIM_LOG: Styled animation mutex grab FAILED.");
        return;
    }
    Serial.println("ANIM_LOG: Styled animation mutex grab SUCCESS.");

    // We have the mutex, now we can safely check the animation flags.
    if (isStyledAnimating || isAnimating) {
        xSemaphoreGive(xAnimationStartMutex); // Release the mutex before returning.
        return;
    }

    // Set the animation flag to prevent other tasks from starting another animation.
    isStyledAnimating = true;

    // The critical section is over, release the mutex.
    xSemaphoreGive(xAnimationStartMutex);

    getFormattedTimeStrings(old_dest_str, old_pres_str, old_last_str);

    styledAnimationStartTime = millis();
    currentStyledPhase = ANIM_START; // Set initial phase to ANIM_START
    updateHaStatus("Animating");

    // Set the animation style for this run
    if (currentSettings.animationStyle == ANIMATION_ALL_DISPLAYS_RANDOM) {
        const int validAnimationStyles[] = {
            ANIMATION_SEQUENTIAL_FLICKER, ANIMATION_RANDOM_FLICKER,
            ANIMATION_COUNTING_UP, ANIMATION_WAVE_FLICKER,
            ANIMATION_TORNADO_FLICKER, ANIMATION_CAPACITOR_CHARGE_UP, ANIMATION_DIGITAL_RAIN,
            ANIMATION_WAVEFORM_COLLAPSE, ANIMATION_TIMELINE_SKIM, ANIMATION_TEMPORAL_DESYNC,
            ANIMATION_GLITCHY_JUMP_CUT, ANIMATION_PLASMA_WARM_UP, ANIMATION_TIME_WARP_STREAKS,
            ANIMATION_CHARACTER_SCANLINE, ANIMATION_FOCUS_IN, ANIMATION_CODE_BREAKER,
            ANIMATION_TEMPORAL_PARADOX, ANIMATION_DIGIT_CASCADE, ANIMATION_ELECTRIC_SURGE,
            ANIMATION_FLIP_DISC_DISPLAY, ANIMATION_INTERFERENCE_PATTERN,
            ANIMATION_ALL_DISPLAYS_RANDOM
        };
        int numStyles = sizeof(validAnimationStyles) / sizeof(validAnimationStyles[0]);
        int randomIndex = random(0, numStyles);
        randomAnimationStyle = validAnimationStyles[randomIndex];
    } else {
        randomAnimationStyle = currentSettings.animationStyle;
    }
}

/**
 * @brief The state machine for the STYLED time travel animation.
 */
void handleStyledAnimation() {
    if (!isStyledAnimating || !hardwareInitialized) return;
#if ENABLE_HARDWARE
    unsigned long elapsed = millis() - styledAnimationStartTime;

    // State variables for the "Unstable Flux" (Random Flicker) animation
    static int glitchRow = -1;
    static unsigned long lastGlitchTriggerTime = 0;
    static AnimationPhase lastStyledPhase = ANIM_INACTIVE;
    // --- FIX: Buffers to hold the formatted time strings for one animation cycle ---
    static char dest_str[17], pres_str[17], last_str[17];

    // If we are entering a new phase, log it.
    if (currentStyledPhase != lastStyledPhase) {
        Serial.printf("ANIM_LOG: Entering styled animation phase: %d\n", currentStyledPhase);
        // If we are entering the first phase of the animation, reset local static variables
        if (currentStyledPhase == ANIM_FLICKER) {
            glitchRow = -1;
            lastGlitchTriggerTime = 0;
            // --- FIX: Get the time strings once at the start of the animation ---
            getFormattedTimeStrings(dest_str, pres_str, last_str);
        }
        lastStyledPhase = currentStyledPhase;
    }


    switch (currentStyledPhase) {
        case ANIM_START:
            // This new state plays the sound and immediately transitions to the next state.
            playSound("/electric_sparks.mp3");
            Serial.println("ANIM_LOG: Keypad sound requested.");
            currentStyledPhase = ANIM_WAIT_FOR_KEYPAD_SOUND;
            styledAnimationStartTime = millis(); // Reset timer for the wait phase
            break;

        case ANIM_WAIT_FOR_KEYPAD_SOUND:
            if (elapsed > 2000) { // Failsafe timeout of 2s
                currentStyledPhase = ANIM_FLICKER;
                styledAnimationStartTime = millis(); // Reset timer for the flicker phase
            } else {
                // Yield to other tasks to prevent blocking the scheduler
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            break;

        case ANIM_FLICKER:
            if (elapsed < 10000) { // --- FIX: Use a fixed 10-second duration ---
                switch (randomAnimationStyle) {
                    case ANIMATION_SEQUENTIAL_FLICKER:
                        animateSequentialFlicker(elapsed, 10000);
                        break;

                    case ANIMATION_RANDOM_FLICKER:
                        // "Unstable Flux": Mostly normal, with brief, intermittent glitches on a single row.
                        if (glitchRow != -1) {
                            // We are in a glitch state
                            DisplayRow& rowToGlitch = (glitchRow == 0) ? destRow : ((glitchRow == 1) ? presRow : lastRow);
                            animateDisplayRowRandomly(rowToGlitch);
                            if (millis() - lastGlitchTriggerTime > 200) { // Glitch lasts for 200ms
                                glitchRow = -1; // End the glitch
                            }
                        } else {
                            // Not glitching, show the normal clock
                            updateNormalClockDisplay();
                            // Check if it's time to trigger a new glitch
                            if (millis() - lastGlitchTriggerTime > 100 && random(100) < 75) {
                                glitchRow = random(3); // Pick a row (0, 1, or 2)
                                lastGlitchTriggerTime = millis();
                            }
                        }
                        break;

                    case ANIMATION_COUNTING_UP:
                        animateLockOnSequence(elapsed, 10000);
                        break;
                    case ANIMATION_TIMELINE_SKIM:
                        animateUnstableSkim(elapsed, 10000, currentSettings.destinationYear);
                        break;
                    case ANIMATION_WAVE_FLICKER:
                    case ANIMATION_WAVEFORM_COLLAPSE:
                        animateWaveformCollapse(elapsed, 10000);
                        break;
                    case ANIMATION_CAPACITOR_CHARGE_UP:
                        animateCapacitorChargeUp(elapsed, 10000);
                        break;
                    case ANIMATION_DIGITAL_RAIN:
                        animateDigitalRain(elapsed, 10000);
                        break;

                    case ANIMATION_ALL_DISPLAYS_RANDOM:
                        // NEW: "Corrupted Data" effect - stable display with random characters
                        animateCorruptedData();
                        break;

                    case ANIMATION_TEMPORAL_DESYNC:
                        animateTemporalDesync();
                        break;

                    case ANIMATION_GLITCHY_JUMP_CUT:
                        animateGlitchyJumpCut(elapsed, 10000);
                        break;

                    case ANIMATION_PLASMA_WARM_UP:
                        animatePlasmaWarmUp(elapsed, 10000);
                        break;

                    case ANIMATION_TIME_WARP_STREAKS:
                        animateTimeWarpStreaks(elapsed, 10000, dest_str, pres_str, last_str);
                        break;

                    case ANIMATION_CHARACTER_SCANLINE:
                        animateCharacterScanline(elapsed, 10000, dest_str, pres_str, last_str);
                        break;

                    case ANIMATION_FOCUS_IN:
                        animateFocusIn(elapsed, 10000, dest_str, pres_str, last_str);
                        break;

                    case ANIMATION_CODE_BREAKER:
                        animateCodeBreaker(elapsed, 10000, dest_str, pres_str, last_str);
                        break;

                    case ANIMATION_TEMPORAL_PARADOX:
                        animateTemporalParadox(elapsed, 10000, dest_str, pres_str, last_str);
                        break;

                    case ANIMATION_DIGIT_CASCADE:
                        animateDigitCascade(elapsed, 10000, dest_str, pres_str, last_str);
                        break;

                    case ANIMATION_ELECTRIC_SURGE:
                        animateElectricSurge(elapsed, 10000, dest_str, pres_str, last_str);
                        break;

                    case ANIMATION_FLIP_DISC_DISPLAY:
                        animateFlipDiscDisplay(elapsed, 10000, dest_str, pres_str, last_str);
                        break;

                    case ANIMATION_INTERFERENCE_PATTERN:
                        animateInterferencePattern(elapsed, 10000, dest_str, pres_str, last_str);
                        break;

                    case ANIMATION_TORNADO_FLICKER:
                    default:
                        // This is now the most intense, full-power flicker effect
                        animateTornadoFlicker();
                        break;
                }
            } else {
                currentStyledPhase = ANIM_LANDING;
                styledAnimationStartTime = millis();
            }
            break;

        case ANIM_LANDING:
            if (elapsed < 1000) {
                animateTornadoFlicker();
            } else {
                // Transition to the cool down phase
                currentStyledPhase = ANIM_COOL_DOWN;
                styledAnimationStartTime = millis();
            }
            break;

        case ANIM_COOL_DOWN:
            if (elapsed > 500) {
                comprehensiveAnimationCleanup();
                isStyledAnimating = false;
                currentStyledPhase = ANIM_INACTIVE;
                updateHaStatus("Idle");
                Serial.println("ANIM_LOG: Styled animation finished. Broadcasting completion.");
                broadcastAnimationComplete();
            }
            break;

        default:
            comprehensiveAnimationCleanup();
            isStyledAnimating = false;
            currentStyledPhase = ANIM_INACTIVE;
            Serial.println("ANIM_LOG: Styled animation entered unknown state. Forcing cleanup.");
            broadcastAnimationComplete();
            break;
    }
#endif
}


// --- OTHER EFFECTS ---

/**
 * @brief Handles the "temporal echo" effect after a time jump.
 */
/**
 * @brief Handles the "temporal echo" visual effect after a time jump.
 * @details For a short period after an animation sequence completes, this function
 * creates a lingering effect by randomly flickering the "Present Time" display,
 * suggesting a temporary instability in the timeline.
 */
void handleTemporalEcho() {
    if (!isEchoEffectActive || isAnimating || isStyledAnimating || !hardwareInitialized) return;
#if ENABLE_HARDWARE
    if (millis() - echoEffectStartTime > 60000) { // Effect lasts for 1 minute
        isEchoEffectActive = false;
        return;
    }

    // Randomly flicker the "Present Time" display
    if (random(100) < 10) {
        animateDisplayRowRandomly(presRow);
    }
#endif
}

// --- BOOT SEQUENCE ---

/**
 * @brief Starts the boot-up animation.
 */
// In AnimationManager.cpp
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
                presRow.month.writeDisplay();
                presRow.day.writeDisplay();
                presRow.year.writeDisplay();
                presRow.time.writeDisplay();
                playSound("/hum.mp3");
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
                    playSound("/relay_activation.mp3");
                    blankAllDisplays();
                    // Instantly display the text instead of typing it out
                    printToDisplay(destRow.month, "");
                    printToDisplay(destRow.day, " TM");
                    printToDisplay(destRow.year, "CIRC");
                    printToDisplay(destRow.time, "UITS");
                    printToDisplay(presRow.month, "");
                    printToDisplay(presRow.day, "");
                    printToDisplay(presRow.year, "ACTI");
                    printToDisplay(presRow.time, "VATE");
                    // Explicitly write to the display hardware
                    destRow.month.writeDisplay();
                    destRow.day.writeDisplay();
                    destRow.year.writeDisplay();
                    destRow.time.writeDisplay();
                    presRow.month.writeDisplay();
                    presRow.day.writeDisplay();
                    presRow.year.writeDisplay();
                    presRow.time.writeDisplay();

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
                playSound("/flux_capacitor_power_on.mp3");
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
                        presRow.month.writeDisplay();
                        presRow.day.writeDisplay();
                        presRow.year.writeDisplay();
                        presRow.time.writeDisplay();
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
                    playSound("/keypad_beeps.mp3");
                    stateActionCompleted = true;
                }

                int currentSecond = elapsed / 2000;

                if (currentSecond != lastDiagSecond) {
                    blankAllDisplays();

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
                    playSound("/engine_rev.mp3");
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

                    printToDisplay(destRow.month, "SYS");
                    printToDisplay(destRow.day, "IS");
                    printToDisplay(presRow.year, "LIVE");
                    printToDisplay(presRow.time, " NOW");
                    destRow.year.writeDisplay();
                    destRow.time.writeDisplay();
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
                    playSound("/time_travel.mp3");
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
                    playSound("/arrival_chime.mp3");
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
                    printToDisplay(destRow.year, "ARRI");
                    printToDisplay(destRow.time, "VAL");
                    printToDisplay(presRow.year, "OUTA");
                    printToDisplay(presRow.time, "TIME");

                    destRow.year.writeDisplay();
                    destRow.time.writeDisplay();
                    presRow.year.writeDisplay();
                    presRow.time.writeDisplay();

                    stateActionCompleted = true; // Mark that the initial action is done
                }

                // Display the "WELCOME" message after a delay
                if (elapsed > 2000) { // Show "WELCOME" after 2 seconds
                    printToDisplay(lastRow.year, " WEL");
                    printToDisplay(lastRow.time, "COME");
                    lastRow.year.writeDisplay();
                    lastRow.time.writeDisplay();
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
void resetDisplayToNormal() {
    // Clear any active override message flags
    isMessageOverrideActive = false;

    // Reset manual text override for all display segments
    for (int r = 0; r < 3; ++r) {
        isRowInManualMode[r] = false;
        for (int s = 0; s < 4; ++s) {
            manualDisplayText[r][s] = "";
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
    for (int r = 0; r < 3; ++r) {
        isRowInManualMode[r] = false;
        for (int s = 0; s < 4; ++s) {
            manualDisplayText[r][s] = "";
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
 * @brief Handles the execution of scripted command sequences.
 * @details This function is called on every main loop iteration. It checks for
 * active sequencer tracks and processes their commands one by one. It supports
 * parallel execution of tracks on different display rows. It now manages all
 * effect states (fade, pulse, flash) locally within each track.
 */
void handleSequencer() {
    bool needsDisplayUpdate = false;
    DisplayRow* rows[] = {&destRow, &presRow, &lastRow};

    for (int i = 0; i < 3; i++) {
        SequencerTrack& track = sequencerTracks[i];
        DisplayRow& row = *rows[i];

        // --- NEW: Check for track timeout ---
        if (track.isActive && (millis() - track.trackStartTime > MAX_SEQUENCE_DURATION)) {
            Log_printf(LOG_LEVEL_WARN, "SEQ: Track %d timed out after %d ms. Aborting.", i, MAX_SEQUENCE_DURATION);
            stopAndCleanupTrack(i);
            needsDisplayUpdate = true;
            continue; // Skip to the next track
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
                } else if (millis() - track.lastPulseToggle[s] > 750) {
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
                    restoreDisplayRow(0);
                    restoreDisplayRow(1);
                    restoreDisplayRow(2);
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
                    playSound(step.stringParam.c_str());
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
                            stopAndCleanupTrack(i);
                            break; // Abort this command.
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
                        stopAndCleanupTrack(i);
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
                    if (step.targetSegment == -1) { // Apply to all segments
                        for (int s = 0; s < 4; s++) {
                            track.isPulsing[s] = true;
                            track.pulseEndTimes[s] = millis() + step.intParam;
                            track.pulseStates[s] = true;
                            track.lastPulseToggle[s] = millis();
                        }
                    } else { // Apply to a single segment
                        track.isPulsing[step.targetSegment] = true;
                        track.pulseEndTimes[step.targetSegment] = millis() + step.intParam;
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
                    // Helper lambda to format and display the countdown string
                    auto display_countdown = [&](int value) {
                        // Combine the prefix text (if any) with the current countdown value
                        std::string text_to_display = step.stringParam + " " + std::to_string(value);

                        // Right-align the combined string on the 13-character display
                        if (text_to_display.length() > 13) {
                            // If the string is too long, truncate from the left
                            text_to_display = text_to_display.substr(text_to_display.length() - 13);
                        } else {
                            // Otherwise, pad with spaces on the left
                            text_to_display = std::string(13 - text_to_display.length(), ' ') + text_to_display;
                        }

                        // This command is assumed to always target the full row
                        updateDisplaySegment(step.targetRow, -1, text_to_display);
                    };

                    // Validate the delay parameter. If it's invalid (e.g., negative), default to 1000ms.
                    unsigned long countdown_delay = (step.intParam2 > 0) ? (unsigned long)step.intParam2 : 1000;

                    if (!track.stepInitialized) {
                        // Initialization: Set the starting value and display it immediately.
                        track.countdownValue = step.intParam;
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
                        std::string visual = step.stringParam;
                        if (visual.empty()) visual = "#";
                        int visual_len = visual.length();
                        std::string scan_str(13, ' '); // 13 spaces

                        // Correctly place the visual without going out of bounds
                        scan_str.replace(track.scannerPosition, visual_len, visual);
                        updateDisplaySegment(step.targetRow, -1, scan_str);

                        if (track.scannerDirection) { // moving right
                            track.scannerPosition++;
                            // Corrected boundary check: reverse when the *end* of the visual hits the edge
                            if (track.scannerPosition + visual_len > 13) {
                                track.scannerPosition = 13 - visual_len;
                                track.scannerDirection = false;
                            }
                        } else { // moving left
                            track.scannerPosition--;
                            // Corrected boundary check: reverse when the *start* of the visual hits the edge
                            if (track.scannerPosition < 0) {
                                track.scannerPosition = 0;
                                track.scannerDirection = true;
                            }
                        }
                        track.lastScannerUpdate = millis();
                    }
                }
                break;

            case SEQ_CMD_TYPEWRITER:
                if (!track.stepInitialized) {
                    track.typewriterIndex = 0;
                    track.lastTypewriterUpdate = millis();
                    updateDisplaySegment(step.targetRow, step.targetSegment, ""); // Clear segment first
                    track.stepInitialized = true;
                }
                if ((unsigned)track.typewriterIndex >= step.stringParam.length()) {
                    advance_step = true;
                } else {
                    if (millis() - track.lastTypewriterUpdate > (unsigned long)step.intParam) {
                        track.typewriterIndex++;
                        updateDisplaySegment(step.targetRow, step.targetSegment, step.stringParam.substr(0, track.typewriterIndex));
                        track.lastTypewriterUpdate = millis();
                    }
                }
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
                        std::string wipe_str = "             ";
                        for(int j=0; j < track.wipeSegment; j++) {
                            // --- FIX: Add bounds check to prevent reading past the end of the string ---
                            if (j < (int)step.stringParam.length()) {
                                wipe_str[j] = step.stringParam[j];
                            }
                        }
                        updateDisplaySegment(step.targetRow, 0, wipe_str.substr(0,3));
                        updateDisplaySegment(step.targetRow, 1, wipe_str.substr(3,2));
                        updateDisplaySegment(step.targetRow, 2, wipe_str.substr(5,4));
                        updateDisplaySegment(step.targetRow, 3, wipe_str.substr(9,4));
                        track.wipeSegment++;
                        track.lastWipeUpdate = millis();
                    }
                }
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
                        std::string final_bar = "|||||||||||||"; // Use '|' character
                        if (!step.stringParam.empty()) {
                            int text_len = step.stringParam.length();
                            int start_pos = (13 - text_len) / 2;
                            if (start_pos < 0) start_pos = 0;
                            final_bar.replace(start_pos, text_len, step.stringParam);
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

                        std::string bar = "             "; // Use spaces for the empty part
                        for (int j = 0; j < lit_count; j++) {
                            bar[j] = '|'; // Use '|' character
                        }

                        // Overlay the text if it exists
                        if (!step.stringParam.empty()) {
                            int text_len = step.stringParam.length();
                            int start_pos = (13 - text_len) / 2;
                            if (start_pos < 0) start_pos = 0;
                            bar.replace(start_pos, text_len, step.stringParam);
                        }

                        updateDisplaySegment(step.targetRow, -1, bar);
                    }
                }
                break;

            case SEQ_CMD_RANDOM_FLICKER_TEXT:
                if (!track.stepInitialized) {
                    track.flickerOriginalText = step.stringParam;
                    track.lastFlickerUpdate = millis();
                    track.stepInitialized = true;
                }
                if (commandElapsed >= (unsigned long)step.intParam) {
                    // Restore original text at the end
                    updateDisplaySegment(step.targetRow, step.targetSegment, track.flickerOriginalText);
                    advance_step = true;
                } else {
                    if (millis() - track.lastFlickerUpdate > (unsigned long)step.intParam2) {
                        std::string temp = track.flickerOriginalText;
                        for (size_t j = 0; j < temp.length(); ++j) {
                            if (random(100) < 30) { // 30% chance to flicker a character
                                temp[j] = (char)random(33, 126);
                            }
                        }
                        updateDisplaySegment(step.targetRow, step.targetSegment, temp);
                        track.lastFlickerUpdate = millis();
                    }
                }
                break;

            case SEQ_CMD_SCRAMBLE_TEXT:
                if (!track.stepInitialized) {
                    track.scrambleCurrentText = std::string(step.stringParam.length(), ' ');
                    track.scrambleCharIndex = 0;
                    track.lastScrambleUpdate = millis();
                    track.lastScrambleLockInTime = millis();
                    track.stepInitialized = true;
                }

                if ((unsigned)track.scrambleCharIndex >= step.stringParam.length()) {
                    // Animation is complete, ensure final text is displayed and advance.
                    updateDisplaySegment(step.targetRow, step.targetSegment, step.stringParam);
                    advance_step = true;
                } else {
                    // Check if it's time to lock in the next character.
                    if (millis() - track.lastScrambleLockInTime >= (unsigned long)step.intParam2) {
                        // --- FIX: Lock in the character before incrementing the index ---
                        if (track.scrambleCharIndex < step.stringParam.length()) {
                            track.scrambleCurrentText[track.scrambleCharIndex] = step.stringParam[track.scrambleCharIndex];
                        }
                        track.scrambleCharIndex++;
                        track.lastScrambleLockInTime = millis();
                    }

                    // Check if it's time to update the flickering characters.
                    if (millis() - track.lastScrambleUpdate >= (unsigned long)step.intParam) {
                        // Build the string to display, starting with the current locked-in state.
                        std::string temp_scramble = track.scrambleCurrentText;
                        // Scramble the characters that haven't been locked in yet.
                        for (size_t j = track.scrambleCharIndex; j < temp_scramble.length(); ++j) {
                            temp_scramble[j] = (char)random(33, 126);
                        }
                        updateDisplaySegment(step.targetRow, step.targetSegment, temp_scramble);
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
                if ((unsigned)track.typewriterIndex >= step.stringParam.length()) {
                    advance_step = true;
                } else {
                    if (millis() - track.lastTypewriterUpdate > (unsigned long)step.intParam) {
                        track.typewriterIndex++;
                        std::string text = step.stringParam.substr(0, track.typewriterIndex);
                        while(text.length() < 13) text = " " + text;
                        updateDisplaySegment(step.targetRow, 0, text.substr(0,3));
                        updateDisplaySegment(step.targetRow, 1, text.substr(3,2));
                        updateDisplaySegment(step.targetRow, 2, text.substr(5,4));
                        updateDisplaySegment(step.targetRow, 3, text.substr(9,4));
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
                    track.haSensorTopic = step.stringParam;
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
                { // New scope for local variable
                    // This is a natural end to a sequence. Check if it's the very last track.
                    bool wasLastTrack = true;
                    for (int j = 0; j < 3; j++) {
                        if (i != j && sequencerTracks[j].isActive) {
                            wasLastTrack = false;
                            break;
                        }
                    }
                    // If it was the last active track, now is the correct time to signal completion.
                    if (wasLastTrack) {
                        broadcastAnimationComplete();
                    }
                }
                stopAndCleanupTrack(i);
                needsDisplayUpdate = true;
                break;

            default:
                Log_printf(LOG_LEVEL_WARN, "SEQ: Track %d entered unknown command state %d. Aborting.", i, step.command);
                stopAndCleanupTrack(i);
                needsDisplayUpdate = true;
                break;
        }

        if (advance_step) {
            track.currentStep++;
            track.stepStartTime = millis();
            track.stepInitialized = false;
        }
    }

    if (needsDisplayUpdate) {
        updateNormalClockDisplay();
    }
}

/**
 * @brief Stops all effects on a specific track and restores its brightness.
 * @param trackIndex The index of the track (0-2) to clean up.
 * @details This function is a failsafe to ensure that when a sequence ends
 * or is aborted, the corresponding display row is returned to a neutral,
 * visible state. It cancels any ongoing fades, pulses, or flashes.
 */
void stopAndCleanupTrack(int trackIndex) {
    if (trackIndex < 0 || trackIndex > 2) return;

    SequencerTrack& track = sequencerTracks[trackIndex];
    DisplayRow* rows[] = {&destRow, &presRow, &lastRow};
    DisplayRow& row = *rows[trackIndex];

    // Restore the brightness to the default setting before resetting the track
    uint8_t defaultBrightness = track.originalBrightness > 0 ? track.originalBrightness : currentSettings.brightness;
    row.month.setBrightness(defaultBrightness);
    row.day.setBrightness(defaultBrightness);
    row.year.setBrightness(defaultBrightness);
    row.time.setBrightness(defaultBrightness);

    // --- FIX: Call the comprehensive reset() method ---
    // This is more robust as it clears all state variables, including marquee text,
    // effect flags, and step commands, preventing any state from bleeding into the next sequence.
    track.reset();

    Log_printf(LOG_LEVEL_INFO, "SEQ: Cleaned up and stopped track %d.", trackIndex);

    // --- NEW: Check if this was the very last active track ---
    bool anyOtherTrackActive = false;
    for (int i = 0; i < 3; ++i) {
        if (sequencerTracks[i].isActive) {
            anyOtherTrackActive = true;
            break; // Found another active track, no need to check further
        }
    }

    // If no other tracks are active, restore the original display mode.
    if (!anyOtherTrackActive) {
        Log_printf(LOG_LEVEL_INFO, "SEQ: All tracks finished. Cleaning up and restoring pre-animation display mode: %d", preAnimationDisplayMode);
        comprehensiveAnimationCleanup(); // Full cleanup of all states
        currentSettings.displayMode = preAnimationDisplayMode;
        // The main loop will now handle updating the display according to the restored mode.
        // NOTE: broadcastAnimationComplete() is now called from the SEQ_CMD_END handler
        // to prevent premature completion signals when one sequence triggers another.
    }
}

/**
 * @brief Stops all active sequencer tracks immediately.
 * @details This function iterates through all available tracks and calls
 * `stopAndCleanupTrack` on each one. This serves as a "master reset" to ensure
 * no sequences are running before starting a new one, preventing conflicts.
 */
void stopAllSequences() {
    Log_printf(LOG_LEVEL_INFO, "SEQ: Stopping all active sequences.");
    for (int i = 0; i < 3; i++) {
        // The cleanup function has its own guards, but checking isActive here is a good practice.
        if (sequencerTracks[i].isActive) {
            stopAndCleanupTrack(i);
        }
    }
}

/**
 * @brief Configures and runs a test for the crossfade command bug.
 * @details This test sets up a single track to demonstrate the infinite loop
 * in the SEQ_CMD_CROSSFADE_TEXT command. It is intended to fail before the
 * fix and pass afterward.
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

/**
 * @brief Configures and runs a test for the WIPE command.
 * @details This test runs the debug wipe sequence to validate the bounds check.
 */
void runWipeTest() {
    Log_printf(LOG_LEVEL_INFO, "SEQ_TEST: --- Running Wipe Fix Test ---");
    triggerAnimation(ANIMATION_DEBUG_WIPE);
    Log_printf(LOG_LEVEL_INFO, "SEQ_TEST: --- Wipe Fix Test Started ---");
}

/**
 * @brief Configures and runs a startup test to verify parallel sequence execution.
 * @details This test sets up two tracks to run simultaneously:
 *          - Track 0: Fades in the entire top display row over 5 seconds.
 *          - Track 1: Pulses the middle display row's "month" segment for 5 seconds.
 *          This is used to confirm that the sequencer's local state management is working.
 */
void runSequencerTest() {
    Log_printf(LOG_LEVEL_INFO, "SEQ_TEST: --- Running Comprehensive Sequencer Test ---");

    // Reset all tracks to ensure a clean slate
    for (int i = 0; i < 3; ++i) {
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

/**
 * @brief Initializes a scrolling marquee effect on a specific sequencer track.
 * @param track The sequencer track to activate the marquee on.
 * @param text The text to be scrolled.
 */
void triggerAnimation(AnimationType animType) {
    // This function is a full takeover. It replaces all running tracks
    // with the new animation.
    Log_printf(LOG_LEVEL_INFO, "SEQ: Triggering new animation %d. All current tracks will be replaced.", (int)animType);

    // Generate the requested animation into a temporary set of tracks.
    SequencerTrack temp_tracks[3];
    generateAnimationSequence(animType, temp_tracks);

    // Stop ALL currently running tracks to prepare for the new animation.
    stopAllSequences();

    // Copy the steps from ALL generated tracks to the main sequencer tracks.
    for (int j = 0; j < 3; ++j) {
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
}

void startSequencerMarquee(SequencerTrack& track, const std::string& text) {
    if (text.empty()) {
        track.isMarqueeActive = false;
        return;
    }
    track.isMarqueeActive = true;
    // Add padding for a smooth scroll-on and scroll-off effect
    track.marqueeText = "             " + text + "             ";
    track.marqueeScrollPosition = 0;
    track.lastMarqueeScrollTime = millis();
    Log_printf(LOG_LEVEL_INFO, "SEQ: Marquee started on track %d", track.steps[track.currentStep].targetRow);
}

/**
 * @brief Handles the continuous scrolling for all active marquee effects.
 * @details This function is called on every main loop. It iterates through all
 * sequencer tracks and, for any track with an active marquee, it updates the
 * scroll position and redraws the relevant display row with the new text segment.
 */
void handleAllSequencerMarquees() {
    for (int i = 0; i < 3; ++i) {
        SequencerTrack& track = sequencerTracks[i];

        if (track.isActive && track.isMarqueeActive) {
            // Use a fixed scroll speed for now. This could be extended to be a parameter.
            if (millis() - track.lastMarqueeScrollTime > 120) { // 120ms scroll speed
                track.marqueeScrollPosition++;

                // Check if the marquee has finished scrolling completely
                if ((unsigned)track.marqueeScrollPosition > track.marqueeText.length() - 13) {
                    track.isMarqueeActive = false;
                    Log_printf(LOG_LEVEL_INFO, "SEQ: Marquee finished on track %d.", i);

                    // --- FIX: Instead of leaving the display blank, center the original text ---
                    SequenceStep& step = track.steps[track.currentStep];
                    std::string originalText = step.stringParam;
                    std::string centeredText;

                    if (originalText.length() < 13) {
                        int padding = (13 - originalText.length()) / 2;
                        centeredText = std::string(padding, ' ') + originalText;
                        while (centeredText.length() < 13) {
                            centeredText += " ";
                        }
                    } else {
                        centeredText = originalText.substr(0, 13);
                    }
                    updateDisplaySegment(i, -1, centeredText);

                } else {
                    // Get the 13-character segment to display
                    std::string displayText = track.marqueeText.substr(track.marqueeScrollPosition, 13);
                    updateDisplaySegment(i, -1, displayText);
                }
                track.lastMarqueeScrollTime = millis();
            }
        }
    }
}