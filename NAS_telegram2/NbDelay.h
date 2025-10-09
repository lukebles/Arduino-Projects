#pragma once
#include <Arduino.h>

class NbDelay {
  uint32_t target = 0;
  bool waiting = false;
public:
  // ritorna true quando il tempo è scaduto (una sola volta)
  bool wait(uint32_t ms) {
    if (!waiting) { target = millis() + ms; waiting = true; return false; }
    if ((int32_t)(millis() - target) >= 0) { waiting = false; return true; }
    return false;
  }
  void reset() { waiting = false; }
};
