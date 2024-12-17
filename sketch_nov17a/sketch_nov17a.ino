#include <IRremote.h>

const int RECV_PIN = 11;

IRrecv irrecv(RECV_PIN);
decode_results results;

void setup() {
  Serial.begin(115200);
  irrecv.enableIRIn();
  Serial.println("Ricevitore IR pronto.");
}

void loop() {
  if (irrecv.decode(&results)) {
    Serial.println("Segnale ricevuto!");
    Serial.print("Codice grezzo (HEX): 0x");
    Serial.println(results.value, HEX); // Codice del tasto in esadecimale
    Serial.print("Lunghezza in bit: ");
    Serial.println(results.bits); // Lunghezza del codice ricevuto
    Serial.print("Tipo protocollo: ");
    Serial.println(results.decode_type); // Protocollo (es. NEC, Panasonic, ecc.)
    irrecv.resume();
  }
}
