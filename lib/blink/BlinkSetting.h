#pragma once

#include <stdint.h>

#include "BlinkController.h"

// Mode <-> wire/UI name mapping.
const char* blinkModeName(BlinkMode mode);
bool parseBlinkMode(const char* name, BlinkMode& out);

// Persisted setting record. The byte layout is what gets stored
// (e.g. in EEPROM); magic/version guard against stale or foreign data.
struct BlinkSetting {
  uint8_t magic;
  uint8_t version;
  uint8_t mode;
  uint32_t periodMs;

  static constexpr uint8_t MAGIC = 0xB1;
  static constexpr uint8_t VERSION = 1;
};

// Snapshot a controller's persistable state.
BlinkSetting blinkSettingFrom(const BlinkController& controller);

// Validate and apply a stored record; on rejection the controller is
// left untouched and false is returned.
bool applyBlinkSetting(const BlinkSetting& setting, BlinkController& controller);
