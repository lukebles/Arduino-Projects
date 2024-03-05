#include "modulo.h"
#include <stdlib.h> // Include la libreria per accesso a atoi()

void setup() {
  setupWiFi(); 
  serialBuffer[0] = '\0';
}

void loop() {
  webSocket.loop();
  server.handleClient();

  checkSerialData(); // Controlla e accumula i dati seriali

  if (newData) { // Se sono disponibili nuovi dati


    addValue(serialBuffer);

    //Serial.println(CONTATOREa);

    //sendArrayJs();

    //Serial.println(receivedNumber);
    memset(serialBuffer, 0, MAX_TEXT_LENGTH); // Resetta il buffer e preparati per la prossima lettura
    newData = false; // Resetta il flag di nuovi dati
  }
}

void checkSerialData() {
  while (Serial.available() > 0) {
    char receivedChar = Serial.read(); // Legge il carattere in arrivo
    if (receivedChar == '\n') { // Se il carattere è una nuova linea
      serialBuffer[serialBufferIndex] = '\0'; // Termina la stringa nel buffer
      serialBufferIndex = 0; // Resetta l'indice per la prossima lettura
      newData = true; // Imposta il flag di nuovi dati disponibili
    } else if (serialBufferIndex < MAX_TEXT_LENGTH - 1) { // Se c'è spazio nel buffer
      serialBuffer[serialBufferIndex++] = receivedChar; // Aggiungi al buffer
      serialBuffer[serialBufferIndex] = '\0'; // Assicura che il buffer sia sempre una stringa valida
    }
  }
}




