/**
 * @file AnimationTypes.h
 * @brief Defines the core AnimationType enum.
 *
 * This header exists to break a circular dependency between HardwareControl.h
 * and AnimationSequences.h. Both files need to know about the AnimationType
 * enum, but they also include each other. By placing the enum in its own
 * file, it can be safely included by any other file without causing
 * compilation errors.
 */
#ifndef ANIMATION_TYPES_H
#define ANIMATION_TYPES_H

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
    ANIMATION_INTRUDER_ALERT,

    // --- Named Sequences from UI (defined in sequences.json) ---
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

#endif // ANIMATION_TYPES_H