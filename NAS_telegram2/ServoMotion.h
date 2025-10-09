#pragma once
#include <ESP32Servo.h>
#include "NbDelay.h"

class ServoMotion {
public:
  ServoMotion(Servo& s, int riposo, int premi, int stepDeg=2, uint16_t stepMs=10)
  : servo(s), RIPOSO_POS(riposo), PREMI_POS(premi),
    stepDeg(stepDeg), stepMs(stepMs) {}

  void tick() {
    switch (stato) {
      case IDLE: break;

      case SALITA:
        if (tStep.wait(stepMs)) {
          pos += stepDeg;
          if (pos >= PREMI_POS) { pos = PREMI_POS; stato = HOLD; tHold.reset(); }
          servo.write(pos);
        }
        break;

      case HOLD:
        if (tHold.wait(holdMs)) { stato = DISCESA; tStep.reset(); }
        break;

      case DISCESA:
        if (tStep.wait(stepMs)) {
          pos -= stepDeg;
          if (pos <= RIPOSO_POS) { pos = RIPOSO_POS; stato = IDLE; }
          servo.write(pos);
        }
        break;
    }
  }

  // trigger delle tue funzioni (restano void nel .ino)
  void triggerAccendi() { holdMs = 1000; stato = SALITA; tStep.reset(); }
  void triggerSpegni()  { holdMs = 6000; stato = SALITA; tStep.reset(); }

  bool busy() const { return stato != IDLE; }

private:
  enum Stato : uint8_t { IDLE, SALITA, HOLD, DISCESA } stato = IDLE;
  Servo& servo;
  const int RIPOSO_POS, PREMI_POS;
  int pos;
  const int stepDeg;
  const uint16_t stepMs;
  uint32_t holdMs = 0;
  NbDelay tStep, tHold;

public:
  // opzionale: da chiamare in setup se vuoi inizializzare la pos interna
  void beginAtRest() { pos = RIPOSO_POS; servo.write(RIPOSO_POS); }
};
