#ifndef Button_h
#define Button_h

#include "Arduino.h"

class Button {
  public:
    Button(uint8_t pin);
    void update();
    bool isPressed();
    bool isReleased();
    bool wasPressed();
    bool wasReleased();
    unsigned long pressedDuration();
    unsigned long releasedDuration();

  private:
    uint8_t _pin;
    bool _currentState;
    bool _previousState;
    unsigned long _lastDebounceTime;
    unsigned long _debounceDelay;
    unsigned long _pressTime;
    unsigned long _releaseTime;
};

#endif
