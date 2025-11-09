#include "LkRadioStructure_RH.h"

// Esempio di struttura
struct Payload {
  uint32_t counter;
  float    temperature;
  int16_t  battery_mv;
};

LkRadioStructure<Payload> radio;

void setup() {
  Serial.begin(115200);
  // Imposta velocità e pin (esempio): speed=2000bps, TX=12, RX=11, PTT=10 (classici AVR)
  // Su ESP8266/ESP32 scegli pin compatibili e NON usati da periferiche critiche.
  LkRadioStructure<Payload>::globalSetup(2000, /*tx*/12, /*rx*/11, /*ptt*/10, /*ptt_inv*/false);
}

void loop() {
  if (radio.haveRawMessage()) {
    Payload p = radio.getStructure();
    Serial.print("RX cnt="); Serial.print(p.counter);
    Serial.print(" T=");      Serial.print(p.temperature, 2);
    Serial.print(" batt=");   Serial.println(p.battery_mv);
  }
}
