/**
 * @file AnimationManager.cpp
 * @brief Manages all visual animations and special effects for the display.
 */

#include "AnimationManager.h"
#include "EventManager.h"
#include "HardwareControl.h"
#include "DisplayManager.h"
#include "MqttManager.h"
#include <WiFi.h>

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
    flashEndTimes[row][segment] = millis() + duration;
    flashStates[row][segment] = true;
    lastFlashToggle[row][segment] = millis();
}

/**
 * @brief Handles the state of any active flash effects. Called in the main loop.
 */
#if ENABLE_HARDWARE // <-- Added this guard
void handleFlashEffect() {
    for (int r = 0; r < 3; ++r) {
        for (int s = 0; s < 4; ++s) {
            if (isFlashing[r][s]) {
                if (millis() > flashEndTimes[r][s]) {
                    isFlashing[r][s] = false;
                    // Restore the display by calling the main update function in the next loop
                } else {
                    if (millis() - lastFlashToggle[r][s] > 100) { // Toggle every 100ms
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
                            if (flashStates[r][s]) {
                                // Turn off the display segment (blank it)
                                displaySegment->clear();
                            } else {
                                // The main display logic will restore the content on the next loop
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
            playSound("FLUX_CAPACITOR_CHARGE");
        }
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
        case ANIM_POWER_UP:
            if (elapsed < 2000) {
                // Initial random flicker
                animateTornadoFlicker();
            } else {
                currentPhase = ANIM_TIME_ACCELERATION;
                animationStartTime = millis(); // Reset timer for next phase
                if (currentSettings.timeTravelSoundToggle) {
                    playSound("ACCELERATION");
                }
            }
            break;

        case ANIM_TIME_ACCELERATION:
            if (elapsed < 3000) {
                int speed = (elapsed / 3000.0) * 88;
                displaySpeed(speed);
                // Flicker the top two rows
                animateDisplayRowRandomly(destRow);
                animateDisplayRowRandomly(presRow);
            } else {
                displaySpeed(88);
                flashAllDisplays(); // White flash at 88 MPH
                currentPhase = ANIM_ARRIVAL;
                animationStartTime = millis();
                 if (currentSettings.timeTravelSoundToggle) {
                    playSound("TIME_TRAVEL");
                }
            }
            break;

        case ANIM_ARRIVAL:
            if (elapsed < currentSettings.timeTravelAnimationDuration) {
                // The main time blur effect
                animateAllRowsTimelineSkim(elapsed, currentSettings.timeTravelAnimationDuration, currentSettings.destinationYear);
            } else {
                currentPhase = ANIM_LANDING;
                animationStartTime = millis();
                 if (currentSettings.timeTravelSoundToggle) {
                    playSound("ARRIVAL_THUD");
                }
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

        // Check for a major malfunction
        if (currentSettings.malfunctionFrequency > 0 && random(currentSettings.malfunctionFrequency) == 0) {
            isMalfunctioning = true;
            malfunctionStartTime = millis();
            currentMalfunctionPhase = MAL_HAYWIRE;
            updateHaStatus("Malfunctioning");
            return; // Prioritize malfunction over a simple glitch
        }

        // Check for a minor glitch
        if (currentSettings.glitchEffectFrequency > 0 && random(100) < currentSettings.glitchEffectFrequency) {
            isGlitching = true;
            glitchStartTime = millis();
        }
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

/**
 * @brief State machine for the system malfunction effect.
 */
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
void runBootSequence() {
    Serial.println("BOOT_LOG: runBootSequence() called.");
    if (bootState == BOOT_INACTIVE) {
        bootState = BOOT_START;
        bootStateStartTime = millis();
        if (hardwareInitialized) {
            playSound("/BOOT_UP.mp3");
        }
        Serial.println("BOOT_LOG: Boot sequence initiated.");
    } else {
        Serial.println("BOOT_LOG: Boot sequence already active.");
    }
}

/**
 * @brief State machine for the boot sequence.
 */
void handleBootSequence() {
    if (bootState == BOOT_INACTIVE) return;

    unsigned long elapsed = millis() - bootStateStartTime;

    switch (bootState) {
        case BOOT_START:
            if (elapsed > 1000) {
                bootState = BOOT_SPEEDOMETER;
                bootStateStartTime = millis();
                speedometerValue = 0;
                Serial.println("BOOT_LOG: Transitioning to BOOT_SPEEDOMETER.");
            }
            break;
        case BOOT_SPEEDOMETER:
            if (elapsed > BOOT_ANIMATION_FRAME_INTERVAL) {
                speedometerValue += 2; // Increment the speed

                if (speedometerValue >= 88) {
                    speedometerValue = 88;
                    if (hardwareInitialized) {
                        playSound("/TT_REACH88.mp3");
                    }
                    bootState = BOOT_REVEAL_INFO;
                    bootStateStartTime = millis(); // Reset timer for the NEW state
                    infoMessageSet = false;
                    Serial.println("BOOT_LOG: Reached 88 MPH. Transitioning to BOOT_REVEAL_INFO.");
                } else {
                    // Update display ONLY if we haven't transitioned
                    if (hardwareInitialized) {
                        char speedo[20];
                        sprintf(speedo, "SPEED %02d MPH", speedometerValue);
                        setOverrideMessage("SYSTEMS READY", speedo, "");
                    }
                }
            }
            break;
        case BOOT_REVEAL_INFO:
            // FIX: Only set the message once to prevent crashing
            if (!infoMessageSet && hardwareInitialized) {
                setOverrideMessage("IP ADDRESS", WiFi.localIP().toString().c_str(), MQTT_UNIQUE_ID);
                infoMessageSet = true;
            }

            if (elapsed > BOOT_INFO_DISPLAY_DURATION) {
                bootState = BOOT_COMPLETE;
                bootStateStartTime = millis();
                Serial.println("BOOT_LOG: Info display complete. Transitioning to BOOT_COMPLETE.");
            }
            break;
        case BOOT_COMPLETE:
            if (elapsed > 500) {
                isMessageOverrideActive = false;
                bootState = BOOT_INACTIVE;
                if (hardwareInitialized) {
                    updateNormalClockDisplay();
                }
                Serial.println("BOOT_LOG: Boot sequence finished. Clock is now active.");
            }
            break;
        case BOOT_INACTIVE:
            // do nothing
            break;
        default:
            // handle unknown state
            bootState = BOOT_INACTIVE;
            Serial.println("BOOT_LOG: Unknown boot state. Resetting to INACTIVE.");
            break;
    }
}

 // Placeholder function, replace with your actual implementation
void playReconfiguringSound() {
    playSound("/ha-alert.mp3");
}

// Placeholder function, replace with your actual implementation
void resetDisplayToNormal() {
    updateNormalClockDisplay();
}
/**
 * @brief Triggers the "temporal glitch" effect when time is first synchronized.
 */
void triggerTemporalGlitch() {
    isGlitching = true;
    glitchStartTime = millis();
}

/**
 * @brief Handles the visual part of the temporal glitch effect after it has been triggered.
 */
void handleTemporalGlitch() {
    if (isGlitching && hardwareInitialized) {
#if ENABLE_HARDWARE
        if (millis() - glitchStartTime < 1500) {
            // Flicker the present time display
            animateDisplayRowRandomly(presRow);
        } else {
            isGlitching = false;
            // The display will be restored in the main loop
        }
#endif
    }
}