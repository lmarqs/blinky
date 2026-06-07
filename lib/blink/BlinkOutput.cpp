#include "BlinkOutput.h"

const char* BlinkOutput::name() const { return on_ ? "on" : "off"; }
