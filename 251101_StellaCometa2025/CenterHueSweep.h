#pragma once
#include <Arduino.h>
#include <NeoPixelBus.h>
#include "Effects.h"   // <— usa IUpdatable del tuo engine

// Queste funzioni le fornisce lo sketch .ino
void __chs_centerSolid(const RgbColor& c);
void __chs_setTail(uint8_t idx, const RgbColor& c);
uint16_t __chs_centerCount();
uint16_t __chs_tailLen();

class CenterHueSweep : public IUpdatable {
public:
  template<typename BusT>
  CenterHueSweep(BusT&, uint16_t, uint32_t durationMs, volatile bool* pauseFlag = nullptr)
  : _duration(durationMs), _pauseFlag(pauseFlag) {}

  void begin() { /* no-op */ }

  void update(uint32_t nowMs) override;
  void trigger();

  void setDuration(uint32_t ms)      { _duration = ms; }
  void setTailStepMs(uint16_t ms)    { _tailStepMs = ms; }
  void setStarFps(uint16_t fps)      { _starFps = (fps ? fps : 60); }
  void setWhiteAmount(float wMax01)  { _whiteMax = constrain(wMax01, 0.0f, 1.0f); }

private:
  bool     _running = false;
  uint32_t _t0 = 0;
  uint32_t _lastStar = 0;
  uint32_t _lastTail = 0;
  uint16_t _tailStep = 0;

  uint32_t _duration   = 10000; // 10s
  uint16_t _starFps    = 60;
  uint16_t _tailStepMs = 180;
  float    _whiteMax   = 0.35f;

  volatile bool* _pauseFlag = nullptr;

  static inline RgbColor _lerp(const RgbColor& a, const RgbColor& b, float t) {
    uint8_t r = a.R + (int)((b.R - a.R) * t);
    uint8_t g = a.G + (int)((b.G - a.G) * t);
    uint8_t bb= a.B + (int)((b.B - a.B) * t);
    return RgbColor(r,g,bb);
  }
  static inline RgbColor _hsvRainbow(float hue01) {
    float hue = fmodf(fabsf(hue01), 1.0f);
    float h = hue * 6.0f; int i = (int)h; float f = h - i; float q = 1.0f - f;
    float r=0,g=0,b=0;
    switch (i % 6) {
      case 0: r=1; g=f; b=0; break;
      case 1: r=q; g=1; b=0; break;
      case 2: r=0; g=1; b=f; break;
      case 3: r=0; g=q; b=1; break;
      case 4: r=f; g=0; b=1; break;
      case 5: r=1; g=0; b=q; break;
    }
    return RgbColor((uint8_t)(r*255),(uint8_t)(g*255),(uint8_t)(b*255));
  }
  inline RgbColor _rainbowWithWhite(float phase01) {
    RgbColor base = _hsvRainbow(phase01);
    float whiteCycle = 0.5f * (sinf(phase01 * 2.0f * (float)M_PI) + 1.0f); // 0..1
    float mix = whiteCycle * _whiteMax;
    return _lerp(base, RgbColor(255,255,255), mix);
  }

  void _applyTail(uint16_t step, uint32_t elapsedMs);
};
