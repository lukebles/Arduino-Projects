#include "LkRadioStructure_RH.h"

struct Payload {
  uint32_t counter;
  float    temperature;
  int16_t  battery_mv;
};

LkRadioStructure<Payload> radio;
uint32_t cnt = 0;

void setup() {
  Serial.begin(115200);
  LkRadioStructure<Payload>::globalSetup(2000, /*tx*/12, /*rx*/11, /*ptt*/10, /*ptt_inv*/false);
}

void loop() {
  Payload p;
  p.counter     = cnt++;
  p.temperature = 23.5;
  p.battery_mv  = 3720;

  radio.sendStructure(p);
  delay(200); // piccolo pacing
}
