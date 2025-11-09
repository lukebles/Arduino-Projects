#include "CenterHueSweep.h"

void CenterHueSweep::trigger() {
  _running = true;
  _t0 = millis();
  _lastStar = _lastTail = 0;
  _tailStep = 0;
  if (_pauseFlag) *_pauseFlag = true;
}

void CenterHueSweep::update(uint32_t now) {
  if (!_running) return;

  uint32_t elapsed = now - _t0;
  if (elapsed >= _duration) {
    _running = false;
    if (_pauseFlag) *_pauseFlag = false;
    return;
  }

  if (_lastStar == 0 || (now - _lastStar) >= (1000u / _starFps)) {
    _lastStar = now;
    float phase01 = (float)elapsed / (float)_duration;
    float hue = fmodf(phase01 * 3.0f, 1.0f);
    __chs_centerSolid(_rainbowWithWhite(hue));
  }

  if (_lastTail == 0 || (now - _lastTail) >= _tailStepMs) {
    _lastTail = now;
    _tailStep = (uint16_t)((_tailStep + 1) % 10000);
    _applyTail(_tailStep, elapsed);
  }
}

void CenterHueSweep::_applyTail(uint16_t step, uint32_t /*elapsedMs*/) {
  const RgbColor TAIL_COLS[5] = {
    RgbColor(255,0,0),
    RgbColor(255,200,0),
    RgbColor(0,255,0),
    RgbColor(0,0,255),
    RgbColor(255,255,255)
  };
  uint8_t litCount = step < 5 ? (step + 1) : 5;

  for (uint8_t s = 0; s < 5; ++s) {
    RgbColor c(0);
    if (litCount < 5) {
      int idxFromHead = (litCount - 1) - s;
      c = (idxFromHead >= 0) ? TAIL_COLS[idxFromHead] : RgbColor(0);
    } else {
      uint8_t idx = (5 + s - (step % 5)) % 5;
      c = TAIL_COLS[idx];
    }
    __chs_setTail(s, c);
  }
}
