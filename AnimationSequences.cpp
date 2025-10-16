/**
 * @file AnimationSequences.cpp
 * @brief Implements the generation of all built-in animation sequences for the Time Circuits.
 * @details This file contains the functions that construct the various animation sequences,
 * both simple and complex, by adding a series of commands to sequencer tracks. It includes
 * generators for C++ defined animations and a parser for JSON-defined sequences.
 */

#include <ArduinoJson.h>
#include "AnimationSequences.h"
#include "HardwareControl.h"
#include "DisplayManager.h"
#include "DebugLog.h"
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
 * @brief Generates a complex, layered glitch effect with independent flickering on each row.
 * @details This animation uses three parallel tracks, one for each display row. Each track
 * runs its own loop, flickering its assigned row for a random duration and then waiting
 * for a random duration. This creates a chaotic, desynchronized flickering effect that
 * is much more visually interesting than a single-track random flicker.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateRandomFlicker(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);

    // --- Track 0: Top Row Flicker ---
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 25, 0);
    // Flicker for a random duration between 100ms and 300ms
    s0 = add_step(tracks[0], s0, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 100 + random(200), 50);
    // Wait for a random duration between 100ms and 300ms
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 100 + random(200), 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0);

    // --- Track 1: Middle Row Flicker ---
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 1, 0, 25, 0);
    // Flicker for a random duration between 100ms and 300ms
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 100 + random(200), 50);
    // Wait for a random duration between 100ms and 300ms
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 100 + random(200), 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 1, 0, 0, 0);

    // --- Track 2: Bottom Row Flicker ---
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_START, 2, 0, 25, 0);
    // Flicker for a random duration between 100ms and 300ms
    s2 = add_step(tracks[2], s2, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 100 + random(200), 50);
    // Wait for a random duration between 100ms and 300ms
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 100 + random(200), 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_END, 2, 0, 0, 0);
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

    s0 = add_step(tracks[0], s0, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, flicker_interval, lock_in_interval, dest_str.c_str());
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCRAMBLE_TEXT, 1, -1, flicker_interval, lock_in_interval, pres_str.c_str());
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCRAMBLE_TEXT, 2, -1, flicker_interval, lock_in_interval, last_str.c_str());
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

    // --- Track 1 (Middle): Progress Bar ---
    // Fills up over 9 seconds.
    s1 = add_step(tracks[1], s1, SEQ_CMD_BAR_GRAPH, 1, -1, 100, 9000, "DECRYPTING");

    // --- Track 2 (Bottom): Status Updates ---
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "ANALYZING...");
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 4500, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "CODE BROKEN!");
    s2 = add_step(tracks[2], s2, SEQ_CMD_PULSE, 2, -1, 4500, 0); // Pulse the final message

    // --- Track 0 (Top): The Code-Breaking Effect ---
    // First, flicker random garbage text for 4.5 seconds.
    s0 = add_step(tracks[0], s0, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 4500, 100, "!@#$%%^&*()_+-=");
    // Then, scramble-reveal the first half of the destination time over 2.25 seconds.
    std::string first_half = std::string(time_strings[0]).substr(0, 7);
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, 50, 320, first_half.c_str());
    // Finally, scramble-reveal the full destination time over the remaining 2.25 seconds.
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, 50, 170, time_strings[0]);

    // --- Final flash to celebrate breaking the code ---
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 500, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 0, -1, 500, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 1, -1, 500, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 2, -1, 500, 0);
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
 * @details This animation creates a sense of energy build-up. It starts with a slow
 * pulse on the top row, adds crackling flickers to the middle, and then culminates
 * in a bright, multi-row flash, simulating a discharge. The sequence repeats,
 * creating a rhythmic surge effect.
 * @param tracks The array of three sequencer tracks to populate.
 * @param time_strings A 2D array containing the formatted time strings for each row.
 */
void generateElectricSurge(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s = 0;
    s = add_intro_sound_steps(tracks[0], s);

    // Loop the entire surge sequence 4 times. Each loop is ~2.5s. Total ~10s.
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 4, 0);

    // 1. Build-up phase (~1.5s)
    s = add_step(tracks[0], s, SEQ_CMD_PULSE, 0, -1, 1500, 0, "ENERGY SURGE"); // Slow pulse on top
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 1500, 100, "><"); // Crackles in middle

    // 2. Discharge phase (~0.5s)
    s = add_step(tracks[0], s, SEQ_CMD_FLASH, 0, -1, 250, 0); // Bright flash on all rows
    s = add_step(tracks[0], s, SEQ_CMD_FLASH, 1, -1, 250, 0);
    s = add_step(tracks[0], s, SEQ_CMD_FLASH, 2, -1, 250, 0);

    // 3. Dissipation and pause (~0.5s)
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 500, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RESTORE_ALL_ROWS, 0, 0, 0, 0); // Reset for next loop

    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
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
 * @details This animation creates a full-display "warm-up" effect.
 * It begins by fading in a `~*~*~` pattern on all rows. Then, it pulses this
 * pattern with increasing speed and intensity across two stages. The sequence
 * culminates in a bright, full-display flash, simulating a discharge of energy.
 * This multi-stage, parallel approach creates a much more dynamic and engaging
 * "warm-up" than a simple fade on a single row.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generatePlasmaWarmup(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    const char* plasma = "~*~*~*~*~*~*~";

    // --- Stage 1: Fade in the plasma field ---
    // All three rows fade in the plasma text over 2 seconds.
    s0 = add_step(tracks[0], s0, SEQ_CMD_FADE_IN, 0, -1, 2000, 0, plasma);
    s1 = add_step(tracks[1], s1, SEQ_CMD_FADE_IN, 1, -1, 2000, 0, plasma);
    s2 = add_step(tracks[2], s2, SEQ_CMD_FADE_IN, 2, -1, 2000, 0, plasma);

    // --- Stage 2: Slow Pulse ---
    // All three rows pulse together for 3 seconds.
    s0 = add_step(tracks[0], s0, SEQ_CMD_PULSE, 0, -1, 3000, 750); // 750ms pulse cycle
    s1 = add_step(tracks[1], s1, SEQ_CMD_PULSE, 1, -1, 3000, 750);
    s2 = add_step(tracks[2], s2, SEQ_CMD_PULSE, 2, -1, 3000, 750);

    // --- Stage 3: Fast Pulse ---
    // All three rows pulse faster for 3 seconds.
    s0 = add_step(tracks[0], s0, SEQ_CMD_PULSE, 0, -1, 3000, 350); // 350ms pulse cycle
    s1 = add_step(tracks[1], s1, SEQ_CMD_PULSE, 1, -1, 3000, 350);
    s2 = add_step(tracks[2], s2, SEQ_CMD_PULSE, 2, -1, 3000, 350);

    // --- Stage 4: Discharge ---
    // A final, bright flash on all rows.
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 0, -1, 1000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_FLASH, 1, -1, 1000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_FLASH, 2, -1, 1000, 0);
}

/**
 * @brief Creates a glitchy, jump-cut effect with rapid flickering and flashes.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateGlitchyJumpCut(SequencerTrack tracks[3]) {
    int s=0;
    s = add_intro_sound_steps(tracks[0], s);
    // Loop 25 times. Each loop is 400ms. 25 * 400ms = 10s.
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 25, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 200, 50);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 200, 50);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 200, 50);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 0, 0);
    s = add_step(tracks[0], s, SEQ_CMD_FLASH, 0, -1, 100, 0);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 100, 0);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
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
 * @brief Generates a "digital rain" effect with cascading, flickering characters.
 * @details This animation uses three parallel tracks with different flicker speeds and
 * character sets to create a layered, cascading effect reminiscent of digital rain.
 * The use of `RANDOM_FLICKER_TEXT` with different parameters on each track ensures a
 * chaotic and visually engaging animation.
 * @param tracks The array of three sequencer tracks to populate.
 */
void generateDigitalRain(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);

    // Run three parallel tracks of flickering text with different speeds and characters
    // to create a layered "rain" effect. Duration is 10s for all.
    s0 = add_step(tracks[0], s0, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 10000, 75, "1010101010101");
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 10000, 100, "ABCDE12345FGHI");
    s2 = add_step(tracks[2], s2, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 10000, 125, "ZYXWV98765UTSR");
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

    // --- Track 1 (Top): Progress Bar ---
    // Fills up over 8 seconds, synchronized with the main countdown.
    s1 = add_step(tracks[1], s1, SEQ_CMD_BAR_GRAPH, 0, -1, 100, 8000);

    // --- Track 2 (Bottom): Status Text ---
    // Pulses "COUNTDOWN" for 8 seconds.
    s2 = add_step(tracks[2], s2, SEQ_CMD_PULSE, 2, -1, 8000, 0, "COUNTDOWN");

    // --- Track 0 (Middle): The Main Countdown ---
    // Display "9" through "1"
    for (int i = 9; i > 0; i--) {
        char num_str[2];
        sprintf(num_str, "%d", i);
        s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, num_str);
        s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 800, 0); // 800ms per number
    }
    // Display "0"
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "0");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 800, 0);

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
 * @brief Parses a JSON string to dynamically create a multi-track animation sequence.
 * @details This function allows for the creation of complex, custom animations defined in
 * JSON format, which can be sent via MQTT or stored in `sequences.json`. It maps
 * string command names to their `SequenceCommand` enum counterparts and populates the
 * sequencer tracks accordingly.
 * @param tracks The array of three sequencer tracks to populate.
 * @param json_string A string containing the JSON definition of the sequence.
 */
void parseSequenceFromJson(SequencerTrack tracks[3], const std::string& json_string) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json_string);

    if (error) {
        Log_printf(LOG_LEVEL_ERROR, "SEQ_PARSE: Failed to parse JSON sequence: %s", error.c_str());
        return;
    }

    JsonArray track_definitions = doc.as<JsonArray>();
    if (track_definitions.isNull()) {
        Log_printf(LOG_LEVEL_ERROR, "SEQ_PARSE: Payload is not a JSON array of track definitions.");
        return;
    }

    for (JsonObject track_def : track_definitions) {
        int targetRow = -1;
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
            continue;
        }

        if (tracks[targetRow].isActive) {
            Log_printf(LOG_LEVEL_WARN, "SEQ_PARSE: Ignoring new sequence for row %d, one is already active.", targetRow);
            continue;
        }

        JsonArray commands = track_def["commands"].as<JsonArray>();
        if (commands.isNull()) {
            Log_printf(LOG_LEVEL_WARN, "SEQ_PARSE: Skipping track for row %d: missing 'commands' array.", targetRow);
            continue;
        }

        int step_idx = 0;
        for (JsonObject command : commands) {
            const char* cmd = command["command"];
            if (!cmd) {
                Log_printf(LOG_LEVEL_WARN, "SEQ_PARSE: Skipping invalid command in track %d: missing 'command' key.", targetRow);
                continue;
            }

            SequenceCommand seq_cmd = SEQ_CMD_NONE;
            if (strcmp(cmd, "SET_TEXT") == 0) seq_cmd = SEQ_CMD_SET_TEXT;
            else if (strcmp(cmd, "CLEAR_SEGMENT") == 0) seq_cmd = SEQ_CMD_CLEAR_SEGMENT;
            else if (strcmp(cmd, "SET_BRIGHTNESS") == 0) seq_cmd = SEQ_CMD_SET_BRIGHTNESS;
            else if (strcmp(cmd, "RESTORE_ROW") == 0) seq_cmd = SEQ_CMD_RESTORE_ROW;
            else if (strcmp(cmd, "RESTORE_ALL_ROWS") == 0) seq_cmd = SEQ_CMD_RESTORE_ALL_ROWS;
            else if (strcmp(cmd, "WAIT") == 0) seq_cmd = SEQ_CMD_WAIT;
            else if (strcmp(cmd, "SOUND") == 0) seq_cmd = SEQ_CMD_SOUND;
            else if (strcmp(cmd, "LOOP_START") == 0) seq_cmd = SEQ_CMD_LOOP_START;
            else if (strcmp(cmd, "LOOP_END") == 0) seq_cmd = SEQ_CMD_LOOP_END;
            else if (strcmp(cmd, "FADE_IN") == 0) seq_cmd = SEQ_CMD_FADE_IN;
            else if (strcmp(cmd, "FADE_OUT") == 0) seq_cmd = SEQ_CMD_FADE_OUT;
            else if (strcmp(cmd, "PULSE") == 0) seq_cmd = SEQ_CMD_PULSE;
            else if (strcmp(cmd, "FLASH") == 0) seq_cmd = SEQ_CMD_FLASH;
            else if (strcmp(cmd, "MARQUEE") == 0) seq_cmd = SEQ_CMD_MARQUEE;
            else if (strcmp(cmd, "COUNTDOWN") == 0) seq_cmd = SEQ_CMD_COUNTDOWN;
            else if (strcmp(cmd, "SCANNER") == 0) seq_cmd = SEQ_CMD_SCANNER;
            else if (strcmp(cmd, "TYPEWRITER") == 0) seq_cmd = SEQ_CMD_TYPEWRITER;
            else if (strcmp(cmd, "WIPE") == 0) seq_cmd = SEQ_CMD_WIPE;
            else if (strcmp(cmd, "SCROLL_IN") == 0) seq_cmd = SEQ_CMD_SCROLL_IN;
            else if (strcmp(cmd, "CROSSFADE_TEXT") == 0) seq_cmd = SEQ_CMD_CROSSFADE_TEXT;
            else if (strcmp(cmd, "RANDOM_FLICKER_TEXT") == 0) seq_cmd = SEQ_CMD_RANDOM_FLICKER_TEXT;
            else if (strcmp(cmd, "SCRAMBLE_TEXT") == 0) seq_cmd = SEQ_CMD_SCRAMBLE_TEXT;
            else if (strcmp(cmd, "BAR_GRAPH") == 0) seq_cmd = SEQ_CMD_BAR_GRAPH;
            else if (strcmp(cmd, "TRIGGER_ANIMATION") == 0) seq_cmd = SEQ_CMD_TRIGGER_ANIMATION;
            else if (strcmp(cmd, "MQTT_PUBLISH") == 0) seq_cmd = SEQ_CMD_MQTT_PUBLISH;
            else if (strcmp(cmd, "DISPLAY_HA_SENSOR") == 0) seq_cmd = SEQ_CMD_DISPLAY_HA_SENSOR;
            else {
                Log_printf(LOG_LEVEL_WARN, "SEQ_PARSE: Unknown sequencer command '%s' in track %d.", cmd, targetRow);
                continue;
            }

            step_idx = add_step(tracks[targetRow], step_idx, seq_cmd, targetRow,
                command["targetSegment"] | -1,
                command["intParam"] | 0,
                command["intParam2"] | 0,
                command["stringParam"] | "",
                command["stringParam2"] | ""
            );
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

    // --- FIX: This animation was originally unbounded and caused crashes.
    // It is now a fixed 10-second sequence.

    // Pre-fill all rows with solid blocks to maximize the visual impact of flashes.
    const char* solid_block = "|||||||||||||";
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, solid_block);
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, solid_block);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, solid_block);
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 10, 0); // Short pause to ensure text is set

    // --- Track 0: Main Lightning Bolts ---
    // Loop for ~10 seconds. Each loop is a main strike and a variable pause.
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 8, 0);
    // Big, intense flash across all rows simultaneously.
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 0, -1, 100, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 1, -1, 100, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 2, -1, 100, 0);
    // Wait for a random duration before the next big strike.
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 500 + random(750), 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0);

    // --- Track 1: Background Sheet Lightning ---
    // Loop for ~10 seconds with rapid, faint flickers.
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 1, 0, 35, 0);
    // Flicker a random row for a short duration.
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, random(3), -1, 100, 50, " ");
    // Wait for a random, short duration.
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 50 + random(200), 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 1, 0, 0, 0);

    // --- Track 2: Crackling Energy ---
    // Loop for ~10 seconds with localized, sharp flickers.
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_START, 2, 0, 45, 0);
    // Flicker a random segment on a random row.
    s2 = add_step(tracks[2], s2, SEQ_CMD_RANDOM_FLICKER_TEXT, random(3), random(4), 50, 50, "|||");
    // Wait for a random, very short duration.
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 100 + random(100), 0);
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
    // Crossfade from the sparkles to the final text over 2 seconds
    s0 = add_step(tracks[0], s0, SEQ_CMD_CROSSFADE_TEXT, 0, -1, 2000, 0, time_strings[0]);

    // --- Track 1: Middle Row ---
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 8000, 100, sparkles);
    s1 = add_step(tracks[1], s1, SEQ_CMD_CROSSFADE_TEXT, 1, -1, 2000, 0, time_strings[1]);

    // --- Track 2: Bottom Row ---
    s2 = add_step(tracks[2], s2, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 8000, 100, sparkles);
    s2 = add_step(tracks[2], s2, SEQ_CMD_CROSSFADE_TEXT, 2, -1, 2000, 0, time_strings[2]);
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
        case ANIMATION_INTRUDER_ALERT:
            // --- FIX: Use intParam2 to set a 10s duration and make more dynamic ---
            parseSequenceFromJson(tracks, R"([{"targetRow":"TOP", "commands":[{"command":"SOUND", "stringParam":"siren.mp3"}, {"command":"SET_TEXT", "stringParam":"INTRUDER ALERT"}, {"command":"PULSE", "targetSegment":-1, "intParam": 500, "intParam2":10000}]}, {"targetRow":"MIDDLE", "commands":[{"command":"SCRAMBLE_TEXT", "stringParam":"BREACH DETECTED", "intParam":50, "intParam2":10000}]}, {"targetRow":"BOTTOM", "commands":[{"command":"SET_TEXT", "stringParam":"LOCKDOWN"}, {"command":"FLASH", "targetSegment":-1, "intParam":500, "intParam2":10000}]}])");
            break;
        case ANIMATION_TIME_TRAVEL:
            parseSequenceFromJson(tracks, R"([{"targetRow": "TOP", "commands": [{"command": "SOUND", "stringParam":"time_travel.mp3"}, {"command": "BAR_GRAPH", "stringParam":"ACCELERATING", "intParam":0, "intParam2":10000}]}, {"targetRow": "MIDDLE", "commands": [{"command": "SET_TEXT", "stringParam":"TIME TRAVEL"}, {"command": "WAIT", "intParam": 3000}, {"command":"SET_TEXT", "stringParam":"ACTIVATED"}, {"command":"WAIT", "intParam":3000}, {"command": "SET_TEXT", "stringParam": "88 MPH"},{"command":"WAIT", "intParam":4000}]}, {"targetRow": "BOTTOM", "commands": [{"command": "FLASH", "targetSegment": -1, "intParam2": 10000}]}])");
            break;
        case ANIMATION_PARTY_MODE:
            // --- FIX: Use intParam2 to set a 10s duration and make more dynamic ---
            parseSequenceFromJson(tracks, R"([{"targetRow":"TOP", "commands":[{"command":"SOUND", "stringParam":"party.mp3"},{"command":"SET_TEXT", "stringParam":"PARTY TIME!"}, {"command":"PULSE", "targetSegment":-1, "intParam": 250, "intParam2":10000}]}, {"targetRow":"MIDDLE", "commands":[{"command":"RANDOM_FLICKER_TEXT", "intParam":100, "intParam2":10000, "stringParam": "DANCE"}]}, {"targetRow":"BOTTOM", "commands":[{"command":"SET_TEXT", "stringParam":"LETS DANCE!"}, {"command":"PULSE", "targetSegment":-1, "intParam":250, "intParam2":10000}]}])");
            break;
        case ANIMATION_KNIGHT_RIDER:
            parseSequenceFromJson(tracks, R"([{"targetRow":2, "commands":[{"command":"SCANNER", "intParam2":10000, "intParam":100}]}])");
            break;
        case ANIMATION_LOADING:
            parseSequenceFromJson(tracks, R"([{"targetRow":0, "commands":[{"command":"SET_TEXT", "stringParam":"FLUX CAPACITOR"}, {"command":"WAIT", "intParam":3300}]}, {"targetRow":1, "commands":[{"command":"WAIT", "intParam":3300}, {"command":"SET_TEXT", "stringParam":"TIME CIRCUITS"}, {"command":"WAIT", "intParam":3300}]}, {"targetRow":2, "commands":[{"command":"WAIT", "intParam":6600}, {"command":"SET_TEXT", "stringParam":"SYSTEMS ONLINE"}, {"command":"WAIT", "intParam":3400}]}])");
            break;
        case ANIMATION_ERROR:
            // --- FIX: Add a duration to MARQUEE and make more dynamic ---
            parseSequenceFromJson(tracks, R"([{"targetRow":0, "commands":[{"command":"SOUND", "stringParam":"error.mp3"}, {"command":"SCRAMBLE_TEXT", "stringParam":"ERROR", "intParam":100, "intParam2":2000}, {"command":"SET_TEXT", "stringParam":"ERROR"}, {"command":"PULSE", "intParam":500, "intParam2":8000}]}, {"targetRow":1, "commands":[{"command":"MARQUEE", "stringParam":"SYSTEM MALFUNCTION", "intParam2":10000}]}])");
            break;
        case ANIMATION_FLUX_CHARGE:
            parseSequenceFromJson(tracks, R"([{"targetRow":2, "commands":[{"command":"SOUND", "stringParam":"flux_capacitor_power_on.mp3"}, {"command":"BAR_GRAPH", "stringParam":"CHARGE", "intParam":0, "intParam2":5000}]}, {"targetRow":0, "commands":[{"command":"WAIT", "intParam":3000}, {"command":"FLASH", "targetSegment":-1, "intParam2":2000}]}, {"targetRow":1, "commands":[{"command":"WAIT", "intParam":3000}, {"command":"FLASH", "targetSegment":-1, "intParam2":2000}]}])");
            break;
        case ANIMATION_TACHYONS:
            parseSequenceFromJson(tracks, R"([{"targetRow":1, "commands":[{"command":"SCRAMBLE_TEXT", "stringParam":"TACHYONS ON", "intParam":150, "intParam2":250}, {"command":"SOUND", "stringParam":"hum.mp3"},{"command":"WAIT", "intParam":3000}]}])");
            break;
        case ANIMATION_DATA_STREAM:
            parseSequenceFromJson(tracks, R"([{"targetRow":0, "commands":[{"command":"RANDOM_FLICKER_TEXT", "intParam":50, "intParam2":10000}]}, {"targetRow":1, "commands":[{"command":"RANDOM_FLICKER_TEXT", "intParam":50, "intParam2":10000}]}, {"targetRow":2, "commands":[{"command":"RANDOM_FLICKER_TEXT", "intParam":50, "intParam2":10000}]}])");
            break;
        case ANIMATION_WORMHOLE_COLLAPSE:
            parseSequenceFromJson(tracks, R"([{"targetRow": 0, "commands": [{"command": "SOUND", "stringParam": "arrival_chime.mp3"}, {"command": "RANDOM_FLICKER_TEXT", "intParam": 100, "intParam2": 3000}, {"command": "FADE_OUT", "intParam": 2000}]}, {"targetRow": 1, "commands": [{"command": "RANDOM_FLICKER_TEXT", "intParam": 100, "intParam2": 3000}, {"command": "WAIT", "intParam": 500}, {"command": "FADE_OUT", "intParam": 3000}]}, {"targetRow": 2, "commands": [{"command": "RANDOM_FLICKER_TEXT", "intParam": 100, "intParam2": 3000}, {"command": "WAIT", "intParam": 1000}, {"command": "FADE_OUT", "intParam": 3000}]}])");
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