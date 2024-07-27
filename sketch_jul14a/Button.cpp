#include "Button.h"

Button::Button(uint8_t pin) {
  _pin = pin;
  pinMode(_pin, INPUT_PULLUP);
  _currentState = digitalRead(_pin);
  _previousState = _currentState;
  _lastDebounceTime = 0;
  _debounceDelay = 25;
  _pressTime = 0;
  _releaseTime = 0;
  _stateChanged = false;  
  _waspres = false;       
  _wasrel = false;       
}

void Button::update() {
  bool reading = digitalRead(_pin);

  if (reading != _previousState) {
    _lastDebounceTime = millis();
  }

  if ((millis() - _lastDebounceTime) > _debounceDelay) {
    if (reading != _currentState) {
      _previousState = _currentState; 
      _currentState = reading;
      _stateChanged = true;  

      if (_currentState == LOW) {
        // TASTO PREMUTO
        _pressTime = millis();
        _waspres = true;
        _wasrel = false;
      } else {
        // TASTO RILASCIATO
        _releaseTime = millis();
        _waspres = false;
        _wasrel = true;
      }
    } else {
      _stateChanged = false;  
    }
  }

  _previousState = reading;  
}

bool Button::isPressed() {
  return (_currentState == LOW);
}

bool Button::isReleased() {
  return (_currentState == HIGH);
}

bool Button::wasPressed() {
  if (stateHasChanged()){
    return _waspres;
  } else {
    return false;
  }
}

bool Button::wasReleased() {
  if (stateHasChanged()){
    return _wasrel;
  } else {
    return false;
  }
}

unsigned long Button::pressedDuration() {
    return millis() - _pressTime;
}

unsigned long Button::releasedDuration() {
    return millis() - _releaseTime;
}

bool Button::stateHasChanged() {
  return _stateChanged;
}
