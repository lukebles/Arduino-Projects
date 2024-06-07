#include <Arduino.h>
#include "LkHexBytes.h"

void setup() {
  Serial.begin(115200);
  while(!Serial){
    ;
  }
  byte myArray[LkHexBytes::MaxByteArraySize];

  // Utilizzo delle funzioni del namespace
  char hexString[] = "1a2b3c";
  if (LkHexBytes::hexCharacterStringToBytes(myArray, hexString)) {
    LkHexBytes::dumpByteArray(myArray, strlen(hexString) / 2);
  } else {
    Serial.println("Errore nella conversione della stringa esadecimale.");
  }
}

void loop() {
  // Codice del loop
}
