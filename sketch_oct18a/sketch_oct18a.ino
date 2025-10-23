#include <Arduino.h>
#include <NeoPixelBus.h>
#include <WiFi.h>
#include "esp_bt.h"
#include <math.h>    // per sinf

// ================== CONFIG ==================
// Stella (centro)
static const uint8_t  CENTER_PIN   = 16;
static const uint16_t CENTER_NLED  = 100;

// Coda (5 strisce)
static const uint8_t  TAIL_PINS[5] = {17, 18, 33, 21, 22};
static const uint8_t  TAIL_NSTRIPS = 5;
static const uint8_t  TAIL_NLED    = 14;   // <--- cambia se serve

// ================== METODO RMT PER CANALE ==================
// Un canale RMT dedicato per strip (massima stabilità)
using Feature = NeoGrbFeature;
using C0 = NeoEsp32Rmt0Ws2812xMethod;
using C1 = NeoEsp32Rmt1Ws2812xMethod;
using C2 = NeoEsp32Rmt2Ws2812xMethod;
using C3 = NeoEsp32Rmt3Ws2812xMethod;
using C4 = NeoEsp32Rmt4Ws2812xMethod;
using C5 = NeoEsp32Rmt5Ws2812xMethod;

// Istanze BUS (ognuna col suo canale RMT)
NeoPixelBus<Feature, C0> center(CENTER_NLED, CENTER_PIN);
NeoPixelBus<Feature, C1> tail0(TAIL_NLED, TAIL_PINS[0]); // 17
NeoPixelBus<Feature, C2> tail1(TAIL_NLED, TAIL_PINS[1]); // 18
NeoPixelBus<Feature, C3> tail2(TAIL_NLED, TAIL_PINS[2]); // 33
NeoPixelBus<Feature, C4> tail3(TAIL_NLED, TAIL_PINS[3]); // 21
NeoPixelBus<Feature, C5> tail4(TAIL_NLED, TAIL_PINS[4]); // 22

// ================== SATURAZIONE (rimpiazzo qadd8/qsub8) ==================
static inline uint8_t sat_add8(uint8_t a, uint8_t b){ uint16_t s = (uint16_t)a + b; return (s > 255) ? 255 : (uint8_t)s; }
static inline uint8_t sat_sub8(uint8_t a, uint8_t b){ return (a > b) ? (uint8_t)(a - b) : 0; }

// ================== STATO: CENTRO (scintillio caldo) ==================
uint8_t c_bri[CENTER_NLED];  // luminanza istantanea con decadimento
uint8_t c_idx[CENTER_NLED];  // indice colore in palette

// ================== STATO: CODA (cometa) ==================
float   tail_pos[TAIL_NSTRIPS];                       // posizione testa (float)
uint8_t tail_trail[TAIL_NSTRIPS] = {7, 8, 6, 9, 7};   // lunghezza scia (px)
uint8_t tail_fade [TAIL_NSTRIPS] = {190,180,170,200,185}; // persistenza scia (0..255)
bool    tail_reverse[TAIL_NSTRIPS] = {false,false,false,false,false};

// attenuazione verso la PUNTA (0..255)
uint8_t tail_end_min[TAIL_NSTRIPS]   = {70, 60, 65, 55, 60};   // quanto resta al tip
uint8_t tail_end_curve[TAIL_NSTRIPS] = {180,200,180,190,200};  // shaping curva

// flicker e “eccezioni” rare
const uint16_t TAIL_EXCEPTION_RATE = 1000;   // 1/1000 frame-pixel senza attenuazione
uint8_t tail_flicker_prob = 40;              // 0..255
uint8_t tail_flicker_min  = 160;             // 160..220
uint8_t tail_flicker_max  = 220;

// -------- Tempi indipendenti per-strip --------
uint8_t  tail_frame_ms[TAIL_NSTRIPS] = { 14, 17, 13, 19, 16 }; // FPS diversi
uint32_t tail_last_ms[TAIL_NSTRIPS]  = {  0,  0,  0,  0,  0 };

// -------- Velocità indipendenti con LFO + jitter lento --------
float    tail_speed_base[TAIL_NSTRIPS] = { 0.18f, 0.26f, 0.34f, 0.21f, 0.30f };
float    tail_lfo_rate[TAIL_NSTRIPS]  = { 0.035f, 0.050f, 0.028f, 0.042f, 0.032f }; // Hz
float    tail_lfo_phase[TAIL_NSTRIPS] = { 0.00f,  1.07f,  2.51f,  4.13f,  5.40f };
uint16_t tail_jitter_ms[TAIL_NSTRIPS]  = { 900, 1100, 1300, 1000, 1200 };
uint32_t tail_last_jitter[TAIL_NSTRIPS]= { 0, 0, 0, 0, 0 };

// ================== PALETTE ==================
// Centro: fuoco caldo
const RgbColor fireWarmPal[16] = {
  RgbColor(0,0,0), RgbColor(20,0,0), RgbColor(80,0,0),  RgbColor(150,0,0),
  RgbColor(255,40,0), RgbColor(255,90,0), RgbColor(255,150,0), RgbColor(255,200,0),
  RgbColor(255,215,40), RgbColor(255,230,100), RgbColor(255,240,160), RgbColor(255,255,220),
  RgbColor(255,200,0), RgbColor(255,120,0), RgbColor(180,20,0), RgbColor(0,0,0)
};
// Coda: rosso → arancio → ciano → bianco
const RgbColor cometPal[16] = {
  RgbColor(0,0,0), RgbColor(150,0,0), RgbColor(255,40,0), RgbColor(255,120,0),
  RgbColor(255,180,40), RgbColor(255,220,120), RgbColor(160,240,255), RgbColor(80,220,255),
  RgbColor(160,230,255), RgbColor(210,240,255), RgbColor(255,255,255), RgbColor(220,240,255),
  RgbColor(255,220,120), RgbColor(255,160,0), RgbColor(255,90,0),  RgbColor(100,0,0)
};

static inline RgbColor palSample(const RgbColor* pal16, uint8_t idx, uint8_t bri=255){
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

// ================== UTILITY ==================

static inline int   clampi(int v, int lo, int hi)   { return (v < lo) ? lo : (v > hi) ? hi : v; }
static inline float clampf(float v, float lo, float hi){ return (v < lo) ? lo : (v > hi) ? hi : v; }

static inline uint8_t randomWarmIndex(){
  uint8_t a = random(120,200);
  uint8_t b = random(200,246);
  return (random(0,256) < 96) ? b : a;
}
static inline uint8_t tipProgress(int s, int i){
  int tipIndex = tail_reverse[s] ? 0 : (TAIL_NLED - 1);
  uint16_t distToTip = abs(i - tipIndex);
  return 255 - (uint8_t)((distToTip * 255) / (TAIL_NLED - 1)); // 0=base, 255=punta
}

// Velocità istantanea con LFO + jitter lento per la strip s
static inline float currentTailSpeed(int s, uint32_t nowMs) {
  float t = nowMs * 0.001f; // secondi
  float lfo = sinf(2.0f * (float)M_PI * tail_lfo_rate[s] * t + tail_lfo_phase[s]); // -1..1
  float v = tail_speed_base[s] * (1.0f + 0.30f * lfo); // ±30%

  // jitter molto lento
  if (nowMs - tail_last_jitter[s] > tail_jitter_ms[s]) {
    tail_last_jitter[s] = nowMs;
    tail_speed_base[s] = clampf(tail_speed_base[s] + ((int)random(-2,3))*0.01f, 0.12f, 0.38f);
    tail_trail[s]      = clampi(tail_trail[s] + (int)random(-1,2), 5, 10);
    tail_fade[s]       = clampi(tail_fade[s]  + (int)random(-10,11), 160, 215);
  }

  return clampf(v, 0.10f, 0.40f);
}


// ================== RENDER: CENTRO ==================
#define CENTER_FRAME_MS          20
#define CENTER_MAX_SPARKS_FRAME   4
#define CENTER_MIN_SPARK_BRI    140
#define CENTER_FADE_PER_FRAME    28
#define CENTER_BACKGROUND_BRI     6

void renderCenter(){
  for (int i=0;i<CENTER_NLED;i++) c_bri[i] = sat_sub8(c_bri[i], CENTER_FADE_PER_FRAME);

  uint8_t sparks = random(0, CENTER_MAX_SPARKS_FRAME + 1);
  for (uint8_t s=0; s<sparks; s++){
    int p = random(0, CENTER_NLED);
    c_idx[p] = randomWarmIndex();
    uint8_t base = random(CENTER_MIN_SPARK_BRI, 256);
    c_bri[p] = sat_add8(c_bri[p], base);
    if (p>0 && random(0,256)<128)            c_bri[p-1] = sat_add8(c_bri[p-1], base/6);
    if (p<CENTER_NLED-1 && random(0,256)<128) c_bri[p+1] = sat_add8(c_bri[p+1], base/6);
  }

  for (int i=0;i<CENTER_NLED;i++){
    uint8_t b = max<uint8_t>(c_bri[i], CENTER_BACKGROUND_BRI);
    center.SetPixelColor(i, palSample(fireWarmPal, c_idx[i], b));
  }
  center.Show();
}

// ================== RENDER: CODA (per BUS) ==================
template<class BUS>
void renderTailStripBus(int s, BUS& bus){
  // Fade scia
  for (int i=0;i<TAIL_NLED;i++){
    RgbColor c = bus.GetPixelColor(i);
    c.R = (uint8_t)(((uint16_t)c.R * tail_fade[s]) / 255);
    c.G = (uint8_t)(((uint16_t)c.G * tail_fade[s]) / 255);
    c.B = (uint8_t)(((uint16_t)c.B * tail_fade[s]) / 255);
    bus.SetPixelColor(i, c);
  }

  // Avanza la testa con velocità istantanea indipendente
  uint32_t _now = millis();
  tail_pos[s] += currentTailSpeed(s, _now);
  if (tail_pos[s] >= (float)TAIL_NLED + 2.0f){
    tail_pos[s] = -random(2,6) * 0.1f;  // riparte da "fuori"
  }

  // Disegna testa + scia
  for (int i=0;i<TAIL_NLED;i++){
    float d = (float)i - tail_pos[s];
    if (d < 0.0f || d > (float)tail_trail[s]) continue;

    float   t   = 1.0f - (d / (float)tail_trail[s]);      // 1..0
    uint8_t bri = (uint8_t)(255.0f * t * t);              // enfasi testa
    uint8_t idx = (uint8_t)(t * 255.0f);
    RgbColor col = palSample(cometPal, idx, bri);

    // attenuazione verso la punta
    uint8_t prog   = tipProgress(s, i);
    uint8_t shaped = (uint8_t)(((uint16_t)prog * tail_end_curve[s]) / 255);
    uint8_t atten  = (uint8_t)(( (uint16_t)255*(255-shaped) + (uint16_t)tail_end_min[s]*shaped )/255);
    bool skipAtten = (random(0, TAIL_EXCEPTION_RATE) == 0);
    if (!skipAtten){
      col = RgbColor((uint8_t)(((uint16_t)col.R*atten)/255),
                     (uint8_t)(((uint16_t)col.G*atten)/255),
                     (uint8_t)(((uint16_t)col.B*atten)/255));
    }

    int pix = tail_reverse[s] ? (TAIL_NLED - 1 - i) : i;
    RgbColor prev = bus.GetPixelColor(pix);
    bus.SetPixelColor(pix, RgbColor(
      sat_add8(prev.R, col.R),
      sat_add8(prev.G, col.G),
      sat_add8(prev.B, col.B)
    ));
  }

  // Flicker leggero
  for (int i=0;i<TAIL_NLED;i++){
    if (random(0,256) < tail_flicker_prob){
      uint8_t att = random(tail_flicker_min, tail_flicker_max+1);
      RgbColor c = bus.GetPixelColor(i);
      bus.SetPixelColor(i, RgbColor(
        (uint8_t)(((uint16_t)c.R*att)/255),
        (uint8_t)(((uint16_t)c.G*att)/255),
        (uint8_t)(((uint16_t)c.B*att)/255)
      ));
    }
  }

  bus.Show();  // aggiorna SOLO questa striscia (RMT dedicato)
}

// ================== SETUP / LOOP ==================
void setup(){
  // meno interrupt = timing più pulito
  WiFi.mode(WIFI_OFF);
  btStop();

  // avvia i bus
  center.Begin();
  tail0.Begin(); tail1.Begin(); tail2.Begin(); tail3.Begin(); tail4.Begin();

  // clear iniziale
  center.ClearTo(RgbColor(0)); center.Show();
  tail0.ClearTo(RgbColor(0)); tail0.Show();
  tail1.ClearTo(RgbColor(0)); tail1.Show();
  tail2.ClearTo(RgbColor(0)); tail2.Show();
  tail3.ClearTo(RgbColor(0)); tail3.Show();
  tail4.ClearTo(RgbColor(0)); tail4.Show();

  // init stato
  for (int i=0;i<CENTER_NLED;i++){ c_bri[i]=0; c_idx[i]=randomWarmIndex(); }
  for (int s=0;s<TAIL_NSTRIPS;s++){
    tail_pos[s]      = - (float)random(0, tail_trail[s]); // partenza sfasata
    tail_last_ms[s]  = 0;
    tail_last_jitter[s] = 0;
  }

  // seme random basato sul rumore (se hai un pin floating, altrimenti lascia così)
  randomSeed(esp_random());
}

void loop(){
  uint32_t now = millis();

  // Centro: frame fisso (20 ms)
  static uint32_t lastCenter = 0;
  if (now - lastCenter >= CENTER_FRAME_MS){
    renderCenter();
    lastCenter = now;
  }

  // Coda: scheduler per-strip (tempi indipendenti)
  if (now - tail_last_ms[0] >= tail_frame_ms[0]) { renderTailStripBus(0, tail0); tail_last_ms[0] = now; }
  if (now - tail_last_ms[1] >= tail_frame_ms[1]) { renderTailStripBus(1, tail1); tail_last_ms[1] = now; }
  if (now - tail_last_ms[2] >= tail_frame_ms[2]) { renderTailStripBus(2, tail2); tail_last_ms[2] = now; }
  if (now - tail_last_ms[3] >= tail_frame_ms[3]) { renderTailStripBus(3, tail3); tail_last_ms[3] = now; }
  if (now - tail_last_ms[4] >= tail_frame_ms[4]) { renderTailStripBus(4, tail4); tail_last_ms[4] = now; }

  delay(1);
}
