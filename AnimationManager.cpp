/**
 * @file AnimationManager.cpp
 * @brief Manages all visual animations and special effects for the display.
 */

#include "AnimationManager.h"
#include "EventManager.h"
#include "HardwareControl.h"
#include "DisplayManager.h"
#include "MqttManager.h"
#include "config.h"
#include "globals.h"
#include <Arduino.h>

// --- FLASH EFFECT ---
bool isFlashing[3][4] = {{false}};
unsigned long flashEndTimes[3][4] = {{0}};

/**
 * @brief Triggers a temporary flashing effect on a specific display segment.
 */
void triggerFlashEffect(int row, int segment, int duration) {
    if (row < 0 || row > 2 || segment < 0 || segment > 3) return;
    isFlashing[row][segment] = true;
    flashEndTimes[row][segment] = millis() + duration;
}

/**
 * @brief Handles the state of any active flash effects. Called in the main loop.
 */
void handleFlashEffect() {
    // This function would need to be implemented to control the visual flashing.
    // For now, it will just manage the state.
    for (int r = 0; r < 3; ++r) {
        for (int s = 0; s < 4; ++s) {
            if (isFlashing[r][s] && millis() > flashEndTimes[r][s]) {
                isFlashing[r][s] = false;
            }
        }
    }
}


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

    if (currentSettings.timeTravelSoundToggle) {
        if (hardwareInitialized) {
            playSound("/FLUX_CAPACITOR_CHARGE.mp3");
        }
    }
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
                    playSound("/ttaccel.mp3");
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
                    playSound("/TIME_TRAVEL.mp3");
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
                    playSound("/ARRIVAL_THUD.mp3");
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
        if (hardwareInitialized) {
            updateNormalClockDisplay();
        }
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
    bootState = BOOT_START;
    bootStateStartTime = millis();
}

/**
 * @brief State machine for the boot sequence.
 */
void handleBootSequence() {
    if (bootState == BOOT_INACTIVE || !hardwareInitialized) return;
#if ENABLE_HARDWARE
    unsigned long elapsed = millis() - bootStateStartTime;

    switch (bootState) {
        case BOOT_START:
            blankAllDisplays();
            bootState = BOOT_CHARGE_UP;
            bootStateStartTime = millis();
            playSound("/FLUX_CAPACITOR_CHARGE.mp3");
            break;
        case BOOT_CHARGE_UP:
            if (elapsed < 2000) {
                animateCapacitorChargeUp(elapsed, 2000);
            } else {
                bootState = BOOT_COMPLETE;
                bootStateStartTime = millis();
            }
            break;
        case BOOT_COMPLETE:
            bootState = BOOT_INACTIVE;
            updateNormalClockDisplay();
            break;
    }
#endif
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