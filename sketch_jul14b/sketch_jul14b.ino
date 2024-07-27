#include <Arduino.h>
#include "MorseDecoder.h"

MorseDecoder morse(0);

void setup() {
  Serial.begin(115200);
}

void loop() {
  morse.update();

  if (morse.hasMessage()) {
    String morseMsg = morse.getMessage();
    if (morseMsg.length() > 0) {
      Serial.println(morseMsg);
    }
  }
}