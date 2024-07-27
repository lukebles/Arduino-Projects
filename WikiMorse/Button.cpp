#include "Button.h"

Button::Button(uint8_t pin) {
  _pin = pin;
  // pinMode(_pin, INPUT);
  _currentState = digitalRead(_pin);
  _previousState = _currentState;
  _lastDebounceTime = 0;
  _debounceDelay = 25;
  _pressTime = 0;
  _releaseTime = 0;
}

void Button::update() {
  bool reading = digitalRead(_pin);

  if (reading != _previousState) {
    _lastDebounceTime = millis();
  }

  if ((millis() - _lastDebounceTime) > _debounceDelay) {
    if (reading != _currentState) {
      _currentState = reading;

      if (_currentState == HIGH) {
        _pressTime = millis();
        _releaseTime = 0;
      } else {
        _releaseTime = millis();
        _pressTime = 0;
      }
    }
  }

  _previousState = reading;
}

bool Button::isPressed() {
  return (_currentState == HIGH);
}

bool Button::isReleased() {
  return (_currentState == LOW);
}

bool Button::wasPressed() {
  return (_currentState == HIGH && _previousState == LOW);
}

bool Button::wasReleased() {
  return (_currentState == LOW && _previousState == HIGH);
}

unsigned long Button::pressedDuration() {
  if (_currentState == HIGH) {
    return millis() - _pressTime;
  } else {
    return 0;
  }
}

unsigned long Button::releasedDuration() {
  if (_currentState == LOW) {
    return millis() - _releaseTime;
  } else {
    return 0;
  }
}
