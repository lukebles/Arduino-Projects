#pragma once
#include <Arduino.h>

// ================== Task/Effetto base ==================
class IUpdatable {
public:
  virtual ~IUpdatable() {}
  virtual void begin() {}
  virtual void update(uint32_t nowMs) = 0;
};

// ================== Engine ==================
class EffectsEngine {
public:
  static const uint8_t MAX_TASKS = 16;

  void add(IUpdatable* t) {
    if (count < MAX_TASKS) list[count++] = t;
  }

  void begin() {
    for (uint8_t i=0;i<count;i++) list[i]->begin();
  }

  void update(uint32_t nowMs) {
    for (uint8_t i=0;i<count;i++) list[i]->update(nowMs);
  }

private:
  IUpdatable* list[MAX_TASKS]{};
  uint8_t count{0};
};

// ================== PeriodicTask (ogni N ms) ==================
class PeriodicTask : public IUpdatable {
public:
  using Fn = void(*)();

  PeriodicTask(uint32_t periodMs, Fn fn)
    : _period(periodMs), _fn(fn) {}

  void begin() override {
    _last = millis();
  }

  void update(uint32_t nowMs) override {
    if (!_fn) return;
    if (nowMs - _last >= _period) {
      _last += _period;       // drift-safe
      _fn();
    }
  }

private:
  uint32_t _period;
  uint32_t _last{0};
  Fn _fn{nullptr};
};
