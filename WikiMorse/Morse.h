#ifndef Morse_h
#define Morse_h

#include "Arduino.h"
#include "Button.h"

class Morse {
  public:
    Morse(uint8_t pin);
    void update();
    String getChar();
    bool hasNewChar();

  private:
    Button _button;
    String _morseCode;
    String _currentChar;
    bool _newChar;
    unsigned long _dotDuration;
    unsigned long _dashDuration;
    unsigned long _charGapDuration;
    unsigned long _wordGapDuration;
    unsigned long _lastSignalTime;
    bool _isGap;
    
    String decodeMorse(String code);
};

#endif
