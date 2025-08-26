#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include "types.h"

void handleFlashEffect();
void handleBootSequence();
void handleMalfunction();
void restoreDisplayAfterGlitch();
void handleTemporalEcho();
void handleGlitchEffect();
void handleDisplayAnimation();
void handleTemporalGlitch();
void triggerFlashEffect(int row, int segment, int duration);
void triggerTemporalGlitch();
void runBootSequence();
void updateHaStatus(const char* status);
void triggerMalfunction(MalfunctionPhase phase);

extern AnimationPhase currentPhase;
extern BootSequenceState bootState;
extern MarqueeState marqueeState;

#endif // EVENT_MANAGER_H