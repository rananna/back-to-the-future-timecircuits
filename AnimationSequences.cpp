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
        track.steps[MAX_SEQUENCE_STEPS - 1] = {SEQ_CMD_END, 0, 0, 0, 0, "", ""};
        // Return the index without advancing it to prevent further writes.
        return step_idx;
    }
    track.steps[step_idx] = {cmd, row, seg, p1, p2, s1, s2};
    return step_idx + 1;
}

// Helper to add the introductory sound effect steps
static int add_intro_sound_steps(SequencerTrack& track, int step_idx) {
    step_idx = add_step(track, step_idx, SEQ_CMD_SOUND, 0, 0, 0, 0, "/electric_sparks.mp3");
    step_idx = add_step(track, step_idx, SEQ_CMD_WAIT, 0, 0, 1000, 0);
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
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 20, 0);
    for (int i = 0; i < 4; i++) {
        s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, i, 100, 50);
        s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 50, 0);
        s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, i, 100, 50);
        s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 50, 0);
        s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, i, 100, 50);
        s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 50, 0);
    }
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
}

void generateAllDisplaysRandom(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_intro_sound_steps(tracks[0], s0);
    const int flicker_interval = 50; // ms for flicker effect refresh rate
    const int total_duration = 8500; // 8.5 seconds total animation time
    const int num_chars = 13; // Standard display width
    const int lock_in_interval = total_duration / num_chars; // ms per character reveal

    // Ensure strings are 13 characters for the animation timing
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
    s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 0, 0, 0, 0, dest_str.substr(0, 3).c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 0, 1, 0, 0, dest_str.substr(3, 2).c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 0, 2, 0, 0, dest_str.substr(5, 4).c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 0, 3, 0, 0, dest_str.substr(9, 4).c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 1, 0, 0, 0, pres_str.substr(0, 3).c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 1, 1, 0, 0, pres_str.substr(3, 2).c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 1, 2, 0, 0, pres_str.substr(5, 4).c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 1, 3, 0, 0, pres_str.substr(9, 4).c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 2, 0, 0, 0, last_str.substr(0, 3).c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 2, 1, 0, 0, last_str.substr(3, 2).c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 2, 2, 0, 0, last_str.substr(5, 4).c_str());
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 2, 3, 0, 0, last_str.substr(9, 4).c_str());
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
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 16, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 1, 0, 16, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_START, 2, 0, 16, 0);
    for (int i = 0; i < 6; i++) {
        std::string wave_str(waves[i]);
        s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, wave_str.c_str());
        s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, wave_str.c_str());
        std::string inverted_str;
        for(char c : wave_str) { inverted_str += (c == '-') ? ' ' : '-'; }
        s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, inverted_str.c_str());
        s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 100, 0);
        s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 100, 0);
        s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 100, 0);
    }
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 1, 0, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_END, 2, 0, 0, 0);
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
    int s = 0;
    s = add_step(tracks[0], s, SEQ_CMD_SOUND, 0, 0, 0, 0, "/countdown.mp3");
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 13, 0);
    for (int i = 0; i < 13; i++) {
        char text[14] = "             ";
        text[i] = time_strings[0][i];
        s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, text);
        text[i] = time_strings[1][i];
        s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, text);
        text[i] = time_strings[2][i];
        s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, text);
        s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 75, 0);
    }
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
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
    s0 = add_step(tracks[0], s0, SEQ_CMD_COUNTDOWN, 0, -1, 99999999, 1);
    s1 = add_step(tracks[1], s1, SEQ_CMD_COUNTDOWN, 1, -1, 99999999, 1);
    s2 = add_step(tracks[2], s2, SEQ_CMD_COUNTDOWN, 2, -1, 99999999, 1);
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
    s0 = add_step(tracks[0], s0, SEQ_CMD_COUNTDOWN, 0, -1, 99999999, 100);
    s1 = add_step(tracks[1], s1, SEQ_CMD_COUNTDOWN, 1, -1, 99999999, 50);
    s2 = add_step(tracks[2], s2, SEQ_CMD_COUNTDOWN, 2, -1, 99999999, 200);
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
    s = add_step(tracks[0], s, SEQ_CMD_SOUND, 0, 0, 0, 0, "/engine_rev.mp3");
    s = add_step(tracks[0], s, SEQ_CMD_MARQUEE, 1, -1, 0, 0, "LIFTOFF!");
}

void generateSystemError(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, 100, 200, "ERROR");
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "ERROR");
    s1 = add_step(tracks[1], s1, SEQ_CMD_MARQUEE, 1, -1, 0, 0, "SYSTEM MALFUNCTION");
    s2 = add_step(tracks[2], s2, SEQ_CMD_SOUND, 0, 0, 0, 0, "/error_beeps.mp3");
}

// --- Special Debugging Sequences ---

/**
 * @brief (DEBUG) A general stress-test sequence.
 * @details This sequence is designed to test multiple complex features at once.
 * - Track 0: Runs a nested loop with sound effects to test timing and loop control.
 * - Track 1: Displays a very long scrolling marquee text to stress the buffer and timing for this effect.
 * - Track 2: Runs a parallel scramble text animation to ensure multiple complex animations can run together.
 */
void generateDebugSequence(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;

    // Announce the main test
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "DEBUG TEST");
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "STRESS TEST");
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "RUNNING...");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 2000, 0);

    // Track 0: Complex nested loops with sound
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "NESTED LOOPS");
    s0 = add_step(tracks[0], s0, SEQ_CMD_SOUND, 0, 0, 0, 0, "/time_travel.mp3");
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 3, 0); // Outer loop
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 500, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 5, 0); // Inner loop
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 0, 0, 100, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 100, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0); // End inner loop
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 500, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0); // End outer loop
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "LOOPS DONE");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 1000, 0);

    // Track 1: Long scrolling text
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "LONG MARQUEE");
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 0, 0, 1000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_MARQUEE, 1, -1, 0, 0, "ROADS? WHERE WE'RE GOING, WE DON'T NEED ROADS. THIS IS A TEST OF THE EMERGENCY BROADCAST SYSTEM. THIS IS ONLY A TEST.");
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "MARQUEE DONE");

    // Track 2: Parallel scramble text
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "SCRAMBLE");
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 0, 0, 1000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCRAMBLE_TEXT, 2, -1, 50, 150, "PARALLEL TEST");
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 0, 0, 4000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "SCRAMBLE DONE");
}

/**
 * @brief (DEBUG) A comprehensive, sequential showcase of all visual effects.
 * @details This sequence demonstrates each visual effect one after another on the
 * center display, with the top and bottom displays showing the name of the
 * currently running test. This is useful for debugging individual effects.
 */
void generateDebugEffectsSequence(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;

    // Announce the test
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "EFFECTS TEST");
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "PARALLEL RUN");
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "-------------");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 2000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 2000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 2000, 0);


    // Track 0: Wipe, Typewriter, Crossfade
    s0 = add_step(tracks[0], s0, SEQ_CMD_WIPE, 0, -1, 75, 0, "WIPE TEST");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 2000, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_TYPEWRITER, 0, -1, 100, 0, "TYPEWRITER");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 2000, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_CROSSFADE_TEXT, 0, -1, 2000, 0, "CROSSFADE");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 2000, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "TRACK 0 DONE");


    // Track 1: Scramble, Scanner, Bar Graph
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCRAMBLE_TEXT, 1, -1, 50, 250, "SCRAMBLE");
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 4000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCANNER, 1, -1, 4000, 80, "<->");
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 4000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_BAR_GRAPH, 1, -1, 4000, 0, "BAR GRAPH");
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 4000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "TRACK 1 DONE");

    // Track 2: Fade, Pulse, Flash
    s2 = add_step(tracks[2], s2, SEQ_CMD_FADE_OUT, 2, -1, 1500, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 500, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_FADE_IN, 2, -1, 1500, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 2000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_PULSE, 2, -1, 4000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 4000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_FLASH, 2, -1, 4000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 4000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "TRACK 2 DONE");
}


/**
 * @brief (DEBUG) A robust, parallel test for the sequencer's logic commands.
 * @details This test is designed to correctly test the parallel execution
 * of the sequencer. It runs three distinct, long-running, and non-conflicting
 * animations on each of the three display rows to demonstrate that the sequencer
 * can handle them all at once without interference.
 * - Track 0: A long scrolling marquee text.
 * - Track 1: A 20-second countdown.
 * - Track 2: A 20-second scanner effect.
 */
void generateDebugParallelLogicSequence(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;

    // Announce the test
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "PARALLEL LOGIC");
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "PARALLEL LOGIC");
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "PARALLEL LOGIC");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 2000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 2000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 2000, 0);


    // Track 0: Long Marquee
    s0 = add_step(tracks[0], s0, SEQ_CMD_MARQUEE, 0, -1, 0, 0, "TRACK 0: A VERY LONG MARQUEE TO TEST PARALLEL EXECUTION");

    // Track 1: Countdown
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "TRACK 1: COUNT");
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 1000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_COUNTDOWN, 1, -1, 20, 1000, ""); // Countdown from 20, 1s interval

    // Track 2: Scanner
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "TRACK 2: SCAN");
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 1000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCANNER, 2, -1, 20000, 80, "<=>"); // Scan for 20s
}

/**
 * @brief (DEBUG) A chaotic stress-test sequence.
 * @details This sequence runs a long loop, firing off random animations on random
 * tracks with random timings. Its purpose is to uncover race conditions, memory
 * leaks, or other instability issues that only appear under high load.
 * It is not designed to be visually coherent.
 */
void generateDebugLogicSequence(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;

    // Announce the test
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "LOGIC TEST");
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "PARALLEL RUN");
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "-------------");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 2000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 2000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 2000, 0);

    // Track 0: Nested loops and MQTT publish
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 2, 0); // Outer loop
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, 0, 0, 0, "LOOP");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 500, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 3, 0); // Inner loop
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 0, 0, 200, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 300, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0); // End inner
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0); // End outer
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "MQTT...");
    s0 = add_step(tracks[0], s0, SEQ_CMD_MQTT_PUBLISH, 0, 0, 0, 0, "timecircuits/test", "Track 0 Done");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 1000, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "TRACK 0 DONE");

    // Track 1: Countdown and restore row
    s1 = add_step(tracks[1], s1, SEQ_CMD_COUNTDOWN, 1, -1, 10, 500); // Countdown from 10, 0.5s interval
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 1000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "RESTORE...");
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 1000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_RESTORE_ROW, 1, 0, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 1000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "TRACK 1 DONE");

    // Track 2: Display HA Sensor and restore all
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, 0, 0, 0, "GET HA");
    s2 = add_step(tracks[2], s2, SEQ_CMD_DISPLAY_HA_SENSOR, 2, 2, 0, 0, "sensor.time", "");
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 3000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "RESTORE ALL..");
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 1000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_RESTORE_ALL_ROWS, 0, 0, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 1000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "TRACK 2 DONE");
}

void generateDebugWipeSequence(SequencerTrack tracks[3]) {
    int s0 = 0;

    // Announce the test
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "WIPE TEST");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 2000, 0);

    // Test 1: Short string to test the bounds check fix
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "SHORT WIPE");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WIPE, 0, -1, 100, 0, "SHORT");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 3000, 0); // Wait to observe

    // Test 2: Full-length string to test for regressions
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "LONG WIPE");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WIPE, 0, -1, 100, 0, "LONG_WIPE_TEST");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 3000, 0); // Wait to observe

    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "TEST DONE");
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "");
}

void generateDebugStressSequence(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;

    // Announce the test
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "STRESS TEST");
    s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, "RANDOM CHAOS");
    s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, "RUNNING...");
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 2000, 0);

    // List of visual effects to choose from randomly
    SequenceCommand effects[] = {
        SEQ_CMD_WIPE, SEQ_CMD_SCROLL_IN, SEQ_CMD_TYPEWRITER, SEQ_CMD_FADE_OUT,
        SEQ_CMD_PULSE, SEQ_CMD_FLASH, SEQ_CMD_RANDOM_FLICKER_TEXT, SEQ_CMD_SCRAMBLE_TEXT,
        SEQ_CMD_SCANNER, SEQ_CMD_BAR_GRAPH
    };
    int num_effects = sizeof(effects) / sizeof(effects[0]);

    for (int i = 0; i < 50; ++i) { // Run 50 random commands
        int track_idx = random(0, 3);
        int effect_idx = random(0, num_effects);
        int duration = random(500, 2000);
        int interval = random(50, 250);

        int* s_ptr = (track_idx == 0) ? &s0 : (track_idx == 1) ? &s1 : &s2;
        SequencerTrack& track = tracks[track_idx];

        *s_ptr = add_step(track, *s_ptr, effects[effect_idx], track_idx, -1, duration, interval, "STRESS");
        *s_ptr = add_step(track, *s_ptr, SEQ_CMD_WAIT, track_idx, 0, duration, 0);
    }
}

// --- New Thematic Animation Generators ---

void generateLightning(SequencerTrack tracks[3]) {
    int s = 0;
    // Start with a sound effect for thunder
    s = add_step(tracks[0], s, SEQ_CMD_SOUND, 0, 0, 0, 0, "/lightning.mp3");
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 10, 0);
    // Random flashes on all displays
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 200, 50);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 200, 50);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 200, 50);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 200, 0);
    // A big flash to simulate a lightning strike
    s = add_step(tracks[0], s, SEQ_CMD_FLASH, 0, -1, 150, 0);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 150, 0);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
}

void generateScanner(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;

    // Announce the animation and play a sound
    s0 = add_step(tracks[0], s0, SEQ_CMD_SOUND, 0, 0, 0, 0, "/scanner.mp3");
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
    s = add_step(tracks[0], s, SEQ_CMD_SOUND, 0, 0, 0, 0, "/time_travel.mp3");
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
    s = add_step(tracks[0], s, SEQ_CMD_SOUND, 0, 0, 0, 0, "/flux_capacitor.mp3");
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
    s = add_step(tracks[0], s, SEQ_CMD_SOUND, 0, 0, 0, 0, "/fire_trails.mp3");
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
    char time_strings[3][17];
    getFormattedTimeStrings(time_strings[0], time_strings[1], time_strings[2]);

    for (int i = 0; i < 3; ++i) {
        tracks[i].reset();
    }

    switch (animType) {
        case ANIMATION_SEQUENTIAL_FLICKER:      generateSequentialFlicker(tracks, time_strings); break;
        case ANIMATION_TORNADO_FLICKER:         generateTornadoFlicker(tracks); break;
        case ANIMATION_ALL_DISPLAYS_RANDOM:     generateAllDisplaysRandom(tracks, time_strings); break;
        case ANIMATION_CAPACITOR_CHARGE_UP:     generateCapacitorChargeUp(tracks); break;
        case ANIMATION_WAVEFORM_COLLAPSE:       generateWaveformCollapse(tracks); break;
        case ANIMATION_WAVE_FLICKER:            generateWaveFlicker(tracks); break;
        case ANIMATION_CODE_BREAKER:            generateCodeBreaker(tracks, time_strings); break;
        case ANIMATION_FLIP_DISC_DISPLAY:       generateFlipDisc(tracks, time_strings); break;
        case ANIMATION_CHARACTER_SCANLINE:      generateCharacterScanline(tracks, time_strings); break;
        case ANIMATION_TEMPORAL_PARADOX:        generateTemporalParadox(tracks, time_strings); break;
        case ANIMATION_INTERFERENCE_PATTERN:    generateInterferencePattern(tracks, time_strings); break;
        case ANIMATION_TIME_WARP_STREAKS:       generateTimeWarpStreaks(tracks, time_strings); break;
        case ANIMATION_FOCUS_IN:                generateFocusIn(tracks, time_strings); break;
        case ANIMATION_ELECTRIC_SURGE:          generateElectricSurge(tracks, time_strings); break;
        case ANIMATION_DIGIT_CASCADE:           generateDigitCascade(tracks, time_strings); break;
        case ANIMATION_PLASMA_WARM_UP:          generatePlasmaWarmup(tracks); break;
        case ANIMATION_GLITCHY_JUMP_CUT:        generateGlitchyJumpCut(tracks); break;
        case ANIMATION_COUNTING_UP:             generateCountingUp(tracks); break;
        case ANIMATION_TIMELINE_SKIM:           generateTimelineSkim(tracks, time_strings); break;
        case ANIMATION_TEMPORAL_DESYNC:         generateTemporalDesync(tracks); break;
        case ANIMATION_DIGITAL_RAIN:            generateDigitalRain(tracks); break;
        case ANIMATION_SPARKLE_REVEAL:          generateSparkleReveal(tracks, time_strings); break;

        // New thematic animations
        case ANIMATION_LIGHTNING:               generateLightning(tracks); break;
        case ANIMATION_SCANNER:                 generateScanner(tracks); break;
        case ANIMATION_TIME_TRAVEL_TUNNEL:      generateTimeTravelTunnel(tracks, time_strings); break;
        case ANIMATION_FLUX_CAPACITOR_OVERLOAD: generateFluxCapacitorOverload(tracks); break;
        case ANIMATION_FIRE_TRAILS:             generateFireTrails(tracks, time_strings); break;
        case ANIMATION_COUNTDOWN:               generateCountdown(tracks); break;
        case ANIMATION_SYSTEM_ERROR:            generateSystemError(tracks); break;

        // --- Special Debug Sequences ---
        case ANIMATION_DEBUG:                   generateDebugSequence(tracks); break;
        case ANIMATION_DEBUG_EFFECTS:           generateDebugEffectsSequence(tracks); break;
        case ANIMATION_DEBUG_LOGIC:             generateDebugLogicSequence(tracks); break;
        case ANIMATION_DEBUG_PARALLEL_LOGIC:    generateDebugParallelLogicSequence(tracks); break;
        case ANIMATION_DEBUG_STRESS:            generateDebugStressSequence(tracks); break;
        case ANIMATION_DEBUG_WIPE:              generateDebugWipeSequence(tracks); break;

        case ANIMATION_RANDOM_FLICKER:          generateRandomFlicker(tracks); break;
        default:
            generateTornadoFlicker(tracks);
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