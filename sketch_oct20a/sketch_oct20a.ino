#define FASTLED_RMT_BUILTIN_DRIVER 1
#define FASTLED_RMT_MAX_CHANNELS 8
#define FASTLED_ALLOW_INTERRUPTS 0

#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include "esp_bt.h"

#define LEDS_PER_STRIP 10
#define COLOR_ORDER    GRB

CRGB pixels[LEDS_PER_STRIP];

// Creiamo 4 controller con DATA_PIN costante (compile-time)
CLEDController* c19;
CLEDController* c23;
CLEDController* c32;
CLEDController* c33;

void setup() {
  WiFi.mode(WIFI_OFF);
  btStop();
  FastLED.setMaxRefreshRate(100);
  FastLED.setDither(0);

  // Per cloni/timing duri puoi provare WS2811 (400kHz) o lasciare WS2813
  c19 = &FastLED.addLeds<WS2813, 19, COLOR_ORDER>(pixels, LEDS_PER_STRIP);
  c23 = &FastLED.addLeds<WS2813, 23, COLOR_ORDER>(pixels, LEDS_PER_STRIP);
  c32 = &FastLED.addLeds<WS2813, 32, COLOR_ORDER>(pixels, LEDS_PER_STRIP);
  c33 = &FastLED.addLeds<WS2813, 33, COLOR_ORDER>(pixels, LEDS_PER_STRIP);

  FastLED.clear(true);
}

void showOne(CLEDController* ctrl) {
  static uint16_t t=0; t++;
  fill_solid(pixels, LEDS_PER_STRIP, CRGB::Black);
  pixels[(t/5) % LEDS_PER_STRIP] = CRGB::Red;
  ctrl->showLeds(255);           // aggiorna SOLO quel pin
  delay(10);
}

void loop() {
  uint32_t t0 = millis();
  while (millis() - t0 < 1000) showOne(c19);
  delayMicroseconds(300);

  t0 = millis();
  while (millis() - t0 < 1000) showOne(c23);
  delayMicroseconds(300);

  t0 = millis();
  while (millis() - t0 < 1000) showOne(c32);
  delayMicroseconds(300);

  t0 = millis();
  while (millis() - t0 < 1000) showOne(c33);
  delayMicroseconds(300);
}
