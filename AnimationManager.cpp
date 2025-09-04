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
                
                // FIX: Add a delay after this burst of writes.
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
    }
}
/**
 * @brief State machine for the boot sequence.
 */
// AnimationManager.cpp
// AnimationManager.cpp

void handleBootSequence() {
    if (bootState == BOOT_INACTIVE) return;

    unsigned long elapsed = millis() - bootStateStartTime;

    switch (bootState) {
        case BOOT_START:
            Serial.println("BS_LOG: Entering BOOT_START");
            // MODIFICATION: Changed the initial sound to the main boot sound
            if (hardwareInitialized && elapsed < 50) { // Run only once
                playSound("/BOOT_UP.mp3");
            }
            if (elapsed > 1000) {
                bootState = BOOT_PLAY_HUM_SOUND;
                bootStateStartTime = millis();
            }
            break;

        case BOOT_PLAY_HUM_SOUND:
             // This state now handles the looping hum and a low-power message
            if (hardwareInitialized && elapsed < 50) { // Run only once
                playSound("/power_hum.mp3");
                setOverrideMessage("", "SYSTEM CHECK...", ""); // Low-power display message
            }
            
            // Wait for 1.5 seconds for audio power to stabilize before animating displays
            if (elapsed > 1500) { 
                bootState = BOOT_POWER_ON_DEST;
                bootStateStartTime = millis();
            }
            break;

        case BOOT_POWER_ON_DEST:
            Serial.println("BS_LOG: Entering BOOT_POWER_ON_DEST");
            if (hardwareInitialized) {
                // MODIFICATION: Ensure override is disabled before starting animations
                if (isMessageOverrideActive) {
                    isMessageOverrideActive = false;
                    blankAllDisplays();
                }
                animateDisplayRowRandomly(destRow);
            }
            if (elapsed > BOOT_POWER_ON_DURATION) {
                bootState = BOOT_POWER_ON_PRES;
                bootStateStartTime = millis();
            }
            break;

        // --- NO CHANGES to BOOT_POWER_ON_PRES or BOOT_POWER_ON_LAST ---
        case BOOT_POWER_ON_PRES:
            Serial.println("BS_LOG: Entering BOOT_POWER_ON_PRES"); // LOGGING
            if (hardwareInitialized) {
                updateNormalClockDisplay(true, false, false);
                animateDisplayRowRandomly(presRow);
            }
            if (elapsed > BOOT_POWER_ON_DURATION) {
                bootState = BOOT_POWER_ON_LAST;
                bootStateStartTime = millis();
            }
            break;

        case BOOT_POWER_ON_LAST:
            Serial.println("BS_LOG: Entering BOOT_POWER_ON_LAST"); // LOGGING
            if (hardwareInitialized) {
                updateNormalClockDisplay(true, true, false);
                animateDisplayRowRandomly(lastRow);
            }
            if (elapsed > BOOT_POWER_ON_DURATION) {
                bootState = BOOT_SYSTEM_CHECK;
                bootStateStartTime = millis();
            }
            break;
        
        // --- NO CHANGES to the rest of the function ---
        case BOOT_SYSTEM_CHECK:
            Serial.println("BS_LOG: Entering BOOT_SYSTEM_CHECK"); // LOGGING
            if (hardwareInitialized) {
                static int lastPhase = -1;
                int phase = elapsed / 2000;
                if (phase != lastPhase) {
                    if (phase > 0) playSound("/sys_beep.mp3");
                    lastPhase = phase;
                }
                const char* line3 = "";
                switch(phase) {
                    case 0: line3 = "SYSTEM CHECK..."; break;
                    case 1: line3 = "FLUX CAPACITOR... OK"; break;
                    case 2: line3 = "TIME CIRCUITS... OK"; break;
                }
                setOverrideMessage("", "", line3);
            }
            if (elapsed > BOOT_SYSTEM_CHECK_DURATION) {
                if (hardwareInitialized) playSound("/ACCELERATION.mp3");
                bootState = BOOT_SPEEDOMETER;
                bootStateStartTime = millis();
            }
            break;

        case BOOT_SPEEDOMETER:
            Serial.println("BS_LOG: Entering BOOT_SPEEDOMETER"); // LOGGING
            if (elapsed < BOOT_SPEEDOMETER_DURATION) {
                float progress = (float)elapsed / BOOT_SPEEDOMETER_DURATION;
                float easedProgress = progress * (2.0f - progress);
                speedometerValue = 88 * easedProgress;

                if (hardwareInitialized) {
                    char speedo[20];
                    sprintf(speedo, "SPEED %02d MPH", speedometerValue);
                    setOverrideMessage("SYSTEMS READY", speedo, "");
                }
            } else {
                speedometerValue = 88;
                if (hardwareInitialized) playSound("/TT_REACH88.mp3");
                bootState = BOOT_FADE_TO_CLOCK;
                bootStateStartTime = millis();
            }
            break;

        case BOOT_FADE_TO_CLOCK:
            Serial.println("BS_LOG: Entering BOOT_FADE_TO_CLOCK"); // LOGGING
            if (hardwareInitialized) {
                static bool fadeSoundPlayed = false;
                if(!fadeSoundPlayed){
                    playSound("/lock_on.mp3");
                    fadeSoundPlayed = true;
                }
                
                // --- START: NEW FADE LOGIC ---
                // Calculate how many characters to reveal (from 0 to 12)
                float progress = (float)elapsed / BOOT_FADE_DURATION;
                int charsToReveal = progress * 12;

                // Get the final, correct time info
                time_t now_t;
                time(&now_t);
                struct tm timeinfo;
                localtime_r(&now_t, &timeinfo);
                
                // Combine all "from" and "to" characters into single strings
                const char* fromText = "SYSTEMS READY SPEED 88 MPH";
                
                // ✅ FIX: Increased buffer size to prevent overflow.
                char toText[64];
                const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
                
                // Destination Time
                int destHour = timeinfo.tm_hour;
                if (!currentSettings.displayFormat24h && destHour > 12) destHour -= 12;
                if (!currentSettings.displayFormat24h && destHour == 0) destHour = 12;
                
                // Present Time
                int presHour = timeinfo.tm_hour;
                if (!currentSettings.displayFormat24h && presHour > 12) presHour -= 12;
                if (!currentSettings.displayFormat24h && presHour == 0) presHour = 12;
                
                // Last Departed Time
                int lastHour = currentSettings.lastTimeDepartedHour;
                if (!currentSettings.displayFormat24h && lastHour > 12) lastHour -= 12;
                if (!currentSettings.displayFormat24h && lastHour == 0) lastHour = 12;

                // ✅ FIX: Validate month index to prevent out-of-bounds access.
                int lastDepartedMonthIndex = currentSettings.lastTimeDepartedMonth - 1;
                if (lastDepartedMonthIndex < 0 || lastDepartedMonthIndex > 11) {
                    lastDepartedMonthIndex = 0; // Default to January if data is corrupt
                }

                sprintf(toText, "%3s %02d %04d%02d%02d%3s %02d %04d%02d%02d%3s %02d %04d%02d%02d",
                    months[timeinfo.tm_mon], timeinfo.tm_mday, currentSettings.destinationYear, destHour, timeinfo.tm_min,
                    months[timeinfo.tm_mon], timeinfo.tm_mday, timeinfo.tm_year + 1900, presHour, timeinfo.tm_min,
                    months[lastDepartedMonthIndex], currentSettings.lastTimeDepartedDay, currentSettings.lastTimeDepartedYear, lastHour, currentSettings.lastTimeDepartedMinute);

                // Helper to update a single 4-char display
                auto updateSegment = [&](Adafruit_AlphaNum4& display, int segmentIndex) {
                    char buffer[5] = "    ";
                    for (int i = 0; i < 4; i++) {
                        int charIndex = segmentIndex * 4 + i;
                        if (charIndex < charsToReveal) {
                            buffer[i] = toText[charIndex];
                        } else {
                            buffer[i] = fromText[charIndex];
                        }
                    }
                    printToDisplay(display, buffer);
                    display.writeDisplay();
                    
                    // FIX: Add the cooperative delay after every I2C write.
                    vTaskDelay(pdMS_TO_TICKS(1));
                };
                
                // Update all 12 display segments
                updateSegment(destRow.month, 0); updateSegment(destRow.day, 1); updateSegment(destRow.year, 2); updateSegment(destRow.time, 3);
                updateSegment(presRow.month, 4); updateSegment(presRow.day, 5); updateSegment(presRow.year, 6); updateSegment(presRow.time, 7);
                updateSegment(lastRow.month, 8); updateSegment(lastRow.day, 9); updateSegment(lastRow.year, 10); updateSegment(lastRow.time, 11);
                // --- END: NEW FADE LOGIC ---
            }
            if (elapsed > BOOT_FADE_DURATION) {
                bootState = BOOT_COMPLETE;
                bootStateStartTime = millis();
            }
            break;
            
        case BOOT_COMPLETE:
            Serial.println("BS_LOG: Entering BOOT_COMPLETE"); // LOGGING
            if (elapsed > 500) {
                isMessageOverrideActive = false;
                bootState = BOOT_INACTIVE;
                if (hardwareInitialized) updateNormalClockDisplay();
                Serial.println("BOOT_LOG: Boot sequence finished. Clock is now active.");
            }
            break;
            
        default:
            bootState = BOOT_INACTIVE;
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