#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <string>
#include "WString.h"

// Maximum number of steps in a single sequence track
#define MAX_SEQUENCE_STEPS 20

// Defines the available commands for the sequencer
enum SequenceCommand {
    SEQ_CMD_NONE,
    SEQ_CMD_WAIT,
    SEQ_CMD_MARQUEE,
    SEQ_CMD_FADE_IN,
    SEQ_CMD_FADE_OUT,
    SEQ_CMD_PULSE,
    SEQ_CMD_FLASH,
    SEQ_CMD_SOUND,
    SEQ_CMD_END // Marks the end of a sequence
};

// Represents a single step in a sequence
struct SequenceStep {
    SequenceCommand command;
    int targetRow;
    int targetSegment;
    int intParam;
    std::string stringParam;
};

// Represents a single track of commands for one display row
struct SequencerTrack {
    bool isActive;
    int currentStep;
    unsigned long stepStartTime;
    bool stepInitialized;

    // --- NEW: Local state for marquee commands ---
    bool isMarqueeActive = false;
    std::string marqueeText;
    int marqueeScrollPosition = 0;
    unsigned long lastMarqueeScrollTime = 0;

    // --- NEW: Local state for fade, pulse, and flash effects ---
    bool isFading = false;
    unsigned long fadeStartTime = 0;
    int fadeDuration = 0;
    bool isFadeIn = false;
    uint8_t originalBrightness = 0;

    // Pulse and flash states are per-segment (4 segments per row)
    bool isPulsing[4] = {false};
    unsigned long pulseEndTimes[4] = {0};
    bool pulseStates[4] = {false};
    unsigned long lastPulseToggle[4] = {0};

    bool isFlashing[4] = {false};
    unsigned long flashEndTimes[4] = {0};
    bool flashStates[4] = {false};
    unsigned long lastFlashToggle[4] = {0};

    SequenceStep steps[MAX_SEQUENCE_STEPS];

    /**
     * @brief Resets the track to a clean, default state.
     * @details Zeros out all state variables, ensuring that no leftover
     * effects or properties from a previous run can interfere with the next
     * sequence. This is critical for preventing visual glitches.
     */
    void reset() {
        isActive = false;
        currentStep = 0;
        stepStartTime = 0;
        stepInitialized = false;

        isMarqueeActive = false;
        marqueeText.clear();
        marqueeScrollPosition = 0;
        lastMarqueeScrollTime = 0;

        isFading = false;
        fadeStartTime = 0;
        fadeDuration = 0;
        isFadeIn = false;
        // Do not reset originalBrightness here, as it's set at the start of a fade

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

        // Clear out the old steps to prevent any carry-over
        for (int i = 0; i < MAX_SEQUENCE_STEPS; ++i) {
            steps[i] = { SEQ_CMD_NONE, 0, 0, 0, "" };
        }
    }
};

#endif // SEQUENCER_H