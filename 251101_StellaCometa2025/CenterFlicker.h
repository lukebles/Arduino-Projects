#pragma once
#include <Arduino.h>
#include <NeoPixelBus.h>
#include "Effects.h"
#include "Palettes.h"

class CenterFlickerEffect : public IUpdatable {
public:
  CenterFlickerEffect(NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>& bus, uint16_t nled)
    : _bus(bus), _n(nled) {}

  // Consenti di collegare una flag esterna per mettere in pausa l'effetto
  void attachPauseFlag(volatile bool* flag) { _pause = flag; }

  void begin() override {
    _last = 0;
    for (uint16_t i=0;i<_n;i++){ _bri[i]=0; _idx[i]=randomWarmIndex(); }
  }

  void update(uint32_t nowMs) override {
    if (_pause && *_pause) return;  // effetto “sospeso” da altri
    if (nowMs - _last < FRAME_MS) return;
    _last = nowMs;

    // fade
    for (uint16_t i=0;i<_n;i++) _bri[i] = sat_sub8(_bri[i], CENTER_FADE_PER_FRAME);

    // sparks
    uint8_t sparks = random(0, CENTER_MAX_SPARKS_FRAME + 1);
    for (uint8_t s=0; s<sparks; s++){
      int p = random(0, _n);
      _idx[p] = randomWarmIndex();
      uint8_t base = random(CENTER_MIN_SPARK_BRI, 256);
      _bri[p] = sat_add8(_bri[p], base);
      if (p>0 && random(0,256)<128)            _bri[p-1] = sat_add8(_bri[p-1], base/6);
      if (p<_n-1 && random(0,256)<128) _bri[p+1] = sat_add8(_bri[p+1], base/6);
    }

    for (uint16_t i=0;i<_n;i++){
      uint8_t b = _bri[i] > CENTER_BACKGROUND_BRI ? _bri[i] : CENTER_BACKGROUND_BRI;
      _bus.SetPixelColor(i, palSample16(fireWarmPal, _idx[i], b));
    }
    _bus.Show();
  }

private:
  NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>& _bus;
  uint16_t _n;
  uint32_t _last{0};
  volatile bool* _pause{nullptr};   // <<< NEW

  // stato
  static const uint16_t MAXN = 200; // sicurezza
  uint8_t _bri[MAXN]{};
  uint8_t _idx[MAXN]{};

  // parametri
  static const uint16_t FRAME_MS = 20;
  static const uint8_t  CENTER_MAX_SPARKS_FRAME = 4;
  static const uint8_t  CENTER_MIN_SPARK_BRI = 140;
  static const uint8_t  CENTER_FADE_PER_FRAME = 28;
  static const uint8_t  CENTER_BACKGROUND_BRI  = 6;

  static inline uint8_t randomWarmIndex(){
    uint8_t a = random(120,200);
    uint8_t b = random(200,246);
    return (random(0,256) < 96) ? b : a;
  }
};
