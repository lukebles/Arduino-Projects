#pragma once
#include <Arduino.h>
#include <NeoPixelBus.h>
#include <math.h>
#include "Effects.h"
#include "Palettes.h"

// ------------------ Interfaccia bus astratta ------------------
class IPixelBus {
public:
  virtual ~IPixelBus(){}
  virtual int length() const = 0;
  virtual RgbColor get(int i) const = 0;
  virtual void set(int i, RgbColor c) = 0;
  virtual void clear(RgbColor c) = 0;
  virtual void show() = 0;
};

// Adapter generico per NeoPixelBus
template<class BUS>
class BusAdapter : public IPixelBus {
public:
  BusAdapter(BUS& b) : _b(b) {}
  int length() const override { return _b.PixelCount(); }
  RgbColor get(int i) const override { return _b.GetPixelColor(i); }
  void set(int i, RgbColor c) override { _b.SetPixelColor(i, c); }
  void clear(RgbColor c) override { _b.ClearTo(c); }
  void show() override { _b.Show(); }
private:
  BUS& _b;
};

// ------------------ Parametri per strip ------------------
struct TailParams {
  uint8_t  frameMs;
  uint8_t  trail;
  uint8_t  fade;
  float    speedBase;
  float    lfoRate;
  float    lfoPhase;
  uint16_t jitterMs;
  uint8_t  endMin;
  uint8_t  endCurve;
  bool     reverse;
};

// ------------------ Effetto coda ------------------
class TailCometEffect : public IUpdatable {
public:
  TailCometEffect(IPixelBus& bus, uint8_t nled, const TailParams& p)
    : _bus(bus), _n(nled), _p(p) {}

  void begin() override {
    _lastFrame = 0;
    _lastJitter = 0;
    _pos = -(float)random(0, _p.trail);
    _flickerProb = 40;
    _flickerMin  = 160;
    _flickerMax  = 220;

    _bus.clear(RgbColor(0));
    _bus.show();
  }

  void update(uint32_t nowMs) override {
    // frame rate individuale
    if (nowMs - _lastFrame < _p.frameMs) return;
    _lastFrame = nowMs;

    // fade scia
    for (int i=0;i<_n;i++){
      RgbColor c = _bus.get(i);
      c.R = (uint8_t)(((uint16_t)c.R * _p.fade) / 255);
      c.G = (uint8_t)(((uint16_t)c.G * _p.fade) / 255);
      c.B = (uint8_t)(((uint16_t)c.B * _p.fade) / 255);
      _bus.set(i, c);
    }

    // velocità istantanea con LFO + jitter
    float v = currentSpeed(nowMs);
    _pos += v;
    if (_pos >= (float)_n + 2.0f){
      _pos = -random(2,6) * 0.1f;
    }

    // testa + scia
    for (int i=0;i<_n;i++){
      float d = (float)i - _pos;
      if (d < 0.0f || d > (float)_p.trail) continue;

      float   t   = 1.0f - (d / (float)_p.trail); // 1..0
      uint8_t bri = (uint8_t)(255.0f * t * t);
      uint8_t idx = (uint8_t)(t * 255.0f);
      RgbColor col = palSample16(cometPal, idx, bri);

      // attenuazione verso punta
      uint8_t prog   = tipProgress(i);
      uint8_t shaped = (uint8_t)(((uint16_t)prog * _p.endCurve) / 255);
      uint8_t atten  = (uint8_t)(( (uint16_t)255*(255-shaped) + (uint16_t)_p.endMin*shaped )/255);
      bool skipAtten = (random(0, EXCEPTION_RATE) == 0);
      if (!skipAtten){
        col = RgbColor((uint8_t)(((uint16_t)col.R*atten)/255),
                       (uint8_t)(((uint16_t)col.G*atten)/255),
                       (uint8_t)(((uint16_t)col.B*atten)/255));
      }

      int pix = _p.reverse ? (_n - 1 - i) : i;
      RgbColor prev = _bus.get(pix);
      _bus.set(pix, RgbColor(
        sat_add8(prev.R, col.R),
        sat_add8(prev.G, col.G),
        sat_add8(prev.B, col.B)
      ));
    }

    // flicker leggero
    for (int i=0;i<_n;i++){
      if (random(0,256) < _flickerProb){
        uint8_t att = random(_flickerMin, _flickerMax+1);
        RgbColor c = _bus.get(i);
        _bus.set(i, RgbColor(
          (uint8_t)(((uint16_t)c.R*att)/255),
          (uint8_t)(((uint16_t)c.G*att)/255),
          (uint8_t)(((uint16_t)c.B*att)/255)
        ));
      }
    }

    _bus.show();
  }

  // API per modifiche “a caldo”
  void setFlicker(uint8_t prob, uint8_t fmin, uint8_t fmax){
    _flickerProb = prob; _flickerMin = fmin; _flickerMax = fmax;
  }

  void setReverse(bool rev){ _p.reverse = rev; }

  void setTrail(uint8_t trail){ _p.trail = clampi(trail, 3, 14); }
  void setFade(uint8_t fade){ _p.fade = clampi(fade, 120, 240); }
  void setSpeedBase(float sb){ _p.speedBase = clampf(sb, 0.10f, 0.45f); }

private:
  IPixelBus& _bus;
  uint8_t _n;
  TailParams _p;

  // stato dinamico
  uint32_t _lastFrame{0};
  uint32_t _lastJitter{0};
  float    _pos{0};

  // flicker
  uint8_t _flickerProb{40};
  uint8_t _flickerMin{160};
  uint8_t _flickerMax{220};

  // costanti
  static const uint16_t EXCEPTION_RATE = 1000;

  uint8_t tipProgress(int i){
    int tipIndex = _p.reverse ? 0 : (_n - 1);
    uint16_t distToTip = abs(i - tipIndex);
    return 255 - (uint8_t)((distToTip * 255) / (_n - 1)); // 0=base, 255=punta
  }

  float currentSpeed(uint32_t nowMs){
    float t = nowMs * 0.001f;
    float lfo = sinf(2.0f * (float)M_PI * _p.lfoRate * t + _p.lfoPhase); // -1..1
    float v = _p.speedBase * (1.0f + 0.30f * lfo); // ±30%

    // jitter lento
    if (nowMs - _lastJitter > _p.jitterMs){
      _lastJitter = nowMs;
      _p.speedBase = clampf(_p.speedBase + ((int)random(-2,3))*0.01f, 0.12f, 0.38f);
      _p.trail     = clampi(_p.trail + (int)random(-1,2), 5, 10);
      _p.fade      = clampi(_p.fade  + (int)random(-10,11), 160, 215);
    }

    return clampf(v, 0.10f, 0.40f);
  }
};
