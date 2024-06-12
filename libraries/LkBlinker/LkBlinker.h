/*
 * Quando enable() viene chiamato, il LED inizia a lampeggiare. 
 * La logica di lampeggio è controllata dai multivibratori fa e fb.
 * Il LED lampeggia un certo numero di volte (controllato da maxBlinkCount), 
 * dopodiché si ferma automaticamente.
*/
#ifndef LKBLINKER_H
#define LKBLINKER_H

#include <LkMultivibrator.h>

class LkBlinker {
  private:
    int _maxBlinkCount;
    int _currentBlinkCount = 0;
    int _pin;
    bool _isEnabled = false;
    bool _inverted = false;
    bool _vibrating = false;
    int _frequency;
    LkMultivibrator _blinkOnTimer;
    LkMultivibrator _blinkOffTimer;

    void setLedState(bool state) {
      if (state) {
        if (_vibrating) {
          // Attiva il tono alla frequenza specificata
          tone(_pin, _frequency);
        } else {
          // Accendi il LED fisso
          digitalWrite(_pin, _inverted ? LOW : HIGH);
        }
      } else {
        // Disattiva il tono e spegni il LED
        noTone(_pin);
        digitalWrite(_pin, _inverted ? HIGH : LOW);
      }
    }

  public:
    // Costruttore con parametri
    LkBlinker(int pin, bool inverted, int maxBlinkCount = 3, bool vibrating = false, int frequency = 3000)
      : _pin(pin), _inverted(inverted), _maxBlinkCount(maxBlinkCount), _vibrating(vibrating), _frequency(frequency), 
        _blinkOnTimer(500, MONOSTABLE), _blinkOffTimer(500, MONOSTABLE) {
      pinMode(_pin, OUTPUT);
      setLedState(false);
    }

    void loop() {
      if (_isEnabled) {
        if (_blinkOnTimer.expired()) {
          setLedState(true);
          _blinkOffTimer.start();
          _currentBlinkCount++;
        }

        if (_blinkOffTimer.expired()) {
          setLedState(false);
          if (_currentBlinkCount >= _maxBlinkCount) {
            _isEnabled = false;
            _currentBlinkCount = 0;
          } else {
            _blinkOnTimer.start();
          }
        }
      }
    }

    void enable() {
      _blinkOnTimer.start();
      _isEnabled = true;
      _currentBlinkCount = 0;  // Resetta il contatore dei lampeggi ogni volta che abiliti
    }
};


#endif // LKBLINKER_H


