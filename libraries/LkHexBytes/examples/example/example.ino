#include <Arduino.h>
#include "LkHexBytes.h" // Include la tua libreria
#include "LkSetupSerial.h"

LkSetupSerial mySerial; 

void setup() {
  
  mySerial.begin();
  
  // Esempio di utilizzo della libreria LkHexBytes
  char hexString[] = "1A3F5B"; // Stringa esadecimale di esempio
  byte byteArray[3]; // Array di byte per memorizzare il risultato
  
  // Conversione da stringa esadecimale ad array di byte
  LkHexBytes::hexCharacterStringToBytes(byteArray, hexString);

  // Stampa dell'array di byte
  Serial.println("Array di byte convertito:");
  LkHexBytes::dumpByteArray(byteArray, sizeof(byteArray) / sizeof(byteArray[0]));
}

void loop() {
  // Qui puoi aggiungere altro codice che viene eseguito ripetutamente
}
