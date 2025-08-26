#ifndef HARDWARE_CONTROL_H
#define HARDWARE_CONTROL_H

#include <time.h>
#include "types.h"
#include <Adafruit_LEDBackpack.h>
#include <Wire.h>

// Forward declarations
struct tm;

// Added missing extern declarations for display rows
extern DisplayRow destRow;
extern DisplayRow presRow;
extern DisplayRow lastRow;

void setupPhysicalDisplay();
void animateDisplayRowRandomly(DisplayRow& row);
void animateTemporalLockOn(DisplayRow& row, const struct tm& timeinfo, int year);
void printToDisplay(Adafruit_AlphaNum4 &display, const char* text, int justification = 0);
void playSound(const char* filepath);
void blankAllDisplays();
void displaySpeed(int speed);
void animateAllRowsTimelineSkim(unsigned long elapsed, int duration, int destinationYear);
void flashAllDisplays();
void animateTornadoFlicker();
void animateCapacitorChargeUp(unsigned long elapsed, int duration);

#endif // HARDWARE_CONTROL_H