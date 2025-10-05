#pragma once
#if !defined(ARDUINO_ARCH_ESP32)
  #error "Seleziona una scheda ESP32 in Strumenti → Scheda"
#endif

#include <Arduino.h>
#include "esp32-hal-adc.h"  // analogSetPinAttenuation, adc_attenuation_t

class LdrBlinkDetector {
public:
  enum LightState : uint8_t { OFF=0, STEADY_ON=1, BLINKING=2 };

  struct Config {
    int pin;
    uint8_t adcBits;
    adc_attenuation_t attenuation;
    int thOn;
    int thOff;
    uint32_t sampleMs;
    uint32_t minFlashPeriodMs;
    uint32_t maxFlashPeriodMs;
    uint32_t steadyOnMs;
    uint32_t steadyOffMs;
    uint8_t  flashScoreTarget;

    Config()
      : pin(34),
        adcBits(12),
        attenuation(ADC_11db),
        thOn(20),
        thOff(0),
        sampleMs(5),
        minFlashPeriodMs(80),
        maxFlashPeriodMs(500),
        steadyOnMs(2000),
        steadyOffMs(2000),
        flashScoreTarget(2) {}
  };

  using OnChangeFn  = void (*)(LightState newState, int raw);
  using OnChangeFn2 = void (*)(LightState oldState, LightState newState, int raw);

  LdrBlinkDetector() : cfg_() {}
  explicit LdrBlinkDetector(const Config& cfg) : cfg_(cfg) {}

  void begin() {
    pinMode(cfg_.pin, INPUT);                 // 34..39: niente pullup interni
    analogReadResolution(cfg_.adcBits);
    analogSetPinAttenuation(cfg_.pin, cfg_.attenuation);
    lastSampleMs_ = millis();
  }

  // Chiama spesso (non blocca). Ritorna true se stato cambiato.
  bool update() {
    uint32_t now = millis();
    if ((uint32_t)(now - lastSampleMs_) < cfg_.sampleMs) return false;
    lastSampleMs_ = now;

    lastRaw_ = analogRead(cfg_.pin);
    bool newLevel = levelFromRaw_(lastRaw_, level_);

    // Edge detection
    if (newLevel != level_) {
      lastLevel_ = level_;
      level_ = newLevel;
      lastEdgeMs_ = now;

      if (level_) { // rising edge
        if (haveRise_) {
          uint32_t period = now - lastRiseMs_;
          if (period >= cfg_.minFlashPeriodMs && period <= cfg_.maxFlashPeriodMs) {
            if (flashScore_ < 255) ++flashScore_;
          } else if (flashScore_ > 0) {
            --flashScore_;
          }
        }
        lastRiseMs_ = now;
        haveRise_ = true;
      }
    }

    // Classificazione con priorità agli stati stabili + decadimento score
    LightState newState = state_;
    if (!level_ && (now - lastEdgeMs_ >= cfg_.steadyOffMs)) {
      newState = OFF;
      flashScore_ = 0;
    } else if (level_ && (now - lastEdgeMs_ >= cfg_.steadyOnMs)) {
      newState = STEADY_ON;
      if (flashScore_ > 0) --flashScore_;                // degrada lo score quando rimane fissa
    } else if (flashScore_ >= cfg_.flashScoreTarget &&
               (now - lastRiseMs_) <= (cfg_.maxFlashPeriodMs * 2)) {
      newState = BLINKING;
    } else {
      // se i lampi non sono recenti, lo score decade
      if ((now - lastRiseMs_) > (cfg_.maxFlashPeriodMs * 2) && flashScore_ > 0) --flashScore_;
    }

    if (newState != state_) {
      LightState oldState = state_;
      state_ = newState;
      if (onChange_)  onChange_(state_, lastRaw_);
      if (onChange2_) onChange2_(oldState, state_, lastRaw_);
      return true;
    }
    return false;
  }

  // --- Query comode ---
  LightState getState() const { return state_; }
  const char* stateName() const {
    switch (state_) {
      case OFF:        return "SPENTA";
      case STEADY_ON:  return "ACCESA FISSA";
      case BLINKING:   return "LAMPEGGIANTE";
    }
    return "?";
  }
  int  stateCode() const     { return (int)state_; } // 0/1/2
  bool isOff() const         { return state_ == OFF; }
  bool isSteadyOn() const    { return state_ == STEADY_ON; }
  bool isBlinking() const    { return state_ == BLINKING; }
  bool isOn() const          { return state_ != OFF; }

  int  raw()   const         { return lastRaw_; }
  bool level() const         { return level_; }   // dopo isteresi
  const Config& config()const{ return cfg_; }

  // Callbacks
  void setOnChange (OnChangeFn  cb) { onChange_  = cb; }
  void setOnChange2(OnChangeFn2 cb) { onChange2_ = cb; }

  // Tuning
  void setThresholds(int thOn, int thOff)                 { cfg_.thOn = thOn; cfg_.thOff = thOff; }
  void setFlashRange(uint32_t minMs, uint32_t maxMs)      { cfg_.minFlashPeriodMs = minMs; cfg_.maxFlashPeriodMs = maxMs; }
  void setSteadyWindows(uint32_t onMs, uint32_t offMs)    { cfg_.steadyOnMs = onMs; cfg_.steadyOffMs = offMs; }
  void setSampleMs(uint32_t ms)                           { cfg_.sampleMs = ms; }

private:
  bool levelFromRaw_(int raw, bool last) const {
    if (raw >= cfg_.thOn)  return true;
    if (raw <= cfg_.thOff) return false;
    return last; // isteresi tra thOff e thOn
  }

  Config     cfg_;
  uint32_t   lastSampleMs_ = 0;
  int        lastRaw_      = 0;
  bool       level_        = false, lastLevel_ = false;
  uint32_t   lastEdgeMs_   = 0;
  uint32_t   lastRiseMs_   = 0;
  bool       haveRise_     = false;
  uint8_t    flashScore_   = 0;
  LightState state_        = OFF;

  OnChangeFn  onChange_  = nullptr;
  OnChangeFn2 onChange2_ = nullptr;
};



