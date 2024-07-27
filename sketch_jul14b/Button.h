#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

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
    bool stateHasChanged();

  private:
    uint8_t _pin;
    bool _currentState;
    bool _previousState;
    unsigned long _lastDebounceTime;
    unsigned long _debounceDelay;
    unsigned long _pressTime;
    unsigned long _releaseTime;
    bool _stateChanged;
    bool _waspres;
    bool _wasrel;
};

#endif
