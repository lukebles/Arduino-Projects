// LkHexBytes Library

/*
 * LkHexBytes Library
 * This library provides functionality for handling hexadecimal strings and byte arrays.
 * It includes methods for converting a string of hexadecimal characters to a byte array,
 * and for dumping a byte array to the Serial output in a human-readable format.
 * Specifically designed for use with Arduino environments, this library is useful
 * for projects that require hexadecimal string parsing or display.
 *
 * ======================
 *
 * Questa libreria fornisce funzionalità per la gestione di stringhe esadecimali e array di byte.
 * Include metodi per convertire una stringa di caratteri esadecimali in un array di byte,
 * e per esportare un array di byte sull'output Seriale in un formato leggibile dall'uomo.
 * Specificamente progettata per l'uso con ambienti Arduino, questa libreria è utile
 * per progetti che richiedono l'analisi o la visualizzazione di stringhe esadecimali.
 *
 */

#ifndef LKHEXBYTES_H
#define LKHEXBYTES_H
#include <Arduino.h>

namespace LkHexBytes {

const byte MaxByteArraySize = 30;

// Static function, visible only within this namespace
static byte nibble(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return 0;  // Not a valid hexadecimal character
}

void dumpByteArray(const byte * byteArray, const byte arraySize) {
  for (int i = 0; i < arraySize; i++) {
    Serial.print("0x");
    if (byteArray[i] < 0x10) {
      Serial.print("0");
    }
    Serial.print(byteArray[i], HEX);
    Serial.print(", ");
  }
  Serial.println();
}

void hexCharacterStringToBytes(byte *byteArray, char *hexString) {
  // Validazione dell'input
  if (hexString == NULL || byteArray == NULL) {
    // Hexadecimal string or array of NULL bytes
    return;
  }

  int len = strlen(hexString);
  if (len == 0 || len > 2 * MaxByteArraySize || len % 2 != 0) {
    // Invalid or even hexadecimal string length
    return;
  }

  for (int i = 0; i < len; i++) {
    if (!isxdigit(hexString[i])) {
      // Non-hexadecimal character found in the string
      return;
    }
  }

  byte currentByte = 0;
  byte byteIndex = 0;

  for (byte charIndex = 0; charIndex < len; charIndex += 2) {
    currentByte = nibble(hexString[charIndex]) << 4;
    currentByte |= nibble(hexString[charIndex + 1]);
    byteArray[byteIndex++] = currentByte;
  }
}

} // namespace LkHexBytes

#endif
