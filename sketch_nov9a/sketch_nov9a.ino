#include "LdrBlinkWinMV.h"

LdrBlinkWinMV ldr(34);  // unico parametro: pin ADC (es. GPIO34 su ADC1)

void setup() {
  Serial.begin(115200);
  ldr.begin();
}

const char* toStr(LdrBlinkWinMV::State s){
  switch(s){
    case LdrBlinkWinMV::LOW_STEADY:  return "BASSO fisso";
    case LdrBlinkWinMV::HIGH_STEADY: return "ALTO fisso";
    case LdrBlinkWinMV::BLINK:       return "LAMPEGGIO";
    default:                         return "UNKNOWN";
  }
}

void loop() {
  if (ldr.update()) {
    Serial.println(toStr(ldr.state()));
  }
}
