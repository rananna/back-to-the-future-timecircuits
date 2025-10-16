/**
 * @file AnimationSequences.h
 * @brief Defines the available built-in animation sequences and their generation functions.
 *
 * This file declares the `AnimationType` enum, which provides a unique identifier for every
 * pre-defined animation sequence available in the firmware. It also provides the function
 * prototypes for generating these sequences, parsing sequences from JSON, and converting
 * the enum to a string for logging and UI purposes.
 */
#ifndef ANIMATION_SEQUENCES_H
#define ANIMATION_SEQUENCES_H

#include "Sequencer.h"

/**
 * @brief Enum to identify which built-in animation sequence to generate.
 * @details This enum serves as a master list of all animations that can be triggered by name
 * from the web UI, Home Assistant, or internal code. The `generateAnimationSequence` function
 * uses a value of this type to determine which specific animation to create.
 *
 * `ANIMATION_TYPE_MAX` is a special member that must always be last. It is not a real
 * animation but is used to validate enum values and determine the total number of available
 * animations, which is useful for features like "Randomize".
 */
enum AnimationType {
    // --- Legacy or programmatically generated animations ---
    ANIMATION_SEQUENTIAL_FLICKER,
    ANIMATION_RANDOM_FLICKER,
    ANIMATION_COUNTING_UP,
    ANIMATION_WAVE_FLICKER,
    ANIMATION_TORNADO_FLICKER,
    ANIMATION_CAPACITOR_CHARGE_UP,
    ANIMATION_DIGITAL_RAIN,
    ANIMATION_WAVEFORM_COLLAPSE,
    ANIMATION_TIMELINE_SKIM,
    ANIMATION_TEMPORAL_DESYNC,
    ANIMATION_GLITCHY_JUMP_CUT,
    ANIMATION_PLASMA_WARM_UP,
    ANIMATION_TIME_WARP_STREAKS,
    ANIMATION_CHARACTER_SCANLINE,
    ANIMATION_FOCUS_IN,
    ANIMATION_CODE_BREAKER,
    ANIMATION_TEMPORAL_PARADOX,
    ANIMATION_DIGIT_CASCADE,
    ANIMATION_ELECTRIC_SURGE,
    ANIMATION_FLIP_DISC_DISPLAY,
    ANIMATION_INTERFERENCE_PATTERN,
    ANIMATION_RANDOMIZE_ALL,
    ANIMATION_ALL_DISPLAYS_RANDOM,
    ANIMATION_LIGHTNING,
    ANIMATION_SCANNER,
    ANIMATION_TIME_TRAVEL_TUNNEL,
    ANIMATION_FLUX_CAPACITOR_OVERLOAD,
    ANIMATION_FIRE_TRAILS,
    ANIMATION_SPARKLE_REVEAL,
    ANIMATION_COUNTDOWN,
    ANIMATION_SYSTEM_ERROR,
    ANIMATION_TIME_CIRCUITS_LOCK_IN,

    // --- Named Sequences from UI (defined in sequences.json) ---
    ANIMATION_INTRUDER_ALERT,
    ANIMATION_TIME_TRAVEL,
    ANIMATION_PARTY_MODE,
    ANIMATION_KNIGHT_RIDER,
    ANIMATION_LOADING,
    ANIMATION_ERROR,
    ANIMATION_FLUX_CHARGE,
    ANIMATION_TACHYONS,
    ANIMATION_DATA_STREAM,
    ANIMATION_WORMHOLE_COLLAPSE,

    /**
     * @brief A meta-value that must always be the last item.
     * Used for counting the number of animations and for input validation.
     */
    ANIMATION_TYPE_MAX
};

/**
 * @brief Main function to generate a sequence of animation steps based on the specified type.
 * @details This function acts as a factory. It takes an `AnimationType` and populates the provided
 * `tracks` array with the corresponding `SequenceStep` commands that create the animation.
 *
 * @param animType The `AnimationType` enum value specifying which animation to generate.
 * @param tracks A pointer to an array of three `SequencerTrack` objects to be populated.
 */
void generateAnimationSequence(AnimationType animType, SequencerTrack tracks[3]);

/**
 * @brief Parses a JSON string to populate the sequencer tracks.
 * @details This function is used for animations defined in `sequences.json` or sent via MQTT.
 * It deserializes the JSON string and populates the `tracks` array with the defined commands.
 *
 * @param tracks A pointer to an array of three `SequencerTrack` objects to be populated.
 * @param json_string A standard string containing the sequence definition in JSON format.
 */
void parseSequenceFromJson(SequencerTrack tracks[3], const std::string& json_string);

/**
 * @brief Helper function to convert an `AnimationType` enum value to its string representation.
 * @details This is useful for logging, debugging, and sending the current animation state to the UI.
 *
 * @param type The `AnimationType` enum value to convert.
 * @return A constant character pointer to the string name of the animation (e.g., "ANIMATION_PARTY_MODE").
 */
const char* animationTypeToString(AnimationType type);

#endif // ANIMATION_SEQUENCES_H