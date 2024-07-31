#include <LittleFS.h>

void setup() {
  Serial.begin(115200);

  // Inizializza LittleFS
  if (!LittleFS.begin()) {
    Serial.println("Errore di montaggio del filesystem");
    return;
  }

  // Formatta il filesystem
  Serial.println("Formattazione del filesystem in corso...");
  if (LittleFS.format()) {
    Serial.println("Formattazione completata con successo.");
  } else {
    Serial.println("Errore durante la formattazione del filesystem.");
  }

  // Prova a montare di nuovo il filesystem per assicurarsi che funzioni
  if (!LittleFS.begin()) {
    Serial.println("Errore di montaggio del filesystem dopo la formattazione.");
    return;
  }

  Serial.println("Filesystem montato con successo dopo la formattazione.");
}

void loop() {
  // Non c'è bisogno di fare nulla qui
}
