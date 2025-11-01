#pragma once
#include <NeoPixelBus.h>

// Palette centro (fuoco)
static const RgbColor fireWarmPal[16] = {
  RgbColor(0,0,0), RgbColor(20,0,0), RgbColor(80,0,0),  RgbColor(150,0,0),
  RgbColor(255,40,0), RgbColor(255,90,0), RgbColor(255,150,0), RgbColor(255,200,0),
  RgbColor(255,215,40), RgbColor(255,230,100), RgbColor(255,240,160), RgbColor(255,255,220),
  RgbColor(255,200,0), RgbColor(255,120,0), RgbColor(180,20,0), RgbColor(0,0,0)
};

// Palette coda (cometa)
static const RgbColor cometPal[16] = {
  RgbColor(0,0,0), RgbColor(150,0,0), RgbColor(255,40,0), RgbColor(255,120,0),
  RgbColor(255,180,40), RgbColor(255,220,120), RgbColor(160,240,255), RgbColor(80,220,255),
  RgbColor(160,230,255), RgbColor(210,240,255), RgbColor(255,255,255), RgbColor(220,240,255),
  RgbColor(255,220,120), RgbColor(255,160,0), RgbColor(255,90,0),  RgbColor(100,0,0)
};

static inline RgbColor palSample16(const RgbColor* pal16, uint8_t idx, uint8_t bri=255){
  uint8_t a = idx >> 4;                // 0..15
  uint8_t b = (a + 1) & 0x0F;
  uint8_t frac = (idx & 0x0F) * 17;    // 0..255
  RgbColor ca = pal16[a], cb = pal16[b];
  RgbColor c(
    (uint8_t)(( (uint16_t)ca.R*(255-frac) + (uint16_t)cb.R*frac )/255),
    (uint8_t)(( (uint16_t)ca.G*(255-frac) + (uint16_t)cb.G*frac )/255),
    (uint8_t)(( (uint16_t)ca.B*(255-frac) + (uint16_t)cb.B*frac )/255)
  );
  return RgbColor((uint8_t)((c.R*bri)/255), (uint8_t)((c.G*bri)/255), (uint8_t)((c.B*bri)/255));
}

// Utils saturazione
static inline uint8_t sat_add8(uint8_t a, uint8_t b){ uint16_t s = (uint16_t)a + b; return (s > 255) ? 255 : (uint8_t)s; }
static inline uint8_t sat_sub8(uint8_t a, uint8_t b){ return (a > b) ? (uint8_t)(a - b) : 0; }

// Clamp
static inline int   clampi(int v, int lo, int hi)   { return (v < lo) ? lo : (v > hi) ? hi : v; }
static inline float clampf(float v, float lo, float hi){ return (v < lo) ? lo : (v > hi) ? hi : v; }
