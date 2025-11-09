#include <Arduino.h>
#include "LkRadioStructure_RH.h"

RH_ASK* RadioInitializer::s_driver      = nullptr;
bool    RadioInitializer::s_initialized = false;

void RadioInitializer::initialize(uint16_t speed,
                                  int8_t transmit_pin,
                                  int8_t receive_pin,
                                  int8_t ptt_pin,
                                  bool   ptt_inverted)
{
    if (s_initialized) return;

    // Caso A: nessun pin specificato -> usa i default interni di RH_ASK
    if (transmit_pin < 0 && receive_pin < 0 && ptt_pin < 0) {
        s_driver = new RH_ASK(speed); // usa i default della libreria
    } else {
        // Caso B: voglio specificare i pin -> richiede TUTTI i pin
        // scegli tu dei default sensati se alcuni restano -1
        uint8_t tx  = (transmit_pin >= 0) ? (uint8_t)transmit_pin : 12; // default classici AVR
        uint8_t rx  = (receive_pin  >= 0) ? (uint8_t)receive_pin  : 11;
        uint8_t ptt = (ptt_pin      >= 0) ? (uint8_t)ptt_pin      : 10;

        s_driver = new RH_ASK(speed, rx, tx, ptt, ptt_inverted);
    }

    (void)s_driver->init();
    s_initialized = true;
}

RH_ASK& RadioInitializer::driver() {
    if (!s_initialized) {
        initialize(2000); // fallback di sicurezza
    }
    return *s_driver;
}
