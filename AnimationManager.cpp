#include "AnimationManager.h"
#include "EventManager.h"
#include "HardwareControl.h"
#include "DisplayManager.h"
#include "MqttManager.h"
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

// --- FADE EFFECT ---
bool isFading = false;
static unsigned long fadeStartTime = 0;
static int fadeDuration = 0;
static bool isFadeIn = false;
static uint8_t originalBrightness = 0;

// --- PULSE EFFECT ---
static bool isPulsing[3][4] = {{false}};
static unsigned long pulseEndTimes[3][4] = {{0}};
static bool pulseStates[3][4] = {{false}};
static unsigned long lastPulseToggle[3][4] = {{0}};

// --- FLASH EFFECT ---
bool isFlashing[3][4] = {{false}};
unsigned long flashEndTimes[3][4] = {{0}};
bool flashStates[3][4] = {{false}};
unsigned long lastFlashToggle[3][4] = {{0}};

// File-scoped variable to hold the chosen animation style for a single run
static int randomAnimationStyle = -1;

/**
 * @brief Triggers a temporary flashing effect on a specific display segment.
 */
void triggerFlashEffect(int row, int segment, int duration) {
    if (row < 0 || row > 2 || segment < 0 || segment > 3) return;
    isFlashing[row][segment] = true;
    // A duration of 0 means flash indefinitely
    flashEndTimes[row][segment] = (duration == 0) ? 0 : millis() + duration;
    flashStates[row][segment] = true;
    lastFlashToggle[row][segment] = millis();
}

/**
 * @brief Handles the state of any active flash effects. Called in the main loop.
 */
#if ENABLE_HARDWARE
void handleFlashEffect() {
    for (int r = 0; r < 3; ++r) {
        for (int s = 0; s < 4; ++s) {
            if (isFlashing[r][s]) {
                if (flashEndTimes[r][s] != 0 && millis() > flashEndTimes[r][s]) {
                    isFlashing[r][s] = false;
                    // Restore the display by calling the main update function in the next loop
                } else {
                    if (millis() - lastFlashToggle[r][s] > 500) { // Toggle every 500ms
                        flashStates[r][s] = !flashStates[r][s];
                        lastFlashToggle[r][s] = millis();

                        DisplayRow* rows[] = {&destRow, &presRow, &lastRow};
                        Adafruit_AlphaNum4* displaySegment = nullptr;

                        switch(s) {
                            case 0: displaySegment = &rows[r]->month; break;
                            case 1: displaySegment = &rows[r]->day; break;
                            case 2: displaySegment = &rows[r]->year; break;
                            case 3: displaySegment = &rows[r]->time; break;
                        }

                        if (displaySegment) {
                           // Special case for the persistent Present Time dot
                           if (r == 1 && s == 3) {
                               if (flashStates[r][s]) {
                                   // Turn ON the dot on the SECOND character (index 1)
                                   displaySegment->displaybuffer[1] |= 0x4000;
                               } else {
                                   // Turn OFF the dot on the SECOND character (index 1)
                                   displaySegment->displaybuffer[1] &= ~0x4000;
                               }
                           } else { // Generic flash for other segments
                                if (flashStates[r][s]) {
                                    displaySegment->clear();
                                } else {
                                    // The main display logic will restore the content
                                }
                           }
                           displaySegment->writeDisplay();
                        }
                    }
                }
            }
        }
    }
}
#endif // ENABLE_HARDWARE

// --- START: NEW FADE AND PULSE IMPLEMENTATIONS ---

void startFadeEffect(int duration, bool fadeIn) {
    if (isFading) return; // Prevent starting a new fade if one is active
    isFading = true;
    isFadeIn = fadeIn;
    fadeDuration = duration;
    fadeStartTime = millis();
    originalBrightness = currentSettings.brightness;
    Log_printf(LOG_LEVEL_INFO, "Starting %s effect. Duration: %dms", fadeIn ? "Fade In" : "Fade Out", duration);
}

void handleFadeEffect() {
    if (!isFading) return;

    unsigned long elapsed = millis() - fadeStartTime;
    if (elapsed >= fadeDuration) {
        isFading = false;
        // Set final brightness and restore original setting
        currentSettings.brightness = isFadeIn ? 7 : 0;
        applyBrightness();
        currentSettings.brightness = originalBrightness; // Restore for future use
        Log_printf(LOG_LEVEL_INFO, "Fade effect finished.");
        return;
    }

    float progress = (float)elapsed / (float)fadeDuration;
    uint8_t newBrightness;

    if (isFadeIn) {
        newBrightness = (uint8_t)(progress * 7.0f);
    } else {
        newBrightness = (uint8_t)((1.0f - progress) * 7.0f);
    }

    if (newBrightness != currentSettings.brightness) {
        currentSettings.brightness = newBrightness;
        applyBrightness();
    }
}

void startPulseEffect(int row, int segment, int duration) {
    if (row < 0 || row > 2 || segment < 0 || segment > 3) return;
    isPulsing[row][segment] = true;
    pulseEndTimes[row][segment] = millis() + duration;
    pulseStates[row][segment] = true; // Start in the ON state
    lastPulseToggle[row][segment] = millis();
    Log_printf(LOG_LEVEL_INFO, "Starting PULSE effect on row %d, seg %d. Duration: %dms", row, segment, duration);
}

void handlePulseEffect() {
    bool needsDisplayUpdate = false;
    for (int r = 0; r < 3; ++r) {
        for (int s = 0; s < 4; ++s) {
            if (isPulsing[r][s]) {
                if (millis() > pulseEndTimes[r][s]) {
                    isPulsing[r][s] = false;
                    // Ensure the segment is left in its normal state
                    needsDisplayUpdate = true;
                } else {
                    if (millis() - lastPulseToggle[r][s] > 750) { // Slower 750ms on/off cycle for pulse
                        pulseStates[r][s] = !pulseStates[r][s];
                        lastPulseToggle[r][s] = millis();
                        needsDisplayUpdate = true;
                    }
                }
            }
        }
    }
    // If any pulse state changed, trigger a general display update.
    // The main display loop will handle redrawing the correct content.
    if (needsDisplayUpdate) {
        updateNormalClockDisplay();
    }
}

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
        isAnimating = false;
        currentPhase = ANIM_INACTIVE;
        updateNormalClockDisplay();
        updateHaStatus("Idle");
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
                isAnimating = false;
                currentPhase = ANIM_INACTIVE;
                lastPhase = ANIM_INACTIVE; // Reset for next run
                updateNormalClockDisplay();
                updateHaStatus("Idle");
                isEchoEffectActive = true;
                echoEffectStartTime = millis();
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
    if (currentSettings.animationStyle == ANIMATION_RANDOM_ALL) {
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
            if (audio.isRunning() || elapsed > 2000) { // Failsafe timeout of 2s
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
                if (audio.isRunning()) {
                    audio.stopSong();
                }
                comprehensiveAnimationCleanup();
                isStyledAnimating = false;
                currentStyledPhase = ANIM_INACTIVE;
                updateHaStatus("Idle");
                Serial.println("ANIM_LOG: Styled animation finished. Broadcasting completion.");
                broadcastAnimationComplete();
            }
            break;

        default:
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
                playSound("/hum.mp3");
                stateActionCompleted = true;
            }
            if (elapsed > BOOT_AWAIT_HUM_DURATION) {
                bootState = BOOT_START;
                bootStateStartTime = millis();
            }
            break;
        case BOOT_START:
            if (elapsed > 1000) {
                bootState = BOOT_WARM_UP;
                bootStateStartTime = millis();
            }
            break;
        case BOOT_WARM_UP:
            if (!stateActionCompleted) {
                playSound("/relay_activation.mp3");
                stateActionCompleted = true;
            }
            if (audio.isRunning()) {
                bootState = BOOT_COLD_START;
                bootStateStartTime = millis();
            }
            break;
        case BOOT_COLD_START:
            {
                const char* textToType = " INITIATE PWR";
                int textLen = strlen(textToType);
                int typingDuration = 5000; // 5 seconds
                int charDuration = typingDuration / textLen;
                static int charsTyped = 0;

                if (!stateActionCompleted) {
                    playSound("/keypad_beeps.mp3");
                    blankAllDisplays();
                    printToDisplay(destRow.day, "TM", 2);
                    printToDisplay(destRow.year, "CIRC");
                    printToDisplay(destRow.time, "UITS");
                    destRow.day.writeDisplay();
                    destRow.year.writeDisplay();
                    destRow.time.writeDisplay();
                    stateActionCompleted = true;
                    charsTyped = 0;
                }

                int charsToShow = elapsed / charDuration;
                if (charsToShow > textLen) {
                    charsToShow = textLen;
                }

                if (charsToShow > charsTyped) {
                    const char* p_month = " IN";
                    const char* p_day = "IT";
                    const char* p_year = "IATE";
                    const char* p_time = " PWR";

                    char monthStr[4];
                    char dayStr[3];
                    char yearStr[5];
                    char timeStr[5];

                    int len_m = strlen(p_month);
                    int len_d = strlen(p_day);
                    int len_y = strlen(p_year);
                    int len_t = strlen(p_time);

                    int chars_m = (charsToShow > len_m) ? len_m : charsToShow;
                    strncpy(monthStr, p_month, chars_m);
                    monthStr[chars_m] = '\0';

                    int chars_d = (charsToShow > len_m + len_d) ? len_d : ((charsToShow > len_m) ? charsToShow - len_m : 0);
                    strncpy(dayStr, p_day, chars_d);
                    dayStr[chars_d] = '\0';

                    int chars_y = (charsToShow > len_m + len_d + len_y) ? len_y : ((charsToShow > len_m + len_d) ? charsToShow - (len_m + len_d) : 0);
                    strncpy(yearStr, p_year, chars_y);
                    yearStr[chars_y] = '\0';

                    int chars_t = (charsToShow > len_m + len_d + len_y + len_t) ? len_t : ((charsToShow > len_m + len_d + len_y) ? charsToShow - (len_m + len_d + len_y) : 0);
                    strncpy(timeStr, p_time, chars_t);
                    timeStr[chars_t] = '\0';

                    printToDisplay(presRow.month, monthStr, 1);
                    printToDisplay(presRow.day, dayStr, 2);
                    printToDisplay(presRow.year, yearStr, 0);
                    printToDisplay(presRow.time, timeStr, 1);

                    presRow.month.writeDisplay();
                    presRow.day.writeDisplay();
                    presRow.year.writeDisplay();
                    presRow.time.writeDisplay();
                    charsTyped = charsToShow;
                }

                if (elapsed > typingDuration + 2000) {
                    if (audio.isRunning()) {
                        audio.stopSong();
                    }
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
            if (audio.isRunning() || elapsed > 2000) { // Failsafe timeout of 2s
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
            if (!stateActionCompleted) {
                playSound("/keypad_beeps.mp3");
                stateActionCompleted = true;
            }
            if (audio.isRunning()) {
                int currentSecond = elapsed / 2000;
                if (currentSecond != lastDiagSecond) {
                    blankAllDisplays();
                    if (currentSecond == 0) {
                        printToDisplay(destRow.month, "CPU", 1);
                        printToDisplay(destRow.day, "OK", 2);
                        destRow.month.writeDisplay();
                        destRow.day.writeDisplay();
                    } else if (currentSecond == 1) {
                        printToDisplay(presRow.month, "MEM", 1);
                        printToDisplay(presRow.day, "OK", 2);
                        presRow.month.writeDisplay();
                        presRow.day.writeDisplay();
                    } else if (currentSecond == 2) {
                        printToDisplay(lastRow.month, "WFI", 1);
                        printToDisplay(lastRow.day, "OK", 2);
                        lastRow.month.writeDisplay();
                        lastRow.day.writeDisplay();
                    } else if (currentSecond == 3) {
                        printToDisplay(lastRow.month, "IP", 1);
                        printToDisplay(lastRow.day, "OK", 2);
                        lastRow.month.writeDisplay();
                        lastRow.day.writeDisplay();
                    } else if (currentSecond == 4) {
                        printToDisplay(lastRow.month, "MQT", 1);
                        printToDisplay(lastRow.day, "OK", 2);
                        lastRow.month.writeDisplay();
                        lastRow.day.writeDisplay();
                    }
                    lastDiagSecond = currentSecond;
                }
            }
            if (elapsed > BOOT_DIAGNOSTICS_DURATION) {
                bootState = BOOT_FINAL_CHECKS;
                bootStateStartTime = millis();
            }
            break;
        case BOOT_FINAL_CHECKS:
            if (!stateActionCompleted) {
                playSound("/engine_rev.mp3");
                stateActionCompleted = true;
            }
            if (audio.isRunning()) {
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

                printToDisplay(destRow.year, "SYS");
                printToDisplay(destRow.time, "GO");
                destRow.year.writeDisplay();
                destRow.time.writeDisplay();
            }
            if (elapsed > BOOT_FINAL_CHECKS_DURATION) {
                bootState = BOOT_TEMPORAL_DISPLACEMENT;
                bootStateStartTime = millis();
            }
            break;
        case BOOT_TEMPORAL_DISPLACEMENT:
            if (!stateActionCompleted) {
                playSound("/time_travel.mp3");
                stateActionCompleted = true;
            }
            if (audio.isRunning()) {
                animateRandomRealTimes();
            }
            if (elapsed > BOOT_TEMPORAL_DISPLACEMENT_DURATION) {
                bootState = BOOT_ARRIVAL;
                bootStateStartTime = millis();
            }
            break;
        case BOOT_ARRIVAL:
            if (!stateActionCompleted) {
                playSound("/arrival_chime.mp3");
                stateActionCompleted = true;
            }
            if (audio.isRunning() || elapsed > 2000) { // Failsafe timeout of 2s
                bootState = BOOT_ARRIVAL_ANIMATION;
                bootStateStartTime = millis(); // Reset the timer for the animation phase
            }
            break;
        case BOOT_ARRIVAL_ANIMATION:
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
            break;
        case BOOT_COOL_DOWN:
            if (!stateActionCompleted) {
                // Manually fade out the audio
                for (int i = currentSettings.notificationVolume; i >= 0; i--) {
                    audio.setVolume(i);
                    delay(50);
                }
                audio.stopSong();
                stateActionCompleted = true;
            }
            {
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
            }

            if (elapsed > BOOT_COOL_DOWN_DURATION) {
                bootState = BOOT_COMPLETE;
                bootStateStartTime = millis();
            }
            break;
        case BOOT_COMPLETE:
            if (elapsed > 500) {
                isMessageOverrideActive = false;
                bootState = BOOT_INACTIVE;

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

                // The main display loop will handle updating the display correctly
                // once the bootState is set to BOOT_INACTIVE.
                Serial.println("BOOT_LOG: Boot sequence finished. Clock is now active.");
            }
            break;
        default:
            Serial.printf("BOOT_LOG: Unknown boot state %d. Resetting to INACTIVE.\n", bootState);
            bootState = BOOT_INACTIVE;
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