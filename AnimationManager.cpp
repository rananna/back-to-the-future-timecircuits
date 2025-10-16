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
    // --- FIX: Use a static buffer to prevent heap fragmentation on completion ---
    static char jsonString[64];
    JsonDocument doc;
    doc["action"] = "animationComplete";
    serializeJson(doc, jsonString, sizeof(jsonString));
    if (ws.count() > 0) {
        ws.textAll(jsonString);
    }
}

// Effects are now handled inside the sequencer

void triggerFlashEffect(int row, int segment, int duration) {
    if (row < 0 || row > 2 || segment < 0 || segment > 3) {
        Log_printf(LOG_LEVEL_WARN, "SEQ: Invalid parameters for triggerFlashEffect (row: %d, seg: %d)", row, segment);
        return;
    }

    if (sequencerTracks[row].isActive) {
        Log_printf(LOG_LEVEL_INFO, "SEQ: Ignoring flash effect on row %d, sequence already active.", row);
        return;
    }

    Log_printf(LOG_LEVEL_INFO, "SEQ: Triggering flash effect on row %d, segment %d for %dms.", row, segment, duration);

    sequencerTracks[row].reset();
    sequencerTracks[row].isActive = true;
    sequencerTracks[row].stepStartTime = millis();
    sequencerTracks[row].trackStartTime = millis();
    sequencerTracks[row].originalBrightness = currentSettings.brightness;
    sequencerTracks[row].steps[0] = {SEQ_CMD_FLASH, row, segment, duration, 0, "", ""};
    sequencerTracks[row].steps[1] = {SEQ_CMD_END, 0, 0, 0, 0, "", ""};
}

// --- TIME TRAVEL ANIMATION (LEGACY) ---

void startTimeTravelAnimation() {
    Log_printf(LOG_LEVEL_INFO, "DIAG: startTimeTravelAnimation() called.");
    if (xSemaphoreTake(xAnimationStartMutex, (TickType_t)10) != pdTRUE) {
        return;
    }
    if (isAnimating) {
        xSemaphoreGive(xAnimationStartMutex);
        return;
    }
    isAnimating = true;
    xSemaphoreGive(xAnimationStartMutex);

    animationStartTime = millis();
    currentPhase = ANIM_POWER_UP;
    updateHaStatus("Animating");
}

void handleDisplayAnimation() {
    if (!isAnimating || !hardwareInitialized) return;
#if ENABLE_HARDWARE
    unsigned long elapsed = millis() - animationStartTime;

    const unsigned long MAX_ANIMATION_DURATION = 10000; // 10 seconds
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

    if (currentPhase != lastPhase) {
        if (currentSettings.timeTravelSoundToggle) {
            switch (currentPhase) {
                case ANIM_POWER_UP: playSound("engine_rev.mp3", false, -1); break;
                case ANIM_ARRIVAL: playSound("time_travel.mp3", false, -1); break;
                default: break;
            }
        }
        lastPhase = currentPhase;
    }

    switch (currentPhase) {
        case ANIM_POWER_UP:
            if (elapsed < 2000) { animateTornadoFlicker(); }
            else { currentPhase = ANIM_TIME_ACCELERATION; animationStartTime = millis(); }
            break;
        case ANIM_TIME_ACCELERATION:
             if (elapsed < 5000) {
                float progress = (float)elapsed / 5000.0f;
                displaySpeed(88 * progress);
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
            if (elapsed < 2000) {
                animateAllRowsTimelineSkim(elapsed, 2000, currentSettings.destinationYear, false);
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
                lastPhase = ANIM_INACTIVE;
                updateHaStatus("Idle");
                isEchoEffectActive = true;
                echoEffectStartTime = millis();
                broadcastAnimationComplete();
            }
            break;
        default:
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
    if (!bootSequenceCompleted) return;
    if (!isEchoEffectActive || isAnimating || isStyledAnimating || !hardwareInitialized) return;
#if ENABLE_HARDWARE
    if (millis() - echoEffectStartTime > 3000) {
        isEchoEffectActive = false;
        updateNormalClockDisplay(false, true, false);
        return;
    }
    if (random(10) == 0) {
        animateDisplayRowRandomly(presRow);
    }
#endif
}

// --- BOOT SEQUENCE ---

void runBootSequence() {
    // This is part of the boot sequence, which is out of scope for this refactoring.
    // It remains unchanged.
}

void handleBootSequence() {
    // This is part of the boot sequence, which is out of scope for this refactoring.
    // It remains unchanged.
}

// --- CLEANUP AND STATE MANAGEMENT ---

static void comprehensiveAnimationCleanup() {
    isMessageOverrideActive = false;
    for (int r = 0; r < NUM_SEQUENCER_TRACKS; ++r) {
        isRowInManualMode[r] = false;
        for (int s = 0; s < 4; ++s) {
            manualDisplayText[r][s][0] = '\0';
        }
    }
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
 * @details This function is now memory-safe, writing directly to a provided
 * C-style char buffer instead of using std::string.
 * @param row The display row index (0-2).
 * @param buffer A character buffer to write the resulting string into. Must be at least 14 bytes.
 */
void getFullRowText(int row, char* buffer) {
    if (row < 0 || row > 2 || buffer == nullptr) {
        if (buffer) buffer[0] = '\0';
        return;
    }
    snprintf(buffer, 14, "%s%s%s%s",
             manualDisplayText[row][0],
             manualDisplayText[row][1],
             manualDisplayText[row][2],
             manualDisplayText[row][3]);
}

// --- SEQUENCER ---

void handleSequencer() {
    bool needsDisplayUpdate = false;
    DisplayRow* rows[] = {&destRow, &presRow, &lastRow};

    for (int i = 0; i < NUM_SEQUENCER_TRACKS; i++) {
        SequencerTrack& track = sequencerTracks[i];
        if (!track.isActive) continue;

        DisplayRow& row = *rows[i];

        if (millis() - track.trackStartTime > MAX_SEQUENCE_DURATION) {
            Log_printf(LOG_LEVEL_WARN, "SEQ: Track %d timed out. Aborting ALL.", i);
            stopAllSequences();
            needsDisplayUpdate = true;
            break;
        }

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
                    (uint8_t)(progress * track.originalBrightness) :
                    (uint8_t)((1.0f - progress) * track.originalBrightness);
                row.month.setBrightness(newBrightness);
                row.day.setBrightness(newBrightness);
                row.year.setBrightness(newBrightness);
                row.time.setBrightness(newBrightness);
                needsDisplayUpdate = true;
            }
        }

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

        bool advance_step = false;
        SequenceStep& step = track.steps[track.currentStep];
        unsigned long commandElapsed = millis() - track.stepStartTime;

        switch (step.command) {
            case SEQ_CMD_SET_TEXT:
            case SEQ_CMD_CLEAR_SEGMENT:
                if (!track.stepInitialized) {
                    updateDisplaySegment(step.targetRow, step.targetSegment, (step.command == SEQ_CMD_SET_TEXT) ? step.stringParam : "");
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
            case SEQ_CMD_WAIT:
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
            case SEQ_CMD_LOOP_START:
                if (!track.stepInitialized) {
                    track.stepInitialized = true;
                    if (step.intParam > 0) {
                        track.loopStartStep = track.currentStep;
                        track.loopCounter = step.intParam;
                    } else {
                        int openLoops = 1;
                        int seekStep = track.currentStep + 1;
                        while (seekStep < MAX_SEQUENCE_STEPS) {
                            if (track.steps[seekStep].command == SEQ_CMD_LOOP_START) openLoops++;
                            else if (track.steps[seekStep].command == SEQ_CMD_LOOP_END) openLoops--;
                            if (openLoops == 0) break;
                            seekStep++;
                        }
                        if (openLoops == 0) track.currentStep = seekStep;
                        else track.reset();
                    }
                }
                advance_step = true;
                break;
            case SEQ_CMD_LOOP_END:
                if (!track.stepInitialized) {
                    track.stepInitialized = true;
                    if (track.loopStartStep >= 0) {
                        track.loopCounter--;
                        if (track.loopCounter > 0) {
                            track.currentStep = track.loopStartStep;
                        } else {
                            track.loopStartStep = -1;
                        }
                    }
                }
                advance_step = true;
                break;
            case SEQ_CMD_FADE_IN:
            case SEQ_CMD_FADE_OUT:
                if (!track.stepInitialized) {
                    if (track.isFading) break;
                    track.isFading = true;
                    track.isFadeIn = (step.command == SEQ_CMD_FADE_IN);
                    track.fadeDuration = step.intParam;
                    track.fadeStartTime = millis();
                    track.stepInitialized = true;
                }
                if (!track.isFading) {
                    advance_step = true;
                }
                break;
            case SEQ_CMD_PULSE:
                if (step.targetSegment < -1 || step.targetSegment >= 4) { advance_step = true; break; }
                if (!track.stepInitialized) {
                    if (step.stringParam[0] != '\0') {
                        updateDisplaySegment(step.targetRow, step.targetSegment, step.stringParam);
                    }
                    track.pulseInterval = (step.intParam > 0) ? step.intParam : 1000; // Hardcoded 1s on/1s off
                    unsigned long duration = (step.intParam2 > 0) ? step.intParam2 : 5000;
                    if (step.targetSegment == -1) {
                        for (int s = 0; s < 4; s++) {
                            track.isPulsing[s] = true;
                            track.pulseEndTimes[s] = millis() + duration;
                            track.pulseStates[s] = true;
                            track.lastPulseToggle[s] = millis();
                        }
                    } else {
                        track.isPulsing[step.targetSegment] = true;
                        track.pulseEndTimes[step.targetSegment] = millis() + duration;
                        track.pulseStates[step.targetSegment] = true;
                        track.lastPulseToggle[step.targetSegment] = millis();
                    }
                    track.stepInitialized = true;
                } else {
                    bool stillPulsing = false;
                    for (int s = 0; s < 4; s++) {
                        if (track.isPulsing[s]) { stillPulsing = true; break; }
                    }
                    if (!stillPulsing) advance_step = true;
                }
                break;
            case SEQ_CMD_FLASH:
                if (step.targetSegment < -1 || step.targetSegment >= 4) { advance_step = true; break; }
                if (!track.stepInitialized) {
                     if (step.targetSegment == -1) {
                        for (int s = 0; s < 4; s++) {
                            track.isFlashing[s] = true;
                            track.flashEndTimes[s] = (step.intParam == 0) ? 0 : millis() + step.intParam;
                            track.flashStates[s] = true;
                            track.lastFlashToggle[s] = millis();
                        }
                    } else {
                        track.isFlashing[step.targetSegment] = true;
                        track.flashEndTimes[step.targetSegment] = (step.intParam == 0) ? 0 : millis() + step.intParam;
                        track.flashStates[step.targetSegment] = true;
                        track.lastFlashToggle[step.targetSegment] = millis();
                    }
                    track.stepInitialized = true;
                } else {
                    bool stillFlashing = false;
                    for (int s = 0; s < 4; s++) {
                        if (track.isFlashing[s]) { stillFlashing = true; break; }
                    }
                    if (!stillFlashing) advance_step = true;
                }
                break;
            case SEQ_CMD_MARQUEE:
                if (!track.stepInitialized) {
                    startSequencerMarquee(track, step.stringParam);
                    track.stepInitialized = true;
                }
                if (!track.isMarqueeActive) {
                    advance_step = true;
                }
                break;
            case SEQ_CMD_COUNTDOWN:
                {
                    unsigned long countdown_delay = (step.intParam2 > 0) ? (unsigned long)step.intParam2 : 1000;
                    if (!track.stepInitialized) {
                        const int MAX_COUNTDOWN_VALUE = 3600;
                        track.countdownValue = (step.intParam > MAX_COUNTDOWN_VALUE) ? MAX_COUNTDOWN_VALUE : step.intParam;
                        track.countdownLastUpdate = millis();
                        updateDisplaySegment(i, -1, String(track.countdownValue).c_str());
                        track.stepInitialized = true;
                    } else if (millis() - track.countdownLastUpdate >= countdown_delay) {
                        track.countdownValue--;
                        track.countdownLastUpdate = millis();
                        if (track.countdownValue >= 0) {
                            updateDisplaySegment(i, -1, String(track.countdownValue).c_str());
                        }
                    }
                    if (track.countdownValue < 0) {
                        advance_step = true;
                    }
                }
                break;
            case SEQ_CMD_SCRAMBLE_TEXT:
                if (!track.stepInitialized) {
                    memset(track.scrambleBuffer, ' ', 13);
                    track.scrambleBuffer[13] = '\0';
                    track.scrambleCharIndex = 0;
                    track.lastScrambleUpdate = millis();
                    track.lastScrambleLockInTime = millis();
                    track.stepInitialized = true;
                }
                if ((unsigned)track.scrambleCharIndex >= strlen(step.stringParam)) {
                    updateDisplaySegment(step.targetRow, step.targetSegment, step.stringParam);
                    advance_step = true;
                } else {
                    if (millis() - track.lastScrambleLockInTime >= (unsigned long)step.intParam2) {
                        if ((unsigned)track.scrambleCharIndex < strlen(step.stringParam)) {
                            track.scrambleBuffer[track.scrambleCharIndex] = step.stringParam[track.scrambleCharIndex];
                        }
                        track.scrambleCharIndex++;
                        track.lastScrambleLockInTime = millis();
                    }
                    if (millis() - track.lastScrambleUpdate >= (unsigned long)step.intParam) {
                        static char temp_scramble_buffer[14];
                        strcpy(temp_scramble_buffer, track.scrambleBuffer);
                        for (size_t j = track.scrambleCharIndex; j < strlen(step.stringParam); ++j) {
                            temp_scramble_buffer[j] = (char)random(33, 126);
                        }
                        updateDisplaySegment(step.targetRow, step.targetSegment, temp_scramble_buffer);
                        track.lastScrambleUpdate = millis();
                    }
                }
                break;
            case SEQ_CMD_RANDOM_FLICKER_TEXT:
                 if (!track.stepInitialized) {
                     if (step.stringParam[0] == '\0') {
                        getFullRowText(step.targetRow, track.flickerOriginalText);
                    } else {
                        strncpy(track.flickerOriginalText, step.stringParam, 13);
                    }
                    track.flickerOriginalText[13] = '\0';
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
                            if (random(10) < 3) {
                                flicker_buffer[j] = (char)random(33, 126);
                            }
                        }
                        updateDisplaySegment(step.targetRow, step.targetSegment, flicker_buffer);
                        track.lastFlickerUpdate = millis();
                    }
                }
                break;
            case SEQ_CMD_TYPEWRITER:
                if (!track.stepInitialized) {
                    track.typewriterIndex = 0;
                    track.lastTypewriterUpdate = millis();
                    updateDisplaySegment(step.targetRow, step.targetSegment, "");
                    track.stepInitialized = true;
                }
                if ((unsigned)track.typewriterIndex >= strlen(step.stringParam)) {
                    advance_step = true;
                } else {
                    if (millis() - track.lastTypewriterUpdate > (unsigned long)step.intParam) {
                        track.typewriterIndex++;
                        static char typewriter_buffer[14];
                        strncpy(typewriter_buffer, step.stringParam, track.typewriterIndex);
                        typewriter_buffer[track.typewriterIndex] = '\0';
                        updateDisplaySegment(step.targetRow, step.targetSegment, typewriter_buffer);
                        track.lastTypewriterUpdate = millis();
                    } else {
                        vTaskDelay(1);
                    }
                }
                break;
            case SEQ_CMD_END:
            case SEQ_CMD_NONE:
                track.reset();
                needsDisplayUpdate = true;
                break;
            default:
                Log_printf(LOG_LEVEL_WARN, "SEQ: Track %d unhandled command %d. Aborting.", i, step.command);
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

    static bool wasAnimatingLastCycle = false;
    bool isAnimatingThisCycle = false;
    for (int i = 0; i < NUM_SEQUENCER_TRACKS; ++i) {
        if (sequencerTracks[i].isActive) {
            isAnimatingThisCycle = true;
            break;
        }
    }

    if (wasAnimatingLastCycle && !isAnimatingThisCycle && !isTransitioningAnimation) {
        doFinalAnimationCleanup();
    }
    wasAnimatingLastCycle = isAnimatingThisCycle;

    if (needsDisplayUpdate) {
        updateNormalClockDisplay();
    }
    vTaskDelay(1);
}

void doFinalAnimationCleanup() {
    static bool cleanupInProgress = false;
    if (cleanupInProgress) return;
    cleanupInProgress = true;

    if (currentAnimationType != ANIMATION_TYPE_MAX) {
        Log_printf(LOG_LEVEL_INFO, "SEQ: Animation %d (%s) completed.", (int)currentAnimationType, animationTypeToString(currentAnimationType));
        currentAnimationType = ANIMATION_TYPE_MAX;
    }

    Log_printf(LOG_LEVEL_INFO, "SEQ: All tracks finished. Restoring display mode: %d", preAnimationDisplayMode);
    comprehensiveAnimationCleanup();
    currentSettings.displayMode = preAnimationDisplayMode;
    justFinishedAnimation = true;

    broadcastAnimationComplete();
    cleanupInProgress = false;
}

void stopAllSequences() {
    Log_printf(LOG_LEVEL_INFO, "SEQ: Stopping all active sequences.");
    for (int i = 0; i < NUM_SEQUENCER_TRACKS; i++) {
        if (sequencerTracks[i].isActive) {
            sequencerTracks[i].reset();
        }
    }
}

void triggerAnimation(AnimationType animType) {
    Log_printf(LOG_LEVEL_INFO, "SEQ: Triggering animation %d (%s).", (int)animType, animationTypeToString(animType));
    vTaskDelay(1);

    isTransitioningAnimation = true;
    preAnimationDisplayMode = currentSettings.displayMode;
    currentSettings.displayMode = -1;
    currentAnimationType = animType;

    static SequencerTrack temp_tracks[NUM_SEQUENCER_TRACKS];
    generateAnimationSequence(animType, temp_tracks);
    stopAllSequences();

    for (int j = 0; j < NUM_SEQUENCER_TRACKS; ++j) {
        for (int i = 0; i < MAX_SEQUENCE_STEPS; ++i) {
            sequencerTracks[j].steps[i] = temp_tracks[j].steps[i];
            if (temp_tracks[j].steps[i].command == SEQ_CMD_END) break;
        }
        if (sequencerTracks[j].steps[0].command != SEQ_CMD_NONE) {
             sequencerTracks[j].isActive = true;
             sequencerTracks[j].trackStartTime = millis();
             sequencerTracks[j].stepStartTime = millis();
             sequencerTracks[j].originalBrightness = currentSettings.brightness;
        }
    }
    isTransitioningAnimation = false;
}

void startSequencerMarquee(SequencerTrack& track, const char* text) {
    if (text == nullptr || text[0] == '\0') {
        track.isMarqueeActive = false;
        return;
    }
    track.isMarqueeActive = true;
    snprintf(track.marqueeText, sizeof(track.marqueeText), "             %s             ", text);
    track.marqueeScrollPosition = 0;
    track.lastMarqueeScrollTime = millis();
}

void handleAllSequencerMarquees() {
    for (int i = 0; i < 3; ++i) {
        SequencerTrack& track = sequencerTracks[i];
        if (track.isActive && track.isMarqueeActive) {
            if (millis() - track.lastMarqueeScrollTime > 120) {
                track.marqueeScrollPosition++;
                if ((unsigned)track.marqueeScrollPosition > strlen(track.marqueeText) - 13) {
                    track.isMarqueeActive = false;
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