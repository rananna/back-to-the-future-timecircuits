/**
 * @file Sequencer.h
 * @brief Defines the data structures and commands for the animation sequencer.
 *
 * This file contains the core components of the multi-track animation sequencer,
 * including the list of all possible commands (SequenceCommand), the structure
 * for a single atomic step in a sequence (SequenceStep), and the structure that
 * holds the state and command list for an entire animation track (SequencerTrack).
 * These components work together to enable complex, parallel animations across the
 * three display rows.
 */
#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <string>
#include "WString.h"

/**
 * @brief The maximum number of steps allowed in a single animation track.
 * @details This value is a trade-off between memory usage and animation complexity.
 * Each SequenceStep is ~136 bytes, so an array of 80 steps consumes about 10.9KB
 * of memory per track. With three tracks, this is a significant portion of the
 * ESP32's available RAM.
 */
#define MAX_SEQUENCE_STEPS 80

/**
 * @brief Defines the set of all available commands that can be executed by the animation sequencer.
 * @details Commands are categorized into basic actions, display manipulation, visual effects,
 * and advanced logic. Some commands are "blocking" (the sequencer waits for them to complete),
 * while others are "non-blocking" (the sequencer executes them and immediately proceeds).
 */
enum SequenceCommand {
    SEQ_CMD_NONE,               /**< No operation. Used as a placeholder or default. */
    SEQ_CMD_END,                /**< Marks the end of a sequence track. */

    // --- Basic, Non-Blocking Commands ---
    SEQ_CMD_WAIT,               /**< (Blocking) Pauses the track for a duration specified in `intParam` (milliseconds). */
    SEQ_CMD_SOUND,              /**< (Non-Blocking) Plays a sound effect specified in `stringParam`. */

    // --- Display Control, Non-Blocking Commands ---
    SEQ_CMD_SET_TEXT,           /**< (Non-Blocking) Sets the text of a display segment. `stringParam` is the text. */
    SEQ_CMD_CLEAR_SEGMENT,      /**< (Non-Blocking) Clears a specific display segment or an entire row. */
    SEQ_CMD_RESTORE_SEGMENT,    /**< (Non-Blocking) Restores a segment to its pre-animation display state. */
    SEQ_CMD_SET_BRIGHTNESS,     /**< (Non-Blocking) Sets the brightness of a display row. `intParam` is the brightness (0-15). */
    SEQ_CMD_RESTORE_ROW,        /**< (Non-Blocking) Restores a row to its pre-animation display state (e.g., clock, weather). */

    // --- Visual Effects, Blocking Commands ---
    SEQ_CMD_FADE_IN,            /**< (Blocking) Fades the display row in. `intParam` is duration (ms). */
    SEQ_CMD_FADE_OUT,           /**< (Blocking) Fades the display row out. `intParam` is duration (ms). */
    SEQ_CMD_PULSE,              /**< (Blocking) Pulses a segment or row. `intParam` is duration (ms). */
    SEQ_CMD_FLASH,              /**< (Blocking) Flashes a segment or row. `intParam` is duration (ms). */
    SEQ_CMD_MARQUEE,            /**< (Blocking) Scrolls text across a row. `stringParam` is the text, `intParam` is speed (ms). */
    SEQ_CMD_SCANNER,            /**< (Blocking) Creates a "Cylon" scanner effect. `intParam` is duration (ms), `intParam2` is speed (ms). */
    SEQ_CMD_TYPEWRITER,         /**< (Blocking) Reveals text one character at a time. `intParam` is speed per char (ms). */
    SEQ_CMD_WIPE,               /**< (Blocking) Wipes a solid bar across the display. `intParam` is duration (ms). */
    SEQ_CMD_BAR_GRAPH,          /**< (Blocking) Draws an animated bar graph. `intParam` is start %, `intParam2` is duration (ms). */
    SEQ_CMD_RANDOM_FLICKER_TEXT,/**< (Blocking) Randomly flickers characters. `intParam` is speed (ms), `intParam2` is duration (ms). */
    SEQ_CMD_SCRAMBLE_TEXT,      /**< (Blocking) Scrambles characters before revealing text. `intParam` is flicker speed (ms), `intParam2` is total duration (ms). */
    SEQ_CMD_SCROLL_IN,          /**< (Blocking) Scrolls text in from the side and centers it. `intParam` is speed (ms). */
    SEQ_CMD_CROSSFADE_TEXT,     /**< (Blocking) Fades from current text to new text. `intParam` is fade duration (ms). */

    // --- Logic & Advanced Commands ---
    SEQ_CMD_LOOP_START,         /**< (Non-Blocking) Marks the beginning of a loop. `intParam` is the number of iterations. */
    SEQ_CMD_LOOP_END,           /**< (Non-Blocking) Marks the end of a loop, jumping back to LOOP_START. */
    SEQ_CMD_COUNTDOWN,          /**< (Blocking) Displays a numeric countdown. `intParam` is the start value. */
    SEQ_CMD_TRIGGER_ANIMATION,  /**< (Global) Stops all tracks and starts a new global animation. `intParam` is the `AnimationType`. */
    SEQ_CMD_MQTT_PUBLISH,       /**< (Non-Blocking) Publishes a message to an MQTT topic. `stringParam` is topic, `stringParam2` is payload. */
    SEQ_CMD_DISPLAY_HA_SENSOR,  /**< (Blocking) Fetches and displays a Home Assistant sensor value. `stringParam` is the entity_id. */
    SEQ_CMD_CLEAR_ALL_ROWS,     /**< (Non-Blocking) Clears the text from all three display rows. */
    SEQ_CMD_RESTORE_ALL_ROWS    /**< (Non-Blocking) Restores all three rows to their pre-animation state. */
};

/**
 * @brief The maximum length for any string parameter used in a sequencer command.
 * @details This is critical for preventing buffer overflows when using `strncpy`.
 * It's set to a generous size to accommodate MQTT topics, text for display, and other string data.
 */
#define MAX_SEQ_STRING_LEN 64

/**
 * @brief Represents a single, atomic operation within an animation sequence.
 * @details Each step contains a command and all the parameters it needs to execute.
 * The use of fixed-size `char` arrays for string parameters is a deliberate design
 * choice to prevent memory corruption. Storing `const char*` from temporary `std::string`
 * objects created during sequence generation led to dangling pointers and crashes.
 * By copying the string data into these arrays, the `SequenceStep` takes ownership
 * of the data, ensuring its lifetime is managed correctly.
 */
struct SequenceStep {
    SequenceCommand command;    /**< The command to be executed for this step. */
    int targetRow;              /**< The target display row (0=TOP, 1=MIDDLE, 2=BOTTOM). */
    int targetSegment;          /**< The target display segment (0-3), or -1 for the entire row. */
    int intParam;               /**< First general-purpose integer parameter (e.g., duration, speed, count). */
    int intParam2;              /**< Second general-purpose integer parameter. */

    /**
     * @brief First string parameter. Used for text, sound file names, MQTT topics, etc.
     * @details Fixed-size array to prevent memory corruption from dangling pointers.
     */
    char stringParam[MAX_SEQ_STRING_LEN];

    /**
     * @brief Second string parameter. Used for MQTT payloads or other secondary text data.
     * @details Fixed-size array to prevent memory corruption from dangling pointers.
     */
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
     * @details This constructor is the primary way of creating new sequence steps.
     * It uses `strncpy` to safely copy the string data into the fixed-size internal
     * buffers, preventing buffer overflows and ensuring null termination.
     *
     * @param cmd The command to execute.
     * @param row The target display row (0-2).
     * @param seg The target segment (-1 for whole row).
     * @param p1 First integer parameter.
     * @param p2 Second integer parameter.
     * @param s1 First string parameter. Will be safely copied.
     * @param s2 Second string parameter. Will be safely copied.
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

/**
 * @brief Represents a single track of commands, managing the state for one of the three display rows.
 * @details An animation can have up to three tracks running in parallel. This struct holds the
 * list of commands (`steps`) for the track, tracks the current execution state (e.g., `currentStep`,
 * `isActive`), and maintains state variables for all possible ongoing effects like fades, pulses,
 * marquees, etc. This allows a command to initialize an effect and have the main sequencer loop
 * manage its execution over time.
 */
struct SequencerTrack {
    // --- Core Track State ---
    bool isActive;                      /**< If true, this track is currently being processed by the sequencer. */
    int currentStep;                    /**< The index of the current step in the `steps` array. */
    unsigned long stepStartTime;        /**< `millis()` timestamp when the current step began execution. */
    bool stepInitialized;               /**< Flag to ensure a step's setup logic is only run once. */
    unsigned long trackStartTime;       /**< `millis()` timestamp when the track started; used for global track timeouts. */

    // --- State for Looping ---
    int loopStartStep = -1;             /**< The index of the `SEQ_CMD_LOOP_START` step. */
    int loopCounter = 0;                /**< The current iteration count for the loop. */

    // --- State for Fade Effect ---
    bool isFading = false;              /**< True if a fade effect is in progress. */
    unsigned long fadeStartTime = 0;    /**< `millis()` timestamp when the fade started. */
    int fadeDuration = 0;               /**< Total duration of the fade in milliseconds. */
    bool isFadeIn = false;              /**< True for fade-in, false for fade-out. */
    uint8_t originalBrightness = 0;     /**< The brightness level before the fade started, to be restored after. */

    // --- State for Marquee (Scrolling Text) Effect ---
    bool isMarqueeActive = false;       /**< True if a marquee effect is in progress. */
    int marqueeSpeed = 120;             /**< The scroll speed in ms for this track's marquee. */
    std::string marqueeText;            /**< The full text being scrolled. */
    int marqueeScrollPosition = 0;      /**< The current horizontal scroll position. */
    unsigned long lastMarqueeScrollTime = 0; /**< `millis()` timestamp of the last scroll update. */

    // --- State for Pulse Effect ---
    bool isPulsing[4] = {false};        /**< True if a segment is pulsing. Indexed by segment (0-3). */
    unsigned long pulseEndTimes[4] = {0}; /**< `millis()` timestamp when the pulse effect should end for each segment. */
    bool pulseStates[4] = {false};      /**< The current on/off state of the pulse visual for each segment. */
    unsigned long lastPulseToggle[4] = {0}; /**< `millis()` timestamp of the last on/off toggle for each segment. */

    // --- State for Flash Effect ---
    bool isFlashing[4] = {false};       /**< True if a segment is flashing. Indexed by segment (0-3). */
    unsigned long flashEndTimes[4] = {0}; /**< `millis()` timestamp when the flash effect should end for each segment. */
    bool flashStates[4] = {false};      /**< The current on/off state of the flash visual for each segment. */
    unsigned long lastFlashToggle[4] = {0}; /**< `millis()` timestamp of the last on/off toggle for each segment. */

    // --- State for Random Flicker Effect ---
    bool isFlickering = false;          /**< True if a random flicker effect is in progress. */
    int flickerSpeed = 50;              /**< Delay in ms between flicker updates for this track. */
    unsigned long flickerEndTime = 0;   /**< `millis()` timestamp when the flicker effect should end. */
    unsigned long lastFlickerUpdate = 0;/**< `millis()` timestamp of the last character flicker. */
    std::string flickerOriginalText;    /**< The original text to restore after a flicker effect. */

    // --- State for High-Level Commands ---
    int countdownValue = 0;             /**< Current value for a `SEQ_CMD_COUNTDOWN`. */
    unsigned long countdownLastUpdate = 0; /**< `millis()` timestamp of the last countdown decrement. */

    int scannerPosition = 0;            /**< Current position (0-12) of the `SEQ_CMD_SCANNER` light. */
    bool scannerDirection = true;       /**< Current direction of the scanner (true=right, false=left). */
    unsigned long lastScannerUpdate = 0; /**< `millis()` timestamp of the last scanner position update. */

    int typewriterIndex = 0;            /**< Current character index for the `SEQ_CMD_TYPEWRITER` effect. */
    unsigned long lastTypewriterUpdate = 0; /**< `millis()` timestamp of the last character reveal. */

    int wipeSegment = 0;                /**< Current segment index for the `SEQ_CMD_WIPE` effect. */
    unsigned long lastWipeUpdate = 0;   /**< `millis()` timestamp of the last wipe segment update. */

    float barGraphPercentage = 0.0f;    /**< Current percentage for the `SEQ_CMD_BAR_GRAPH` effect. */
    unsigned long lastBarGraphUpdate = 0; /**< `millis()` timestamp of the last bar graph visual update. */
    unsigned long barGraphStartTime = 0;/**< `millis()` timestamp when the bar graph animation started. */

    int scrambleCharIndex = 0;          /**< The index of the character currently being "locked in" for the scramble effect. */
    unsigned long lastScrambleUpdate = 0; /**< `millis()` timestamp of the last random character update. */
    unsigned long lastScrambleLockInTime = 0; /**< `millis()` timestamp of the last character reveal/lock-in. */
    unsigned long scrambleLockInDelay = 0; /**< The calculated delay between each character lock-in. */
    std::string scrambleCurrentText;    /**< The string buffer holding the mix of scrambled and revealed text. */

    int crossfadePhase = 0;             /**< The current phase of the crossfade effect (0=inactive, 1=fading out, 2=fading in). */

    // --- State for Home Assistant Integration ---
    bool isWaitingForHAState = false;   /**< True if the track is paused, waiting for an MQTT message with a sensor state. */
    std::string haSensorTopic;          /**< The MQTT topic being subscribed to for the sensor state. */
    bool haStateReceived = false;       /**< Becomes true when the expected MQTT message arrives. */

    /**
     * @brief The array of sequence steps that define this track's animation.
     */
    SequenceStep steps[MAX_SEQUENCE_STEPS];

    /**
     * @brief Resets the track to a clean, default state.
     * @details This is crucial for ensuring that a new animation starts without any leftover
     * state from a previous one. It clears all flags, counters, and state variables.
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
        marqueeSpeed = 120; // Default scroll speed
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

        isFlickering = false;
        flickerSpeed = 50; // Default flicker speed
        flickerEndTime = 0;
        lastFlickerUpdate = 0;
        flickerOriginalText.clear();

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
        scrambleCharIndex = 0;
        lastScrambleUpdate = 0;
        lastScrambleLockInTime = 0;
        scrambleLockInDelay = 0;
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