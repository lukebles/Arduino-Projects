#include <IRremote.hpp>

const int RECV_PIN = 2; // Pin collegato al sensore IR

void setup() {
  Serial.begin(115200);
  IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK); // Avvia il ricevitore IR
  Serial.println("Pronto a ricevere segnali IR");
}

void loop() {
  if (IrReceiver.decode()) { // Controlla se c'è un segnale
    if (IrReceiver.decodedIRData.protocol != UNKNOWN) { // Verifica che il protocollo sia riconosciuto
      //Serial.print("Tasto premuto, comando: ");
      int segnale = IrReceiver.decodedIRData.command;
      if (segnale != 129){
        Serial.println(segnale); // Stampa il comando come numero decimale
      }
    } else {
      Serial.println("Segnale non riconosciuto");
    }
    IrReceiver.resume(); // Pronto a ricevere il prossimo segnale
  }
}
