/**
 * @file AnimationSequences.cpp
 * @brief Implements the generation of all built-in animation sequences for the Time Circuits.
 * @details This file contains the functions that construct the various animation sequences,
 * both simple and complex, by adding a series of commands to sequencer tracks. It includes
 * generators for C++ defined animations and a parser for JSON-defined sequences.
 */

#include <ArduinoJson.h>
#include "AnimationSequences.h"
#include "HardwareControl.hh"
#include "DisplayManager.h"
#include "DebugLog.h"
#include <Arduino.h>

static int add_step(SequencerTrack& track, int step_idx, SequenceCommand cmd, int row, int seg, int p1, int p2, const char* s1 = "", const char* s2 = "") {
    if (step_idx >= MAX_SEQUENCE_STEPS) {
        Log_printf(LOG_LEVEL_WARN, "SEQ_GEN: Sequence has too many steps! Truncating. Max is %d.", MAX_SEQUENCE_STEPS);
        track.steps[MAX_SEQUENCE_STEPS - 1] = SequenceStep(SEQ_CMD_END, 0, 0, 0, 0, "", "");
        return step_idx;
    }
    track.steps[step_idx] = SequenceStep(cmd, row, seg, p1, p2, s1, s2);
    return step_idx + 1;
}

static int add_intro_sound_steps(SequencerTrack& track, int step_idx) {
    return step_idx;
}

// --- C++ Based Animation Generators ---

void generateRandomFlicker(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    const int loop_count = 25;
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, loop_count, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 200, 50);
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 200, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 1, 0, loop_count, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 100, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 200, 50);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 100, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 1, 0, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_START, 2, 0, loop_count, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 200, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 150, 50);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 50, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_END, 2, 0, 0, 0);
}

void generateTornadoFlicker(SequencerTrack tracks[3]) {
    int s = 0;
    for (int i = 0; i < 13; i++) {
        s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, i % 4, 100, 50);
        s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 150, 0);
        s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, i % 4, 100, 50);
        s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 150, 0);
        s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, i % 4, 100, 50);
        s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 150, 0);
    }
}

void generateAllDisplaysRandom(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    const int lock_in_interval = 10000 / 13;
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, 50, lock_in_interval, time_strings[0]);
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCRAMBLE_TEXT, 1, -1, 50, lock_in_interval, time_strings[1]);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCRAMBLE_TEXT, 2, -1, 50, lock_in_interval, time_strings[2]);
}

void generateSequentialFlicker(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s = 0;
    const int delay = 830;
    static char buffer[5];
    auto add_segment_step = [&](int row, int seg, int start, int len) {
        strncpy(buffer, time_strings[row] + start, len);
        buffer[len] = '\0';
        return add_step(tracks[0], s, SEQ_CMD_SET_TEXT, row, seg, 0, 0, buffer);
    };
    for(int row_idx = 0; row_idx < 3; ++row_idx) {
        s = add_segment_step(row_idx, 0, 0, 3);
        s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
        s = add_segment_step(row_idx, 1, 3, 2);
        s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
        s = add_segment_step(row_idx, 2, 5, 4);
        s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
        s = add_segment_step(row_idx, 3, 9, 4);
        if (row_idx < 2) s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, delay, 0);
    }
}

void generateCapacitorChargeUp(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_BAR_GRAPH, 1, -1, 100, 9500);
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 9500, 150, "-.-");
    s2 = add_step(tracks[2], s2, SEQ_CMD_PULSE, 2, -1, 500, 9500, "CHARGING...");
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 0, -1, 500, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 1, -1, 500, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_FLASH, 2, -1, 500, 0);
}

void generateWaveformCollapse(SequencerTrack tracks[3]) {
    const char* waves[] = {"-------------", " ---     --- ", "  ---   ---  ", "   -------   ", "  ---   ---  ", " ---     --- "};
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 8, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 1, 0, 8, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_START, 2, 0, 8, 0);
    static char inverted_buffer[14];
    for (int i = 0; i < 6; i++) {
        const char* wave_str = waves[i];
        s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, wave_str);
        s2 = add_step(tracks[2], s2, SEQ_CMD_SET_TEXT, 2, -1, 0, 0, wave_str);
        for(int j=0; j < 13; j++) { inverted_buffer[j] = (wave_str[j] == '-') ? ' ' : '-'; }
        inverted_buffer[13] = '\0';
        s1 = add_step(tracks[1], s1, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, inverted_buffer);
        s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 200, 0);
        s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 200, 0);
        s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 200, 0);
    }
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 1, 0, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_END, 2, 0, 0, 0);
}

void generateWaveFlicker(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    const char* wave_pattern = "---     ---";
    const char* inverted_wave = "   -----   ";
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 5, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 1, 0, 5, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_START, 2, 0, 5, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_WIPE, 0, -1, 75, 0, wave_pattern);
    s1 = add_step(tracks[1], s1, SEQ_CMD_PULSE, 1, -1, 500, 1000, inverted_wave);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCROLL_IN, 2, -1, 75, 0, wave_pattern);
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 500, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 500, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 500, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_CLEAR_SEGMENT, 0, -1, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_CLEAR_SEGMENT, 1, -1, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_CLEAR_SEGMENT, 2, -1, 0, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 500, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 500, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 500, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 1, 0, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_END, 2, 0, 0, 0);
}

void generateCodeBreaker(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 4, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 1, 0, 4, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_START, 2, 0, 4, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, 25, 120, time_strings[0]);
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCRAMBLE_TEXT, 1, -1, 25, 120, time_strings[1]);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCRAMBLE_TEXT, 2, -1, 25, 120, time_strings[2]);
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 1000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 1000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 1000, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 1, 0, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_END, 2, 0, 0, 0);
}

void generateFlipDisc(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 5, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 1, 0, 5, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_START, 2, 0, 5, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_WIPE, 0, -1, 75, 0, time_strings[0]);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WIPE, 1, -1, 75, 0, time_strings[1]);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WIPE, 2, -1, 75, 0, time_strings[2]);
    s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 1000, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 1, 0, 1000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 1000, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 1, 0, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_END, 2, 0, 0, 0);
}

void generateCharacterScanline(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
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

void generateTemporalParadox(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s = 0;
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 12, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, time_strings[1]);
    s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, time_strings[0]);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 400, 50);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 400, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RESTORE_ROW, 0, -1, 0, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RESTORE_ROW, 1, -1, 0, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 400, 50);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 400, 0);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
}

void generateInterferencePattern(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    const char* interference = "!@#$%%^&*()_+-=";
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 5, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 1, 0, 5, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_START, 2, 0, 5, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_WIPE, 0, -1, 150, 0, interference);
    s1 = add_step(tracks[1], s1, SEQ_CMD_PULSE, 1, -1, 1000, 2000, time_strings[1]);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCROLL_IN, 2, -1, 150, 0, interference);
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 1, 0, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_END, 2, 0, 0, 0);
}

void generateTimeWarpStreaks(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0=0, s1=0, s2=0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_START, 0, 0, 10, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 1, 0, 10, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_START, 2, 0, 10, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCROLL_IN, 0, -1, 75, 0, time_strings[0]);
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCROLL_IN, 1, -1, 75, 0, time_strings[1]);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCROLL_IN, 2, -1, 75, 0, time_strings[2]);
    s0 = add_step(tracks[0], s0, SEQ_CMD_CLEAR_SEGMENT, 0, -1, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_CLEAR_SEGMENT, 1, -1, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_CLEAR_SEGMENT, 2, -1, 0, 0);
    s0 = add_step(tracks[0], s0, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 1, 0, 0, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_LOOP_END, 2, 0, 0, 0);
}

void generateFocusIn(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s = 0;
    s = add_step(tracks[0], s, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, 100, 200, time_strings[0]);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 100, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SCRAMBLE_TEXT, 1, -1, 100, 200, time_strings[1]);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 100, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SCRAMBLE_TEXT, 2, -1, 100, 200, time_strings[2]);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 2200, 0);
}

void generateElectricSurge(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s = 0;
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 4, 0);
    s = add_step(tracks[0], s, SEQ_CMD_PULSE, 0, -1, 1000, 1500, "ENERGY SURGE");
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 1500, 100, "><");
    s = add_step(tracks[0], s, SEQ_CMD_FLASH, 0, -1, 250, 0);
    s = add_step(tracks[0], s, SEQ_CMD_FLASH, 1, -1, 250, 0);
    s = add_step(tracks[0], s, SEQ_CMD_FLASH, 2, -1, 250, 0);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 750, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RESTORE_ALL_ROWS, 0, 0, 0, 0);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
}

void generateDigitCascade(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
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

void generatePlasmaWarmup(SequencerTrack tracks[3]) {
    int s = 0;
    s = add_step(tracks[0], s, SEQ_CMD_FADE_IN, 0, -1, 5000, 0);
    s = add_step(tracks[0], s, SEQ_CMD_FADE_OUT, 0, -1, 5000, 0);
}

void generateGlitchyJumpCut(SequencerTrack tracks[3]) {
    int s=0;
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 25, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 200, 50);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 200, 50);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 200, 50);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 0, 0);
    s = add_step(tracks[0], s, SEQ_CMD_FLASH, 0, -1, 100, 0);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 100, 0);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
}

void generateCountingUp(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s1 = add_step(tracks[1], s1, SEQ_CMD_BAR_GRAPH, 0, -1, 100, 10000);
    s2 = add_step(tracks[2], s2, SEQ_CMD_PULSE, 2, -1, 1000, 10000, "CALCULATING..");
    static char buffer[14];
    for (int i = 0; i <= 39; i++) {
        snprintf(buffer, sizeof(buffer), "%13d", i * 6921);
        s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, buffer);
        s0 = add_step(tracks[0], s0, SEQ_CMD_WAIT, 0, 0, 250, 0);
    }
}

void generateTimelineSkim(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    static char buffer[17];
    for (int i = 0; i < 10; i++) {
        snprintf(buffer, sizeof(buffer), "%s %02d %04d %02d%02d", months[random(12)], random(28) + 1, random(101) + 1950, random(24), random(60));
        s0 = add_step(tracks[0], s0, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, 50, 80, buffer);
        snprintf(buffer, sizeof(buffer), "%s %02d %04d %02d%02d", months[random(12)], random(28) + 1, random(101) + 1950, random(24), random(60));
        s1 = add_step(tracks[1], s1, SEQ_CMD_SCRAMBLE_TEXT, 1, -1, 50, 80, buffer);
        snprintf(buffer, sizeof(buffer), "%s %02d %04d %02d%02d", months[random(12)], random(28) + 1, random(101) + 1950, random(24), random(60));
        s2 = add_step(tracks[2], s2, SEQ_CMD_SCRAMBLE_TEXT, 2, -1, 50, 80, buffer);
    }
}

void generateTemporalDesync(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    char time_strings[3][17];
    getFormattedTimeStrings(time_strings[0], time_strings[1], time_strings[2]);
    s0 = add_step(tracks[0], s0, SEQ_CMD_PULSE, 1, -1, 1000, 10000, time_strings[1]);
    static char drift_time_str[17];
    snprintf(drift_time_str, sizeof(drift_time_str), "JAN 01 1985 1003");
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_START, 0, 0, 8, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCROLL_IN, 0, -1, 75, 0, drift_time_str);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WAIT, 0, 0, 250, 0);
    s1 = add_step(tracks[1], s1, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
    static char conflict_time_str[17];
    snprintf(conflict_time_str, sizeof(conflict_time_str), "OCT 26 2085 0429");
    s2 = add_step(tracks[2], s2, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 10000, 200, conflict_time_str);
}

void generateDigitalRain(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 10000, 75, "1010101010101");
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 10000, 100, "ABCDE12345FGHI");
    s2 = add_step(tracks[2], s2, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 10000, 125, "ZYXWV98765UTSR");
}

void generateCountdown(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_BAR_GRAPH, 0, -1, 100, 10000, "COUNTDOWN");
    s1 = add_step(tracks[1], s1, SEQ_CMD_COUNTDOWN, 1, -1, 10, 850);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WAIT, 2, 0, 10000, 0);
    s2 = add_step(tracks[2], s2, SEQ_CMD_MARQUEE, 2, -1, 0, 0, "LIFTOFF!");
}

void generateSystemError(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, 100, 200, "ERROR");
    s0 = add_step(tracks[0], s0, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, "ERROR");
    s0 = add_step(tracks[0], s0, SEQ_CMD_PULSE, 0, -1, 750, 8000);
    s1 = add_step(tracks[1], s1, SEQ_CMD_MARQUEE, 1, -1, 0, 0, "SYSTEM MALFUNCTION");
}

void generateTimeCircuitsLockIn(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_SOUND, 0, 0, 0, 0, "relay_activation.mp3");
    const int lock_in_interval = 10000 / 13;
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, 50, lock_in_interval, time_strings[0]);
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCRAMBLE_TEXT, 1, -1, 50, lock_in_interval, time_strings[1]);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCRAMBLE_TEXT, 2, -1, 50, lock_in_interval, time_strings[2]);
}

void parseSequenceFromJson(SequencerTrack tracks[3], const char* json_string) {
    static JsonDocument doc;
    doc.clear();
    DeserializationError error = deserializeJson(doc, json_string);
    if (error) {
        Log_printf(LOG_LEVEL_ERROR, "SEQ_PARSE: Failed to parse JSON sequence: %s", error.c_str());
        return;
    }
    JsonArray track_definitions = doc.as<JsonArray>();
    if (track_definitions.isNull()) return;

    for (JsonObject track_def : track_definitions) {
        int targetRow = -1;
        if (track_def["targetRow"].is<int>()) {
            targetRow = track_def["targetRow"].as<int>();
        } else if (track_def["targetRow"].is<const char*>()) {
            const char* rowStr = track_def["targetRow"].as<const char*>();
            if (strcmp(rowStr, "TOP") == 0) targetRow = 0;
            else if (strcmp(rowStr, "MIDDLE") == 0) targetRow = 1;
            else if (strcmp(rowStr, "BOTTOM") == 0) targetRow = 2;
        }
        if (targetRow < 0 || targetRow > 2 || tracks[targetRow].isActive) continue;
        JsonArray commands = track_def["commands"].as<JsonArray>();
        if (commands.isNull()) continue;
        int step_idx = 0;
        for (JsonObject command : commands) {
            const char* cmd_str = command["command"];
            if (!cmd_str) continue;
            SequenceCommand seq_cmd = SEQ_CMD_NONE;
            if (strcmp(cmd_str, "SET_TEXT") == 0) seq_cmd = SEQ_CMD_SET_TEXT;
            else if (strcmp(cmd_str, "CLEAR_SEGMENT") == 0) seq_cmd = SEQ_CMD_CLEAR_SEGMENT;
            else if (strcmp(cmd_str, "SET_BRIGHTNESS") == 0) seq_cmd = SEQ_CMD_SET_BRIGHTNESS;
            else if (strcmp(cmd_str, "RESTORE_ROW") == 0) seq_cmd = SEQ_CMD_RESTORE_ROW;
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
            else continue;
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

AnimationType animationTypeFromString(const char* str) {
    if (strcmp(str, "Intruder Alert") == 0) return ANIMATION_INTRUDER_ALERT;
    if (strcmp(str, "Time Travel") == 0) return ANIMATION_TIME_TRAVEL;
    if (strcmp(str, "Party Mode") == 0) return ANIMATION_PARTY_MODE;
    if (strcmp(str, "Countdown") == 0) return ANIMATION_COUNTDOWN;
    if (strcmp(str, "Knight Rider") == 0) return ANIMATION_KNIGHT_RIDER;
    if (strcmp(str, "Lightning") == 0) return ANIMATION_LIGHTNING;
    if (strcmp(str, "Loading") == 0) return ANIMATION_LOADING;
    if (strcmp(str, "Error") == 0) return ANIMATION_ERROR;
    if (strcmp(str, "Flux Capacitor Charge-Up") == 0) return ANIMATION_FLUX_CHARGE;
    if (strcmp(str, "Tachyons Detected") == 0) return ANIMATION_TACHYONS;
    if (strcmp(str, "Data Stream") == 0) return ANIMATION_DATA_STREAM;
    if (strcmp(str, "Wormhole Collapse") == 0) return ANIMATION_WORMHOLE_COLLAPSE;
    if (strcmp(str, "All Displays Random") == 0) return ANIMATION_ALL_DISPLAYS_RANDOM;
    if (strcmp(str, "Time Travel Tunnel") == 0) return ANIMATION_TIME_TRAVEL_TUNNEL;
    if (strcmp(str, "Fire Trails") == 0) return ANIMATION_FIRE_TRAILS;
    if (strcmp(str, "Sparkle Reveal") == 0) return ANIMATION_SPARKLE_REVEAL;
    if (strcmp(str, "Sequential Flicker") == 0) return ANIMATION_SEQUENTIAL_FLICKER;
    if (strcmp(str, "Random Flicker") == 0) return ANIMATION_RANDOM_FLICKER;
    if (strcmp(str, "Counting Up") == 0) return ANIMATION_COUNTING_UP;
    if (strcmp(str, "Wave Flicker") == 0) return ANIMATION_WAVE_FLICKER;
    if (strcmp(str, "Tornado Flicker") == 0) return ANIMATION_TORNADO_FLICKER;
    if (strcmp(str, "Capacitor Charge-Up") == 0) return ANIMATION_CAPACITOR_CHARGE_UP;
    if (strcmp(str, "Digital Rain") == 0) return ANIMATION_DIGITAL_RAIN;
    if (strcmp(str, "Waveform Collapse") == 0) return ANIMATION_WAVEFORM_COLLAPSE;
    if (strcmp(str, "Timeline Skim") == 0) return ANIMATION_TIMELINE_SKIM;
    if (strcmp(str, "Temporal Desync") == 0) return ANIMATION_TEMPORAL_DESYNC;
    if (strcmp(str, "Glitchy Jump-Cut") == 0) return ANIMATION_GLITCHY_JUMP_CUT;
    if (strcmp(str, "Plasma Warm-Up") == 0) return ANIMATION_PLASMA_WARM_UP;
    if (strcmp(str, "Time Warp Streaks") == 0) return ANIMATION_TIME_WARP_STREAKS;
    if (strcmp(str, "Character Scanline") == 0) return ANIMATION_CHARACTER_SCANLINE;
    if (strcmp(str, "Focus In") == 0) return ANIMATION_FOCUS_IN;
    if (strcmp(str, "Code Breaker") == 0) return ANIMATION_CODE_BREAKER;
    if (strcmp(str, "Temporal Paradox") == 0) return ANIMATION_TEMPORAL_PARADOX;
    if (strcmp(str, "Digit Cascade") == 0) return ANIMATION_DIGIT_CASCADE;
    if (strcmp(str, "Electric Surge") == 0) return ANIMATION_ELECTRIC_SURGE;
    if (strcmp(str, "Flip-Disc Display") == 0) return ANIMATION_FLIP_DISC_DISPLAY;
    if (strcmp(str, "Interference Pattern") == 0) return ANIMATION_INTERFERENCE_PATTERN;
    if (strcmp(str, "Randomize All") == 0) return ANIMATION_RANDOMIZE_ALL;
    return ANIMATION_RANDOMIZE_ALL;
}

const char* animationTypeToString(AnimationType type) {
    switch (type) {
        case ANIMATION_INTRUDER_ALERT: return "Intruder Alert";
        case ANIMATION_TIME_TRAVEL: return "Time Travel";
        case ANIMATION_PARTY_MODE: return "Party Mode";
        // ... all other cases
        default: return "Unknown Animation";
    }
}

void generateAnimationSequence(AnimationType animType, SequencerTrack tracks[3]) {
    if (animType == ANIMATION_RANDOMIZE_ALL) {
        do {
            static const AnimationType cpp_animations[] = {
                ANIMATION_ALL_DISPLAYS_RANDOM, ANIMATION_LIGHTNING, ANIMATION_SCANNER,
                ANIMATION_TIME_TRAVEL_TUNNEL, ANIMATION_FLUX_CAPACITOR_OVERLOAD, ANIMATION_FIRE_TRAILS,
                ANIMATION_SPARKLE_REVEAL, ANIMATION_SEQUENTIAL_FLICKER, ANIMATION_RANDOM_FLICKER,
                ANIMATION_TORNADO_FLICKER, ANIMATION_CAPACITOR_CHARGE_UP, ANIMATION_WAVEFORM_COLLAPSE,
                ANIMATION_TIMELINE_SKIM, ANIMATION_TEMPORAL_DESYNC, ANIMATION_GLITCHY_JUMP_CUT,
                ANIMATION_PLASMA_WARM_UP, ANIMATION_TIME_WARP_STREAKS, ANIMATION_CHARACTER_SCANLINE,
                ANIMATION_FOCUS_IN, ANIMATION_CODE_BREAKER, ANIMATION_TEMPORAL_PARADOX,
                ANIMATION_DIGIT_CASCADE, ANIMATION_ELECTRIC_SURGE, ANIMATION_FLIP_DISC_DISPLAY,
                ANIMATION_INTERFERENCE_PATTERN
            };
            int num_cpp_animations = sizeof(cpp_animations) / sizeof(cpp_animations[0]);
            animType = cpp_animations[random(num_cpp_animations)];
        } while (animType == ANIMATION_RANDOMIZE_ALL);
    }

    char time_strings[3][17];
    getFormattedTimeStrings(time_strings[0], time_strings[1], time_strings[2]);
    for (int i = 0; i < 3; ++i) { tracks[i].reset(); }

    switch (animType) {
        case ANIMATION_INTRUDER_ALERT:
            parseSequenceFromJson(tracks, R"([{"targetRow":"TOP","commands":[{"command":"SET_TEXT","stringParam":"INTRUDER ALERT"},{"command":"PULSE","intParam":250,"intParam2":10000}]},{"targetRow":"MIDDLE","commands":[{"command":"FLASH","intParam":10000}]},{"targetRow":"BOTTOM","commands":[{"command":"SET_TEXT","stringParam":"LOCKDOWN MODE"},{"command":"PULSE","intParam":250,"intParam2":10000}]}])");
            break;
        case ANIMATION_TIME_TRAVEL:
            parseSequenceFromJson(tracks, R"([{"targetRow":"TOP","commands":[{"command":"SOUND","stringParam":"time_travel.mp3"},{"command":"SCRAMBLE_TEXT","stringParam":"ACCELERATING","intParam":50,"intParam2":769}]},{"targetRow":"MIDDLE","commands":[{"command":"BAR_GRAPH","stringParam":"88 MPH","intParam":0,"intParam2":10000}]},{"targetRow":"BOTTOM","commands":[{"command":"FLASH","intParam":10000}]}])");
            break;
        case ANIMATION_PARTY_MODE:
            parseSequenceFromJson(tracks, R"([{"targetRow":"TOP","commands":[{"command":"PULSE","stringParam":"LETS","intParam":500,"intParam2":10000}]},{"targetRow":"MIDDLE","commands":[{"command":"PULSE","stringParam":"PARTY","intParam":500,"intParam2":10000}]},{"targetRow":"BOTTOM","commands":[{"command":"PULSE","stringParam":"HARD","intParam":500,"intParam2":10000}]}])");
            break;
        case ANIMATION_KNIGHT_RIDER:
            parseSequenceFromJson(tracks, R"([{"targetRow":0,"commands":[{"command":"SCANNER","intParam":10000,"intParam2":80,"stringParam":"---"}]},{"targetRow":1,"commands":[{"command":"SCANNER","intParam":10000,"intParam2":80,"stringParam":"---"}]},{"targetRow":2,"commands":[{"command":"SCANNER","intParam":10000,"intParam2":80,"stringParam":"---"}]}])");
            break;
        case ANIMATION_LOADING:
            parseSequenceFromJson(tracks, R"([{"targetRow":0,"commands":[{"command":"TYPEWRITER","stringParam":"LOADING","intParam":150},{"command":"WAIT","intParam":1000},{"command":"CLEAR_SEGMENT"}]},{"targetRow":1,"commands":[{"command":"WAIT","intParam":1000},{"command":"BAR_GRAPH","stringParam":"PLEASE WAIT","intParam":0,"intParam2":8000}]},{"targetRow":2,"commands":[{"command":"WAIT","intParam":9000},{"command":"SET_TEXT","stringParam":"COMPLETE"}]}])");
            break;
        case ANIMATION_ERROR:
            parseSequenceFromJson(tracks, R"([{"targetRow":"TOP","commands":[{"command":"SET_TEXT","stringParam":"ERROR"},{"command":"PULSE","intParam":250,"intParam2":10000}]},{"targetRow":"MIDDLE","commands":[{"command":"SCRAMBLE_TEXT","stringParam":"SYSTEM HALTED","intParam":50,"intParam2":700}]},{"targetRow":"BOTTOM","commands":[{"command":"FLASH","intParam":10000}]}])");
            break;
        case ANIMATION_FLUX_CHARGE:
            parseSequenceFromJson(tracks, R"([{"targetRow":0,"commands":[{"command":"RANDOM_FLICKER_TEXT","intParam":150,"intParam2":9500,"stringParam":"-.-"}]},{"targetRow":1,"commands":[{"command":"SOUND","stringParam":"flux_capacitor_power_on.mp3"},{"command":"BAR_GRAPH","stringParam":"CHARGE","intParam":0,"intParam2":9500}]},{"targetRow":2,"commands":[{"command":"PULSE","stringParam":"FLUX ENERGY","intParam":500,"intParam2":9500}]},{"targetRow":0,"commands":[{"command":"FLASH","intParam":500}]},{"targetRow":1,"commands":[{"command":"FLASH","intParam":500}]},{"targetRow":2,"commands":[{"command":"FLASH","intParam":500}]}])");
            break;
        case ANIMATION_TACHYONS:
            parseSequenceFromJson(tracks, R"([{"targetRow":0,"commands":[{"command":"SCROLL_IN","stringParam":"TACHYON","intParam":100},{"command":"WAIT","intParam":8700}]},{"targetRow":1,"commands":[{"command":"WAIT","intParam":1300},{"command":"SCROLL_IN","stringParam":"PULSE","intParam":100},{"command":"WAIT","intParam":7400}]},{"targetRow":2,"commands":[{"command":"WAIT","intParam":2600},{"command":"SCROLL_IN","stringParam":"DETECTED","intParam":100}]}])");
            break;
        case ANIMATION_DATA_STREAM:
            parseSequenceFromJson(tracks, R"([{"targetRow":0,"commands":[{"command":"RANDOM_FLICKER_TEXT","intParam":75,"intParam2":10000,"stringParam":"1010101010101"}]},{"targetRow":1,"commands":[{"command":"RANDOM_FLICKER_TEXT","intParam":100,"intParam2":10000,"stringParam":"ABCDE12345FGHI"}]},{"targetRow":2,"commands":[{"command":"RANDOM_FLICKER_TEXT","intParam":125,"intParam2":10000,"stringParam":"ZYXWV98765UTSR"}]}])");
            break;
        case ANIMATION_WORMHOLE_COLLAPSE:
            parseSequenceFromJson(tracks, R"([{"targetRow":0,"commands":[{"command":"SOUND","stringParam":"arrival_chime.mp3"},{"command":"RANDOM_FLICKER_TEXT","intParam":100,"intParam2":5000},{"command":"FADE_OUT","intParam":5000}]},{"targetRow":1,"commands":[{"command":"RANDOM_FLICKER_TEXT","intParam":100,"intParam2":5000},{"command":"WAIT","intParam":500},{"command":"FADE_OUT","intParam":4500}]},{"targetRow":2,"commands":[{"command":"RANDOM_FLICKER_TEXT","intParam":100,"intParam2":5000},{"command":"WAIT","intParam":1000},{"command":"FADE_OUT","intParam":4000}]}])");
            break;
        // C++ Generated Animations
        default:
            generateAllDisplaysRandom(tracks, time_strings);
            break;
    }

    int end_idx = 0;
    while(end_idx < MAX_SEQUENCE_STEPS && tracks[0].steps[end_idx].command != SEQ_CMD_NONE) { end_idx++; }
    if (end_idx < MAX_SEQUENCE_STEPS - 2) {
        end_idx = add_step(tracks[0], end_idx, SEQ_CMD_WAIT, 0, -1, 1000, 0);
        end_idx = add_step(tracks[0], end_idx, SEQ_CMD_RESTORE_ALL_ROWS, 0, -1, 0, 0);
    }
    for (int i = 0; i < 3; i++) {
        int track_end_idx = 0;
        while(track_end_idx < MAX_SEQUENCE_STEPS && tracks[i].steps[track_end_idx].command != SEQ_CMD_NONE) { track_end_idx++; }
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