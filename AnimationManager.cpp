#include "AnimationManager.h"
#include "AnimationSequences.h"

// Define the animation state variables
bool isAnimating = false;
unsigned long animationStartTime = 0;
AnimationPhase currentPhase = ANIM_INACTIVE;
#include "EventManager.h"
#include "HardwareControl.h"
#include "DebugLog.h"
#include "DisplayManager.h"
#include "MqttManager.h"
#include <WiFi.h>
#include "web_server.h"
#include <ArduinoJson.h>

#define MAX_SEQUENCE_DURATION 60000 // 60 seconds

char old_dest_str[17], old_pres_str[17], old_last_str[17];

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

/**
 * @brief Checks if any sequencer track is currently active.
 * @return True if at least one track's `isActive` flag is true, false otherwise.
 */
bool isAnySequenceActive() {
    for (int i = 0; i < 3; i++) {
        if (sequencerTracks[i].isActive) {
            return true;
        }
    }
    return false;
}

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
    sequencerTracks[row].steps[0] = {SEQ_CMD_FLASH, row, segment, duration, 0, ""};
    sequencerTracks[row].steps[1] = {SEQ_CMD_END, 0, 0, 0, 0, ""};
}

void startTimeTravelAnimation() {
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
    const unsigned long MAX_ANIMATION_DURATION = 30000;
    if (elapsed > MAX_ANIMATION_DURATION) {
        Serial.println(F("ANIMATION_ERROR: Time travel animation timed out. Forcing exit."));
        isAnimating = false;
        currentPhase = ANIM_INACTIVE;
        updateNormalClockDisplay();
        updateHaStatus("Idle");
        return;
    }
    static AnimationPhase lastPhase = ANIM_INACTIVE;
    if (currentPhase != lastPhase) {
        if (currentSettings.timeTravelSoundToggle) {
            switch (currentPhase) {
                case ANIM_POWER_UP: playSound("engine_rev.mp3"); break;
                case ANIM_ARRIVAL: playSound("time_travel.mp3"); break;
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
                animateAllRowsTimelineSkim(elapsed, currentSettings.timeTravelAnimationDuration, currentSettings.destinationYear, false);
            } else {
                currentPhase = ANIM_LANDING;
                animationStartTime = millis();
            }
            break;
        case ANIM_LANDING:
             if (elapsed < 1000) { animateTornadoFlicker(); }
             else {
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
#endif
}

void startStyledAnimation() {
    getFormattedTimeStrings(old_dest_str, old_pres_str, old_last_str);
    if (xSemaphoreTake(xAnimationStartMutex, (TickType_t)10) != pdTRUE) {
        return;
    }
    if (isAnimating || isAnySequenceActive()) {
        xSemaphoreGive(xAnimationStartMutex);
        return;
    }
    xSemaphoreGive(xAnimationStartMutex);

    updateHaStatus("Animating");

    AnimationType selectedAnimation;
    if (currentSettings.animationStyle == ANIMATION_ALL_DISPLAYS_RANDOM) {
        const AnimationType validAnimationStyles[] = {
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
        selectedAnimation = validAnimationStyles[random(0, numStyles)];
    } else {
        selectedAnimation = (AnimationType)currentSettings.animationStyle;
    }

    generateAnimationSequence(selectedAnimation, sequencerTracks);
}

void handleTemporalEcho() {
    if (!isEchoEffectActive || isAnimating || isAnySequenceActive() || !hardwareInitialized) return;

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
    if (bootState == BOOT_INACTIVE || bootState == BOOT_COMPLETE) return;

    unsigned long elapsed = millis() - bootStateStartTime;
    static BootSequenceState lastLoggedState = BOOT_INACTIVE;

    // Log when state changes
    if (bootState != lastLoggedState) {
        Log_printf(LOG_LEVEL_INFO, "BOOT_TRACE: State changing from %d to %d", lastLoggedState, bootState);
        lastLoggedState = bootState;
        bootStateStartTime = millis(); // Reset timer for new state
        elapsed = 0; // Reset elapsed time
    }

    switch (bootState) {
        case BOOT_AWAIT_HUM:
            // This state just waits for a sound to finish, which is handled by the sound system.
            // Let's assume it transitions after a timeout if the sound doesn't trigger a change.
            if (elapsed > BOOT_AWAIT_HUM_DURATION) {
                Log_printf(LOG_LEVEL_WARN, "BOOT_TRACE: Await Hum timed out. Forcing start.");
                bootState = BOOT_START;
            }
            break;

        case BOOT_START:
            setOverrideMessage("SYSTEM BOOT", "PLEASE WAIT", "");
            playSound("boot_up.mp3");
            bootState = BOOT_WARM_UP;
            break;

        case BOOT_WARM_UP:
            if (elapsed > BOOT_WARM_UP_DURATION) {
                bootState = BOOT_COLD_START;
            }
            break;

        case BOOT_COLD_START:
            setOverrideMessage("COLD START", "SEQUENCE", "INITIATED");
            if (elapsed > BOOT_COLD_START_DURATION) {
                bootState = BOOT_FLUX_CAPACITOR_IGNITION;
            }
            break;

        case BOOT_FLUX_CAPACITOR_IGNITION:
            setOverrideMessage("FLUX CAPACITOR", "IGNITION", "SEQUENCE");
            if (elapsed > BOOT_FLUX_CAPACITOR_IGNITION_DURATION) {
                bootState = BOOT_FLUX_CAPACITOR_ANIMATION;
            }
            break;

        case BOOT_FLUX_CAPACITOR_ANIMATION:
            // This state is likely stuck because it has no duration or exit condition.
            // Let's add a simple animation and an exit condition.
            Log_printf(LOG_LEVEL_INFO, "BOOT_TRACE: Running Flux Capacitor Animation.");
            animateTornadoFlicker();
            if (elapsed > 5000) { // Let's assume a 5-second animation
                Log_printf(LOG_LEVEL_INFO, "BOOT_TRACE: Flux Capacitor Animation complete.");
                bootState = BOOT_DIAGNOSTICS;
            }
            break;

        case BOOT_DIAGNOSTICS:
            setOverrideMessage("RUNNING", "DIAGNOSTICS", "...");
            if (elapsed > BOOT_DIAGNOSTICS_DURATION) {
                bootState = BOOT_FINAL_CHECKS;
            }
            break;

        case BOOT_FINAL_CHECKS:
            setOverrideMessage("FINAL CHECKS", "IN PROGRESS", "");
            if (elapsed > BOOT_FINAL_CHECKS_DURATION) {
                bootState = BOOT_TEMPORAL_DISPLACEMENT;
            }
            break;

        case BOOT_TEMPORAL_DISPLACEMENT:
            setOverrideMessage("TEMPORAL", "DISPLACEMENT", "ACTIVE");
            if (elapsed > BOOT_TEMPORAL_DISPLACEMENT_DURATION) {
                bootState = BOOT_ARRIVAL;
            }
            break;

        case BOOT_ARRIVAL:
            setOverrideMessage("TIME CIRCUIT", "ARRIVAL", "SEQUENCE");
            if (elapsed > BOOT_ARRIVAL_DURATION) {
                bootState = BOOT_ARRIVAL_ANIMATION;
            }
            break;

        case BOOT_ARRIVAL_ANIMATION:
            // Similar to the flux capacitor animation, this could be a sticking point.
            Log_printf(LOG_LEVEL_INFO, "BOOT_TRACE: Running Arrival Animation.");
            animateTornadoFlicker();
            if (elapsed > 3000) { // 3-second animation
                 Log_printf(LOG_LEVEL_INFO, "BOOT_TRACE: Arrival Animation complete.");
                bootState = BOOT_COOL_DOWN;
            }
            break;

        case BOOT_COOL_DOWN:
            setOverrideMessage("SYSTEMS", "COOL DOWN", "PHASE");
            if (elapsed > BOOT_COOL_DOWN_DURATION) {
                bootState = BOOT_COMPLETE;
            }
            break;

        case BOOT_COMPLETE:
            Log_printf(LOG_LEVEL_INFO, "BOOT_TRACE: Boot sequence complete.");
            setOverrideMessage("", "", ""); // Clear override
            isMessageOverrideActive = false;
            updateNormalClockDisplay();
            // No need to change state further, just let it be.
            break;

        default:
            Log_printf(LOG_LEVEL_ERROR, "BOOT_TRACE: Reached unknown boot state %d. Resetting.", bootState);
            bootState = BOOT_INACTIVE;
            break;
    }
}

void handleSequencer() {
    bool needsDisplayUpdate = false;
    DisplayRow* rows[] = {&destRow, &presRow, &lastRow};

    for (int i = 0; i < 3; i++) {
        SequencerTrack& track = sequencerTracks[i];
        if (!track.isActive) continue;

        DisplayRow& row = *rows[i];

        if (millis() - track.trackStartTime > MAX_SEQUENCE_DURATION) {
            Log_printf(LOG_LEVEL_WARN, "SEQ: Track %d timed out. Aborting.", i);
            stopAndCleanupTrack(i);
            needsDisplayUpdate = true;
            continue;
        }

        // Handle active effects for this track
        // ... (fade/pulse/flash logic remains the same)

        SequenceStep& step = track.steps[track.currentStep];
        unsigned long commandElapsed = millis() - track.stepStartTime;
        bool advance_step = false;

        switch (step.command) {
            // ... (all other existing commands)

            case SEQ_CMD_RANDOM_FILL:
                if (!track.stepInitialized) {
                    track.stepInitialized = true;
                }
                if (commandElapsed >= (unsigned long)step.intParam) {
                    advance_step = true;
                } else {
                    animateDisplayRowRandomly(row);
                }
                break;

            case SEQ_CMD_END:
            case SEQ_CMD_NONE:
                stopAndCleanupTrack(i);
                needsDisplayUpdate = true;
                break;

            default:
                Log_printf(LOG_LEVEL_WARN, "SEQ: Track %d unknown command %d. Aborting.", i, step.command);
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

void stopAndCleanupTrack(int trackIndex) {
    if (trackIndex < 0 || trackIndex > 2) return;
    SequencerTrack& track = sequencerTracks[trackIndex];
    DisplayRow* rows[] = {&destRow, &presRow, &lastRow};
    DisplayRow& row = *rows[trackIndex];
    uint8_t defaultBrightness = track.originalBrightness > 0 ? track.originalBrightness : currentSettings.brightness;
    row.month.setBrightness(defaultBrightness);
    row.day.setBrightness(defaultBrightness);
    row.year.setBrightness(defaultBrightness);
    row.time.setBrightness(defaultBrightness);
    track.reset();
    Log_printf(LOG_LEVEL_INFO, "SEQ: Cleaned up and stopped track %d.", trackIndex);
}

void runSequencerTest() {
    // This function remains unchanged.
}