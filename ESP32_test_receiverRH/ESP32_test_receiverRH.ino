#include <Arduino.h>
#include "LkRadioStructure_RH.h"

/*
============ USARE CORE 2.0.14 ================
1. Soluzione robusta: usare RadioHead “vera”
È la via corta e affidabile.
Opzione A – Tornare a core 2.0.x
In Arduino IDE:
Strumenti → Scheda → Gestore schede…
Cerca “esp32 by Espressif Systems”
Installa la versione 2.0.14 (o 2.0.11–2.0.14).
Usa il classico:
#include <RH_ASK.h>
RH_ASK rf_driver(2000, 21, 22, 0);
con il tuo sketch minimale di prima.
Su quell’accoppiata (RadioHead 1.1xx + core 2.0.x) RH_ASK è già stato usato e testato da un sacco di gente.
*/

// Tipo fittizio, perché ci interessa solo il buffer grezzo
struct DummyPayload {
  uint8_t dummy;
};

// Alias per comodità
using RxRadio = LkRadioStructure<DummyPayload>;

RxRadio radio;

void printHex(const uint8_t* buf, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    if (buf[i] < 0x10) Serial.print('0');
    Serial.print(buf[i], HEX);
    if (i < len - 1) Serial.print(' ');
  }
}

void printAscii(const uint8_t* buf, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    char c = (char)buf[i];
    if (c >= 32 && c <= 126) Serial.print(c);
    else                     Serial.print('.');
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {;}
  delay(3000);

  Serial.println();
  Serial.println(F("=== ESP32 + LkRadioStructure_RH RX 2000 bps (GPIO21) ==="));

  Serial.print(F("RH_PLATFORM = "));
  Serial.println(RH_PLATFORM);

  // Inizializza RadioHead dentro la tua libreria:
  // speed = 2000 bps
  // TX pin = 22 (non usato ma deve essere un pin valido)
  // RX pin = 21
  // PTT pin = 23 (non usato)
  RxRadio::globalSetup(
      2000,   // speed
      22,     // transmit_pin
      21,     // receive_pin
      23,     // ptt_pin
      false   // ptt_inverted
  );

  Serial.println(F("In ascolto sui pacchetti RH_ASK/VirtualWire..."));
}

void loop() {
  if (radio.haveRawMessage()) {
    uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
    uint8_t len = 0;
    radio.getRawBuffer(buf, len);

    unsigned long ts = radio.getMicrosec();

    Serial.print(F("["));
    Serial.print(ts);
    Serial.print(F(" us] len="));
    Serial.print(len);
    Serial.println(F(" bytes"));

    Serial.print(F("HEX:   "));
    printHex(buf, len);
    Serial.println();

    Serial.print(F("ASCII: "));
    printAscii(buf, len);
    Serial.println();

    Serial.println(F("--------------------------------"));
  }

  delay(1);  // piccolo respiro
}
