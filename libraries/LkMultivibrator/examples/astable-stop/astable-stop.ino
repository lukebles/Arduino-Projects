/*

* In this example, two multivibrators are used:
 * one astable @ 300 ms and one monostable @ 3000 ms.
 * Every 300 ms the character A is sent to the serial. 
 * After 3000 ms this input is stopped permanently.

 ==================

 * In questo esempio, vengono usati due multivibratori:
 *       uno astabile @ 300 ms e un monostabile @ 3000 ms
 * Ogni 300 ms viene inviato sulla seriale il carattere A. 
 * Dopo 3000 ms questa immissione viene interrotta definitivamente
 */


#include <LkMultivibrator.h>
#include "LkSetupSerial.h"

LkMultivibrator stopSendingTimer(3000,MONOSTABLE);
LkMultivibrator sendCharacterTimer(300,ASTABLE);
LkSetupSerial mySetupSerial;

void setup() {
  mySetupSerial.begin();
}

void loop() {
  if(stopSendingTimer.expired()){
    sendCharacterTimer.stop();
  } else {
    if (sendCharacterTimer.expired()){
      Serial.print("A");
    }
  }
}
