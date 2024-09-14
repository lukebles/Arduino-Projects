
// LkMultivibrator.h

/*
 * 
 * This class implements a multivibrator, configurable as either astable or monostable.
 * An astable multivibrator continuously oscillates, whereas a monostable multivibrator 
 * stops oscillating after a set period.
 * 
 * Key methods:
 *  - start(): Initiates the multivibrator.
 *  - stop(): Stops the multivibrator.
 *  - expired(): Checks if the monostable multivibrator's time has expired.
 * 
 * Time overflow handling is implemented to prevent unexpected behavior 
 * after extended periods of operation.
 */

#ifndef LK_MULTIVIBRATOR_H  
#define LK_MULTIVIBRATOR_H 

enum MultivibratorType { ASTABLE, MONOSTABLE };

class LkMultivibrator {
    const unsigned long _delayMs;
    unsigned long _previousMillis;
    MultivibratorType _type;
    bool _flag = false;
    bool _running = true;

public:
    LkMultivibrator(unsigned long delayMs, MultivibratorType type) :
        _delayMs(delayMs), _type(type), _previousMillis(millis()) {
    }

    void stop() {
        _running = false;
    }

    void start() {
        _running = true;
        _flag = false;
        _previousMillis = millis();
    }

    bool expired() {
        if (!_running) return false;

        unsigned long currentMillis = millis();

        // Handle overflow
        if (currentMillis < _previousMillis) _previousMillis = currentMillis;

        if ((currentMillis - _previousMillis) >= _delayMs) {
            _previousMillis = currentMillis; // reset timer

            if (_type == ASTABLE) {
                return true;
            } else {
                if (!_flag) {
                    _flag = true;
                    return true;
                }
            }
        }
        return false;
    }
};

#endif // LK_MULTIVIBRATOR_H
