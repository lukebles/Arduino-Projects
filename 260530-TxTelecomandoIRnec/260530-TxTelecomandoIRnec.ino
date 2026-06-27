#include <IRremote.hpp>

#define IR_SEND_PIN 12

struct ComandoIR {
  const char* nome;
  uint16_t address;
  uint16_t command;
  uint32_t rawData;
};

ComandoIR comandiTV[] = {
  { "TASTO_0", 0xDF, 0x9A, 0x659A00DF },
  { "TASTO_1", 0xDF, 0x8E, 0x718E00DF },
  { "TASTO_2", 0xDF, 0x86, 0x798600DF },
  { "TASTO_3", 0xDF, 0x8F, 0x708F00DF },
  { "TASTO_4", 0xDF, 0x92, 0x6D9200DF },
  { "TASTO_5", 0xDF, 0x87, 0x788700DF },
  { "TASTO_6", 0xDF, 0x93, 0x6C9300DF },
  { "TASTO_7", 0xDF, 0x96, 0x699600DF },
  { "TASTO_8", 0xDF, 0x82, 0x7D8200DF },
  { "TASTO_9", 0xDF, 0x97, 0x689700DF },
  { "RADIO_TV", 0xDF, 0xCC, 0x33CC00DF }
};

void inviaTasto(int indice) {
  IrSender.sendNEC(
    comandiTV[indice].address,
    comandiTV[indice].command,
    0);
}

void setup() {
  IrSender.begin(IR_SEND_PIN);
}

void loop() {
  inviaTasto(0);  // invia TASTO_0
  delay(1000);

  inviaTasto(7);  // invia RADIO_TV
  delay(1000);

    inviaTasto(0);  // invia TASTO_0
  delay(1000);

  inviaTasto(3);  // invia RADIO_TV
  delay(10000);
}