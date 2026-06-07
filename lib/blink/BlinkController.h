#pragma once

#include <stdint.h>

#include "BlinkOutput.h"

// Lamp behavior. OFF/ON force the output; BLINK toggles it every period.
enum class BlinkMode : uint8_t {
  OFF = 0,
  ON = 1,
  BLINK = 2,
};

// Pure blink state machine. No Arduino dependencies: the clock is passed
// into update() so it runs identically on the device and in native tests.
// Timing uses unsigned subtraction, so it survives the 32-bit millis()
// rollover (~49.7 days).
class BlinkController {
 public:
  static constexpr uint32_t PERIOD_MIN_MS = 200;
  static constexpr uint32_t PERIOD_MAX_MS = 3600000;  // 1 hour
  static constexpr uint32_t PERIOD_DEFAULT_MS = 5000;
  static constexpr BlinkMode MODE_DEFAULT = BlinkMode::BLINK;

  void setMode(BlinkMode mode);
  void setPeriod(uint32_t periodMs);  // clamped to [PERIOD_MIN_MS, PERIOD_MAX_MS]

  // Apply persisted settings atomically (e.g. from EEPROM at boot).
  void restore(BlinkMode mode, uint32_t periodMs);

  BlinkMode mode() const { return mode_; }
  uint32_t period() const { return periodMs_; }
  BlinkOutput output() const { return BlinkOutput{output_}; }  // current lamp state

  // Advance the state machine; returns true when output() changed.
  // In BLINK mode the first call establishes the timing baseline
  // instead of toggling immediately.
  bool update(uint32_t nowMs);

 private:
  static uint32_t clampPeriod(uint32_t periodMs);

  BlinkMode mode_ = MODE_DEFAULT;
  uint32_t periodMs_ = PERIOD_DEFAULT_MS;
  bool output_ = false;
  bool started_ = false;
  uint32_t lastToggleMs_ = 0;
};
