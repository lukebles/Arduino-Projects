#pragma once
#include <Arduino.h>
#include "LkMultivibrator.h"

class LdrBlinkWinMV {
public:
  enum State : uint8_t { UNKNOWN=0, LOW_STEADY=1, HIGH_STEADY=2, BLINK=3 };

  explicit LdrBlinkWinMV(int adcPin)
  : pin(adcPin),
    tick(TICK_MS, ASTABLE),
    win(WINDOW_MS, MONOSTABLE),
    mixedStreak(MIXED_MS, MONOSTABLE) {}

  void begin() {
    pinMode(pin, INPUT);
    tick.start();          // campione ogni 10 ms
    win.start();           // finestra di 2 s
    mixedStreak.stop();    // non in corso
    resetWindowCounters();
    lastReported = UNKNOWN;
  }

  // Chiama spesso nel loop. Ritorna true quando lo stato "deciso" cambia.
  bool update() {
    // 1) Campiona ogni 10 ms (ASTABLE)
    if (tick.expired()) {
      const int v = analogRead(pin);
      //Serial.println(v);
      samplesInWin++;
      if (v == LOW_THR){
        lowCount++;
      } else {
        highCount++;
      }
    }

    // 2) Alla fine di ogni finestra da 2 s, valuta
    State decided = UNKNOWN;
    if (win.expired()) {
      const bool allHigh = (samplesInWin > 0) && (highCount == samplesInWin);
      const bool allLow  = (samplesInWin > 0) && (lowCount  == samplesInWin);
      const bool mixed   = !allHigh && !allLow; // almeno un alto e almeno un basso

      if (allHigh) {
        decided = HIGH_STEADY;
        mixedActive = false;
        mixedStreak.stop(); // interrompe l'eventuale “mista”
      } else if (allLow) {
        decided = LOW_STEADY;
        mixedActive = false;
        mixedStreak.stop();
      } else if (mixed) {
        // finestra mista: avvia/continua il cronometro di 4 s
        if (!mixedActive) {
          mixedStreak.start();
          mixedActive = true;
        } else if (mixedStreak.expired()) {
          decided = BLINK;  // mista per 4 s filati → lampeggio
          // si potrebbe lasciare mixedActive=true per continuare a monitorare
        }
      }

      // prepara la prossima finestra 2 s
      win.start();              // riavvia la finestra successiva
      resetWindowCounters();    // azzera i contatori per i prossimi 2 s
    }

    // 3) Pubblica solo se c'è un nuovo stato deciso
    if (decided != UNKNOWN && decided != lastReported) {
      lastReported = decided;
      return true;
    }
    return false;
  }

  State state() const { return lastReported; }

private:
  // --- Parametri semplici ---
  static constexpr unsigned long TICK_MS   = 10;     // 10 ms = 100 Hz
  static constexpr unsigned long WINDOW_MS = 2000;   // 2 s
  static constexpr unsigned long MIXED_MS  = 4000;   // 4 s di “misto” consecutivo
  static constexpr int           LOW_THR   = 0;      //  0  => basso (nota: su ESP32 analogRead() >= 0)
  //static constexpr int           HIGH_THR  = 1;     //  1 => alto

  // --- Stato ---
  int pin;
  LkMultivibrator tick;         // ASTABLE 10 ms
  LkMultivibrator win;          // MONOSTABLE 2 s (valutazione finestra)
  LkMultivibrator mixedStreak;  // MONOSTABLE 4 s (continuità “mista”)

  // contatori della finestra corrente
  uint16_t samplesInWin = 0;
  uint16_t highCount    = 0;
  uint16_t lowCount     = 0;

  bool mixedActive      = false;
  State lastReported    = UNKNOWN;

  void resetWindowCounters() {
    samplesInWin = 0;
    highCount    = 0;
    lowCount     = 0;
  }
};
