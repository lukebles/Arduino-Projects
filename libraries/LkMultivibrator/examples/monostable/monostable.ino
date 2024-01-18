/*

 * In this example the monostable multivibrator is used as 'astable'
 * to continuously send the character 'A' to the serial - every 3 seconds.
 * instead of using an astable, note how the re-'start' operation allows.
 * more control than the same solution with an astable

  ==================
 
 * In questo esempio il multivibratore monostabile è usato come 'astabile'
 * per inviare continuamente sulla seriale - ogni 3 secondi - il carattere "A".
 * anzichè usare un astabile, si noti come l'operazione di re-"start" consenta
 * un maggior controllo rispetto alla medesima soluzione con l'utilizzo
 * di un astabile.

 */

#include <LkMultivibrator.h>

LkMultivibrator m(3000,MONOSTABLE);

void setup() {
  Serial.begin(38400);
}

void loop() {
  if(m.expired()){
    Serial.println("A");
    m.start();
  }
}
