#include <LkMultivibrator.h>

/*
 * In this example, two LEDs are alternately turned on and off, using four multivibrators.
 * Two of these (multivibratorA and multivibratorB) control the on-times of the LEDs,
 * while the other two (flasherA and flasherB) manage the off-times.
 * This setup provides four distinct timings: two for turning the LEDs on and two for turning them off.
 */

/*
 * In questo esempio, due LED vengono accesi e spenti alternativamente, utilizzando quattro multivibratori.
 * Due di questi (multivibratorA e multivibratorB) determinano i tempi di accensione dei LED,
 * mentre gli altri due (flasherA e flasherB) gestiscono i tempi di spegnimento.
 * Questa configurazione permette di avere quattro tempi distinti: due per l'accensione e due per lo spegnimento dei LED.
 */

// Definizione dei multivibratori
LkMultivibrator timeOffA(300, ASTABLE);
LkMultivibrator timeOffB(1000, ASTABLE);
LkMultivibrator timeOnA(10, MONOSTABLE);
LkMultivibrator timeOnB(33, MONOSTABLE);

// Definizione dei pin
const int PIN_A = 13;
const int PIN_B = 12;

void setup() {
  pinMode(PIN_A, OUTPUT);
  pinMode(PIN_B, OUTPUT);
}

void loop() {
  handleMultivibrator(timeOffA, timeOnA, PIN_A);
  handleMultivibrator(timeOffB, timeOnB, PIN_B);
}

void handleMultivibrator(LkMultivibrator& timeOff, LkMultivibrator& timeOn, int pin) {
  if (timeOff.expired()){
    digitalWrite(pin, HIGH);
    timeOn.start();
  }

  if (timeOn.expired()){
    digitalWrite(pin, LOW);
  }
}
