#include "AnimationManager.h"
#include "EventManager.h"
#include "HardwareControl.h"
#include "DisplayManager.h"
#include "MqttManager.h"
#include <WiFi.h>

static BootSequenceState nextStateAfterSound = BOOT_INACTIVE;
static AnimationPhase nextPhaseAfterSound = ANIM_INACTIVE;

static bool infoMessageSet = false;

extern void setOverrideMessage(const char* line1, const char* line2, const char* line3);
extern bool isMessageOverrideActive;
extern unsigned long bootStateStartTime;

void playReconfiguringSound();
void resetDisplayToNormal();

extern int speedometerValue;

#if ENABLE_HARDWARE
extern DisplayRow destRow, presRow, lastRow;
#endif

bool isFlashing[3][4] = {{false}};
unsigned long flashEndTimes[3][4] = {{0}};
bool flashStates[3][4] = {{false}};
unsigned long lastFlashToggle[3][4] = {{0}};

void triggerFlashEffect(int row, int segment, int duration) {
    if (row < 0 || row > 2 || segment < 0 || segment > 3) return;
    isFlashing[row][segment] = true;
    flashEndTimes[row][segment] = (duration == 0) ? 0 : millis() + duration;
    flashStates[row][segment] = true;
    lastFlashToggle[row][segment] = millis();
}

#if ENABLE_HARDWARE
void handleFlashEffect() {
    for (int r = 0; r < 3; ++r) {
        for (int s = 0; s < 4; ++s) {
            if (isFlashing[r][s]) {
                if (flashEndTimes[r][s] != 0 && millis() > flashEndTimes[r][s]) {
                    isFlashing[r][s] = false;
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
                           if (r == 1 && s == 3) {
                               if (flashStates[r][s]) {
                                   displaySegment->displaybuffer[1] |= 0x4000;
                               } else {
                                   displaySegment->displaybuffer[1] &= ~0x4000;
                               }
                           } else { 
                                if (flashStates[r][s]) {
                                    displaySegment->clear();
                                }
                           }
                           displaySegment->writeDisplay();
                           vTaskDelay(pdMS_TO_TICKS(2));
                        }
                    }
                }
            }
        }
    }
}
#endif

void playSoundAndSetNextPhase(const char* filename, AnimationPhase nextPhase) {
    if (hardwareInitialized && currentSettings.timeTravelSoundToggle) {
        playSound(filename);
    }
    nextPhaseAfterSound = nextPhase;
    currentPhase = ANIM_WAIT_FOR_SOUND;
    animationStartTime = millis();
}

void startTimeTravelAnimation() {
    if (isAnimating) return;
    isAnimating = true;
    animationStartTime = millis();
    currentPhase = ANIM_POWER_UP; 
    updateHaStatus("Animating");

    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    currentSettings.lastTimeDepartedYear = timeinfo.tm_year + 1900;
    currentSettings.lastTimeDepartedMonth = timeinfo.tm_mon + 1;
    currentSettings.lastTimeDepartedDay = timeinfo.tm_mday;
    currentSettings.lastTimeDepartedHour = timeinfo.tm_hour;
    currentSettings.lastTimeDepartedMinute = timeinfo.tm_min;
}

void handleDisplayAnimation() {
    if (!isAnimating || !hardwareInitialized) return;
#if ENABLE_HARDWARE
    unsigned long elapsed = millis() - animationStartTime;
    static AnimationPhase lastPhase = ANIM_INACTIVE;

    if (currentPhase != lastPhase) {
        if (currentSettings.timeTravelSoundToggle) {
            switch (currentPhase) {
                case ANIM_POWER_UP:
                    playSound("SAVE_POWER_UP.mp3");
                    break;
                case ANIM_TIME_ACCELERATION:
                    playSound("SAVE_ACCELERATION.mp3");
                    break;
                case ANIM_ARRIVAL:
                    playSound("SAVE_TIME_TRAVEL.mp3");
                    break;
                case ANIM_LANDING:
                    playSound("SAVE_LANDING.mp3");
                    break;
                default:
                    break;
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
                animationStartTime = millis();
            }
            break;

        case ANIM_TIME_ACCELERATION:
             if (elapsed < 10000) {
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
                animateAllRowsTimelineSkim(elapsed, currentSettings.timeTravelAnimationDuration, currentSettings.destinationYear);
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
                lastPhase = ANIM_INACTIVE;
                updateNormalClockDisplay();
                updateHaStatus("Idle");
                isEchoEffectActive = true;
                echoEffectStartTime = millis();
            }
            break;
        default:
            isAnimating = false;
            currentPhase = ANIM_INACTIVE;
            lastPhase = ANIM_INACTIVE;
            break;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
#endif
}

void handleTemporalEcho() {
    if (!isEchoEffectActive || !hardwareInitialized) return;
#if ENABLE_HARDWARE
    if (millis() - echoEffectStartTime > 60000) {
        isEchoEffectActive = false;
        return;
    }

    if (random(100) < 10) {
        animateDisplayRowRandomly(presRow);
    }
#endif
}

void handleGlitchEffect() {
    if (isAnimating || isDisplayAsleep || isMalfunctioning) return;
    if (millis() - lastGlitchTime > 1000) {
        lastGlitchTime = millis();
    }
}

void restoreDisplayAfterGlitch() {
    if (isGlitching && (millis() - glitchStartTime > 200)) {
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
        case MAL_ERROR_MESSAGE:
            if (elapsed < 3000) {
                printToDisplay(presRow.month, "ERR", 1);
                printToDisplay(presRow.day, "", 2);
                printToDisplay(presRow.year, "FAIL");
                printToDisplay(presRow.time, "----");
                presRow.month.writeDisplay();
                vTaskDelay(pdMS_TO_TICKS(2));
                presRow.day.writeDisplay();
                vTaskDelay(pdMS_TO_TICKS(2));
                presRow.year.writeDisplay();
                vTaskDelay(pdMS_TO_TICKS(2));
                presRow.time.writeDisplay();
                vTaskDelay(pdMS_TO_TICKS(2));
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
                runBootSequence();
                updateHaStatus("Idle");
            }
            break;
        default:
            break;
    }
#endif
}

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
            if (!stateActionCompleted) {
                blankAllDisplays();
                printToDisplay(destRow.day, "TM", 2);
                printToDisplay(destRow.year, "CIRC");
                printToDisplay(destRow.time, "UITS");
                destRow.day.writeDisplay();
                vTaskDelay(pdMS_TO_TICKS(2));
                destRow.year.writeDisplay();
                vTaskDelay(pdMS_TO_TICKS(2));
                destRow.time.writeDisplay();
                vTaskDelay(pdMS_TO_TICKS(2));
                stateActionCompleted = true;
            }
            if (elapsed > 1000 && !typingStarted) {
                typeTextOnDisplay(presRow, "INITIATE PWR", 100, true);
                typingStarted = true;
            }
            if (elapsed > BOOT_COLD_START_DURATION) {
                bootState = BOOT_FLUX_CAPACITOR_IGNITION;
                bootStateStartTime = millis();
            }
            break;
        
        case BOOT_FLUX_CAPACITOR_IGNITION:
            if (!stateActionCompleted) {
                playSound("/flux_capacitor_power_on.mp3");
                stateActionCompleted = true;
            }
            if (audio.isRunning() || elapsed > 2000) {
                bootState = BOOT_FLUX_CAPACITOR_ANIMATION;
                bootStateStartTime = millis();
            }
            break;

        case BOOT_FLUX_CAPACITOR_ANIMATION:
            if (elapsed < BOOT_FLUX_CAPACITOR_IGNITION_DURATION) {
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
                        vTaskDelay(pdMS_TO_TICKS(2));
                        presRow.day.writeDisplay();
                        vTaskDelay(pdMS_TO_TICKS(2));
                        presRow.year.writeDisplay();
                        vTaskDelay(pdMS_TO_TICKS(2));
                        presRow.time.writeDisplay();
                        vTaskDelay(pdMS_TO_TICKS(2));
                    }
                }
            } else {
                bootState = BOOT_DIAGNOSTICS;
                bootStateStartTime = millis();
            }
            break;

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
                        vTaskDelay(pdMS_TO_TICKS(2));
                        destRow.day.writeDisplay();
                        vTaskDelay(pdMS_TO_TICKS(2));
                    } else if (currentSecond == 1) {
                        printToDisplay(presRow.month, "MEM", 1);
                        printToDisplay(presRow.day, "OK", 2);
                        presRow.month.writeDisplay();
                        vTaskDelay(pdMS_TO_TICKS(2));
                        presRow.day.writeDisplay();
                        vTaskDelay(pdMS_TO_TICKS(2));
                    } else if (currentSecond == 2) {
                        printToDisplay(lastRow.month, "WFI", 1);
                        printToDisplay(lastRow.day, "OK", 2);
                        lastRow.month.writeDisplay();
                        vTaskDelay(pdMS_TO_TICKS(2));
                        lastRow.day.writeDisplay();
                        vTaskDelay(pdMS_TO_TICKS(2));
                    } else if (currentSecond == 3) {
                        printToDisplay(lastRow.month, "IP", 1);
                        printToDisplay(lastRow.day, "OK", 2);
                        lastRow.month.writeDisplay();
                        vTaskDelay(pdMS_TO_TICKS(2));
                        lastRow.day.writeDisplay();
                        vTaskDelay(pdMS_TO_TICKS(2));
                    } else if (currentSecond == 4) {
                        printToDisplay(lastRow.month, "MQT", 1);
                        printToDisplay(lastRow.day, "OK", 2);
                        lastRow.month.writeDisplay();
                        vTaskDelay(pdMS_TO_TICKS(2));
                        lastRow.day.writeDisplay();
                        vTaskDelay(pdMS_TO_TICKS(2));
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
                vTaskDelay(pdMS_TO_TICKS(2));
                destRow.time.writeDisplay();
                vTaskDelay(pdMS_TO_TICKS(2));
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
            if (audio.isRunning()) {
                if (!stateActionCompleted) {
                    blankAllDisplays();
                    printToDisplay(destRow.year, "ARRI");
                    printToDisplay(destRow.time, "VAL");
                    printToDisplay(presRow.year, "OUTA");
                    printToDisplay(presRow.time, "TIME");

                    destRow.year.writeDisplay();
                    vTaskDelay(pdMS_TO_TICKS(2));
                    destRow.time.writeDisplay();
                    vTaskDelay(pdMS_TO_TICKS(2));
                    presRow.year.writeDisplay();
                    vTaskDelay(pdMS_TO_TICKS(2));
                    presRow.time.writeDisplay();
                    vTaskDelay(pdMS_TO_TICKS(2));
                    stateActionCompleted = true;
                }
                if (elapsed > 3000) {
                    printToDisplay(lastRow.year, "WEL");
                    printToDisplay(lastRow.time, "COME");
                    lastRow.year.writeDisplay();
                    vTaskDelay(pdMS_TO_TICKS(2));
                    lastRow.time.writeDisplay();
                    vTaskDelay(pdMS_TO_TICKS(2));
                }
            }
            if (elapsed > BOOT_ARRIVAL_DURATION) {
                bootState = BOOT_COOL_DOWN;
                bootStateStartTime = millis();
            }
            break;
        case BOOT_COOL_DOWN:
            if (!stateActionCompleted) {
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

void resetDisplayToNormal() {
    isMessageOverrideActive = false;
    isMarqueeOverrideActive = false;

    for (int r = 0; r < 3; ++r) {
        isRowInManualMode[r] = false;
        for (int s = 0; s < 4; ++s) {
            manualDisplayText[r][s] = "";
        }
    }

    updateNormalClockDisplay(true, true, true);
}

void triggerTemporalGlitch() {
    if (!isGlitching) {
        isGlitching = true;
        glitchStartTime = millis();
    }
}

void handleTemporalGlitch() {
    if (!isGlitching || !hardwareInitialized) {
        return;
    }

#if ENABLE_HARDWARE
    unsigned long elapsed = millis() - glitchStartTime;

    const int GLITCH_DURATION_MS = 1500;

    if (elapsed < GLITCH_DURATION_MS) {
        static unsigned long lastFlickerTime = 0;
        if (millis() - lastFlickerTime > 100) {
            lastFlickerTime = millis();

            if (random(100) < 50) {
                animateDisplayRowRandomly(presRow);
            } else {
                updateNormalClockDisplay(false, true, false);
            }
        }
    } else {
        isGlitching = false;
        
        resetDisplayToNormal();
    }
#endif
}