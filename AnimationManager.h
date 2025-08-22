#ifndef ANIMATION_MANAGER_H
#define ANIMATION_MANAGER_H

#include "HardwareControl.h"

// Function Declarations for animations and effects
void startTimeTravelAnimation();
void handleDisplayAnimation();
void handleTemporalEcho();
void handleGlitchEffect();
void restoreDisplayAfterGlitch();
void handleMalfunction();
void runBootSequence();
void handleBootSequence();
void triggerTemporalGlitch();
void handleTemporalGlitch();
void triggerFlashEffect(int row, int segment, int duration = 500);
void handleFlashEffect();


#endif // ANIMATION_MANAGER_H