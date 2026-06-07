#pragma once

// The lamp's on/off state as a domain value rather than a bare bool, so the
// on/off vocabulary lives in one host-tested place (cf. blinkModeName in
// BlinkSetting). Pure and Arduino-free, like the rest of lib/blink.
class BlinkOutput {
 public:
  constexpr explicit BlinkOutput(bool on) : on_(on) {}

  constexpr bool isOn() const { return on_; }
  const char* name() const;  // "on" | "off"

 private:
  bool on_;
};
