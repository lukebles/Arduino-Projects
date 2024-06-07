// LkHexBytes Library

/*

Questo codice fornisce strumenti utili per convertire stringhe 
esadecimali in array di byte e per la loro visualizzazione, garantendo 
la validità degli input attraverso controlli rigorosi.

Il codice fornito appartiene al namespace `LkHexBytes` e offre funzionalità per la gestione 
e la conversione di stringhe esadecimali in array di byte. 


 */

// 6/6/2024 migliorato codice e sistemati dettagli

namespace LkHexBytes {

// la massima dimensione a livello di stringa è quindi 2 * MaxByteArraySize
const byte MaxByteArraySize = 30;

// La funzione privata `nibble(char c)` converte un singolo carattere esadecimale 
// in un valore numerico a 4 bit, gestendo sia le cifre da '0' a '9' che le lettere 
// da 'a' a 'f' e da 'A' a 'F'. In caso di carattere non valido, restituisce 0.

byte nibble(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return 0;  // Carattere non valido esadecimale
}

// La funzione `dumpByteArray` prende un array di byte e la sua dimensione, 
// e stampa i valori in formato esadecimale su una seriale, formattati con il prefisso 
// "0x" e separati da virgole.

void dumpByteArray(const byte * byteArray, const byte arraySize) {
  for (byte i = 0; i < arraySize; i++) {
    Serial.print("0x");
    if (byteArray[i] < 0x10) {
      Serial.print("0");
    }
    Serial.print(byteArray[i], HEX);
    if (i < arraySize - 1) {
      Serial.print(", ");
    }
  }
  Serial.println();
}

// La funzione `hexCharacterStringToBytes` converte una stringa esadecimale in un 
// array di byte. Esegue vari controlli di validità sull'input: verifica che 
// la stringa e l'array di byte non siano nulli, controlla la lunghezza della 
// stringa e conferma che contenga solo caratteri esadecimali validi. 
// La conversione avviene leggendo due caratteri alla volta dalla stringa esadecimale 
// e combinandoli in un singolo byte.

bool hexCharacterStringToBytes(byte *byteArray, const char *hexString) {
  // Validazione dell'input
  if (hexString == nullptr || byteArray == nullptr) {
    // Stringa esadecimale o array di byte è null
    return false;
  }

  int len = strlen(hexString);
  if (len == 0 || len > 2 * MaxByteArraySize || len % 2 != 0) {
    // Lunghezza della stringa esadecimale non valida
    return false;
  }

  for (int i = 0; i < len; i++) {
    if (!isxdigit(hexString[i])) {
      // Carattere non esadecimale trovato nella stringa
      return false;
    }
  }

  for (byte charIndex = 0, byteIndex = 0; charIndex < len; charIndex += 2) {
    byteArray[byteIndex++] = (nibble(hexString[charIndex]) << 4) | nibble(hexString[charIndex + 1]);
  }

  return true;
}

} // namespace LkHexBytes


