#include "BlinkSettingUpdate.h"

#include <stdio.h>
#include <stdlib.h>

#include "BlinkSetting.h"

BlinkOutcome applyMode(BlinkController& controller, const char* arg, uint32_t now) {
  BlinkMode mode;
  if (!parseBlinkMode(arg, mode)) {
    return BlinkOutcome::Rejected;
  }
  if (mode == controller.mode()) {
    return BlinkOutcome::Unchanged;
  }
  controller.setMode(mode);
  controller.update(now);
  return BlinkOutcome::Accepted;
}

BlinkOutcome applyPeriod(BlinkController& controller, const char* arg, uint32_t now) {
  char* end;
  const long requested = strtol(arg, &end, 10);
  // Reject empty, non-numeric, trailing-garbage, and non-positive input.
  if (end == arg || *end != '\0' || requested <= 0) {
    return BlinkOutcome::Rejected;
  }

  const uint32_t before = controller.period();
  controller.setPeriod(static_cast<uint32_t>(requested));  // clamps
  if (controller.period() == before) {
    return BlinkOutcome::Unchanged;
  }
  controller.update(now);
  return BlinkOutcome::Accepted;
}

void statusJson(const BlinkController& controller, char* buf, size_t n) {
  snprintf(buf, n, "{\"mode\":\"%s\",\"period\":%lu,\"lamp\":%s}", blinkModeName(controller.mode()),
           static_cast<unsigned long>(controller.period()), controller.output().isOn() ? "true" : "false");
}
