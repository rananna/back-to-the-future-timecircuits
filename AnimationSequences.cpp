#include "AnimationSequences.h"
#include "HardwareControl.h"
#include "DisplayManager.h"
#include <Arduino.h>
#include <string>

// Helper to add a step to a track safely, returns the new index
static int add_step(SequencerTrack& track, int step_idx, SequenceCommand cmd, int row, int seg, int p1, int p2, const char* s1 = "", const char* s2 = "") {
    if (step_idx < MAX_SEQUENCE_STEPS -1) {
        track.steps[step_idx] = {cmd, row, seg, p1, p2, s1, s2};
        return step_idx + 1;
    }
    return step_idx;
}

// --- Individual Animation Generators ---

void generateTornadoFlicker(SequencerTrack tracks[3]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_RANDOM_FILL, 0, -1, 10000, 50);
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FILL, 1, -1, 10000, 50);
    s2 = add_step(tracks[2], s2, SEQ_CMD_RANDOM_FILL, 2, -1, 10000, 50);
}

void generateAllDisplaysRandom(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 10000, 100, time_strings[0]);
    s1 = add_step(tracks[1], s1, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 10000, 100, time_strings[1]);
    s2 = add_step(tracks[2], s2, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 10000, 100, time_strings[2]);
}

void generateSequentialFlicker(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s = 0;
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
    int s = 0;
    s = add_step(tracks[0], s, SEQ_CMD_BAR_GRAPH, 2, -1, 3333, 250);
    s = add_step(tracks[0], s, SEQ_CMD_BAR_GRAPH, 1, -1, 3333, 250);
    s = add_step(tracks[0], s, SEQ_CMD_BAR_GRAPH, 0, -1, 3334, 250);
}

void generateWaveformCollapse(SequencerTrack tracks[3]) {
    const char* waves[] = {"-------------", " ---     --- ", "  ---   ---  ", "   -------   ", "  ---   ---  ", " ---     --- "};
    int s0 = 0, s1 = 0, s2 = 0;
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

void generateCodeBreaker(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCRAMBLE_TEXT, 0, -1, 50, 100, time_strings[0]);
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCRAMBLE_TEXT, 1, -1, 50, 100, time_strings[1]);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCRAMBLE_TEXT, 2, -1, 50, 100, time_strings[2]);
}

void generateFlipDisc(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_WIPE, 0, -1, 75, 0, time_strings[0]);
    s1 = add_step(tracks[1], s1, SEQ_CMD_WIPE, 1, -1, 75, 0, time_strings[1]);
    s2 = add_step(tracks[2], s2, SEQ_CMD_WIPE, 2, -1, 75, 0, time_strings[2]);
}

void generateCharacterScanline(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0 = 0, s1 = 0, s2 = 0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_TYPEWRITER, 0, -1, 75, 0, time_strings[0]);
    s1 = add_step(tracks[1], s1, SEQ_CMD_TYPEWRITER, 1, -1, 75, 0, time_strings[1]);
    s2 = add_step(tracks[2], s2, SEQ_CMD_TYPEWRITER, 2, -1, 75, 0, time_strings[2]);
}

void generateTemporalParadox(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s = 0;
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 25, 0);
    s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 0, -1, 0, 0, time_strings[1]);
    s = add_step(tracks[0], s, SEQ_CMD_SET_TEXT, 1, -1, 0, 0, time_strings[0]);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FILL, 2, -1, 200, 50);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 200, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RESTORE_ROW, 0, -1, 0, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RESTORE_ROW, 1, -1, 0, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FILL, 2, -1, 200, 50);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 200, 0);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
}

void generateInterferencePattern(SequencerTrack tracks[3], const char time_strings[3][17]) {
    generateAllDisplaysRandom(tracks, time_strings);
}

void generateTimeWarpStreaks(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0=0, s1=0, s2=0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_SCROLL_IN, 0, -1, 75, 0, time_strings[0]);
    s1 = add_step(tracks[1], s1, SEQ_CMD_SCROLL_IN, 1, -1, 75, 0, time_strings[1]);
    s2 = add_step(tracks[2], s2, SEQ_CMD_SCROLL_IN, 2, -1, 75, 0, time_strings[2]);
}

void generateFocusIn(SequencerTrack tracks[3], const char time_strings[3][17]) {
    generateCodeBreaker(tracks, time_strings);
}

void generateElectricSurge(SequencerTrack tracks[3], const char time_strings[3][17]) {
    generateFlipDisc(tracks, time_strings);
}

void generateDigitCascade(SequencerTrack tracks[3], const char time_strings[3][17]) {
    int s0=0, s1=0, s2=0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_COUNTDOWN, 0, 2, 9999, 1);
    s1 = add_step(tracks[1], s1, SEQ_CMD_COUNTDOWN, 1, 2, 9999, 1);
    s2 = add_step(tracks[2], s2, SEQ_CMD_COUNTDOWN, 2, 2, 9999, 1);
}

void generatePlasmaWarmup(SequencerTrack tracks[3]) {
    int s = 0;
    s = add_step(tracks[0], s, SEQ_CMD_FADE_IN, 0, -1, 5000, 0);
    s = add_step(tracks[0], s, SEQ_CMD_FADE_OUT, 0, -1, 5000, 0);
}

void generateGlitchyJumpCut(SequencerTrack tracks[3]) {
    int s=0;
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 20, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FILL, 0, -1, 200, 50);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FILL, 1, -1, 200, 50);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FILL, 2, -1, 200, 50);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 200, 0);
    s = add_step(tracks[0], s, SEQ_CMD_FLASH, 0, -1, 100, 0);
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 100, 0);
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
}

void generateCountingUp(SequencerTrack tracks[3]) {
    int s0=0, s1=0, s2=0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_COUNTDOWN, 0, -1, 99999999, 1);
    s1 = add_step(tracks[1], s1, SEQ_CMD_COUNTDOWN, 1, -1, 99999999, 1);
    s2 = add_step(tracks[2], s2, SEQ_CMD_COUNTDOWN, 2, -1, 99999999, 1);
}

void generateTimelineSkim(SequencerTrack tracks[3]) {
    generateTornadoFlicker(tracks); // Fallback for a complex animation
}

void generateTemporalDesync(SequencerTrack tracks[3]) {
    int s0=0, s1=0, s2=0;
    s0 = add_step(tracks[0], s0, SEQ_CMD_COUNTDOWN, 0, -1, 99999999, 100);
    s1 = add_step(tracks[1], s1, SEQ_CMD_COUNTDOWN, 1, -1, 99999999, 50);
    s2 = add_step(tracks[2], s2, SEQ_CMD_COUNTDOWN, 2, -1, 99999999, 200);
}

void generateDigitalRain(SequencerTrack tracks[3]) {
    int s=0;
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_START, 0, 0, 100, 0);
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 0, -1, 100, 50, "                ");
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 1, -1, 100, 50, "                ");
    s = add_step(tracks[0], s, SEQ_CMD_RANDOM_FLICKER_TEXT, 2, -1, 100, 50, "                ");
    s = add_step(tracks[0], s, SEQ_CMD_LOOP_END, 0, 0, 0, 0);
}

// --- Main Generation Function ---

void generateAnimationSequence(AnimationType animType, SequencerTrack tracks[3]) {
    char time_strings[3][17];
    getFormattedTimeStrings(time_strings[0], time_strings[1], time_strings[2]);

    for (int i = 0; i < 3; ++i) {
        tracks[i].reset();
    }

    int s = 0;
    s = add_step(tracks[0], s, SEQ_CMD_SOUND, 0, 0, 0, 0, "/electric_sparks.mp3");
    s = add_step(tracks[0], s, SEQ_CMD_WAIT, 0, 0, 1000, 0);

    switch (animType) {
        case ANIMATION_SEQUENTIAL_FLICKER:      generateSequentialFlicker(tracks, time_strings); break;
        case ANIMATION_TORNADO_FLICKER:         generateTornadoFlicker(tracks); break;
        case ANIMATION_ALL_DISPLAYS_RANDOM:     generateAllDisplaysRandom(tracks, time_strings); break;
        case ANIMATION_CAPACITOR_CHARGE_UP:     generateCapacitorChargeUp(tracks); break;
        case ANIMATION_WAVEFORM_COLLAPSE:
        case ANIMATION_WAVE_FLICKER:            generateWaveformCollapse(tracks); break;
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
        case ANIMATION_TIMELINE_SKIM:           generateTimelineSkim(tracks); break;
        case ANIMATION_TEMPORAL_DESYNC:         generateTemporalDesync(tracks); break;
        case ANIMATION_DIGITAL_RAIN:            generateDigitalRain(tracks); break;
        case ANIMATION_RANDOM_FLICKER:
        default:
            generateTornadoFlicker(tracks);
            break;
    }

    for (int i = 0; i < 3; i++) {
        int end_idx = 0;
        while(end_idx < MAX_SEQUENCE_STEPS && tracks[i].steps[end_idx].command != SEQ_CMD_NONE) {
            end_idx++;
        }
        if (end_idx < MAX_SEQUENCE_STEPS - 2) {
            end_idx = add_step(tracks[i], end_idx, SEQ_CMD_WAIT, i, -1, 1000, 0);
            end_idx = add_step(tracks[i], end_idx, SEQ_CMD_RESTORE_ROW, i, -1, 0, 0);
        }
        add_step(tracks[i], end_idx, SEQ_CMD_END, i, 0, 0, 0);
    }

    for (int i = 0; i < 3; ++i) {
        if (tracks[i].steps[0].command != SEQ_CMD_NONE) {
            tracks[i].isActive = true;
            tracks[i].trackStartTime = millis();
            tracks[i].stepStartTime = millis();
        }
    }
}