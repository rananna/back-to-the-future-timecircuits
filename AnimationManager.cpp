#include "AnimationManager.h"
#include "EventManager.h"
#include "DisplayManager.h" // For updateNormalClockDisplay

// Structs to hold time info specifically for the hardcoded time travel animation sequence.
struct tm realDepartureTimeInfo; // The actual time the animation starts
struct tm animDestTimeInfo;      // Hardcoded movie destination time (Nov 05, 1955)
struct tm animPresTimeInfo;      // Hardcoded movie present time (Oct 26, 1985)
struct tm animLastTimeInfo;      // Hardcoded movie last departed time (Oct 26, 1985)

/**
 * @brief Kicks off the main time travel visual and audio sequence.
 */
void startTimeTravelAnimation() {
    if (isAnimating) {
        return;
    }
    isAnimating = true;
    animationStartTime = millis();
    
    // Capture the REAL current time as the departure time for saving later.
    time_t now;
    time(&now);
    setenv("TZ", TZ_DATA[currentSettings.presentTimezoneIndex].tzString, 1);
    tzset();
    localtime_r(&now, &realDepartureTimeInfo);

    // Set up the hardcoded iconic movie dates for the animation visuals.
    animDestTimeInfo = {0};
    animDestTimeInfo.tm_year = 1955 - 1900; animDestTimeInfo.tm_mon = 10; animDestTimeInfo.tm_mday = 5;
    animDestTimeInfo.tm_hour = 6; animDestTimeInfo.tm_min = 0;

    animPresTimeInfo = {0};
    animPresTimeInfo.tm_year = 1985 - 1900; animPresTimeInfo.tm_mon = 9; animPresTimeInfo.tm_mday = 26;
    animPresTimeInfo.tm_hour = 1; animPresTimeInfo.tm_min = 21;

    animLastTimeInfo = {0};
    animLastTimeInfo.tm_year = 1985 - 1900; animLastTimeInfo.tm_mon = 9; animLastTimeInfo.tm_mday = 26;
    animLastTimeInfo.tm_hour = 1; animLastTimeInfo.tm_min = 20;

    currentPhase = ANIM_POWER_UP;
    #if ENABLE_HARDWARE
    if (currentSettings.timeTravelSoundToggle) {
        playSound("FLUX_CAPACITOR_CHARGE");
    }
    #endif
}

/**
 * @brief The main state machine for handling the multi-phase time travel animation.
 */
void handleDisplayAnimation() {
  #if ENABLE_HARDWARE
  if (!isAnimating) return;
  unsigned long currentTime = millis();
  unsigned long elapsed = currentTime - animationStartTime;

  const int ACCELERATION_DURATION = 4000;
  const int WHITE_FLASH_DURATION = 150;
  const int FLICKER_DURATION = 1000;
  const int TIME_BLUR_DURATION = 2000;
  const int ARRIVAL_ECHO_DURATION = 300;
  const int TOTAL_DURATION = ACCELERATION_DURATION + WHITE_FLASH_DURATION + FLICKER_DURATION + TIME_BLUR_DURATION + ARRIVAL_ECHO_DURATION;

  switch (currentPhase) {
    case ANIM_POWER_UP:
      if (currentTime - lastAnimationFrameTime > 50) { 
          float progress = (float)elapsed / ACCELERATION_DURATION;
          int speed = 88 * pow(progress, 2.5); 
          if (speed > 88) speed = 88;
          
          displaySpeed(speed);
          
          animateTemporalLockOn(destRow, animDestTimeInfo, 1955);
          animateTemporalLockOn(presRow, animPresTimeInfo, 1985);
          
          lastAnimationFrameTime = currentTime;
      }
      if (elapsed >= ACCELERATION_DURATION) {
          flashAllDisplays();
          delay(WHITE_FLASH_DURATION);
          currentPhase = ANIM_FLICKER;
          if(currentSettings.timeTravelSoundToggle) playSound("ACCELERATION");
      }
      break;

    case ANIM_FLICKER:
      if (currentTime - lastAnimationFrameTime > 50) {
          animateDisplayRowRandomly(destRow);
          animateDisplayRowRandomly(presRow);
          animateDisplayRowRandomly(lastRow);
          lastAnimationFrameTime = currentTime;
      }
      if (elapsed >= (ACCELERATION_DURATION + WHITE_FLASH_DURATION + FLICKER_DURATION)) {
          currentPhase = ANIM_TIME_ACCELERATION;
      }
      break;

    case ANIM_TIME_ACCELERATION:
      if (currentTime - lastAnimationFrameTime > 50) {
          unsigned long time_blur_elapsed = elapsed - (ACCELERATION_DURATION + WHITE_FLASH_DURATION + FLICKER_DURATION);
          animateAllRowsTimelineSkim(time_blur_elapsed, TIME_BLUR_DURATION, 1955);
          lastAnimationFrameTime = currentTime;
      }
      if (elapsed >= (ACCELERATION_DURATION + WHITE_FLASH_DURATION + FLICKER_DURATION + TIME_BLUR_DURATION)) {
          currentPhase = ANIM_ARRIVAL;
      }
      break;

    case ANIM_ARRIVAL:
      updateDisplayRow(presRow, animDestTimeInfo, 1955);
      if(currentSettings.timeTravelSoundToggle) playSound("ARRIVAL_THUD");
      currentPhase = ANIM_LANDING;
      break;

    case ANIM_LANDING:
      if (elapsed >= TOTAL_DURATION) {
          currentSettings.lastTimeDepartedYear = realDepartureTimeInfo.tm_year + 1900;
          currentSettings.lastTimeDepartedMonth = realDepartureTimeInfo.tm_mon + 1;
          currentSettings.lastTimeDepartedDay = realDepartureTimeInfo.tm_mday;
          currentSettings.lastTimeDepartedHour = realDepartureTimeInfo.tm_hour;
          currentSettings.lastTimeDepartedMinute = realDepartureTimeInfo.tm_min;
          
          isAnimating = false;
          currentPhase = ANIM_INACTIVE;
          updateNormalClockDisplay();
          
          isEchoEffectActive = true;
          echoEffectStartTime = millis();
          lastEchoCheckTime = millis();
      }
      break;
      
    case ANIM_INACTIVE:
      break;
    }
  #endif
}

/**
 * @brief Handles a post-time-travel effect where the "Present Time" display occasionally flickers to show the "Last Time Departed".
 */
void handleTemporalEcho() {
  #if ENABLE_HARDWARE
  if (!isEchoEffectActive) {
    return;
  }

  if (millis() - echoEffectStartTime > 180000) {
    isEchoEffectActive = false;
    isFlickeringNow = false;
    return;
  }

  if (isFlickeringNow) {
    if (millis() - flickerStartTime > 150) {
      isFlickeringNow = false;
      flickerDisplayIndex = -1;
      updateNormalClockDisplay();
    }
    return;
  }

  if (millis() - lastEchoCheckTime > 10000) {
    lastEchoCheckTime = millis();
    if (random(100) < 25) {
      isFlickeringNow = true;
      flickerStartTime = millis();
      flickerDisplayIndex = random(4);

      struct tm lastTimeDepartedInfo = {0};
      lastTimeDepartedInfo.tm_year = currentSettings.lastTimeDepartedYear - 1900;
      lastTimeDepartedInfo.tm_mon = currentSettings.lastTimeDepartedMonth - 1;
      lastTimeDepartedInfo.tm_mday = currentSettings.lastTimeDepartedDay;
      lastTimeDepartedInfo.tm_hour = currentSettings.lastTimeDepartedHour;
      lastTimeDepartedInfo.tm_min = currentSettings.lastTimeDepartedMinute;
      const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
      char buffer[5];

      switch (flickerDisplayIndex) {
        case 0:
          printToDisplay(presRow.month, months[lastTimeDepartedInfo.tm_mon], 1);
          presRow.month.writeDisplay();
          break;
        case 1:
          sprintf(buffer, "%02d", lastTimeDepartedInfo.tm_mday);
          printToDisplay(presRow.day, buffer, 2);
          presRow.day.writeDisplay();
          break;
        case 2:
          sprintf(buffer, "%04d", currentSettings.lastTimeDepartedYear);
          printToDisplay(presRow.year, buffer);
          presRow.year.writeDisplay();
          break;
        case 3:
          char timeBuffer[5];
          sprintf(timeBuffer, "%02d%02d", lastTimeDepartedInfo.tm_hour, lastTimeDepartedInfo.tm_min);
          presRow.time.clear();
          presRow.time.writeDigitAscii(0, timeBuffer[0]);
          presRow.time.writeDigitAscii(1, timeBuffer[1] | 0x80); // Add decimal point
          presRow.time.writeDigitAscii(2, timeBuffer[2]);
          presRow.time.writeDigitAscii(3, timeBuffer[3]);
          presRow.time.writeDisplay();
          break;
      }
    }
  }
  #endif
}

/**
 * @brief Handles the state machine for a major "malfunction" visual effect.
 */
void handleMalfunction() {
  #if ENABLE_HARDWARE
  if (!isMalfunctioning) return;
  unsigned long elapsed = millis() - malfunctionStartTime;
  switch (currentMalfunctionPhase) {
    case MAL_HAYWIRE:
      if (elapsed < 3000) {
        if (millis() - lastAnimationFrameTime > 100) {
          printToDisplay(destRow.month, "888", 1);
          printToDisplay(destRow.day, "88", 2);
          printToDisplay(destRow.year, "8888");
          printToDisplay(destRow.time, "8888");
          destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();
          lastAnimationFrameTime = millis();
        }
      } else {
        malfunctionStartTime = millis();
        currentMalfunctionPhase = MAL_ERROR_MESSAGE;
      }
      break;
    case MAL_ERROR_MESSAGE:
      if (elapsed < 4000) {
        printToDisplay(destRow.month, "TIM", 1);
        printToDisplay(destRow.day, "CI", 2); printToDisplay(destRow.year, "RCUT"); printToDisplay(destRow.time, "OVER");
        printToDisplay(presRow.month, "LOA", 1); printToDisplay(presRow.day, "D", 2); presRow.year.clear(); presRow.time.clear();
        printToDisplay(lastRow.month, "FLX", 1);
        printToDisplay(lastRow.day, "OF", 2); printToDisplay(lastRow.year, "FLIN"); lastRow.time.clear();
        destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();
        presRow.month.writeDisplay(); presRow.day.writeDisplay(); presRow.year.writeDisplay(); presRow.time.writeDisplay();
        lastRow.month.writeDisplay(); lastRow.day.writeDisplay(); lastRow.year.writeDisplay(); lastRow.time.writeDisplay();
      } else {
        malfunctionStartTime = millis();
        currentMalfunctionPhase = MAL_REBOOT;
      }
      break;
    case MAL_REBOOT:
      blankAllDisplays();
      runBootSequence();
      break;
    case MAL_INACTIVE:
      break;
  }
  #endif
}

/**
 * @brief Kicks off the boot sequence animation.
 */
void runBootSequence() {
  bootState = BOOT_START;
  bootStateStartTime = millis();
}

/**
 * @brief Handles the state machine for the boot sequence animation.
 */
void handleBootSequence() {
  if (bootState == BOOT_INACTIVE || bootState == BOOT_COMPLETE) return;
  unsigned long elapsed = millis() - bootStateStartTime;
  if (elapsed > 2000) {
    bootState = static_cast<BootSequenceState>(bootState + 1);
    bootStateStartTime = millis();
    if (bootState >= BOOT_COMPLETE) {
      bootState = BOOT_COMPLETE;
      updateNormalClockDisplay();
      return;
    }
  }
  #if ENABLE_HARDWARE
  switch (bootState) {
    case BOOT_88MPH:
      // (Handled by default display state on boot)
      break;
    case BOOT_RECALIBRATING:
      printToDisplay(destRow.month, "REC", 1); printToDisplay(destRow.day, "AL", 2); printToDisplay(destRow.year, "IBRA"); printToDisplay(destRow.time, "TING");
      destRow.month.writeDisplay(); destRow.day.writeDisplay(); destRow.year.writeDisplay(); destRow.time.writeDisplay();
      break;
    case BOOT_CAPACITOR:
      printToDisplay(presRow.month, "CAP", 1); printToDisplay(presRow.day, "AC", 2); printToDisplay(presRow.year, "ITOR"); printToDisplay(presRow.time, "FULL");
      presRow.month.writeDisplay();
      presRow.day.writeDisplay(); presRow.year.writeDisplay(); presRow.time.writeDisplay();
      break;
    default:
      break;
  }
  #endif
}

/**
 * @brief Restores the normal clock display after a short glitch effect is finished.
 */
void restoreDisplayAfterGlitch() {
  if (isGlitching && millis() - glitchStartTime > 150) { // Glitch duration.
    updateNormalClockDisplay();
    isGlitching = false;
  }
}

/**
 * @brief Periodically triggers random glitch or malfunction effects based on configured probability.
 */
void handleGlitchEffect() {
  if (isAnimating || isDisplayAsleep || isGlitching || isMalfunctioning || currentSettings.glitchEffectFrequency == 0) return;
  
  if (millis() - lastGlitchTime > 60000) {
    lastGlitchTime = millis();
    if (random(100) < currentSettings.glitchEffectFrequency) {
      if (currentSettings.malfunctionFrequency > 0 && random(currentSettings.malfunctionFrequency) == 0) {
        isMalfunctioning = true;
        malfunctionStartTime = millis();
        currentMalfunctionPhase = MAL_HAYWIRE;
      } else {
        isGlitching = true;
        glitchStartTime = millis();
        #if ENABLE_HARDWARE
        animateDisplayRowRandomly(destRow);
        animateDisplayRowRandomly(presRow);
        animateDisplayRowRandomly(lastRow);
        #endif
      }
    }
  }
}