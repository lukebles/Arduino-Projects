#include "Morse.h"

Morse morse(0);

void setup() {
  Serial.begin(9600);
}

void loop() {
  morse.update();

  if (morse.hasNewChar()) {
    String morseChar = morse.getChar();
    if (morseChar.length() > 0) {
      Serial.println(morseChar);
    }
  }
}
