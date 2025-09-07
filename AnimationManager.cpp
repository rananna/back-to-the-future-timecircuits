#include "AnimationManager.h"
#include "EventManager.h"
#include "HardwareControl.h"
#include "DisplayManager.h"
#include "MqttManager.h"
#include <WiFi.h>

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

extern int speedometerValue;

// Extern variables should be within the conditional block
#if ENABLE_HARDWARE
extern DisplayRow destRow, presRow, lastRow;
#endif

// --- FLASH EFFECT ---
bool isFlashing[3][4] = {{false}};
unsigned long flashEndTimes[3][4] = {{0}};
bool flashStates[3][4] = {{false}};
unsigned long lastFlashToggle[3][4] = {{0}};

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
void startTimeTravelAnimation() {
    if (isAnimating) return;
    isAnimating = true;
    animationStartTime = millis();
    currentPhase = ANIM_POWER_UP;
    updateHaStatus("Animating");

    // For authenticity, save the current "Last Time Departed" and set it to the current "Present Time"
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    currentSettings.lastTimeDepartedYear = timeinfo.tm_year + 1900;
    currentSettings.lastTimeDepartedMonth = timeinfo.tm_mon + 1;
    currentSettings.lastTimeDepartedDay = timeinfo.tm_mday;
    currentSettings.lastTimeDepartedHour = timeinfo.tm_hour;
    currentSettings.lastTimeDepartedMinute = timeinfo.tm_min;

#if ENABLE_HARDWARE
    if (currentSettings.timeTravelSoundToggle) {
        if (hardwareInitialized) {
            playSoundAndSetNextPhase("FLUX_CAPACITOR_CHARGE", ANIM_TIME_ACCELERATION);
        }
    } else {
        currentPhase = ANIM_TIME_ACCELERATION;
    }
#endif
}

/**
 * @brief The main state machine for the time travel animation. Called in the main loop.
 */
void handleDisplayAnimation() {
    if (!isAnimating || !hardwareInitialized) return;
#if ENABLE_HARDWARE
    unsigned long elapsed = millis() - animationStartTime;

    switch (currentPhase) {
        case ANIM_WAIT_FOR_SOUND:
            if (!isPlayingSound) {
                currentPhase = nextPhaseAfterSound;
                animationStartTime = millis();
            }
            break;
        case ANIM_POWER_UP:
            if (elapsed < 2000) {
                // Initial random flicker
                animateTornadoFlicker();
            } else {
                playSoundAndSetNextPhase("ACCELERATION", ANIM_TIME_ACCELERATION);
            }
            break;

        case ANIM_TIME_ACCELERATION:
             if (elapsed < 30000) { // Increased duration to 30 seconds
                // Calculate progress as a value from 0.0 to 1.0
                float progress = (float)elapsed / 30000.0f;
                // Apply a quadratic ease-out function
                float easedProgress = progress * (2.0f - progress);
                int speed = 88 * easedProgress;
                
                displaySpeed(speed);
                // Flicker the top two rows while accelerating
                animateDisplayRowRandomly(destRow);
                animateDisplayRowRandomly(presRow);
            } else {
                displaySpeed(88);
                flashAllDisplays(); // White flash at 88 MPH
                playSoundAndSetNextPhase("TIME_TRAVEL", ANIM_ARRIVAL);
            }
            break;

        case ANIM_ARRIVAL:
            if (elapsed < currentSettings.timeTravelAnimationDuration) {
                // The main time blur effect
                animateAllRowsTimelineSkim(elapsed, currentSettings.timeTravelAnimationDuration, currentSettings.destinationYear);
            } else {
                playSoundAndSetNextPhase("ARRIVAL_THUD", ANIM_LANDING);
            }
            break;

        case ANIM_LANDING:
             if (elapsed < 1000) {
                // Final arrival flicker
                animateTornadoFlicker();
            } else {
                isAnimating = false;
                currentPhase = ANIM_INACTIVE;
                updateNormalClockDisplay(); // Restore the correct time
                updateHaStatus("Idle");
                isEchoEffectActive = true; // Start the post-travel echo effect
                echoEffectStartTime = millis();
            }
            break;
        default:
            break;
    }
#endif
}

// --- OTHER EFFECTS ---

/**
 * @brief Handles the "temporal echo" effect after a time jump.
 */
void handleTemporalEcho() {
    if (!isEchoEffectActive || !hardwareInitialized) return;
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

/**
 * @brief Handles the random glitch and malfunction effects during normal operation.
 */
void handleGlitchEffect() {
    if (isAnimating || isDisplayAsleep || isMalfunctioning) return;
    if (millis() - lastGlitchTime > 1000) { // Check once per second
        lastGlitchTime = millis();

        /*
        // Check for a major malfunction
        if (currentSettings.malfunctionFrequency > 0 && random(currentSettings.malfunctionFrequency) == 0) {
            isMalfunctioning = true;
            malfunctionStartTime = millis();
            currentMalfunctionPhase = MAL_HAYWIRE;
            updateHaStatus("Malfunctioning");
            return; // Prioritize malfunction over a simple glitch
        }
        */

        /*
        // Check for a minor glitch
        if (currentSettings.glitchEffectFrequency > 0 && random(100) < currentSettings.glitchEffectFrequency) {
            isGlitching = true;
            glitchStartTime = millis();
        }
        */
    }
}
/**
 * @brief Restores the display to its normal state after a brief glitch effect has completed.
 */
void restoreDisplayAfterGlitch() {
    if (isGlitching && (millis() - glitchStartTime > 200)) { // Glitch duration
        isGlitching = false;
#if ENABLE_HARDWARE
        if (hardwareInitialized) {
            updateNormalClockDisplay();
        }
#endif
    }
}

void handleMalfunction() {
    if (!isMalfunctioning || !hardwareInitialized) return;
#if ENABLE_HARDWARE
    unsigned long elapsed = millis() - malfunctionStartTime;

    switch (currentMalfunctionPhase) {
        case MAL_HAYWIRE:
            if (elapsed < 3000) {
                animateTornadoFlicker();
            } else {
                currentMalfunctionPhase = MAL_ERROR_MESSAGE;
                malfunctionStartTime = millis();
            }
            break;
// In AnimationManager.cpp, inside handleMalfunction()

        case MAL_ERROR_MESSAGE:
            if (elapsed < 3000) {
                printToDisplay(presRow.month, "ERR", 1);
                printToDisplay(presRow.day, "", 2);
                printToDisplay(presRow.year, "FAIL");
                printToDisplay(presRow.time, "----");
                presRow.month.writeDisplay();
                presRow.day.writeDisplay();
                presRow.year.writeDisplay();
                presRow.time.writeDisplay();
                vTaskDelay(pdMS_TO_TICKS(5));
            } else {
                currentMalfunctionPhase = MAL_REBOOT;
                malfunctionStartTime = millis();
            }
            break;
        case MAL_REBOOT:
            if (elapsed < 2000) {
                blankAllDisplays();
            } else {
                isMalfunctioning = false;
                currentMalfunctionPhase = MAL_INACTIVE;
                runBootSequence(); // Simulate a reboot by running the boot sequence
                updateHaStatus("Idle");
            }
            break;
        default:
            break;
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
        bootState = BOOT_START; // Only sets the initial state
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
        case BOOT_START:
            if (elapsed > 1000) {
                bootState = BOOT_WARM_UP;
                bootStateStartTime = millis();
            }
            break;
        case BOOT_WAIT_FOR_SOUND:
            if (isPlayingSound) {
                bootState = nextStateAfterSound;
                bootStateStartTime = millis();
            }
            break;
        case BOOT_WARM_UP:
            if (!stateActionCompleted) {
                blankAllDisplays();
                stateActionCompleted = true;
            }
            if (elapsed > BOOT_WARM_UP_DURATION) {
                playSound("/relay_activation.mp3");
                nextStateAfterSound = BOOT_COLD_START;
                bootState = BOOT_WAIT_FOR_SOUND;
            }
            break;
        case BOOT_COLD_START:
            if (!stateActionCompleted) {
                blankAllDisplays();
                printToDisplay(destRow.day, "TM", 2);
                printToDisplay(destRow.year, "CIRC");
                printToDisplay(destRow.time, "UITS");
                destRow.day.writeDisplay();
                destRow.year.writeDisplay();
                destRow.time.writeDisplay();
                stateActionCompleted = true;
            }
            if (elapsed > 1000 && !typingStarted) {
                typeTextOnDisplay(presRow, "INITIATE PWR", 100, true);
                typingStarted = true;
            }
            if (elapsed > BOOT_COLD_START_DURATION) {
                playSound("/flux_capacitor_power_on.mp3");
                nextStateAfterSound = BOOT_FLUX_CAPACITOR_IGNITION;
                bootState = BOOT_WAIT_FOR_SOUND;
            }
            break;
        case BOOT_FLUX_CAPACITOR_IGNITION:
            if (elapsed < 3000) {
                if ((elapsed / 250) % 2 == 0) {
                    flashAllDisplays();
                } else {
                    blankAllDisplays();
                }
            } else {
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
            if (elapsed > BOOT_FLUX_CAPACITOR_IGNITION_DURATION) {
                playSound("/keypad_beeps.mp3");
                nextStateAfterSound = BOOT_DIAGNOSTICS;
                bootState = BOOT_WAIT_FOR_SOUND;
            }
            break;
        case BOOT_DIAGNOSTICS: {
            if (!stateActionCompleted) {
                blankAllDisplays(); 
                stateActionCompleted = true;
                lastDiagSecond = -1; 
            }

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

            if (elapsed > BOOT_DIAGNOSTICS_DURATION) {
                playSound("/engine_rev.mp3");
                nextStateAfterSound = BOOT_FINAL_CHECKS;
                bootState = BOOT_WAIT_FOR_SOUND;
            }
            break;
        }
        case BOOT_FINAL_CHECKS: {
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

            if (elapsed > BOOT_FINAL_CHECKS_DURATION) {
                playSound("/time_travel.mp3");
                nextStateAfterSound = BOOT_TEMPORAL_DISPLACEMENT;
                bootState = BOOT_WAIT_FOR_SOUND;
            }
            break;
        }
        case BOOT_TEMPORAL_DISPLACEMENT:
            animateRandomRealTimes();
            if (elapsed > BOOT_TEMPORAL_DISPLACEMENT_DURATION) {
                playSound("/arrival_chime.mp3");
                nextStateAfterSound = BOOT_ARRIVAL;
                bootState = BOOT_WAIT_FOR_SOUND;
            }
            break;
        case BOOT_ARRIVAL:
            if (!stateActionCompleted) {
                blankAllDisplays();
                printToDisplay(destRow.year, "ARRI");
                printToDisplay(destRow.time, "VAL");
                printToDisplay(presRow.year, "OUTA");
                printToDisplay(presRow.time, "TIME");

                destRow.year.writeDisplay();
                destRow.time.writeDisplay();
                presRow.year.writeDisplay();
                presRow.time.writeDisplay();
                stateActionCompleted = true;
            }
             if (elapsed > 3000) {
                printToDisplay(lastRow.year, "WEL");
                printToDisplay(lastRow.time, "COME");
                lastRow.year.writeDisplay();
                lastRow.time.writeDisplay();
            }

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
                uint8_t brightness = 15 * (1.0 - progress);

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
                
                uint8_t saved_brightness = round((currentSettings.brightness / 7.0) * 15.0);

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

                if (hardwareInitialized) updateNormalClockDisplay();
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
    isMarqueeOverrideActive = false;

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
 * @brief Triggers the "temporal glitch" effect when time is first synchronized.
 */
/**
 * @brief Triggers the start of the temporal glitch effect.
 * @details This function simply sets the global state flags to initiate
 * the glitch effect, which is then handled by handleTemporalGlitch()
 * in the main loop.
 */
void triggerTemporalGlitch() {
    // Only start a new glitch if one is not already active.
    if (!isGlitching) {
        isGlitching = true;
        glitchStartTime = millis();
    }
}

/**
 * @brief Handles the visual part of the temporal glitch effect.
 * @details This function creates a controlled flicker on the "Present Time"
 * display for a set duration. It limits the rate of I2C updates to prevent
 * bus flooding and ensures the display is properly restored when the
 * effect is complete.
 */
void handleTemporalGlitch() {
    if (!isGlitching || !hardwareInitialized) {
        return;
    }

#if ENABLE_HARDWARE
    unsigned long elapsed = millis() - glitchStartTime;

    // The total duration of the glitch effect.
    const int GLITCH_DURATION_MS = 1500;

    if (elapsed < GLITCH_DURATION_MS) {
        // To prevent I2C bus flooding, only update the display periodically.
        static unsigned long lastFlickerTime = 0;
        if (millis() - lastFlickerTime > 100) { // Flicker every 100ms.
            lastFlickerTime = millis();

            // Alternate between showing random characters and the correct time
            // for a more realistic "glitching" effect.
            if (random(100) < 50) {
                animateDisplayRowRandomly(presRow);
            } else {
                // Briefly show the correct time.
                updateNormalClockDisplay(false, true, false);
            }
        }
    } else {
        // The glitch effect is over.
        isGlitching = false;
        
        // Explicitly restore all displays to their normal state.
        resetDisplayToNormal();
    }
#endif
}