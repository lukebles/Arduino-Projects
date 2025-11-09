// ================== INCLUDES ==================
#include <Arduino.h>
#include <NeoPixelBus.h>
#include <WiFi.h>
#include "esp_bt.h"
#include "CenterHueSweep.h"


#include "Effects.h"
#include "Palettes.h"
#include "CenterFlicker.h"
#include "TailComet.h"

// ======================================================
// ===============  CENTER HUE SWEEP (inline) ===========
// ======================================================
// Prototipi glue che definiremo più sotto (dopo i bus)
void __chs_centerSolid(const RgbColor& c);
void __chs_setTail(uint8_t idx, const RgbColor& c);
uint16_t __chs_centerCount();
uint16_t __chs_tailLen();


// ================== CONFIG ==================
// Stella (centro)
static const uint8_t  CENTER_PIN   = 16;
static const uint16_t CENTER_NLED  = 100;

// Coda (5 strisce)
static const uint8_t  TAIL_PINS[5] = {17, 18, 33, 21, 22};
static const uint8_t  TAIL_NSTRIPS = 5;
static const uint8_t  TAIL_NLED    = 14; // <-- cambia se serve

// ================== RMT PER STRISCIA ==================
using Feature = NeoGrbFeature;
using C0 = NeoEsp32Rmt0Ws2812xMethod;
using C1 = NeoEsp32Rmt1Ws2812xMethod;
using C2 = NeoEsp32Rmt2Ws2812xMethod;
using C3 = NeoEsp32Rmt3Ws2812xMethod;
using C4 = NeoEsp32Rmt4Ws2812xMethod;
using C5 = NeoEsp32Rmt5Ws2812xMethod;

// ================== BUS LED ==================
NeoPixelBus<Feature, C0> centerBus(CENTER_NLED, CENTER_PIN);
NeoPixelBus<Feature, C1> tailBus0(TAIL_NLED, TAIL_PINS[0]);
NeoPixelBus<Feature, C2> tailBus1(TAIL_NLED, TAIL_PINS[1]);
NeoPixelBus<Feature, C3> tailBus2(TAIL_NLED, TAIL_PINS[2]);
NeoPixelBus<Feature, C4> tailBus3(TAIL_NLED, TAIL_PINS[3]);
NeoPixelBus<Feature, C5> tailBus4(TAIL_NLED, TAIL_PINS[4]);

// ============ GLUE per CenterHueSweep (usa i bus sopra) ============
void __chs_centerSolid(const RgbColor& c) {
  for (uint16_t i = 0; i < CENTER_NLED; ++i) centerBus.SetPixelColor(i, c);
  centerBus.Show();
}
void __chs_setTail(uint8_t idx, const RgbColor& c) {
  // mappa a mano i 5 bus (ordine 0..4)
  switch (idx) {
    case 0: for (uint16_t p=0;p<TAIL_NLED;++p) tailBus0.SetPixelColor(p,c); tailBus0.Show(); break;
    case 1: for (uint16_t p=0;p<TAIL_NLED;++p) tailBus1.SetPixelColor(p,c); tailBus1.Show(); break;
    case 2: for (uint16_t p=0;p<TAIL_NLED;++p) tailBus2.SetPixelColor(p,c); tailBus2.Show(); break;
    case 3: for (uint16_t p=0;p<TAIL_NLED;++p) tailBus3.SetPixelColor(p,c); tailBus3.Show(); break;
    case 4: for (uint16_t p=0;p<TAIL_NLED;++p) tailBus4.SetPixelColor(p,c); tailBus4.Show(); break;
    default: break;
  }
}
uint16_t __chs_centerCount() { return CENTER_NLED; }
uint16_t __chs_tailLen()     { return TAIL_NLED;  }

// ================== ENGINE & EFFETTI ==================
EffectsEngine engine;
// Flag condivisa per “bloccare” il centro quando gira l’arcobaleno
volatile bool g_centerPaused = false;

// Effetti esistenti
CenterFlickerEffect centerEffect(centerBus, CENTER_NLED);

// NEW: CenterHueSweep (10s)
CenterHueSweep centerRainbow(centerBus, CENTER_NLED, 10000, &g_centerPaused);

// Adapter per i bus della coda
BusAdapter<decltype(tailBus0)> a0(tailBus0);
BusAdapter<decltype(tailBus1)> a1(tailBus1);
BusAdapter<decltype(tailBus2)> a2(tailBus2);
BusAdapter<decltype(tailBus3)> a3(tailBus3);
BusAdapter<decltype(tailBus4)> a4(tailBus4);

// Parametri individuali per le 5 strisce (copiati dal tuo codice)
TailParams p0{ .frameMs=14, .trail=7, .fade=190, .speedBase=0.18f, .lfoRate=0.035f, .lfoPhase=0.00f,
               .jitterMs=900,  .endMin=70, .endCurve=180, .reverse=false };
TailParams p1{ .frameMs=17, .trail=8, .fade=180, .speedBase=0.26f, .lfoRate=0.050f, .lfoPhase=1.07f,
               .jitterMs=1100, .endMin=60, .endCurve=200, .reverse=false };
TailParams p2{ .frameMs=13, .trail=6, .fade=170, .speedBase=0.34f, .lfoRate=0.028f, .lfoPhase=2.51f,
               .jitterMs=1300, .endMin=65, .endCurve=180, .reverse=false };
TailParams p3{ .frameMs=19, .trail=9, .fade=200, .speedBase=0.21f, .lfoRate=0.042f, .lfoPhase=4.13f,
               .jitterMs=1000, .endMin=55, .endCurve=190, .reverse=false };
TailParams p4{ .frameMs=16, .trail=7, .fade=185, .speedBase=0.30f, .lfoRate=0.032f, .lfoPhase=5.40f,
               .jitterMs=1200, .endMin=60, .endCurve=200, .reverse=false };

TailCometEffect tail0(a0, TAIL_NLED, p0);
TailCometEffect tail1(a1, TAIL_NLED, p1);
TailCometEffect tail2(a2, TAIL_NLED, p2);
TailCometEffect tail3(a3, TAIL_NLED, p3);
TailCometEffect tail4(a4, TAIL_NLED, p4);

// Esempio di task “periodico ogni minuto”: variazione flicker + trigger arcobaleno
void everyMinuteTask() {
  uint8_t prob = random(25, 50);
  tail0.setFlicker(prob, 160, 220);
  tail1.setFlicker(prob, 160, 220);
  tail2.setFlicker(prob, 160, 220);
  tail3.setFlicker(prob, 160, 220);
  tail4.setFlicker(prob, 160, 220);

  // TRIGGER dell’arcobaleno sul centro (stella + inseguimento coda)
  centerRainbow.trigger();
}

PeriodicTask minuteTick(60000, everyMinuteTask); // 60s

// ================== SETUP ==================
void setup() {
  // timing pulito
  WiFi.mode(WIFI_OFF);
  btStop();

  randomSeed(esp_random());

  // Init bus
  centerBus.Begin();
  tailBus0.Begin(); tailBus1.Begin(); tailBus2.Begin(); tailBus3.Begin(); tailBus4.Begin();

  centerBus.ClearTo(RgbColor(0)); centerBus.Show();
  tailBus0.ClearTo(RgbColor(0)); tailBus0.Show();
  tailBus1.ClearTo(RgbColor(0)); tailBus1.Show();
  tailBus2.ClearTo(RgbColor(0)); tailBus2.Show();
  tailBus3.ClearTo(RgbColor(0)); tailBus3.Show();
  tailBus4.ClearTo(RgbColor(0)); tailBus4.Show();

  // Registra flag di pausa nel flicker del centro
  centerEffect.attachPauseFlag(&g_centerPaused);

  // Registra effetti
  engine.add(&centerEffect);
  engine.add(&centerRainbow);   // NEW
  engine.add(&tail0); engine.add(&tail1); engine.add(&tail2); engine.add(&tail3); engine.add(&tail4);
  engine.add(&minuteTick);      // task periodico

  engine.begin();
}

// ================== LOOP ==================
void loop() {
  engine.update(millis());
  delay(1);
}
