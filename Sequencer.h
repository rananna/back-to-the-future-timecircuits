#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <string>
#include "WString.h"

// Maximum number of steps in a single sequence track
#define MAX_SEQUENCE_STEPS 80

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
    SEQ_CMD_DISPLAY_HA_SENSOR,
    SEQ_CMD_RESTORE_ALL_ROWS
};

// --- FIX: Define max string length for sequencer commands ---
#define MAX_SEQ_STRING_LEN 64

// Represents a single step in a sequence
struct SequenceStep {
    SequenceCommand command;
    int targetRow;
    int targetSegment;
    int intParam;
    int intParam2;
    // --- FIX: Use fixed-size char arrays to prevent memory corruption ---
    // Storing const char* from temporary std::string objects led to dangling pointers.
    // These arrays ensure the string data is safely copied and owned by the step.
    char stringParam[MAX_SEQ_STRING_LEN];
    char stringParam2[MAX_SEQ_STRING_LEN];

    /**
     * @brief Default constructor. Initializes the step to a safe, empty state.
     */
    SequenceStep() :
        command(SEQ_CMD_NONE),
        targetRow(0),
        targetSegment(0),
        intParam(0),
        intParam2(0)
    {
        stringParam[0] = '\0';
        stringParam2[0] = '\0';
    }

    /**
     * @brief Constructs a sequence step and safely copies string parameters.
     *
     * @param cmd The command to execute.
     * @param row The target display row (0-2).
     * @param seg The target segment (-1 for whole row).
     * @param p1 First integer parameter.
     * @param p2 Second integer parameter.
     * @param s1 First string parameter. Safely copied.
     * @param s2 Second string parameter. Safely copied.
     */
    SequenceStep(SequenceCommand cmd, int row, int seg, int p1, int p2, const char* s1, const char* s2) :
        command(cmd),
        targetRow(row),
        targetSegment(seg),
        intParam(p1),
        intParam2(p2)
    {
        if (s1) {
            strncpy(stringParam, s1, MAX_SEQ_STRING_LEN - 1);
            stringParam[MAX_SEQ_STRING_LEN - 1] = '\0'; // Ensure null termination
        } else {
            stringParam[0] = '\0';
        }

        if (s2) {
            strncpy(stringParam2, s2, MAX_SEQ_STRING_LEN - 1);
            stringParam2[MAX_SEQ_STRING_LEN - 1] = '\0'; // Ensure null termination
        } else {
            stringParam2[0] = '\0';
        }
    }
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
    unsigned long barGraphStartTime = 0; // New timer for bar graph animation

    unsigned long lastFlickerUpdate = 0;
    std::string flickerOriginalText;

    int scrambleCharIndex = 0;
    unsigned long lastScrambleUpdate = 0;
    unsigned long lastScrambleLockInTime = 0; // New timer for character lock-in
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
        barGraphStartTime = 0;
        lastFlickerUpdate = 0;
        flickerOriginalText.clear();
        scrambleCharIndex = 0;
        lastScrambleUpdate = 0;
        lastScrambleLockInTime = 0;
        scrambleCurrentText.clear();

        crossfadePhase = 0;

        isWaitingForHAState = false;
        haSensorTopic.clear();
        haStateReceived = false;

        for (int i = 0; i < MAX_SEQUENCE_STEPS; ++i) {
            steps[i] = SequenceStep(); // Use the default constructor
        }
    }
};

#endif // SEQUENCER_H