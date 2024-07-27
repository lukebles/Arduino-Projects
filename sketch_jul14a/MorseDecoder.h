#ifndef MORSEDECODER_H
#define MORSEDECODER_H

#include "Button.h"
#include <Arduino.h>

class MorseDecoder {
public:
    MorseDecoder(int pin);
    void update();
    bool hasMessage();
    String getMessage();
    
private:
    String message;
    Button button;
    String morseCode;
    unsigned long lastChangeTime;
    bool messageAvailable;

    void decodeMorse();
    String decodeCharacter(String code);
};

#endif
