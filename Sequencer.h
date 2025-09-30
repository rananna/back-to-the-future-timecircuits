#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <string>
#include "WString.h"

// Maximum number of steps in a single sequence track
#define MAX_SEQUENCE_STEPS 20

// Defines the available commands for the sequencer
enum SequenceCommand {
    SEQ_CMD_NONE,
    SEQ_CMD_END, // Marks the end of a sequence

    // Basic Commands
    SEQ_CMD_WAIT,
    SEQ_CMD_SOUND,

    // Display Control
    SEQ_CMD_SET_TEXT,
    SEQ_CMD_CLEAR_SEGMENT,
    SEQ_CMD_SET_BRIGHTNESS,
    SEQ_CMD_RESTORE_ROW,

    // Visual Effects
    SEQ_CMD_FADE_IN,
    SEQ_CMD_FADE_OUT,
    SEQ_CMD_PULSE,
    SEQ_CMD_FLASH,
    SEQ_CMD_MARQUEE,
    SEQ_CMD_SCANNER,
    SEQ_CMD_TYPEWRITER,
    SEQ_CMD_WIPE,
    SEQ_CMD_BAR_GRAPH,
    SEQ_CMD_RANDOM_FLICKER_TEXT,
    SEQ_CMD_SCRAMBLE_TEXT,
    SEQ_CMD_SCROLL_IN,
    SEQ_CMD_CROSSFADE_TEXT,

    // Logic & Advanced
    SEQ_CMD_LOOP_START,
    SEQ_CMD_LOOP_END,
    SEQ_CMD_COUNTDOWN,
    SEQ_CMD_TRIGGER_ANIMATION,
    SEQ_CMD_MQTT_PUBLISH,
    SEQ_CMD_DISPLAY_HA_SENSOR
};

// Represents a single step in a sequence
struct SequenceStep {
    SequenceCommand command;
    int targetRow;
    int targetSegment;
    int intParam;
    int intParam2; // Added for commands needing a second integer
    std::string stringParam;
    std::string stringParam2; // Added for commands needing a second string
};

// Represents a single track of commands for one display row
struct SequencerTrack {
    bool isActive;
    int currentStep;
    unsigned long stepStartTime;
    bool stepInitialized;
    unsigned long trackStartTime; // --- NEW: Timeout for the entire track ---

    // --- State for Looping ---
    int loopStartStep = -1;
    int loopCounter = 0;

    // --- State for Effects ---
    bool isFading = false;
    unsigned long fadeStartTime = 0;
    int fadeDuration = 0;
    bool isFadeIn = false;
    uint8_t originalBrightness = 0;

    bool isMarqueeActive = false;
    std::string marqueeText;
    int marqueeScrollPosition = 0;
    unsigned long lastMarqueeScrollTime = 0;

    bool isPulsing[4] = {false};
    unsigned long pulseEndTimes[4] = {0};
    bool pulseStates[4] = {false};
    unsigned long lastPulseToggle[4] = {0};

    bool isFlashing[4] = {false};
    unsigned long flashEndTimes[4] = {0};
    bool flashStates[4] = {false};
    unsigned long lastFlashToggle[4] = {0};

    // --- State for New High-Level Commands ---
    int countdownValue = 0;
    unsigned long countdownLastUpdate = 0;

    int scannerPosition = 0;
    bool scannerDirection = true; // true=right, false=left
    unsigned long lastScannerUpdate = 0;

    int typewriterIndex = 0;
    unsigned long lastTypewriterUpdate = 0;

    int wipeSegment = 0;
    unsigned long lastWipeUpdate = 0;

    float barGraphPercentage = 0.0f;
    unsigned long lastBarGraphUpdate = 0;

    unsigned long lastFlickerUpdate = 0;
    std::string flickerOriginalText;

    int scrambleCharIndex = 0;
    unsigned long lastScrambleUpdate = 0;
    std::string scrambleCurrentText;

    // --- NEW: State for Crossfade command ---
    int crossfadePhase = 0; // 0=inactive, 1=fading out, 2=fading in

    // --- State for HA Sensor Command ---
    bool isWaitingForHAState = false;
    std::string haSensorTopic;
    bool haStateReceived = false;

    SequenceStep steps[MAX_SEQUENCE_STEPS];

    /**
     * @brief Resets the track to a clean, default state.
     */
    void reset() {
        isActive = false;
        currentStep = 0;
        stepStartTime = 0;
        stepInitialized = false;
        trackStartTime = 0;

        loopStartStep = -1;
        loopCounter = 0;

        isMarqueeActive = false;
        marqueeText.clear();
        marqueeScrollPosition = 0;
        lastMarqueeScrollTime = 0;

        isFading = false;
        fadeStartTime = 0;
        fadeDuration = 0;
        isFadeIn = false;

        for (int i = 0; i < 4; ++i) {
            isPulsing[i] = false;
            pulseEndTimes[i] = 0;
            pulseStates[i] = false;
            lastPulseToggle[i] = 0;

            isFlashing[i] = false;
            flashEndTimes[i] = 0;
            flashStates[i] = false;
            lastFlashToggle[i] = 0;
        }

        countdownValue = 0;
        countdownLastUpdate = 0;
        scannerPosition = 0;
        scannerDirection = true;
        lastScannerUpdate = 0;
        typewriterIndex = 0;
        lastTypewriterUpdate = 0;
        wipeSegment = 0;
        lastWipeUpdate = 0;
        barGraphPercentage = 0.0f;
        lastBarGraphUpdate = 0;
        lastFlickerUpdate = 0;
        flickerOriginalText.clear();
        scrambleCharIndex = 0;
        lastScrambleUpdate = 0;
        scrambleCurrentText.clear();

        crossfadePhase = 0;

        isWaitingForHAState = false;
        haSensorTopic.clear();
        haStateReceived = false;

        for (int i = 0; i < MAX_SEQUENCE_STEPS; ++i) {
            steps[i] = { SEQ_CMD_NONE, 0, 0, 0, 0, "", "" };
        }
    }
};

#endif // SEQUENCER_H