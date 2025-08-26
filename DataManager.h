#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "types.h"
#include <Preferences.h>

// Function declarations
void saveSettings();
void loadSettings();
void resetToDefaults();
void applyJsonToSettings(const String& jsonString);

extern ClockSettings currentSettings;
extern Preferences preferences;

#endif // DATAMANAGER_H