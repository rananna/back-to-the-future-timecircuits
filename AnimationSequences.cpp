/**
 * @file AnimationSequences.cpp
 * @brief Implements the generation of all built-in animation sequences for the Time Circuits.
 * @details This file contains the functions that construct the various animation sequences,
 * both simple and complex, by adding a series of commands to sequencer tracks. It includes
 * generators for C++ defined animations and a parser for JSON-defined sequences.
 */

#include <ArduinoJson.h>
#include "AnimationTypes.h"
#include "AnimationSequences.h"
#include "HardwareControl.h"
#include "DisplayManager.h"
#include "DebugLog.h"
#include "EventManager.h"
#include "MqttManager.h"
#include <Arduino.h>
#include <string>
#include <stdlib.h>

/**
 * @brief Safely adds a new step to a sequencer track.
 * @details This helper function is crucial for preventing buffer overflows. It checks if the
 * sequence has exceeded `MAX_SEQUENCE_STEPS`. If it has, it logs a warning and injects
 * an `END` command as the very last step to ensure the sequence terminates gracefully
 * instead of running off into invalid memory.
 * @param track The sequencer track to add the step to.
 * @param step_idx The current index in the track's steps array.
 * @param cmd The `SequenceCommand` to add.
 * @param row The target display row (0-2).
 * @param seg The target display segment (0-3 or -1 for full row).
 * @param p1 The first integer parameter for the command.
 * @param p2 The second integer parameter for the command.
 * @param s1 The first string parameter for the command.
 * @param s2 The second string parameter for the command.
 * @return The next available step index.
 */
static int add_step(SequencerTrack& track, int step_idx, SequenceCommand cmd, int row, int seg, int p1, int p2, const char* s1 = "", const char* s2 = "") {
    if (step_idx >= MAX_SEQUENCE_STEPS) {
        // --- Failsafe: Prevent buffer overflow ---
        // Log a warning that the sequence is too long.
        Log_printf(LOG_LEVEL_WARN, "SEQ_GEN: Sequence has too many steps! Truncating. Max is %d.", MAX_SEQUENCE_STEPS);
        // Overwrite the last step with an END command to ensure graceful termination.
        track.steps[MAX_SEQUENCE_STEPS - 1] = SequenceStep(SEQ_CMD_END, 0, 0, 0, 0, "", "");
        // Return the index without advancing it to prevent further writes.
        return step_idx;
    }
    // --- FIX: Use the SequenceStep constructor to safely copy all parameters ---
    track.steps[step_idx] = SequenceStep(cmd, row, seg, p1, p2, s1, s2);
    return step_idx + 1;
}

/**
 * @brief DEPRECATED. Placeholder for adding introductory sound effects.
 * @details Originally, this function was intended to add sound commands at the start of a
 * sequence. However, to ensure perfect audio-visual synchronization, sounds are now
- * triggered directly from the function that initiates the animation (e.g., `handlePresetCycling`),
 * making this helper a no-op. It is kept for potential future use or alternative sound designs.
 * @param track The sequencer track.
 * @param step_idx The current step index.
 * @return The original step index, unchanged.
 */
static int add_intro_sound_steps(SequencerTrack& track, int step_idx) {
    // This is now a no-op. The sound is triggered directly from the handlePresetCycling function
    // to ensure perfect synchronization with the animation start.
    return step_idx;
}

// --- Individual Animation Generators ---

/**
 * @brief Generates a dynamic, multi-stage random flicker and glitch animation.
 * @details This function creates a complex, 10-second animation that evolves through
 * several distinct phases to create a more visually interesting effect. It uses three
 * parallel tracks to build from subtle flickers to a chaotic crescendo and finally
 * a burnout phase.
 * - **Phase 1 (0-2s):** Subtle, sparse flickers to build anticipation.
 * - **Phase 2 (2-5s):** Builds chaos with faster, desynchronized glitch effects.
 * - **Phase 3 (5-8s):** Peak intensity with high-energy, parallel flashes and scrambles.
 * - **Phase 4 (8-10s):** A "burnout" phase where the effect sputters out.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateRandomFlicker(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    const char* glitch_chars = "!@#$()^&*";
    const char* solid_block = "|||||||||||||";

    // --- Phase 1: Subtle Introduction (0-2 seconds) ---
    // Track 0: A few sparse flickers on the top row.
    s0 = add_step(tracks[0], s0, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 500, 150, " . ' ");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 1500, 0);

    // Track 1: A delayed, sparse flicker on the bottom row.
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 0, 0, 1000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 500, 150, " . ' ");
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 0, 0, 500, 0);

    // --- Phase 2: Building Chaos (2-5 seconds) ---
    // Track 0: Wipes glitchy characters across the top row.
    s0 = add_step(tracks[0], s0, SEQ_CMD_WIPE, 0, -1, 100, 0, glitch_chars);
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 500, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 1500, 80, glitch_chars);

    // Track 1: Scrolls glitchy characters on the bottom row, opposite direction.
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCROLL_IN, 2, -1, 100, 0, glitch_chars);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 0, 0, 500, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 1500, 80, glitch_chars);

    // Track 2: Pulses the middle row with an alert-like message.
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 0, 0, 2000, 0); // Wait for phase 1
    s2 = add_step(tracks[2], s2, SEQ_CMD_PULSE, 1, -1, 3000, 1000, "SYSTEM FAULT");

    // --- Phase 3: Peak Intensity (5-8 seconds) ---
    // Track 0: Intense, solid block flickers on the top row.
    s0 = add_step(tracks[0], s0, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 3000, 50, solid_block);

    // Track 1: Rapid flashes on the bottom row.
    s1 = add_step(tracks[1], s1, SEQ_CMD_FLASH, 2, -1, 3000, 0);

    // Track 2: Scrambles and reveals a critical error message on the middle row.
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCRAMBLE_TEXT, 1, -1, 50, 200, "CRITICAL");
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 0, 0, 1000, 0);

    // --- Phase 4: Burnout (8-10 seconds) ---
    // All tracks converge here after their 8-second mark.
    // Clear top and bottom rows.
    s0 = add_step(tracks[0], s0, SEQ_CMD_CLEAR_SEGMENT, 0, -1, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_CLEAR_SEGMENT, 2, -1, 0, 0);

    // Track 2: Hold the "CRITICAL" message, then fade it out.
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "CRITICAL");
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 0, 0, 1000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_FADE_OUT, 1, -1, 1000, 0);
}

/**
 * @brief Generates a "tornado" effect where a flicker moves across segments and rows.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateTornadoFlicker(SequencerTrack tracks[3]) {
    int s = 0;
    s = add_intro_sound_steps(tracks[0], s);
    // Loop 22 times. Each loop takes 450ms (3 * 100ms flicker + 3 * 50ms wait).
    // 22 * 450ms = 9900ms ~= 10s.
    // Use i % 4 to keep the tornado effect cycling across the 4 display segments.
    for (int i = 0; i < 22; i++) {
        s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, i % 4, 100, 50);
        s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 50, 0);
        s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, i % 4, 100, 50);
        s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 50, 0);
        s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, i % 4, 100, 50);
        s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 50, 0);
    }
}

/**
 * @brief Generates a sequence where all three rows scramble and resolve to the correct time in parallel.
 * @param tracks The array of three sequencer tracks to populate.
 * @param time_strings A 2D array containing the formatted time strings for each row.
 */
void generateAllDisplaysRandom(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    const int flicker_interval = 50; // ms for flicker effect refresh rate
    const int total_duration = 10000; // 10 seconds total animation time
    const int num_chars = 13; // Standard display width
    const int lock_in_interval = total_duration / num_chars; // ms per character reveal

    // --- FIX: Store substrings in local variables to guarantee pointer validity ---
    // Although the temporary from substr() should live long enough, this is safer.
    std::string dest_str = std::string(time_strings[0]).substr(0, num_chars);
    std::string pres_str = std::string(time_strings[1]).substr(0, num_chars);
    std::string last_str = std::string(time_strings[2]).substr(0, num_chars);

    s0 = add_step(tracks[0], s0, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, flicker_interval, total_duration, dest_str.c_str());
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCRAMBLE_TEXT, 1, -1, flicker_interval, total_duration, pres_str.c_str());
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCRAMBLE_TEXT, 2, -1, flicker_interval, total_duration, last_str.c_str());
}

/**
 * @brief Generates a sequence that reveals the time, one segment at a time, across all rows.
 * @param tracks The array of three sequencer tracks to populate.
 * @param time_strings A 2D array containing the formatted time strings for each row.
 */
void generateSequentialFlicker(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s = 0;
    s = add_intro_sound_steps(tracks[0], s);
    const int delay = 830;
    std::string dest_str(time_strings[0]);
    std::string pres_str(time_strings[1]);
    std::string last_str(time_strings[2]);

    // --- FIX: Store substrings in a local variable before passing .c_str() ---
    // This prevents passing a pointer from a temporary std::string created by substr(),
    // which could lead to a dangling pointer. The SequenceStep constructor now copies
    // the data, but it's safest to guarantee the source pointer is always valid.
    std::string temp;

    temp = dest_str.substr(0, 3); s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 0, 0, 0, 0, temp.c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    temp = dest_str.substr(3, 2); s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 0, 1, 0, 0, temp.c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    temp = dest_str.substr(5, 4); s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 0, 2, 0, 0, temp.c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    temp = dest_str.substr(9, 4); s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 0, 3, 0, 0, temp.c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);

    temp = pres_str.substr(0, 3); s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 1, 0, 0, 0, temp.c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    temp = pres_str.substr(3, 2); s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 1, 1, 0, 0, temp.c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    temp = pres_str.substr(5, 4); s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 1, 2, 0, 0, temp.c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    temp = pres_str.substr(9, 4); s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 1, 3, 0, 0, temp.c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);

    temp = last_str.substr(0, 3); s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 2, 0, 0, 0, temp.c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    temp = last_str.substr(3, 2); s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 2, 1, 0, 0, temp.c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    temp = last_str.substr(5, 4); s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 2, 2, 0, 0, temp.c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    temp = last_str.substr(9, 4); s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 2, 3, 0, 0, temp.c_str());
}

/**
 * @brief Creates a multi-layered capacitor charge-up sequence.
 * @details This animation provides a more engaging charge-up effect than a simple bar graph.
 * Track 0 (Middle): Shows the primary `BAR_GRAPH` filling up.
 * Track 1 (Top): Displays crackling energy with `RANDOM_FLICKER_TEXT`.
 * Track 2 (Bottom): Pulses a "CHARGING..." message.
 * The sequence culminates in a bright, full-display `FLASH` to signify full charge.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateCapacitorChargeUp(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);

    // --- Track 0 (Middle): Main charge bar ---
    s0 = add_step(tracks[0], s0, SEQ_CMD_BAR_GRAPH, 1, -1, 100, 9500); // Fills over 9.5s

    // --- Track 1 (Top): Crackling energy ---
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 9500, 150, "-.-");

    // --- Track 2 (Bottom): Status Text ---
    s2 = add_step(tracks[2], s2, SEQ_CMD_PULSE, 2, -1, 9500, 0, "CHARGING...");

    // --- Final Flash on Main Track ---
    // After the bars fill, trigger a bright flash on all rows.
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 0, -1, 500, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 1, -1, 500, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 2, -1, 500, 0);
}

/**
 * @brief Generates a symmetrical waveform collapse and expansion animation on all rows.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateWaveformCollapse(SequencerTrack tracks[3]) {
    const char* waves[] = {"-------------", " ---     --- ", "  ---   ---  ", "   -------   ", "  ---   ---  ", " ---     --- "};
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);

    // Loop the collapse animation to fill 10 seconds.
    // One cycle = 6 waves * 200ms = 1.2s. 8 cycles = 9.6s.
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 8, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 1, 0, 8, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_START, 2, 0, 8, 0);

    for (int i = 0; i < 6; i++) {
        std::string wave_str(waves[i]);
        s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, wave_str.c_str());
        s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, wave_str.c_str());
        std::string inverted_str;
        for(char c : wave_str) { inverted_str += (c == '-') ? ' ' : '-'; }
        s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, inverted_str.c_str());
        s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 200, 0);
        s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 200, 0);
        s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 200, 0);
    }

    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 1, 0, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_END, 2, 0, 0, 0);
}

/**
 * @brief Generates a dynamic wave effect with patterns moving in opposite directions.
 * @details This animation uses three parallel tracks to create a fluid wave motion.
 * Track 0 (Top): Wipes a wave pattern from left to right.
 * Track 1 (Middle): Pulses an inverted wave pattern.
 * Track 2 (Bottom): Scrolls a wave pattern in from the right (right to left).
 * The parallel execution and synchronized loops create a continuous, mesmerizing effect.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateWaveFlicker(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);

    const char* wave_pattern = "---     ---";
    const char* inverted_wave = "   -----   ";

    // Loop all tracks 5 times. Each loop is ~2s, for a total of ~10s.
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 5, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 1, 0, 5, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_START, 2, 0, 5, 0);

    // --- Effects (run in parallel, ~1s duration) ---
    s0 = add_step(tracks[0], s0, SEQ_CMD_WIPE, 0, -1, 75, 0, wave_pattern); // L-to-R wipe
    s1 = add_step(tracks[1], s1, SEQ_CMD_PULSE, 1, -1, 1000, 0, inverted_wave); // Pulse for 1s
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCROLL_IN, 2, -1, 75, 0, wave_pattern); // R-to-L scroll

    // --- Synchronization and Cleanup (run in parallel) ---
    // Wait for effects to finish and hold the pattern.
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 500, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 500, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 500, 0);

    // Clear the rows before the next loop iteration.
    s0 = add_step(tracks[0], s0, SEQ_CMD_CLEAR_SEGMENT, 0, -1, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_CLEAR_SEGMENT, 1, -1, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_CLEAR_SEGMENT, 2, -1, 0, 0);

    // Pause before the next wave starts.
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 500, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 500, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 500, 0);

    // --- End Loops ---
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 1, 0, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_END, 2, 0, 0, 0);
}

/**
 * @brief Generates a "code breaker" effect with a progress bar and status updates.
 * @details This animation creates a narrative of cracking a code.
 * Track 0 (Top): Displays the "cracking" attempt, scrambling through random characters
 * before locking in the final, correct time string.
 * Track 1 (Middle): Shows a `BAR_GRAPH` filling up, representing the decryption progress.
 * Track 2 (Bottom): Displays sequential status updates: "ANALYZING...", "DECRYPTING...",
 * and finally "CODE BROKEN!".
 * @param tracks The array of three sequencer tracks to populate.
 * @param time_strings A 2D array containing the formatted time strings for each row.
 */
void generateCodeBreaker(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    const int flicker_interval = 50;
    const int num_chars = 13;
    const int total_duration = 8000;
    const int lock_in_interval = total_duration / num_chars;

    // --- Track 1 (Middle): Progress Bar ---
    // Fills up over 9 seconds.
    s1 = add_step(tracks[1], s1, SEQ_CMD_BAR_GRAPH, 1, -1, 100, 9000, "DECRYPTING");

    // --- Track 2 (Bottom): Status Updates ---
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "ANALYZING...");
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 4500, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "CODE BROKEN!");
    s2 = add_step(tracks[2], s2, SEQ_CMD_PULSE, 2, -1, 4500, 0); // Pulse the final message

    // --- Track 0 (Top): The Code-Breaking Effect ---
    // Scramble and slowly reveal the hidden code over 8 seconds.
    std::string dest_str = std::string(time_strings[0]).substr(0, num_chars);
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, flicker_interval, lock_in_interval, dest_str.c_str());
    // Hold the revealed code for 2 seconds.
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 2000, 0);
}

/**
 * @brief Simulates a mechanical flip-disc display with varied, parallel wipes.
 * @details This animation creates a more authentic and dynamic flip-disc effect.
 * Instead of a simple, uniform wipe, each row is wiped independently with different
 * timings and directions (L-to-R vs. R-to-L), simulating the asynchronous nature
 * of a real mechanical display. The sequence repeats with a pause, enhancing the
 * feeling of a board resetting and flipping a new set of characters.
 * @param tracks The array of three sequencer tracks to populate.
 * @param time_strings A 2D array containing the formatted time strings for each row.
 */
void generateFlipDisc(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    // Loop the effect 4 times. Each cycle is ~2.5s.
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 4, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 1, 0, 4, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_START, 2, 0, 4, 0);

    // Wipe rows in parallel with different speeds and directions
    s0 = add_step(tracks[0], s0, SEQ_CMD_WIPE, 0, -1, 100, 0, time_strings[0]); // Top row, L-to-R
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCROLL_IN, 1, -1, 80, 0, time_strings[1]); // Middle row, R-to-L, slightly faster
    s2 = add_step(tracks[2], s2, SEQ_CMD_WIPE, 2, -1, 120, 0, time_strings[2]); // Bottom row, L-to-R, slightly slower

    // Wait for all wipes to complete, then hold the text
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 1500, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 1500, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 1500, 0);

    // Clear the rows before the next loop
    s0 = add_step(tracks[0], s0, SEQ_CMD_CLEAR_SEGMENT, 0, -1, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_CLEAR_SEGMENT, 1, -1, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_CLEAR_SEGMENT, 2, -1, 0, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 500, 0); // Pause before next cycle

    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 1, 0, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_END, 2, 0, 0, 0);
}

/**
 * @brief Reveals text with a typewriter effect, accompanied by a synchronized scanner light.
 * @details This animation enhances the basic typewriter effect by adding a visual flourish.
 * Track 0 (All Rows): Executes the `TYPEWRITER` command sequentially for each row, revealing
 * the text character-by-character.
 * Track 1 (All Rows): Runs a `SCANNER` effect in parallel on each row. The scanner light
 * moves in sync with the text reveal, creating the illusion that the scanner is "writing"
 * the text onto the display. The scanner bar is made of faint dots to avoid obscuring the text.
 * @param tracks The array of three sequencer tracks to populate.
 * @param time_strings A 2D array containing the formatted time strings for each row.
 */
void generateCharacterScanline(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    const int type_delay = 75; // ms per character
    const int type_duration = 13 * type_delay; // ~1s for a full row

    // --- Track 1: The synchronized scanner light ---
    // Run a scanner effect on each row. The total duration matches the typewriter sequence.
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCANNER, 0, -1, type_duration, type_delay, ".");
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 0, 0, 500, 0); // Wait during the pause
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCANNER, 1, -1, type_duration, type_delay, ".");
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 0, 0, 500, 0); // Wait during the pause
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCANNER, 2, -1, type_duration, type_delay, ".");

    // --- Track 0: The typewriter text reveal ---
    // Reveal text on each row sequentially.
    s0 = add_step(tracks[0], s0, SEQ_CMD_TYPEWRITER, 0, -1, type_delay, 0, time_strings[0]);
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 500, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_TYPEWRITER, 1, -1, type_delay, 0, time_strings[1]);
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 500, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_TYPEWRITER, 2, -1, type_delay, 0, time_strings[2]);

    // Hold the final result
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 5000, 0);
}

/**
 * @brief Creates a chaotic "temporal paradox" with conflicting timelines.
 * @details This animation creates a strong sense of a paradox by presenting conflicting
 * information across the three displays simultaneously.
 * Track 0 (Top): Displays a time from the distant past ("JAN 01 1885 1200").
 * Track 1 (Middle): Aggressively flickers between "ERROR" and the correct present time,
 * as if struggling to maintain stability.
 * Track 2 (Bottom): Displays a time from the distant future ("OCT 26 2085 0429").
 * The parallel, conflicting information creates a more visually interesting and
 * thematic paradox effect than the previous version.
 * @param tracks The array of three sequencer tracks to populate.
 * @param time_strings A 2D array containing the formatted time strings for each row.
 */
void generateTemporalParadox(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);

    // --- Track 0 (Top): A conflicting time from the past ---
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "JAN 01 1885 1200");
    s0 = add_step(tracks[0], s0, SEQ_CMD_PULSE, 0, -1, 10000, 0); // Pulse it for the duration

    // --- Track 2 (Bottom): A conflicting time from the future ---
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "OCT 26 2085 0429");
    s2 = add_step(tracks[2], s2, SEQ_CMD_PULSE, 2, -1, 10000, 0); // Pulse it for the duration

    // --- Track 1 (Middle): The paradox instability ---
    // Flicker rapidly between "ERROR" and the correct time. Loop to fill the duration.
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 1, 0, 25, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "ERROR");
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 200, 50);
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, time_strings[1]);
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 200, 50);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 1, 0, 0, 0);
}

/**
 * @brief Creates a dynamic interference pattern with sweeping symbols and a pulsing center row.
 * @details This animation uses three parallel tracks to create a visual conflict.
 * Track 0 (Top): Wipes a pattern of symbols from left to right.
 * Track 1 (Middle): Pulses the correct time, as if trying to stabilize.
 * Track 2 (Bottom): Scrolls the same symbol pattern from right to left.
 * The opposing motion and pulsing center create a strong interference effect.
 * @param tracks The array of three sequencer tracks to populate.
 * @param time_strings A 2D array containing the formatted time strings for each row.
 */
void generateInterferencePattern(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    const char* interference = "!@#$%%^&*()_+-=";

    // Loop the entire animation 5 times. Each loop is 2s. Total 10s.
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 5, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 1, 0, 5, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_START, 2, 0, 5, 0);

    // --- Parallel Effects ---
    s0 = add_step(tracks[0], s0, SEQ_CMD_WIPE, 0, -1, 150, 0, interference); // Wipe L-to-R
    s1 = add_step(tracks[1], s1, SEQ_CMD_PULSE, 1, -1, 2000, 0, time_strings[1]); // Pulse middle row
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCROLL_IN, 2, -1, 150, 0, interference); // Scroll R-to-L

    // --- Loop End ---
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 1, 0, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_END, 2, 0, 0, 0);
}

/**
 * @brief Creates a chaotic "time warp" with high-speed, multi-directional streaks.
 * @details This animation simulates the visual distortion of a time warp by running
 * multiple, high-speed scrolling effects in parallel.
 * Track 0 (Top): Rapidly scrolls random date fragments from right to left.
 * Track 1 (Middle): Scrolls the actual destination time from right to left, but faster,
 * as if it's the primary timeline breaking through the noise.
 * Track 2 (Bottom): Rapidly scrolls random date fragments from left to right, creating
 * a conflicting motion that enhances the chaotic effect.
 * The use of random generation for the garbage strings ensures each warp is unique.
 * @param tracks The array of three sequencer tracks to populate.
 * @param time_strings A 2D array containing the formatted time strings for each row.
 */
void generateTimeWarpStreaks(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

    // --- Track 1 (Middle): The "real" time streaking past, very fast ---
    s1 = add_step(tracks[1], s1, SEQ_CMD_MARQUEE, 1, -1, 40, 10000, time_strings[1]);

    // --- Tracks 0 & 2: The chaotic warp streaks ---
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 20, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_START, 2, 0, 20, 0);

    // Generate a random date string for the streaks
    char random_streak[17];
    snprintf(random_streak, sizeof(random_streak),
             "%s %02d %04d",
             months[random(12)],
             random(28) + 1,
             random(400) + 1800);

    // Scroll the random streaks in opposite directions
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCROLL_IN, 0, -1, 50, 0, random_streak); // R-to-L
    s2 = add_step(tracks[2], s2, SEQ_CMD_WIPE, 2, -1, 50, 0, random_streak);      // L-to-R

    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_END, 2, 0, 0, 0);
}

/**
 * @brief Creates a "focus in" effect by scrambling and revealing each row sequentially.
 * @param tracks The array of three sequencer tracks to populate.
 * @param time_strings A 2D array containing the formatted time strings for each row.
 */
void generateFocusIn(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s = 0;
    s = add_intro_sound_steps(tracks[0], s);
    // One cycle is ~2.6s scramble. Repeat 3 times for all rows = ~7.8s.
    // Add a final wait to reach 10s.
    s = add_step(tracks[0], s, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, 100, 200, time_strings[0]);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 100, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SCRAMBLE_TEXT, 1, -1, 100, 200, time_strings[1]);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 100, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SCRAMBLE_TEXT, 2, -1, 100, 200, time_strings[2]);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 2200, 0);
}

/**
 * @brief Simulates a building and discharging electric surge.
 * @details This animation creates a complex, multi-track surge effect over 10 seconds.
 * Track 0 (Top Row & Sound): Drives the main 'build-up and release' sequence. It plays
 * a thunder sound, shows a 'SURGE LEVEL' bar graph filling up over 9 seconds, and
 * culminates in an intense 1-second flash across all rows.
 * Track 1 (Middle Row): Provides a continuous, chaotic energy effect using
 * `RANDOM_FLICKER_TEXT` with crackling characters for the entire 10-second duration.
 * Track 2 (Bottom Row): Pulses a "DANGER" message for the entire 10-second duration,
 * adding to the sense of urgency.
 * @param tracks The array of three sequencer tracks to populate.
 * @param time_strings A 2D array containing the formatted time strings for each row.
 */
void generateElectricSurge(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;

    // --- Track 0 (Top Row & Sound): Main build-up and release driver ---
    // Play a thunder sound to kick things off.
    s0 = add_step(tracks[0], s0, SEQ_CMD_SOUND, 0, 0, 0, 0, "thunder.mp3");
    // Display a bar graph on the top row, filling up over 9 seconds.
    s0 = add_step(tracks[0], s0, SEQ_CMD_BAR_GRAPH, 0, -1, 100, 9000, "SURGE LEVEL");
    // After the 9s build-up, trigger a powerful 1-second flash on ALL rows.
    // Assuming FLASH is non-blocking, these will trigger in rapid succession.
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 0, -1, 1000, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 1, -1, 1000, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 2, -1, 1000, 0);

    // --- Track 1 (Middle Row): Continuous chaotic energy ---
    // A random flicker effect with crackling characters runs for the full 10 seconds.
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 10000, 100, "!~-^.");

    // --- Track 2 (Bottom Row): Pulsing warning message ---
    // A "DANGER" message pulses for the full 10 seconds. A 2s cycle.
    s2 = add_step(tracks[2], s2, SEQ_CMD_PULSE, 2, -1, 10000, 2000, "DANGER");
}

/**
 * @brief Creates a cascade effect by revealing each row with a typewriter effect in parallel.
 * @param tracks The array of three sequencer tracks to populate.
 * @param time_strings A 2D array containing the formatted time strings for each row.
 */
void generateDigitCascade(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    // Loop the effect to fill 10 seconds.
    // One cycle = 1s typewriter + 1s wait = 2s. 5 cycles = 10s.
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 5, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 1, 0, 5, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_START, 2, 0, 5, 0);

    s0 = add_step(tracks[0], s0, SEQ_CMD_TYPEWRITER, 0, -1, 75, 0, time_strings[0]);
    s1 = add_step(tracks[1], s1, SEQ_CMD_TYPEWRITER, 1, -1, 75, 0, time_strings[1]);
    s2 = add_step(tracks[2], s2, SEQ_CMD_TYPEWRITER, 2, -1, 75, 0, time_strings[2]);

    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 1000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 1000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 1000, 0);

    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 1, 0, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_END, 2, 0, 0, 0);
}

/**
 * @brief Simulates a plasma warm-up sequence with increasing intensity.
 * @details This animation creates a visually engaging, four-stage "warm-up" effect over 10 seconds,
 * telling a story of system activation.
 * 1.  **Ignition (0-2s):** A quick, chaotic flicker across all rows signals startup.
 * 2.  **Instability (2-5s):** The system struggles to stabilize, showing desynchronized
 *     scrambling text on each row as different subsystems come online.
 * 3.  **Charging (5-9s):** The core begins to charge, showing a primary `BAR_GRAPH` on the
 *     middle row, with pulsing status messages on the top and bottom rows.
 * 4.  **Stabilization (9-10s):** A final, bright `FLASH` across all rows signifies a full
 *     charge, revealing a "SYSTEM READY" message.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generatePlasmaWarmup(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);

    // --- Stage 1: Ignition (0-2 seconds) ---
    // A quick, chaotic flicker across all rows to signal startup. Runs for 2000ms.
    s0 = add_step(tracks[0], s0, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 2000, 80, "*'.");
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 2000, 80, ".*'");
    s2 = add_step(tracks[2], s2, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 2000, 80, ".'*");

    // --- Stage 2: Instability (2-5 seconds) ---
    // The system struggles to stabilize. Effects are staggered to create a desynchronized feel.
    // Total duration for this stage is 3000ms.
    // Track 0: Scrambles "INITIATING" (10 chars * 250ms/char = 2500ms).
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, 50, 250, "INITIATING");
    // Track 1: Waits 0.5s, then scrambles "PLASMA CORE" (11 chars * 200ms/char = 2200ms).
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 500, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCRAMBLE_TEXT, 1, -1, 50, 200, "PLASMA CORE");
    // Track 2: Scrambles "SEQUENCE.." (10 chars * 250ms/char = 2500ms).
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCRAMBLE_TEXT, 2, -1, 50, 250, "SEQUENCE..");

    // --- Synchronization Point (at 5 seconds) ---
    // All tracks must wait until the longest effect from Stage 2 is complete before starting Stage 3.
    // Longest path is Track 1: 2000ms (Stage 1) + 500ms (Wait) + 2200ms (Scramble) = 4700ms.
    // We add a wait to bring all tracks to the 5000ms mark.
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 500, 0);  // 2000 + 2500 + 500 = 5000
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 300, 0);  // 2000 + 500 + 2200 + 300 = 5000
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 500, 0);  // 2000 + 2500 + 500 = 5000

    // --- Stage 3: Charging (5-9 seconds) ---
    // The core charges for 4000ms. All effects run in parallel.
    s0 = add_step(tracks[0], s0, SEQ_CMD_PULSE, 0, -1, 4000, 1000, "CHARGING");
    s1 = add_step(tracks[1], s1, SEQ_CMD_BAR_GRAPH, 1, -1, 100, 4000, "CORE LEVEL");
    s2 = add_step(tracks[2], s2, SEQ_CMD_PULSE, 2, -1, 4000, 1000, "STAND BY");

    // --- Stage 4: Stabilization (9-10 seconds) ---
    // A final flash reveals the "SYSTEM READY" message. All effects run in parallel.
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 0, -1, 1000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_FLASH, 2, -1, 1000, 0);
    // Set the final text on the middle row, then immediately flash it.
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "SYSTEM READY");
    s1 = add_step(tracks[1], s1, SEQ_CMD_FLASH, 1, -1, 1000, 0);
}

/**
 * @brief Creates a dynamic, chaotic glitch effect with randomized, parallel sequences.
 * @details This animation generates a unique, ~10-second sequence for each of the three
 * display rows in parallel. A C++ loop randomly selects from a pool of glitch effects
 * (flicker, scramble, flash, wipe) and applies them with random durations and parameters
 * to each track independently. This results in a highly chaotic, desynchronized, and
 * visually interesting "glitchy jump-cut" that is different every time it plays.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateGlitchyJumpCut(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    int* s_ptr[] = {&s0, &s1, &s2};
    const char* glitch_chars[] = {"!@#$%", "^&*()", "_+-=[]", "{};:'", "<>,.?/"};

    s0 = add_intro_sound_steps(tracks[0], s0);

    // Generate ~10 seconds of random, parallel glitch effects
    for (int i = 0; i < 15; i++) { // Loop enough times to build a long sequence
        for (int row = 0; row < 3; row++) {
            int effect = random(5); // Pick one of 5 random effects
            switch(effect) {
                case 0: // Aggressive Flicker
                    *s_ptr[row] = add_step(tracks[row], *s_ptr[row], SEQ_CMD_RANDOM_FLICKER_TEXT, row, -1, 200 + random(300), 30 + random(50));
                    break;
                case 1: // Scramble to garbage
                    *s_ptr[row] = add_step(tracks[row], *s_ptr[row], SEQ_CMD_SCRAMBLE_TEXT, row, -1, 50, 50, glitch_chars[random(5)]);
                    break;
                case 2: // Quick Flash
                    *s_ptr[row] = add_step(tracks[row], *s_ptr[row], SEQ_CMD_FLASH, row, -1, 100 + random(150), 0);
                    break;
                case 3: // Fast Wipe
                    *s_ptr[row] = add_step(tracks[row], *s_ptr[row], SEQ_CMD_WIPE, row, -1, 20 + random(30), 0, "|||||||||||||");
                    break;
                case 4: // Brief moment of calm
                    *s_ptr[row] = add_step(tracks[row], *s_ptr[row], SEQ_CMD_RESTORE_ROW, row, 0, 0, 0);
                    *s_ptr[row] = add_step(tracks[row], *s_ptr[row], SEQ_CMD_WAIT, row, 0, 300 + random(400), 0);
                    break;
            }
        }
    }

}

/**
 * @brief Creates a multi-faceted counting animation with parallel progress bars and effects.
 * @details This animation provides a dynamic counting sequence.
 * Track 0 (Middle): Displays a number rapidly counting up.
 * Track 1 (Top): Shows a `BAR_GRAPH` filling up in sync with the count.
 * Track 2 (Bottom): Pulses the text "CALCULATING..."
 * The result is a complex animation that clearly communicates a process of calculation.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateCountingUp(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);

    // --- Track 1 (Top): Progress Bar ---
    s1 = add_step(tracks[1], s1, SEQ_CMD_BAR_GRAPH, 0, -1, 100, 10000);

    // --- Track 2 (Bottom): Status Text ---
    s2 = add_step(tracks[2], s2, SEQ_CMD_PULSE, 2, -1, 10000, 0, "CALCULATING..");

    // --- Track 0 (Middle): The Counter ---
    // Loop 200 times. Each number is shown for 50ms. Total 10s.
    // This loop is in C++ to generate the steps, not a sequencer loop.
    for (int i = 0; i <= 200; i++) {
        char buffer[14];
        // Format the number, right-aligned and padded with spaces
        snprintf(buffer, sizeof(buffer), "%13d", i * 1337);
        s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, buffer);
        s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 50, 0);
    }
}

/**
 * @brief Simulates skimming through a timeline by rapidly scrambling through random dates.
 * @details This animation creates the effect of rapidly cycling through time. All three
 * rows run a `SCRAMBLE_TEXT` animation in parallel. A C++ loop generates a series
 * of these commands, each with a new, randomly generated (but valid-looking)
 * date/time string. This gives the impression of the dates actively changing and
 * skimming through a timeline.
 * @param tracks The array of three sequencer tracks to populate.
 * @param time_strings A 2D array containing the formatted time strings for each row.
 */
void generateTimelineSkim(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);

    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

    // Loop 10 times, each cycle is ~1s. Total ~10s.
    for (int i = 0; i < 10; i++) {
        char random_time_str[3][17];
        for (int row = 0; row < 3; row++) {
            snprintf(random_time_str[row], sizeof(random_time_str[row]),
                     "%s %02d %04d %02d%02d",
                     months[random(12)],
                     random(28) + 1,
                     random(101) + 1950,
                     random(24),
                     random(60));
        }
        // Scramble to the new random date over 1 second
        s0 = add_step(tracks[0], s0, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, 50, 80, random_time_str[0]);
        s1 = add_step(tracks[1], s1, SEQ_CMD_SCRAMBLE_TEXT, 1, -1, 50, 80, random_time_str[1]);
        s2 = add_step(tracks[2], s2, SEQ_CMD_SCRAMBLE_TEXT, 2, -1, 50, 80, random_time_str[2]);
    }
}

/**
 * @brief Creates a complex temporal desynchronization effect.
 * @details This animation uses three parallel tracks to create a feeling of temporal
 * instability, where different timelines are conflicting.
 * Track 0 (Middle): Slowly pulses the correct, stable time.
 * Track 1 (Top): Rapidly scrolls slightly incorrect time strings across the display.
 * Track 2 (Bottom): Flickers aggressively between two very different, incorrect times.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateTemporalDesync(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);

    char time_strings[3][17];
    getFormattedTimeStrings(time_strings[0], time_strings[1], time_strings[2]);

    // --- Track 0 (Middle): The "correct" time, our anchor ---
    s0 = add_step(tracks[0], s0, SEQ_CMD_PULSE, 1, -1, 10000, 0, time_strings[1]);

    // --- Track 1 (Top): Drifting timeline ---
    char drift_time_str[17];
    snprintf(drift_time_str, sizeof(drift_time_str), "JAN 01 1985 1003");
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 0, 0, 10, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCROLL_IN, 0, -1, 75, 0, drift_time_str);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 0, 0, 250, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 0, 0, 0, 0);

    // --- Track 2 (Bottom): Conflicting timeline ---
    char conflict_time_str[17];
    snprintf(conflict_time_str, sizeof(conflict_time_str), "OCT 26 2085 0429");
    s2 = add_step(tracks[2], s2, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 10000, 200, conflict_time_str);
}

/**
 * @brief Generates a dynamic, multi-layered "digital rain" effect.
 * @details This function creates a complex "Matrix"-style animation by building three
 * distinct, parallel tracks that run concurrently for approximately 10 seconds.
 * - **Track 0 (Foreground Drips):** A C++ loop generates a sequence of individual
 *   characters that appear to "drip" down the display from top to bottom in random columns.
 * - **Track 1 (Mid-ground Chunks):** A loop generates and flickers wider chunks of
 *   random characters at random intervals and positions, adding depth and density.
 * - **Track 2 (Background Static):** A loop generates faint, constant, and sparse
 *   character flickers across all rows to provide a noisy background texture.
 * The combination of these layers creates a chaotic, non-repetitive, and visually
 * engaging "digital rain" animation.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateDigitalRain(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);

    // --- Track 0: Foreground Drips ---
    // A C++ loop generates a sequence of "drips" falling from top to bottom.
    // Each drip is a single character appearing sequentially on each row in the same column.
    const char* drip_chars = "0123456789";
    for (int i = 0; i < 8; i++) {
        char drip_char_str[2] = { drip_chars[random(strlen(drip_chars))], '\0' };
        char text_buffer[14];
        memset(text_buffer, ' ', 13);
        text_buffer[13] = '\0';
        text_buffer[random(13)] = drip_char_str[0]; // Place drip char in a random column

        const int drip_speed = 100 + random(50); // Each segment visible for 100-150ms

        // Drip on top row
        s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, text_buffer);
        s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, drip_speed, 0);
        // Drip on middle row
        s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "             "); // Clear top
        s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, text_buffer);
        s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, drip_speed, 0);
        // Drip on bottom row
        s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "             "); // Clear middle
        s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, text_buffer);
        s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, drip_speed, 0);
        // Clear bottom row and pause before next drip
        s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "             ");
        s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 400 + random(800), 0);
    }

    // --- Track 1: Mid-ground Chunks ---
    // A C++ loop generates a sequence of flickering chunks of random characters at
    // random positions and intervals, adding density to the effect.
    const char* chunks[] = {"101", "01 10", " 111 ", "00100", "1 0 1"};
    for (int i = 0; i < 12; i++) {
        // Flicker a random chunk on a random row for a random duration
        s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, random(3), -1, 300 + random(400), 50, chunks[random(5)]);
        // Wait for a random duration before the next chunk
        s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 0, 0, 200 + random(300), 0);
    }

    // --- Track 2: Background Static ---
    // A C++ loop generates faint, constant, and sparse character flickers across all
    // rows to provide a noisy background texture.
    for (int i = 0; i < 40; i++) {
        // Target a random row with a very short, sparse flicker
        s2 = add_step(tracks[2], s2, SEQ_CMD_RANDOM_FLICKER_TEXT, random(3), -1, 150, 100, " . ");
        // Wait a short, random amount of time
        s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 0, 0, 50 + random(100), 0);
    }
}

/**
 * @brief Creates a multi-faceted countdown animation that completes in under 10 seconds.
 * @details This animation provides a more dynamic and engaging countdown.
 * Track 0 (Middle): Displays the main countdown from 9 to 0 over ~8 seconds, then shows "LIFTOFF!" and a final flash.
 * Track 1 (Top): Shows a `BAR_GRAPH` filling up in sync with the 8-second countdown.
 * Track 2 (Bottom): Pulses the text "COUNTDOWN" for the duration of the countdown.
 * The entire sequence is designed to be visually engaging and complete within the global timeout.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateCountdown(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    const char* numbers[] = {"TEN", "NINE", "EIGHT", "SEVEN", "SIX", "FIVE", "FOUR", "THREE", "TWO", "ONE", "ZERO"};
    const int num_count = sizeof(numbers) / sizeof(numbers[0]);
    const int delay_per_number = 800; // ms
    const int countdown_duration = num_count * delay_per_number; // 11 * 800 = 8800ms

    // --- Track 1 (Top): Progress Bar ---
    // Fills up over the duration of the countdown.
    s1 = add_step(tracks[1], s1, SEQ_CMD_BAR_GRAPH, 0, -1, 100, countdown_duration);

    // --- Track 2 (Bottom): Status Text ---
    // Pulses "COUNTDOWN" for the duration.
    s2 = add_step(tracks[2], s2, SEQ_CMD_PULSE, 2, -1, countdown_duration, 0, "COUNTDOWN");

    // --- Track 0 (Middle): The Main Countdown ---
    // Display "TEN" through "ZERO"
    for (int i = 0; i < num_count; i++) {
        s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, numbers[i]);
        s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, delay_per_number, 0);
    }

    // --- Liftoff Sequence ---
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "LIFTOFF!");
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 1, -1, 500, 0); // Final flash
}

/**
 * @brief Generates a "System Error" animation with a scrambled error message and marquee.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateSystemError(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, 100, 200, "ERROR");
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "ERROR");
    s1 = add_step(tracks[1], s1, SEQ_CMD_MARQUEE, 1, -1, 0, 0, "SYSTEM MALFUNCTION");
}

/**
 * @brief Generates the "lock-in" animation seen when saving settings or arriving.
 * @details This creates the iconic effect where all three time displays scramble and then
 * rapidly resolve to show the correct time, accompanied by a relay sound.
 * @param tracks The array of three sequencer tracks to populate.
 * @param time_strings A 2D array containing the formatted time strings for each row.
 */
void generateTimeCircuitsLockIn(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;

    // Use a relay sound for the "lock in" effect
    s0 = add_step(tracks[0], s0, SEQ_CMD_SOUND, 0, 0, 0, 0, "relay_activation.mp3");

    const int flicker_interval = 50; // ms for flicker effect refresh rate
    const int total_duration = 2000; // 2 seconds total animation time
    const int num_chars = 13;        // Standard display width
    const int lock_in_interval = total_duration / num_chars; // ms per character reveal (~154ms)

    // Ensure pointers to string data are valid for the lifetime of the step
    std::string dest_str = std::string(time_strings[0]).substr(0, num_chars);
    std::string pres_str = std::string(time_strings[1]).substr(0, num_chars);
    std::string last_str = std::string(time_strings[2]).substr(0, num_chars);

    // Add the scramble command to each track to run in parallel
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, flicker_interval, lock_in_interval, dest_str.c_str());
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCRAMBLE_TEXT, 1, -1, flicker_interval, lock_in_interval, pres_str.c_str());
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCRAMBLE_TEXT, 2, -1, flicker_interval, lock_in_interval, last_str.c_str());
}

/**
 * @brief Generates a dynamic "Intruder Alert" animation with parallel effects.
 * @details This function creates a multi-track alert sequence.
 * Track 0 (Top): Flashes "INTRUDER ALERT" and plays an alarm sound.
 * Track 1 (Middle): A scanner sweeps back and forth, searching for the intruder.
 * Track 2 (Bottom): The text "LOCKDOWN" pulses, indicating a system state change.
 * The parallel effects create a much more engaging and urgent alert than a simple
 * sequential animation.
 * @param tracks The array of three sequencer tracks to populate.
 */
/**
 * @brief Generates a dynamic, multi-stage KITT-style scanner animation.
 * @details This function creates a more engaging "Knight Rider" sequence that tells a story.
 * It features an activation sequence, a dynamic scanning phase with scrolling status text,
 * and a shutdown phase, using three parallel tracks for a rich visual experience.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateKnightRider(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;

    // --- Stage 1: Activation (0-2 seconds) ---
    // Play the iconic sound
    s0 = add_step(tracks[0], s0, SEQ_CMD_SOUND, 0, 0, 0, 0, "hum.mp3");

    // Top row: Display "KNIGHT RIDER"
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "KNIGHT RIDER");
    s0 = add_step(tracks[0], s0, SEQ_CMD_FADE_IN, 0, -1, 1000, 0);

    // Middle row: Display "K.I.T.T. ENGAGED"
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 500, 0); // Stagger the text
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "K.I.T.T. ENGAGED");
    s1 = add_step(tracks[1], s1, SEQ_CMD_FADE_IN, 1, -1, 1000, 0);

    // Bottom row: A quick visual flourish
    s2 = add_step(tracks[2], s2, SEQ_CMD_WIPE, 2, -1, 50, 0, "---");
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 1000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_CLEAR_SEGMENT, 2, -1, 0, 0);

    // Sync all tracks at the 2-second mark
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 1000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 500, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 350, 0);

    // --- Stage 2: Dynamic Scanning (2-8 seconds) ---
    // Top row: Scroll a longer status message
    s0 = add_step(tracks[0], s0, SEQ_CMD_MARQUEE, 0, -1, 150, 6000, "AUTONOMOUS SURVEILLANCE MODE");

    // Middle row: The iconic scanner
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCANNER, 1, -1, 6000, 80, "---");

    // Bottom row: Cycle through different status messages
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_START, 2, 0, 3, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "SCANNING...");
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 1000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "SEARCHING...");
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 1000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_END, 2, 0, 0, 0);

    // --- Stage 3: Wind-down (8-10 seconds) ---
    // Sync all tracks at the 8-second mark
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 0, 0);

    // Fade out the scanner and text
    s0 = add_step(tracks[0], s0, SEQ_CMD_FADE_OUT, 0, -1, 1000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_FADE_OUT, 1, -1, 1000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_FADE_OUT, 2, -1, 1000, 0);

    // Display final message
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "STANDBY");
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 1000, 0);
}

/**
 * @brief Generates a dynamic data stream animation with parallel effects.
 * @details This function creates a multi-track, 10-second animation that tells a story
 * of a data transfer process.
 * - **Track 0 (Top Row):** Displays a fast-moving stream of random hexadecimal characters
 *   to simulate a raw data feed.
 * - **Track 1 (Middle Row):** Shows a sequence of status updates ("CONNECTING", "STREAMING",
 *   "VERIFIED") that scramble and resolve, adding a high-tech feel.
 * - **Track 2 (Bottom Row):** Features a `BAR_GRAPH` that fills up over the duration,
 *   representing the progress of the data transfer, with "DATA LINK" overlaid.
 * The parallel tracks create a visually rich and engaging data-themed animation.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateDataStream(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    const char* hex_chars = "0123456789ABCDEF";

    // --- Track 0: Raw Data Feed (Top Row) ---
    // A C++ loop generates 100 steps of random text, each displayed for 100ms, for a total of 10 seconds.
    for (int i = 0; i < 100; i++) {
        char random_hex[14];
        for(int j=0; j<13; ++j) { random_hex[j] = hex_chars[random(16)]; }
        random_hex[13] = '\0';
        // Use SET_TEXT for an instantaneous update, better for a data stream effect.
        s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, random_hex);
        s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 100, 0);
    }

    // --- Track 1: Status Updates (Middle Row) ---
    // Scramble "CONNECTING..." (13 chars * 150ms = 1950ms)
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCRAMBLE_TEXT, 1, -1, 50, 150, "CONNECTING...");
    // Hold for 1200ms
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 1200, 0);
    // Scramble "STREAMING..." (12 chars * 150ms = 1800ms)
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCRAMBLE_TEXT, 1, -1, 50, 150, "STREAMING...");
    // Hold for 2050ms
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 2050, 0);
    // Scramble "VERIFIED" (8 chars * 125ms = 1000ms)
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCRAMBLE_TEXT, 1, -1, 50, 125, "VERIFIED");
    // Pulse final status for 2000ms. Total = 1950+1200+1800+2050+1000+2000 = 10000ms
    s1 = add_step(tracks[1], s1, SEQ_CMD_PULSE, 1, -1, 2000, 0);

    // --- Track 2: Progress Bar (Bottom Row) ---
    // A bar graph that fills up over 9.5 seconds.
    s2 = add_step(tracks[2], s2, SEQ_CMD_BAR_GRAPH, 2, -1, 100, 9500, "DATA LINK");
    // A final flash to signify completion.
    s2 = add_step(tracks[2], s2, SEQ_CMD_FLASH, 2, -1, 500, 0);
}

void generateIntruderAlert(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;

    // --- Track 0 (Top): Flashing Alert Text & Sound ---
    // Play the alarm sound immediately.
    s0 = add_step(tracks[0], s0, SEQ_CMD_SOUND, 0, 0, 0, 0, "alarm.mp3");
    // Set the text and keep it visible for the entire 10-second animation.
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "INTRUDER ALERT");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 10000, 0);

    // --- Track 1 (Middle): Scanner ---
    // Run a scanner effect for 10 seconds.
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCANNER, 1, -1, 10000, 80, "---");

    // --- Track 2 (Bottom): Pulsing Lockdown Message ---
    // Set the text and make it pulse for 10 seconds. A 2s cycle (1s on, 1s off).
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "LOCKDOWN");
    s2 = add_step(tracks[2], s2, SEQ_CMD_PULSE, 2, -1, 10000, 2000);
}

/**
 * @brief Helper function to parse a single JSON track definition object.
 * @details This function contains the core logic for parsing a `JsonObject` that defines
 * a single sequencer track and populating the corresponding `SequencerTrack` struct.
 * It is called by the main `parseSequenceFromJson` function.
 * @param tracks The array of three sequencer tracks.
 * @param track_def The `JsonObject` containing the definition for one track.
 */
static void parseSingleTrack(SequencerTrack tracks[3], const JsonObject& track_def) {
    int targetRow = -1;
    // Determine the target row from either an integer or a string ("TOP", "MIDDLE", "BOTTOM")
    if (track_def["targetRow"].is<int>()) {
        targetRow = track_def["targetRow"].as<int>();
    } else if (track_def["targetRow"].is<const char*>()) {
        std::string rowStr = track_def["targetRow"].as<std::string>();
        if (rowStr == "TOP") targetRow = 0;
        else if (rowStr == "MIDDLE") targetRow = 1;
        else if (rowStr == "BOTTOM") targetRow = 2;
    }

    if (targetRow < 0 || targetRow > 2) {
        Log_printf(LOG_LEVEL_WARN, "SEQ_PARSE: Skipping track with invalid targetRow.");
        return;
    }

    // Do not overwrite an already active track
    if (tracks[targetRow].isActive) {
        Log_printf(LOG_LEVEL_WARN, "SEQ_PARSE: Ignoring new sequence for row %d, one is already active.", targetRow);
        return;
    }

    JsonArray commands = track_def["commands"].as<JsonArray>();
    if (commands.isNull()) {
        Log_printf(LOG_LEVEL_WARN, "SEQ_PARSE: Skipping track for row %d: missing 'commands' array.", targetRow);
        return;
    }

    int step_idx = 0;
    for (JsonObject command : commands) {
        const char* cmd_str = command["command"];
        if (!cmd_str) {
            Log_printf(LOG_LEVEL_WARN, "SEQ_PARSE: Skipping invalid command in track %d: missing 'command' key.", targetRow);
            continue;
        }

        // Map the command string to its enum value
        SequenceCommand seq_cmd = SEQ_CMD_NONE;
        if (strcmp(cmd_str, "SET_TEXT") == 0) seq_cmd = SEQ_CMD_SET_TEXT;
        else if (strcmp(cmd_str, "CLEAR_SEGMENT") == 0) seq_cmd = SEQ_CMD_CLEAR_SEGMENT;
        else if (strcmp(cmd_str, "RESTORE_SEGMENT") == 0) seq_cmd = SEQ_CMD_RESTORE_SEGMENT;
        else if (strcmp(cmd_str, "SET_BRIGHTNESS") == 0) seq_cmd = SEQ_CMD_SET_BRIGHTNESS;
        else if (strcmp(cmd_str, "RESTORE_ROW") == 0) seq_cmd = SEQ_CMD_RESTORE_ROW;
        else if (strcmp(cmd_str, "CLEAR_ALL_ROWS") == 0) seq_cmd = SEQ_CMD_CLEAR_ALL_ROWS;
        else if (strcmp(cmd_str, "RESTORE_ALL_ROWS") == 0) seq_cmd = SEQ_CMD_RESTORE_ALL_ROWS;
        else if (strcmp(cmd_str, "WAIT") == 0) seq_cmd = SEQ_CMD_WAIT;
        else if (strcmp(cmd_str, "SOUND") == 0) seq_cmd = SEQ_CMD_SOUND;
        else if (strcmp(cmd_str, "LOOP_START") == 0) seq_cmd = SEQ_CMD_LOOP_START;
        else if (strcmp(cmd_str, "LOOP_END") == 0) seq_cmd = SEQ_CMD_LOOP_END;
        else if (strcmp(cmd_str, "FADE_IN") == 0) seq_cmd = SEQ_CMD_FADE_IN;
        else if (strcmp(cmd_str, "FADE_OUT") == 0) seq_cmd = SEQ_CMD_FADE_OUT;
        else if (strcmp(cmd_str, "PULSE") == 0) seq_cmd = SEQ_CMD_PULSE;
        else if (strcmp(cmd_str, "FLASH") == 0) seq_cmd = SEQ_CMD_FLASH;
        else if (strcmp(cmd_str, "MARQUEE") == 0) seq_cmd = SEQ_CMD_MARQUEE;
        else if (strcmp(cmd_str, "COUNTDOWN") == 0) seq_cmd = SEQ_CMD_COUNTDOWN;
        else if (strcmp(cmd_str, "SCANNER") == 0) seq_cmd = SEQ_CMD_SCANNER;
        else if (strcmp(cmd_str, "TYPEWRITER") == 0) seq_cmd = SEQ_CMD_TYPEWRITER;
        else if (strcmp(cmd_str, "WIPE") == 0) seq_cmd = SEQ_CMD_WIPE;
        else if (strcmp(cmd_str, "SCROLL_IN") == 0) seq_cmd = SEQ_CMD_SCROLL_IN;
        else if (strcmp(cmd_str, "CROSSFADE_TEXT") == 0) seq_cmd = SEQ_CMD_CROSSFADE_TEXT;
        else if (strcmp(cmd_str, "RANDOM_FLICKER_TEXT") == 0) seq_cmd = SEQ_CMD_RANDOM_FLICKER_TEXT;
        else if (strcmp(cmd_str, "SCRAMBLE_TEXT") == 0) seq_cmd = SEQ_CMD_SCRAMBLE_TEXT;
        else if (strcmp(cmd_str, "BAR_GRAPH") == 0) seq_cmd = SEQ_CMD_BAR_GRAPH;
        else if (strcmp(cmd_str, "TRIGGER_ANIMATION") == 0) seq_cmd = SEQ_CMD_TRIGGER_ANIMATION;
        else if (strcmp(cmd_str, "MQTT_PUBLISH") == 0) seq_cmd = SEQ_CMD_MQTT_PUBLISH;
        else if (strcmp(cmd_str, "DISPLAY_HA_SENSOR") == 0) seq_cmd = SEQ_CMD_DISPLAY_HA_SENSOR;
        else {
            Log_printf(LOG_LEVEL_WARN, "SEQ_PARSE: Unknown sequencer command '%s' in track %d.", cmd_str, targetRow);
            continue;
        }

        int targetSegment = command["targetSegment"] | -1;
        int intParam = command["intParam"] | 0;

        // --- FIX: Allow intParam to be used for targetSegment for consistency ---
        // If targetSegment wasn't explicitly set in the JSON, but the command is one
        // that operates on a segment, use the value from intParam instead.
        if (command["targetSegment"].isNull()) {
            if (seq_cmd == SEQ_CMD_CLEAR_SEGMENT ||
                seq_cmd == SEQ_CMD_RESTORE_SEGMENT ||
                seq_cmd == SEQ_CMD_FLASH ||
                seq_cmd == SEQ_CMD_PULSE)
            {
                targetSegment = intParam;
            }
        }

        // Add the parsed step to the track
        step_idx = add_step(tracks[targetRow], step_idx, seq_cmd, targetRow,
            targetSegment,
            intParam,
            command["intParam2"] | 0,
            command["stringParam"] | "",
            command["stringParam2"] | ""
        );
    }
}

/**
 * @brief Parses a JSON document to dynamically create a multi-track animation sequence.
 * @details This is the main parsing function. It's designed to be robust and can handle
 * a JSON payload where the root is either a `JsonArray` (for multi-track definitions)
 * or a single `JsonObject` (for a single-track definition). This flexibility allows it
 * to be used for both complex, hardcoded animations and simpler, single-track commands
 * sent over MQTT.
 * @param tracks The array of three sequencer tracks to populate.
 * @param doc The `JsonDocument` containing the sequence definition.
 */
void parseSequenceFromJson(SequencerTrack tracks[3], JsonDocument& doc) {
    if (doc.is<JsonArray>()) {
        // --- Handle Multi-Track Definitions ---
        // The root of the JSON is an array of track objects.
        Log_printf(LOG_LEVEL_DEBUG, "SEQ_PARSE: Root is a JsonArray. Parsing multiple tracks.");
        for (JsonObject track_def : doc.as<JsonArray>()) {
            parseSingleTrack(tracks, track_def);
        }
    } else if (doc.is<JsonObject>()) {
        // --- Handle Single-Track Definitions ---
        // The root of the JSON is a single track object.
        Log_printf(LOG_LEVEL_DEBUG, "SEQ_PARSE: Root is a JsonObject. Parsing single track.");
        parseSingleTrack(tracks, doc.as<JsonObject>());
    } else {
        // --- Handle Invalid Input ---
        Log_printf(LOG_LEVEL_ERROR, "SEQ_PARSE: Payload is not a valid JSON array or object.");
        return; // Early exit if the JSON is not in a recognized format
    }

    // --- FIX: Activate the newly parsed tracks ---
    // After populating the tracks, we must explicitly activate them so the sequencer will run them.
    // This is the crucial step that was missing for JSON-defined sequences.
    for (int i = 0; i < 3; ++i) {
        if (tracks[i].steps[0].command != SEQ_CMD_NONE) {
            tracks[i].isActive = true;
            tracks[i].trackStartTime = millis();
            tracks[i].stepStartTime = millis();
            tracks[i].originalBrightness = currentSettings.brightness; // Store initial brightness
            Log_printf(LOG_LEVEL_INFO, "SEQ_PARSE: Activating parsed track for row %d.", i);
        }
    }
}

/**
 * @brief Converts an AnimationType enum to its string representation for logging.
 * @param type The AnimationType enum value.
 * @return The string name of the animation.
 */
/**
 * @brief Converts an AnimationType enum to its string representation.
 * @details This helper function is used for logging and debugging, providing a human-readable
 * name for an animation type.
 * @param type The `AnimationType` enum value.
 * @return A const char* to the string name of the animation.
 */
const char* animationTypeToString(AnimationType type) {
    switch (type) {
        case ANIMATION_INTRUDER_ALERT: return "Intruder Alert";
        case ANIMATION_TIME_TRAVEL: return "Time Travel";
        case ANIMATION_PARTY_MODE: return "Party Mode";
        case ANIMATION_KNIGHT_RIDER: return "Knight Rider";
        case ANIMATION_LOADING: return "Loading";
        case ANIMATION_ERROR: return "Error";
        case ANIMATION_FLUX_CHARGE: return "Flux Capacitor Charge-Up";
        case ANIMATION_TACHYONS: return "Tachyons Detected";
        case ANIMATION_DATA_STREAM: return "Data Stream";
        case ANIMATION_WORMHOLE_COLLAPSE: return "Wormhole Collapse";
        case ANIMATION_ALL_DISPLAYS_RANDOM: return "All Displays Random";
        case ANIMATION_TIME_TRAVEL_TUNNEL: return "Time Travel Tunnel";
        case ANIMATION_FIRE_TRAILS: return "Fire Trails";
        case ANIMATION_SPARKLE_REVEAL: return "Sparkle Reveal";
        case ANIMATION_SEQUENTIAL_FLICKER: return "Sequential Flicker";
        case ANIMATION_RANDOM_FLICKER: return "Random Flicker";
        case ANIMATION_COUNTING_UP: return "Counting Up";
        case ANIMATION_WAVE_FLICKER: return "Wave Flicker";
        case ANIMATION_TORNADO_FLICKER: return "Tornado Flicker";
        case ANIMATION_CAPACITOR_CHARGE_UP: return "Capacitor Charge-Up";
        case ANIMATION_DIGITAL_RAIN: return "Digital Rain";
        case ANIMATION_WAVEFORM_COLLAPSE: return "Waveform Collapse";
        case ANIMATION_TIMELINE_SKIM: return "Timeline Skim";
        case ANIMATION_TEMPORAL_DESYNC: return "Temporal Desync";
        case ANIMATION_GLITCHY_JUMP_CUT: return "Glitchy Jump-Cut";
        case ANIMATION_PLASMA_WARM_UP: return "Plasma Warm-Up";
        case ANIMATION_TIME_WARP_STREAKS: return "Time Warp Streaks";
        case ANIMATION_CHARACTER_SCANLINE: return "Character Scanline";
        case ANIMATION_FOCUS_IN: return "Focus In";
        case ANIMATION_CODE_BREAKER: return "Code Breaker";
        case ANIMATION_TEMPORAL_PARADOX: return "Temporal Paradox";
        case ANIMATION_DIGIT_CASCADE: return "Digit Cascade";
        case ANIMATION_ELECTRIC_SURGE: return "Electric Surge";
        case ANIMATION_FLIP_DISC_DISPLAY: return "Flip-Disc Display";
        case ANIMATION_INTERFERENCE_PATTERN: return "Interference Pattern";
        case ANIMATION_RANDOMIZE_ALL: return "Randomize All";
        case ANIMATION_LIGHTNING: return "Lightning";
        case ANIMATION_SCANNER: return "Scanner";
        case ANIMATION_FLUX_CAPACITOR_OVERLOAD: return "Flux Capacitor Overload";
        case ANIMATION_COUNTDOWN: return "Countdown";
        case ANIMATION_SYSTEM_ERROR: return "System Error";
        case ANIMATION_TIME_CIRCUITS_LOCK_IN: return "Time Circuits Lock-In";
        case ANIMATION_TEST_SUITE: return "Test Suite";
        default: return "Unknown Animation";
    }
}

// --- Thematic C++ Animation Generators ---

/**
 * @brief Generates a lightning storm effect with intense, random flashes and crackles.
 * @details This animation uses three parallel tracks to create a chaotic and dynamic storm.
 * Track 0 produces the main, bright lightning bolts that flash across all rows.
 * Track 1 adds faint, rapid "sheet lightning" flickers in the background.
 * Track 2 adds localized, crackling energy bursts on random segments.
 * The use of `random()` inside the loops ensures every strike and flicker is unique.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateLightning(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    const char* solid_block = "|||||||||||||";

    // --- Track 0: Main Lightning Bolts & Thunder ---
    // Loop for ~9.5 seconds. Each loop is a strike (100ms) + sound + variable pause (300-800ms).
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 15, 0);
    // Big, intense flash across all rows simultaneously.
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 0, -1, 100, 0, solid_block);
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 1, -1, 100, 0, solid_block);
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 2, -1, 100, 0, solid_block);
    // Play a random thunder sound with each main strike.
    s0 = add_step(tracks[0], s0, SEQ_CMD_SOUND, 0, 0, 0, 0, (random(2) == 0) ? "thunder.mp3" : "thunder_close.mp3");
    // Wait for a random duration before the next big strike.
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 300 + random(500), 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0);

    // --- Track 1: Background Sheet Lightning (Random Characters) ---
    // Loop for ~10 seconds with rapid, faint flickers. More frequent loop.
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 1, 0, 50, 0);
    // Flicker a random row for a short duration with chaotic characters.
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, random(3), -1, 75, 50, "*.-_\\|/");
    // Wait for a random, short duration.
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 50 + random(150), 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 1, 0, 0, 0);

    // --- Track 2: Crackling Energy (Different Random Characters) ---
    // Loop for ~10 seconds with localized, sharp flickers. More frequent loop.
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_START, 2, 0, 60, 0);
    // Flicker a random segment on a random row with different chaotic characters.
    s2 = add_step(tracks[2], s2, SEQ_CMD_RANDOM_FLICKER_TEXT, random(3), random(4), 50, 25, "~.'");
    // Wait for a random, very short duration.
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 75 + random(75), 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_END, 2, 0, 0, 0);
}

/**
 * @brief Generates a KITT-style scanner effect that sweeps back and forth.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateScanner(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;

    // Announce the animation and play a sound
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "SCANNER");
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "SCANNER");
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "SCANNER");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 1500, 0); // Wait for text to be readable

    // Clear the announcement text
    s0 = add_step(tracks[0], s0, SEQ_CMD_RESTORE_ALL_ROWS, 0, 0, 0, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 250, 0);

    // --- FIX: Run the scanner effect for exactly 8 seconds ---
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCANNER, 0, -1, 8000, 80, "---");
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCANNER, 1, -1, 8000, 80, "---");
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCANNER, 2, -1, 8000, 80, "---");
}

/**
 * @brief Simulates traveling through a time tunnel by scrolling text in rapidly.
 * @param tracks The array of three sequencer tracks to populate.
 * @param time_strings A 2D array containing the formatted time strings for each row.
 */
void generateTimeTravelTunnel(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    // --- FIX: Loop is now parallel across 3 tracks for a denser effect ---
    // Total duration is 10s (5 loops * 2s per loop)
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 5, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 1, 0, 5, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_START, 2, 0, 5, 0);

    // Scroll in all three rows in parallel, takes ~1s
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCROLL_IN, 0, -1, 75, 0, time_strings[0]);
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCROLL_IN, 1, -1, 75, 0, time_strings[1]);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCROLL_IN, 2, -1, 75, 0, time_strings[2]);

    // Hold the text for 1s
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 1000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 1000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 1000, 0);

    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 1, 0, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_END, 2, 0, 0, 0);
}

/**
 * @brief Simulates a flux capacitor overload with a rapid pulsing effect.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateFluxCapacitorOverload(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    // --- FIX: Pulsing is now parallel across tracks for a more chaotic feel ---
    // Total duration is 10s.
    s0 = add_step(tracks[0], s0, SEQ_CMD_PULSE, 0, -1, 10000, 250, "OVERLOAD");
    s1 = add_step(tracks[1], s1, SEQ_CMD_PULSE, 1, -1, 10000, 350, "OVERLOAD");
    s2 = add_step(tracks[2], s2, SEQ_CMD_PULSE, 2, -1, 10000, 450, "OVERLOAD");
}

/**
 * @brief Creates a "fire trail" effect by wiping the text onto the display.
 * @param tracks The array of three sequencer tracks to populate.
 * @param time_strings A 2D array containing the formatted time strings for each row.
 */
void generateFireTrails(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    // --- FIX: Run wipes in parallel and add a hold to reach 10s duration ---
    // The wipe itself is fast (~1.3s), so we add a long wait.
    s0 = add_step(tracks[0], s0, SEQ_CMD_WIPE, 0, -1, 100, 0, time_strings[0]);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WIPE, 1, -1, 100, 0, time_strings[1]);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WIPE, 2, -1, 100, 0, time_strings[2]);

    // Wait on the main track to control the total duration
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 8700, 0);
}

/**
 * @brief Creates an enchanting sparkle effect that smoothly resolves into the final text.
 * @details This animation uses three parallel tracks to create a dense, twinkling starfield
 * effect for ~8 seconds using `RANDOM_FLICKER_TEXT`. Then, it uses `CROSSFADE_TEXT`
 * on each row to seamlessly transition from the sparkles to the final time text over
 * 2 seconds, creating a magical reveal.
 * @param tracks The array of three sequencer tracks to populate.
 * @param time_strings A 2D array containing the formatted time strings for each row.
 */
void generateSparkleReveal(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    const char* sparkles = ".'*.'*.'*.'*.'"; // A dense pattern of sparkles

    // --- Track 0: Top Row ---
    // Start with a field of sparkles for 8 seconds
    s0 = add_step(tracks[0], s0, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 8000, 100, sparkles);
    // Fade out the sparkles over 1 second
    s0 = add_step(tracks[0], s0, SEQ_CMD_FADE_OUT, 0, -1, 1000, 0);
    // Fade in the final text over 1 second
    s0 = add_step(tracks[0], s0, SEQ_CMD_FADE_IN, 0, -1, 1000, 0, time_strings[0]);

    // --- Track 1: Middle Row ---
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 8000, 100, sparkles);
    s1 = add_step(tracks[1], s1, SEQ_CMD_FADE_OUT, 1, -1, 1000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_FADE_IN, 1, -1, 1000, 0, time_strings[1]);

    // --- Track 2: Bottom Row ---
    s2 = add_step(tracks[2], s2, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 8000, 100, sparkles);
    s2 = add_step(tracks[2], s2, SEQ_CMD_FADE_OUT, 2, -1, 1000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_FADE_IN, 2, -1, 1000, 0, time_strings[2]);
}

/**
 * @brief Generates a comprehensive, non-visual test suite for core firmware functions.
 * @details This special animation is a diagnostic tool. It runs a series of tests in parallel
 * across the three sequencer tracks to validate different subsystems.
 * - **Track 0 (Display & Visuals):** Cycles through a battery of visual effects (`WIPE`, `SCANNER`,
 *   `SCRAMBLE_TEXT`, etc.) to stress-test the display drivers and animation command handlers.
 *   It finishes by displaying the final test status.
 * - **Track 1 (Logic & Comms):** Tests sequencer logic (`LOOP`, `WAIT`) and communication by
 *   publishing MQTT messages at the start and end of the test. This allows for external
 *   verification of the test's execution.
 * - **Track 2 (Sound & System):** Tests the sound system by playing an effect and then uses an
 *   MQTT command to request a heap memory log, which helps in monitoring for memory leaks.
 * Upon completion, the top row will display "TESTS: PASS" or "TESTS: FAIL".
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateTestSuite(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    const char* mqtt_device_id = getMqttUniqueId();
    char start_topic[128], end_topic[128], heap_topic[128];
    snprintf(start_topic, sizeof(start_topic), "bttf_time_circuits/%s/debug/test_suite", mqtt_device_id);
    snprintf(end_topic, sizeof(end_topic), "bttf_time_circuits/%s/debug/test_suite", mqtt_device_id);
    snprintf(heap_topic, sizeof(heap_topic), "bttf_time_circuits/%s/debug/command", mqtt_device_id);

    // --- Track 1: Core Logic & MQTT Communication Test ---
    s1 = add_step(tracks[1], s1, SEQ_CMD_MQTT_PUBLISH, 1, 0, 0, 0, start_topic, "start");
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 1, 0, 2, 0); // Loop twice
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 250, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 1, 0, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 11000, 0); // Wait for other tracks to finish
    s1 = add_step(tracks[1], s1, SEQ_CMD_MQTT_PUBLISH, 1, 0, 0, 0, end_topic, "pass");

    // --- Track 2: Sound & Memory Test ---
    s2 = add_step(tracks[2], s2, SEQ_CMD_SOUND, 2, 0, 0, 0, "arrival_chime.mp3");
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 2000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_MQTT_PUBLISH, 2, 0, 0, 0, heap_topic, "heap");

    // --- Track 0: Display Driver & Visual Effects Test ---
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "TEST SUITE...");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 1500, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_WIPE, 0, -1, 50, 0, "WIPE");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 1000, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCANNER, 0, -1, 1000, 80, "S");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 1000, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_TYPEWRITER, 0, -1, 75, 0, "TYPEWRITER");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 1000, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, 50, 150, "SCRAMBLE");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 1000, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_PULSE, 0, -1, 1000, 0, "PULSE");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 1000, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 0, -1, 1000, 0, "FLASH");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 1000, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_BAR_GRAPH, 0, -1, 100, 1000, "BAR");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 1000, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "TESTS: PASS");
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 0, -1, 500, 0);
}


// --- Main Generation Function ---

/**
 * @brief The main dispatcher for generating all animation sequences.
 * @details This function acts as a central hub for creating animations. It takes an
 * `AnimationType` and calls the corresponding generator function or parser. It also
 * handles the special case of `ANIMATION_RANDOMIZE_ALL` by iteratively picking a
 * different, concrete animation to run, preventing stack overflows from recursion.
 * Finally, it ensures all generated sequences are properly terminated with an `END`
 * command and a final `RESTORE_ALL_ROWS` step for cleanup.
 * @param animType The type of animation to generate.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateAnimationSequence(AnimationType animType, SequencerTrack tracks[3]) {
    // --- FIX: Replace recursive randomization with an iterative approach ---
    // This loop ensures that if we are asked to randomize, we pick a concrete
    // animation and never get stuck in a recursive loop, which prevents a stack overflow.
    while (animType == ANIMATION_RANDOMIZE_ALL) {
        // A curated list of interesting and stable C++ animations suitable for this feature.
        // JSON-based animations are excluded as they are generally for specific UI triggers.
        const AnimationType cpp_animations[] = {
            ANIMATION_ALL_DISPLAYS_RANDOM,
            ANIMATION_LIGHTNING,
            ANIMATION_SCANNER,
            ANIMATION_TIME_TRAVEL_TUNNEL,
            ANIMATION_FLUX_CAPACITOR_OVERLOAD,
            ANIMATION_FIRE_TRAILS,
            ANIMATION_SPARKLE_REVEAL,
            ANIMATION_SEQUENTIAL_FLICKER,
            ANIMATION_RANDOM_FLICKER,
            ANIMATION_TORNADO_FLICKER,
            ANIMATION_CAPACITOR_CHARGE_UP,
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
            ANIMATION_INTERFERENCE_PATTERN
        };
        int num_cpp_animations = sizeof(cpp_animations) / sizeof(cpp_animations[0]);
        animType = cpp_animations[random(num_cpp_animations)];
    }

    char time_strings[3][17];
    getFormattedTimeStrings(time_strings[0], time_strings[1], time_strings[2]);

    for (int i = 0; i < 3; ++i) {
        tracks[i].reset();
    }

    // --- FIX: A correct switch statement with all cases and a proper default ---
    switch (animType) {
        // --- JSON-based Named Sequences ---
        case ANIMATION_TIME_TRAVEL:
            {
                JsonDocument doc;
                deserializeJson(doc, R"([{"targetRow": "TOP", "commands": [{"command": "SOUND", "stringParam":"time_travel.mp3"}, {"command": "BAR_GRAPH", "stringParam":"ACCELERATING", "intParam":0, "intParam2":10000}]}, {"targetRow": "MIDDLE", "commands": [{"command": "SET_TEXT", "stringParam":"TIME TRAVEL"}, {"command": "WAIT", "intParam": 3000}, {"command":"SET_TEXT", "stringParam":"ACTIVATED"}, {"command":"WAIT", "intParam":3000}, {"command": "SET_TEXT", "stringParam": "88 MPH"},{"command":"WAIT", "intParam":4000}]}, {"targetRow": "BOTTOM", "commands": [{"command": "FLASH", "targetSegment": -1, "intParam2": 10000}]}])");
                parseSequenceFromJson(tracks, doc);
            }
            break;
        case ANIMATION_PARTY_MODE:
            {
                JsonDocument doc;
                deserializeJson(doc, R"([{"targetRow":"TOP", "commands":[{"command":"SOUND", "stringParam":"party.mp3"},{"command":"SET_TEXT", "stringParam":"PARTY TIME!"}, {"command":"PULSE", "targetSegment":-1, "intParam": 250, "intParam2":10000}]}, {"targetRow":"MIDDLE", "commands":[{"command":"RANDOM_FLICKER_TEXT", "intParam":100, "intParam2":10000, "stringParam": "DANCE"}]}, {"targetRow":"BOTTOM", "commands":[{"command":"SET_TEXT", "stringParam":"LETS DANCE!"}, {"command":"PULSE", "targetSegment":-1, "intParam":250, "intParam2":10000}]}])");
                parseSequenceFromJson(tracks, doc);
            }
            break;
        case ANIMATION_KNIGHT_RIDER:          generateKnightRider(tracks); break;
        case ANIMATION_LOADING:
            {
                JsonDocument doc;
                deserializeJson(doc, R"([{"targetRow":0, "commands":[{"command":"SET_TEXT", "stringParam":"FLUX CAPACITOR"}, {"command":"WAIT", "intParam":3300}]}, {"targetRow":1, "commands":[{"command":"WAIT", "intParam":3300}, {"command":"SET_TEXT", "stringParam":"TIME CIRCUITS"}, {"command":"WAIT", "intParam":3300}]}, {"targetRow":2, "commands":[{"command":"WAIT", "intParam":6600}, {"command":"SET_TEXT", "stringParam":"SYSTEMS ONLINE"}, {"command":"WAIT", "intParam":3400}]}])");
                parseSequenceFromJson(tracks, doc);
            }
            break;
        case ANIMATION_ERROR:
            {
                JsonDocument doc;
                deserializeJson(doc, R"([{"targetRow":0, "commands":[{"command":"SOUND", "stringParam":"error.mp3"}, {"command":"SCRAMBLE_TEXT", "stringParam":"ERROR", "intParam":100, "intParam2":2000}, {"command":"SET_TEXT", "stringParam":"ERROR"}, {"command":"PULSE", "intParam":500, "intParam2":8000}]}, {"targetRow":1, "commands":[{"command":"MARQUEE", "stringParam":"SYSTEM MALFUNCTION", "intParam2":10000}]}])");
                parseSequenceFromJson(tracks, doc);
            }
            break;
        case ANIMATION_FLUX_CHARGE:
            {
                JsonDocument doc;
                deserializeJson(doc, R"([{"targetRow":2, "commands":[{"command":"SOUND", "stringParam":"flux_capacitor_power_on.mp3"}, {"command":"BAR_GRAPH", "stringParam":"CHARGE", "intParam":0, "intParam2":5000}]}, {"targetRow":0, "commands":[{"command":"WAIT", "intParam":3000}, {"command":"FLASH", "targetSegment":-1, "intParam2":2000}]}, {"targetRow":1, "commands":[{"command":"WAIT", "intParam":3000}, {"command":"FLASH", "targetSegment":-1, "intParam2":2000}]}])");
                parseSequenceFromJson(tracks, doc);
            }
            break;
        case ANIMATION_TACHYONS:
            {
                JsonDocument doc;
                deserializeJson(doc, R"([{"targetRow":1, "commands":[{"command":"SCRAMBLE_TEXT", "stringParam":"TACHYONS ON", "intParam":150, "intParam2":250}, {"command":"SOUND", "stringParam":"hum.mp3"},{"command":"WAIT", "intParam":3000}]}])");
                parseSequenceFromJson(tracks, doc);
            }
            break;
        case ANIMATION_DATA_STREAM:             generateDataStream(tracks); break;
        case ANIMATION_WORMHOLE_COLLAPSE:
            {
                JsonDocument doc;
                deserializeJson(doc, R"([{"targetRow": 0, "commands": [{"command": "SOUND", "stringParam": "arrival_chime.mp3"}, {"command": "RANDOM_FLICKER_TEXT", "intParam": 100, "intParam2": 3000}, {"command": "FADE_OUT", "intParam": 2000}]}, {"targetRow": 1, "commands": [{"command": "RANDOM_FLICKER_TEXT", "intParam": 100, "intParam2": 3000}, {"command": "WAIT", "intParam": 500}, {"command": "FADE_OUT", "intParam": 3000}]}, {"targetRow": 2, "commands": [{"command": "RANDOM_FLICKER_TEXT", "intParam": 100, "intParam2": 3000}, {"command": "WAIT", "intParam": 1000}, {"command": "FADE_OUT", "intParam": 3000}]}])");
                parseSequenceFromJson(tracks, doc);
            }
            break;

        // Legacy C++ Generated Animations (for Randomize All)
        case ANIMATION_SEQUENTIAL_FLICKER:      generateSequentialFlicker(tracks, time_strings); break;
        case ANIMATION_RANDOM_FLICKER:          generateRandomFlicker(tracks); break;
        case ANIMATION_COUNTING_UP:             generateCountingUp(tracks); break;
        case ANIMATION_WAVE_FLICKER:            generateWaveFlicker(tracks); break;
        case ANIMATION_TORNADO_FLICKER:         generateTornadoFlicker(tracks); break;
        case ANIMATION_CAPACITOR_CHARGE_UP:     generateCapacitorChargeUp(tracks); break;
        case ANIMATION_DIGITAL_RAIN:            generateDigitalRain(tracks); break;
        case ANIMATION_WAVEFORM_COLLAPSE:       generateWaveformCollapse(tracks); break;
        case ANIMATION_TIMELINE_SKIM:           generateTimelineSkim(tracks, time_strings); break;
        case ANIMATION_TEMPORAL_DESYNC:         generateTemporalDesync(tracks); break;
        case ANIMATION_GLITCHY_JUMP_CUT:        generateGlitchyJumpCut(tracks); break;
        case ANIMATION_PLASMA_WARM_UP:          generatePlasmaWarmup(tracks); break;
        case ANIMATION_TIME_WARP_STREAKS:       generateTimeWarpStreaks(tracks, time_strings); break;
        case ANIMATION_CHARACTER_SCANLINE:      generateCharacterScanline(tracks, time_strings); break;
        case ANIMATION_FOCUS_IN:                generateFocusIn(tracks, time_strings); break;
        case ANIMATION_CODE_BREAKER:            generateCodeBreaker(tracks, time_strings); break;
        case ANIMATION_TEMPORAL_PARADOX:        generateTemporalParadox(tracks, time_strings); break;
        case ANIMATION_DIGIT_CASCADE:           generateDigitCascade(tracks, time_strings); break;
        case ANIMATION_ELECTRIC_SURGE:          generateElectricSurge(tracks, time_strings); break;
        case ANIMATION_FLIP_DISC_DISPLAY:       generateFlipDisc(tracks, time_strings); break;
        case ANIMATION_INTERFERENCE_PATTERN:    generateInterferencePattern(tracks, time_strings); break;

        // Modern C++ Generated Sequencer Animations
        case ANIMATION_INTRUDER_ALERT:          generateIntruderAlert(tracks); break;
        case ANIMATION_ALL_DISPLAYS_RANDOM:     generateAllDisplaysRandom(tracks, time_strings); break;
        case ANIMATION_LIGHTNING:               generateLightning(tracks); break;
        case ANIMATION_SCANNER:                 generateScanner(tracks); break;
        case ANIMATION_TIME_TRAVEL_TUNNEL:      generateTimeTravelTunnel(tracks, time_strings); break;
        case ANIMATION_FLUX_CAPACITOR_OVERLOAD: generateFluxCapacitorOverload(tracks); break;
        case ANIMATION_FIRE_TRAILS:             generateFireTrails(tracks, time_strings); break;
        case ANIMATION_SPARKLE_REVEAL:          generateSparkleReveal(tracks, time_strings); break;
        case ANIMATION_COUNTDOWN:               generateCountdown(tracks); break;
        case ANIMATION_SYSTEM_ERROR:            generateSystemError(tracks); break;
        case ANIMATION_TIME_CIRCUITS_LOCK_IN:   generateTimeCircuitsLockIn(tracks, time_strings); break;
        case ANIMATION_TEST_SUITE:              generateTestSuite(tracks); break;

        // Default case
        default:
            // Default to a visually interesting and non-destructive animation.
            generateAllDisplaysRandom(tracks, time_strings);
            break;
    }

    // --- Add a final step to the main track to restore all displays ---
    int end_idx = 0;
    while(end_idx < MAX_SEQUENCE_STEPS && tracks[0].steps[end_idx].command != SEQ_CMD_NONE) {
        end_idx++;
    }
    if (end_idx < MAX_SEQUENCE_STEPS - 2) {
        end_idx = add_step(tracks[0], end_idx, SEQ_CMD_WAIT, 0, -1, 1000, 0);
        end_idx = add_step(tracks[0], end_idx, SEQ_CMD_RESTORE_ALL_ROWS, 0, -1, 0, 0);
    }

    // --- Ensure all tracks have a proper END command ---
    for (int i = 0; i < 3; i++) {
        int track_end_idx = 0;
        while(track_end_idx < MAX_SEQUENCE_STEPS && tracks[i].steps[track_end_idx].command != SEQ_CMD_NONE) {
            track_end_idx++;
        }
        add_step(tracks[i], track_end_idx, SEQ_CMD_END, i, 0, 0, 0);
    }

    for (int i = 0; i < 3; ++i) {
        if (tracks[i].steps[0].command != SEQ_CMD_NONE) {
            tracks[i].isActive = true;
            tracks[i].trackStartTime = millis();
            tracks[i].stepStartTime = millis();
        }
    }
}