#ifndef LK_RADIOSTRUCTURE_RH_H
#define LK_RADIOSTRUCTURE_RH_H

#include <Arduino.h>
#include <RH_ASK.h>
#include <SPI.h>          // richiesto dalla libreria RadioHead su alcune piattaforme
#include "LkArraylize.h"  // come nel tuo progetto

// In RadioHead la massima lunghezza del payload è RH_ASK_MAX_MESSAGE_LEN.
// Attenzione: includere eventuali header tuoi dentro a T resta a tuo carico.

class RadioInitializer {
public:
    // Crea e inizializza una singola istanza globale di RH_ASK
    // speed in bps (es. 2000), pin -1 = usa i default della libreria
    static void initialize(uint16_t speed,
                           int8_t transmit_pin = -1,
                           int8_t receive_pin  = -1,
                           int8_t ptt_pin      = -1,
                           bool   ptt_inverted = false);

    // Accesso all'istanza del driver (già inizializzata)
    static RH_ASK& driver();

private:
    static RH_ASK* s_driver;
    static bool    s_initialized;
};

template<class T>
class LkRadioStructure {
private:
    T _retStructure;
    bool _have_structure = false;
    unsigned long _microsec = 0;

    // buffer "grezzo" dell’ultimo pacchetto ricevuto
    uint8_t _rawBuffer[RH_ASK_MAX_MESSAGE_LEN];
    uint8_t _rawBufferLen = 0;

public:
    // Setup globale del radio (una sola volta)
    static void globalSetup(uint16_t speed,
                            int8_t transmit_pin = -1,
                            int8_t receive_pin  = -1,
                            int8_t ptt_pin      = -1,
                            bool   ptt_inverted = false)
    {
        RadioInitializer::initialize(speed, transmit_pin, receive_pin, ptt_pin, ptt_inverted);
    }

    // Ritorna true se c’è un messaggio e lo copia nel buffer interno
    bool haveRawMessage() {
        RH_ASK& drv = RadioInitializer::driver();
        if (drv.available()) {
            uint8_t buflen = RH_ASK_MAX_MESSAGE_LEN;
            if (drv.recv(_rawBuffer, &buflen)) {
                _rawBufferLen   = buflen;
                _have_structure = true;
                _microsec       = micros();  // timestamp ricezione
                return true;
            }
        }
        return false;
    }

    void getRawBuffer(uint8_t* buffer, uint8_t &length) const {
        memcpy(buffer, _rawBuffer, _rawBufferLen);
        length = _rawBufferLen;
    }

    // Deserializza il buffer ricevuto in una struttura T
    T getStructure() {
        LkArraylize<T> T_o;
        _retStructure = T_o.deArraylize(_rawBuffer);
        _have_structure = false;
        return _retStructure;
    }

    // Serializza e invia una struttura T
    void sendStructure(const T& value) {
        // opzionale: verifica a compile-time per non superare la MTU del driver
        static_assert(sizeof(T) <= RH_ASK_MAX_MESSAGE_LEN,
                      "T troppo grande per RH_ASK_MAX_MESSAGE_LEN");

        uint8_t buf[sizeof(T)];
        LkArraylize<T> T_o;
        T_o.arraylize(buf, value);

        RH_ASK& drv = RadioInitializer::driver();
        drv.send(buf, sizeof(buf));
        drv.waitPacketSent();
    }

    unsigned long getMicrosec() const {
        return _microsec;
    }
};

#endif // LK_RADIOSTRUCTURE_RH_H
