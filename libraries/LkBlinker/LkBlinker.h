/*
 * Quando enable() viene chiamato, il LED inizia a lampeggiare. 
 * La logica di lampeggio è controllata dai multivibratori fa e fb.
 * Il LED lampeggia un certo numero di volte (controllato da maxBlinkCount), 
 * dopodiché si ferma automaticamente.
*/

#include <LkMultivibrator.h>

LkMultivibrator fa(500, MONOSTABLE);
LkMultivibrator fb(500, MONOSTABLE);

class LkBlinker {
  private:
    int _maxBlinkCount;
    int _currentBlinkCount = 0;
    int _pinBlink;
    bool _isEnabled = false;
    bool _isInverted = false;

    void setLedState(bool state) {
      digitalWrite(_pinBlink, (_isInverted != state) ? HIGH : LOW);
    }

  public:
    // Aggiunta del parametro _maxBlinkCount con un valore di default a 3
    LkBlinker(int pinBlink, bool isInverted, int maxBlinkCount = 3) :
      _pinBlink(pinBlink),
      _isInverted(isInverted),
      _maxBlinkCount(maxBlinkCount)
    {
      pinMode(_pinBlink, OUTPUT);
      setLedState(false);
    }

    void loop() {
      if (_isEnabled) {
        if (fa.expired()) {
          setLedState(true);
          fb.start();
          _currentBlinkCount++;
        }

        if (fb.expired()) {
          setLedState(false);
          if (_currentBlinkCount >= _maxBlinkCount) {
            _isEnabled = false;
            _currentBlinkCount = 0;
          } else {
            fa.start();
          }
        }
      }
    }

    void enable() {
      fa.start();
      _isEnabled = true;
      _currentBlinkCount = 0;  // Resetta il contatore dei lampeggi ogni volta che abiliti
    }
};



