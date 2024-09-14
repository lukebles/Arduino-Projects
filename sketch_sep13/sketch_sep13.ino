#include <LkBlinker.h>

LkBlinker blinkerFixed(3, false, 5);                // LED fisso (default) sul pin 13, non invertito, con 5 lampeggi
//LkBlinker blinkerVibrating(12, false, 5, true, 3000); // LED vibrante a 3 kHz sul pin 12, non invertito, con 5 lampeggi
//LkBlinker blinkerCustom(11, false, 5, true, 2000);   // LED vibrante a 2 kHz sul pin 11, non invertito, con 5 lampeggi

void setup() {
  // Abilita il lampeggio all'avvio
  blinkerFixed.enable();
  // blinkerVibrating.enable();
  // blinkerCustom.enable();
}

void loop() {
  // Chiama il metodo loop del blinker per gestire il lampeggio
  blinkerFixed.loop();
  // blinkerVibrating.loop();
  // blinkerCustom.loop();
}

