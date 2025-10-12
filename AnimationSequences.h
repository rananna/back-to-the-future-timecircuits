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
#include "AnimationTypes.h"

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

/**
 * @brief Helper function to convert a string representation of an animation to its `AnimationType` enum value.
 * @details This is the reverse of `animationTypeToString`. It's used to parse animation names
 * received from the UI or MQTT.
 *
 * @param str A constant character pointer to the string name of the animation (e.g., "Time Travel").
 * @return The `AnimationType` enum value corresponding to the string.
 */
AnimationType animationTypeFromString(const char* str);

#endif // ANIMATION_SEQUENCES_H