#pragma once
#include "config.h"
#include <ArduinoJson.h>
bool loadSettings(Settings &settings);
bool saveSettings(const Settings &settings);
bool validSettings(const Settings &settings);
bool updateWifiProfiles(Settings &next, const Settings &previous, JsonArrayConst profiles);
