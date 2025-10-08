#include <ArduinoJson.h>
#include "AnimationSequences.h"
#include "HardwareControl.h"
#include "DisplayManager.h"
#include "DebugLog.h"
#include <Arduino.h>
#include <string>
#include <stdlib.h>

// Helper to add a step to a track safely, returns the new index
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

// Helper to add the introductory sound effect steps
static int add_intro_sound_steps(SequencerTrack& track, int step_idx) {
    // This is now a no-op. The sound is triggered directly from the handlePresetCycling function
    // to ensure perfect synchronization with the animation start.
    return step_idx;
}

// --- Individual Animation Generators ---

void generateRandomFlicker(SequencerTrack tracks[3]) {
    int s = 0;
    s = add_intro_sound_steps(tracks[0], s);
    // Loop for 10 seconds (100 * 100ms)
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 100, 0);
    // Restore all rows to normal at the start of each loop
    s = add_step(tracks[0], s, SEQ_CMD_RESTORE_ALL_ROWS, 0, 0, 0, 0);
    // Glitch a random row for a short duration (200ms)
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, random(0, 3), -1, 200, 50);
    // Wait for the remainder of the 100ms interval, plus a random extra delay
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 80 + random(0, 200), 0);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
}

void generateTornadoFlicker(SequencerTrack tracks[3]) {
    int s = 0;
    s = add_intro_sound_steps(tracks[0], s);
    // The C++ for loop below generates the full animation sequence.
    // The previous SEQ_CMD_LOOP commands were redundant and caused a buffer overflow.
    for (int i = 0; i < 4; i++) {
        s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, i, 100, 50);
        s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 50, 0);
        s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, i, 100, 50);
        s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 50, 0);
        s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, i, 100, 50);
        s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 50, 0);
    }
}

void generateAllDisplaysRandom(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    const int flicker_interval = 50; // ms for flicker effect refresh rate
    const int total_duration = 8500; // 8.5 seconds total animation time
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

void generateSequentialFlicker(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s = 0;
    s = add_intro_sound_steps(tracks[0], s);
    const int delay = 83;
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

void generateCapacitorChargeUp(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_BAR_GRAPH, 0, -1, 10000, 250);
    s1 = add_step(tracks[1], s1, SEQ_CMD_BAR_GRAPH, 1, -1, 10000, 250);
    s2 = add_step(tracks[2], s2, SEQ_CMD_BAR_GRAPH, 2, -1, 10000, 250);
}

void generateWaveformCollapse(SequencerTrack tracks[3]) {
    const char* waves[] = {"-------------", " ---     --- ", "  ---   ---  ", "   -------   ", "  ---   ---  ", " ---     --- "};
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);

    for (int i = 0; i < 6; i++) {
        // --- FIX: Use local std::string to guarantee pointer validity ---
        std::string wave_str(waves[i]);
        s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, wave_str.c_str());
        s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, wave_str.c_str());

        std::string inverted_str;
        for(char c : wave_str) { inverted_str += (c == '-') ? ' ' : '-'; }
        s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, inverted_str.c_str());

        // Add waits to each track to keep them in sync
        s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 100, 0);
        s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 100, 0);
        s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 100, 0);
    }
}

void generateWaveFlicker(SequencerTrack tracks[3]) {
    int s = 0;
    s = add_intro_sound_steps(tracks[0], s);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 10, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 200, 50, "---     ---");
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 200, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 200, 50, "  ---   --- ");
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 200, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 200, 50, "   -------  ");
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 200, 0);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
}

void generateCodeBreaker(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, 15, 33, time_strings[0]);
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCRAMBLE_TEXT, 1, -1, 15, 33, time_strings[1]);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCRAMBLE_TEXT, 2, -1, 15, 33, time_strings[2]);
}

void generateFlipDisc(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_WIPE, 0, -1, 75, 0, time_strings[0]);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WIPE, 1, -1, 75, 0, time_strings[1]);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WIPE, 2, -1, 75, 0, time_strings[2]);
}

void generateCharacterScanline(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_TYPEWRITER, 0, -1, 75, 0, time_strings[0]);
    s1 = add_step(tracks[1], s1, SEQ_CMD_TYPEWRITER, 1, -1, 75, 0, time_strings[1]);
    s2 = add_step(tracks[2], s2, SEQ_CMD_TYPEWRITER, 2, -1, 75, 0, time_strings[2]);
}

void generateTemporalParadox(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s = 0;
    s = add_intro_sound_steps(tracks[0], s);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 25, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, time_strings[1]);
    s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, time_strings[0]);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 200, 50);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 200, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RESTORE_ROW, 0, -1, 0, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RESTORE_ROW, 1, -1, 0, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 200, 50);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 200, 0);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
}

void generateInterferencePattern(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s = 0;
    s = add_intro_sound_steps(tracks[0], s);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 15, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 150, 100, "!@#$%%^&*()_+-=");
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 150, 100, time_strings[1]);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 150, 100, "!@#$%%^&*()_+-=");
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 150, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RESTORE_ROW, 1, -1, 0, 0);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 50, 0);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
}

void generateTimeWarpStreaks(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0=0, s1=0, s2=0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCROLL_IN, 0, -1, 75, 0, time_strings[0]);
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCROLL_IN, 1, -1, 75, 0, time_strings[1]);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCROLL_IN, 2, -1, 75, 0, time_strings[2]);
}

void generateFocusIn(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s = 0;
    s = add_intro_sound_steps(tracks[0], s);
    s = add_step(tracks[0], s, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, 200, 200, time_strings[0]);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 100, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SCRAMBLE_TEXT, 1, -1, 200, 200, time_strings[1]);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 100, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SCRAMBLE_TEXT, 2, -1, 200, 200, time_strings[2]);
}

void generateElectricSurge(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s = 0;
    s = add_intro_sound_steps(tracks[0], s);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 10, 0);
    s = add_step(tracks[0], s, SEQ_CMD_FLASH, 0, -1, 50, 0);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 25, 0);
    s = add_step(tracks[0], s, SEQ_CMD_FLASH, 1, -1, 50, 0);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 25, 0);
    s = add_step(tracks[0], s, SEQ_CMD_FLASH, 2, -1, 50, 0);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 100, 0);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
}

void generateDigitCascade(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    // This implementation now uses the much cleaner and more efficient TYPEWRITER command.
    // The previous version was inefficient and risked a buffer overflow.
    s0 = add_step(tracks[0], s0, SEQ_CMD_TYPEWRITER, 0, -1, 75, 0, time_strings[0]);
    s1 = add_step(tracks[1], s1, SEQ_CMD_TYPEWRITER, 1, -1, 75, 0, time_strings[1]);
    s2 = add_step(tracks[2], s2, SEQ_CMD_TYPEWRITER, 2, -1, 75, 0, time_strings[2]);
}

void generatePlasmaWarmup(SequencerTrack tracks[3]) {
    int s = 0;
    s = add_intro_sound_steps(tracks[0], s);
    s = add_step(tracks[0], s, SEQ_CMD_FADE_IN, 0, -1, 5000, 0);
    s = add_step(tracks[0], s, SEQ_CMD_FADE_OUT, 0, -1, 5000, 0);
}

void generateGlitchyJumpCut(SequencerTrack tracks[3]) {
    int s=0;
    s = add_intro_sound_steps(tracks[0], s);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 20, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 200, 50);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 200, 50);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 200, 50);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 200, 0);
    s = add_step(tracks[0], s, SEQ_CMD_FLASH, 0, -1, 100, 0);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 100, 0);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
}

void generateCountingUp(SequencerTrack tracks[3]) {
    int s0=0, s1=0, s2=0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    // --- FIX: Replace hang-guaranteed countdown with a finite visual effect ---
    s0 = add_step(tracks[0], s0, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 10000, 50);
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 10000, 50);
    s2 = add_step(tracks[2], s2, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 10000, 50);
}

void generateTimelineSkim(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s = 0;
    s = add_intro_sound_steps(tracks[0], s);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 3, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 500, 50);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 500, 50);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 500, 50);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 500, 0);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
    s = add_step(tracks[0], s, SEQ_CMD_TYPEWRITER, 0, -1, 100, 0, time_strings[0]);
    s = add_step(tracks[0], s, SEQ_CMD_TYPEWRITER, 1, -1, 100, 0, time_strings[1]);
    s = add_step(tracks[0], s, SEQ_CMD_TYPEWRITER, 2, -1, 100, 0, time_strings[2]);
}

void generateTemporalDesync(SequencerTrack tracks[3]) {
    int s0=0, s1=0, s2=0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    // --- FIX: Replace hang-guaranteed countdown with a finite visual effect ---
    s0 = add_step(tracks[0], s0, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 10000, 100);
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 10000, 50);
    s2 = add_step(tracks[2], s2, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 10000, 200);
}

void generateDigitalRain(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 10000, 50);
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 10000, 50);
    s2 = add_step(tracks[2], s2, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 10000, 50);
}

void generateCountdown(SequencerTrack tracks[3]) {
    int s = 0;
    s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "COUNTDOWN");
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 1500, 0);
    // --- FIX: The COUNTDOWN command now correctly handles the full sequence including '0'.
    // No need for a separate SET_TEXT command.
    s = add_step(tracks[0], s, SEQ_CMD_COUNTDOWN, 1, -1, 10, 1000);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 500, 0); // Brief pause before liftoff
    s = add_step(tracks[0], s, SEQ_CMD_MARQUEE, 1, -1, 0, 0, "LIFTOFF!");
}

void generateSystemError(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, 100, 200, "ERROR");
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "ERROR");
    s1 = add_step(tracks[1], s1, SEQ_CMD_MARQUEE, 1, -1, 0, 0, "SYSTEM MALFUNCTION");
}

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

// --- New Thematic Animation Generators ---

void generateLightning(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;

    // A more dramatic, multi-stage lightning effect.

    // --- Stage 1: Initial Strike ---
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "  DANGER!    ");
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "HIGH VOLTAGE ");
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "-------------");

    // Quick, intense flash on all rows
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 0, -1, 250, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_FLASH, 1, -1, 250, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_FLASH, 2, -1, 250, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 300, 0);

    // --- Stage 2: Building Chaos ---
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 10, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 1, 0, 10, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_START, 2, 0, 10, 0);

    // Flicker all rows with random timings to create a chaotic effect.
    // The command will use the text already on the display.
    s0 = add_step(tracks[0], s0, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, random(100, 200), 50);
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, random(50, 150), 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, random(100, 250), 50);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, random(50, 200), 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, random(100, 300), 50);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, random(50, 250), 0);

    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 1, 0, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_END, 2, 0, 0, 0);

    // --- Stage 3: The "1.21 Gigawatts" Moment ---
    // Make the scramble duration (13 chars * 115ms) ~1500ms to match the flash commands.
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCRAMBLE_TEXT, 1, -1, 50, 115, "  1.21 GW!!  ");

    // While the middle row is scrambling, keep flashing the top and bottom
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 0, -1, 1500, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_FLASH, 2, -1, 1500, 0);

    // --- Stage 4: Final Power Surge ---
    // All tracks are now synchronized after Stage 3. No extra wait is needed.
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "SYSTEMS LIVE ");
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "  1.21 GW!!  ");
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "-------------");

    // Final pulse to end the sequence
    s0 = add_step(tracks[0], s0, SEQ_CMD_PULSE, 0, -1, 2000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_PULSE, 1, -1, 2000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_PULSE, 2, -1, 2000, 0);
}

void generateScanner(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;

    // Announce the animation and play a sound
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "SCANNER");
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "SCANNER");
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "SCANNER");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 1500, 0); // Wait for text to be readable

    // Clear the announcement text
    s0 = add_step(tracks[0], s0, SEQ_CMD_RESTORE_ALL_ROWS, 0, 0, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_NONE, 0, 0, 0, 0); // No more steps for track 1
    s2 = add_step(tracks[2], s2, SEQ_CMD_NONE, 0, 0, 0, 0); // No more steps for track 2
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 250, 0);

    // Run the scanner effect on all three rows in parallel
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCANNER, 0, -1, 10000, 80, "---");
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCANNER, 1, -1, 10000, 80, "---");
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCANNER, 2, -1, 10000, 80, "---");
}

void generateTimeTravelTunnel(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s = 0;
    // Simulate traveling through a time vortex
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 5, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SCROLL_IN, 0, -1, 50, 0, time_strings[0]);
    s = add_step(tracks[0], s, SEQ_CMD_SCROLL_IN, 1, -1, 50, 0, time_strings[1]);
    s = add_step(tracks[0], s, SEQ_CMD_SCROLL_IN, 2, -1, 50, 0, time_strings[2]);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 500, 0);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
}

void generateFluxCapacitorOverload(SequencerTrack tracks[3]) {
    int s = 0;
    // Show the Flux Capacitor pulsing with energy
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 10, 0);
    s = add_step(tracks[0], s, SEQ_CMD_PULSE, 0, -1, 500, 250);
    s = add_step(tracks[0], s, SEQ_CMD_PULSE, 1, -1, 500, 250);
    s = add_step(tracks[0], s, SEQ_CMD_PULSE, 2, -1, 500, 250);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 500, 0);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
}

void generateFireTrails(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s = 0;
    // Burn the date onto the display with a fire trail effect
    s = add_step(tracks[0], s, SEQ_CMD_WIPE, 0, -1, 100, 0, time_strings[0]);
    s = add_step(tracks[0], s, SEQ_CMD_WIPE, 1, -1, 100, 0, time_strings[1]);
    s = add_step(tracks[0], s, SEQ_CMD_WIPE, 2, -1, 100, 0, time_strings[2]);
}

void generateSparkleReveal(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s = 0;
    s = add_intro_sound_steps(tracks[0], s);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 20, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 100, 50, " . ");
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 50, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 100, 50, ". .");
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 50, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 100, 50, " . ");
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
    s = add_step(tracks[0], s, SEQ_CMD_WIPE, 0, -1, 100, 0, time_strings[0]);
    s = add_step(tracks[0], s, SEQ_CMD_WIPE, 1, -1, 100, 0, time_strings[1]);
    s = add_step(tracks[0], s, SEQ_CMD_WIPE, 2, -1, 100, 0, time_strings[2]);
}


// --- Main Generation Function ---

void generateAnimationSequence(AnimationType animType, SequencerTrack tracks[3]) {
    // --- START: MODIFICATION - Centralized Randomization ---
    if (animType == ANIMATION_RANDOMIZE_ALL) {
        const AnimationType validAnimationStyles[] = {
            // A curated list of interesting and stable animations suitable for this feature
            ANIMATION_ALL_DISPLAYS_RANDOM,
            ANIMATION_LIGHTNING,
            ANIMATION_SCANNER,
            ANIMATION_TIME_TRAVEL_TUNNEL,
            ANIMATION_FLUX_CAPACITOR_OVERLOAD,
            ANIMATION_FIRE_TRAILS,
            ANIMATION_SPARKLE_REVEAL,
            ANIMATION_SYSTEM_ERROR,
            ANIMATION_SEQUENTIAL_FLICKER,
            ANIMATION_TORNADO_FLICKER,
            ANIMATION_CAPACITOR_CHARGE_UP,
            ANIMATION_WAVEFORM_COLLAPSE,
            ANIMATION_TIMELINE_SKIM,
            ANIMATION_TIME_WARP_STREAKS,
            ANIMATION_CHARACTER_SCANLINE,
            ANIMATION_CODE_BREAKER,
            ANIMATION_ELECTRIC_SURGE,
            ANIMATION_FLIP_DISC_DISPLAY
        };
        int numStyles = sizeof(validAnimationStyles) / sizeof(validAnimationStyles[0]);
        int randomIndex = random(0, numStyles);
        // Overwrite the animType with the new, randomly selected type.
        // This avoids a recursive call which can lead to a stack overflow.
        animType = validAnimationStyles[randomIndex];
        // --- Let the function fall through to the main switch statement ---
    }
    // --- END: MODIFICATION ---

    char time_strings[3][17];
    getFormattedTimeStrings(time_strings[0], time_strings[1], time_strings[2]);

    for (int i = 0; i < 3; ++i) {
        tracks[i].reset();
    }

    // --- FIX: A correct switch statement with all cases and a proper default ---
    switch (animType) {
        // --- JSON-based Named Sequences ---
        case ANIMATION_INTRUDER_ALERT:
            parseSequenceFromJson(tracks, R"([{"targetRow":"TOP", "commands":[{"command":"MARQUEE", "stringParam":"INTRUDER ALERT"}, {"command":"PULSE", "targetSegment":-1, "intParam":5000}]}, {"targetRow":"MIDDLE", "commands":[{"command":"SCRAMBLE_TEXT", "stringParam":"BREACH DETECTED", "intParam":100, "intParam2":400}]}, {"targetRow":"BOTTOM", "commands":[{"command":"MARQUEE", "stringParam":"LOCKDOWN INITIATED"}, {"command":"PULSE", "targetSegment":-1, "intParam":5000}]}])");
            break;
        case ANIMATION_TIME_TRAVEL:
            parseSequenceFromJson(tracks, R"([{"targetRow": "TOP", "commands": [{"command": "SOUND", "stringParam":"time_travel.mp3"}, {"command": "BAR_GRAPH", "stringParam":"ACCELERATING", "intParam":0, "intParam2":8000}]}, {"targetRow": "MIDDLE", "commands": [{"command": "SET_TEXT", "stringParam":"TIME TRAVEL"}, {"command": "WAIT", "intParam": 1000}, {"command":"SET_TEXT", "stringParam":"ACTIVATED"}, {"command":"WAIT", "intParam":1000}, {"command": "SET_TEXT", "stringParam": "88 MPH"}]}, {"targetRow": "BOTTOM", "commands": [{"command": "FLASH", "targetSegment": -1, "intParam": 8000}]}])");
            break;
        case ANIMATION_PARTY_MODE:
            parseSequenceFromJson(tracks, R"([{"targetRow":"TOP", "commands":[{"command":"MARQUEE", "stringParam":"PARTY TIME"}, {"command":"LOOP_START", "intParam":5}, {"command":"PULSE", "targetSegment":-1, "intParam":1000}, {"command":"WAIT", "intParam":1000}, {"command":"LOOP_END"}]}, {"targetRow":"MIDDLE", "commands":[{"command":"LOOP_START", "intParam":5}, {"command":"MARQUEE", "stringParam":"DANCE"}, {"command":"WAIT", "intParam":2000}, {"command":"MARQUEE", "stringParam":"PARTY"}, {"command":"WAIT", "intParam":2000}, {"command":"LOOP_END"}]}, {"targetRow":"BOTTOM", "commands":[{"command":"LOOP_START", "intParam":5}, {"command":"MARQUEE", "stringParam":"WOOHOO"}, {"command":"WAIT", "intParam":5000}, {"command":"LOOP_END"}]}])");
            break;
        case ANIMATION_KNIGHT_RIDER:
            parseSequenceFromJson(tracks, R"([{"targetRow":2, "commands":[{"command":"SCANNER", "intParam":10000, "intParam2":100}]}])");
            break;
        case ANIMATION_LOADING:
            parseSequenceFromJson(tracks, R"([{"targetRow":0, "commands":[{"command":"SET_TEXT", "stringParam":"FLUX CAPACITOR"}, {"command":"WAIT", "intParam":1500}]}, {"targetRow":1, "commands":[{"command":"WAIT", "intParam":1500}, {"command":"SET_TEXT", "stringParam":"TIME CIRCUITS"}, {"command":"WAIT", "intParam":1500}]}, {"targetRow":2, "commands":[{"command":"WAIT", "intParam":3000}, {"command":"SET_TEXT", "stringParam":"SYSTEMS ONLINE"}, {"command":"WAIT", "intParam":1500}]}])");
            break;
        case ANIMATION_ERROR:
            parseSequenceFromJson(tracks, R"([{"targetRow":0, "commands":[{"command":"SCRAMBLE_TEXT", "stringParam":"ERROR", "intParam":100, "intParam2":200}, {"command":"SET_TEXT", "stringParam":"ERROR"}]}, {"targetRow":1, "commands":[{"command":"MARQUEE", "stringParam":"SYSTEM MALFUNCTION"}]}])");
            break;
        case ANIMATION_FLUX_CHARGE:
            parseSequenceFromJson(tracks, R"([{"targetRow":2, "commands":[{"command":"SOUND", "stringParam":"flux_capacitor_power_on.mp3"}, {"command":"BAR_GRAPH", "stringParam":"CHARGE", "intParam":0, "intParam2":5000}]}, {"targetRow":0, "commands":[{"command":"WAIT", "intParam":3000}, {"command":"FLASH", "targetSegment":-1, "intParam":2000}]}, {"targetRow":1, "commands":[{"command":"WAIT", "intParam":3000}, {"command":"FLASH", "targetSegment":-1, "intParam":2000}]}])");
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