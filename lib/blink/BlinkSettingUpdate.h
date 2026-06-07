#pragma once

#include <stddef.h>
#include <stdint.h>

#include "BlinkController.h"

// Applying a Remote's request to the live Setting, and reporting it back.
// Pure (no Arduino includes): the raw request comes in as a C string and the
// clock as a parameter, so the parse -> validate -> apply -> serialize logic
// runs identically on the device and in native tests. The transport (HTTP
// status codes, the EEPROM write, the GPIO drive) stays in src/main.cpp.

// The result of applying a request. Accepted means the Setting changed and
// should be persisted + actuated; Unchanged means the request was valid but a
// no-op; Rejected means the request was malformed and nothing was applied.
enum class BlinkOutcome { Accepted, Rejected, Unchanged };

// Apply a mode request. arg is the raw name ("blink"|"on"|"off"). On Accepted
// the controller is advanced (update(now)), so output() and statusJson() are
// current before the caller actuates.
BlinkOutcome applyMode(BlinkController& controller, const char* arg, uint32_t now);

// Apply a period request. arg is the raw string; it must be a positive integer
// (no trailing garbage). The value is clamped by the controller, so a valid but
// out-of-range request still Accepts at the clamped period.
BlinkOutcome applyPeriod(BlinkController& controller, const char* arg, uint32_t now);

// Serialize the current Setting + lamp as the status JSON, writing up to n bytes.
void statusJson(const BlinkController& controller, char* buf, size_t n);
