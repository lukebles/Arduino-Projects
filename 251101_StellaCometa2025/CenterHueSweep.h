#pragma once
#include <Arduino.h>
#include <NeoPixelBus.h>
#include "Effects.h"

// Effetto ONE-SHOT: sfuma tutti i LED del centro tra tutti i colori e poi si disattiva
class CenterHueSweep : public IUpdatable {
public:
  // durationMs ~10000 (10 s)
  CenterHueSweep(NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>& bus,
                 uint16_t nled, uint32_t durationMs,
                 volatile bool* pauseFlagForOthers)
  : _bus(bus), _n(nled), _dur(durationMs), _pause(pauseFlagForOthers) {}

  // Chiamare per avviare l'effetto
  void trigger() {
    _active = true;
    _start  = millis();
    if (_pause) *_pause = true; // sospende altri effetti sul centro
  }

  void begin() override {
    _active = false;
    _start = 0;
  }

  void update(uint32_t nowMs) override {
    if (!_active) return;

    uint32_t elapsed = nowMs - _start;
    if (elapsed >= _dur) {
      // fine: pulisci leggermente e rilascia la pausa
      _bus.ClearTo(RgbColor(0));
      _bus.Show();
      if (_pause) *_pause = false;
      _active = false;
      return;
    }

    // progress 0..1 -> hue 0..1 (intera ruota in _dur)
    float t = (float)elapsed / (float)_dur;
    // opzionale: easing morbido
    // t = t*t*(3-2*t);

    HslColor hsl(t, 1.0f, 0.5f); // sat 100%, lightness 50% (luminoso ma non brucia)
    RgbColor rgb(hsl);

    for (uint16_t i=0;i<_n;i++) _bus.SetPixelColor(i, rgb);
    _bus.Show();
  }

private:
  NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>& _bus;
  uint16_t _n;
  uint32_t _dur;
  volatile bool* _pause{nullptr};

  bool     _active{false};
  uint32_t _start{0};
};
