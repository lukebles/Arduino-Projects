// LkRadioStructure.h
//
// The LkRadioStructure library offers a streamlined and efficient way to transmit 
// arbitrary data structures through radio communication using the VirtualWire library. 
// It utilizes the LkArraylize template to seamlessly convert data structures into byte arrays 
// and back, ensuring compatibility with the transmission protocols of VirtualWire. 
// This abstraction not only simplifies the process of radio communication in Arduino projects 
// but also enhances code readability and maintainability. Whether sending sensor readings, 
// control commands, or any custom data packets, LkRadioStructure handles the complexity 
// of data serialization and radio transmission, enabling developers to focus on 
// the core functionality of their applications.

// ===============================

// La libreria LkRadioStructure offre un modo semplificato ed efficiente per trasmettere 
// strutture di dati arbitrarie attraverso la comunicazione radio utilizzando la libreria VirtualWire. 
// Utilizza il template LkArraylize per convertire senza problemi le strutture di dati in array di byte 
// e viceversa, garantendo la compatibilità con i protocolli di trasmissione di VirtualWire. 
// Questa astrazione non solo semplifica il processo di comunicazione radio nei progetti Arduino, 
// ma migliora anche la leggibilità e la manutenibilità del codice. Che si tratti di inviare letture di sensori, 
// comandi di controllo o qualsiasi pacchetto di dati personalizzato, LkRadioStructure gestisce la complessità 
// della serializzazione dei dati e della trasmissione radio, consentendo agli sviluppatori di concentrarsi sulla 
// funzionalità principale delle loro applicazioni.

#ifndef LK_RADIOSTRUCTURE_H
#define LK_RADIOSTRUCTURE_H

#include <VirtualWire.h>
#include "LkArraylize.h"

class RadioInitializer {
public:
    static void initialize(int speed, int8_t transmit_pin = -1, int8_t receive_pin = -1, int8_t ptt_pin = -1, bool ptt_inverted = false) {
        static bool isInitialized = false;
        if (!isInitialized) {
            if (transmit_pin != -1) {
                vw_set_tx_pin(transmit_pin);
            }
            if (receive_pin != -1) {
                vw_set_rx_pin(receive_pin);
            }
            if (ptt_pin != -1) {
                vw_set_ptt_pin(ptt_pin);
                vw_set_ptt_inverted(ptt_inverted);
            }
            vw_setup(speed);
            if (receive_pin != -1) {
                vw_rx_start();
            }
            isInitialized = true;
        }
    }
};

template<class T>
class LkRadioStructure {
private:
    T _retStructure;
    bool _have_structure = false;
    unsigned long _microsec = 0;
    uint8_t _rawBuffer[VW_MAX_MESSAGE_LEN];
    uint8_t _rawBufferLen = 0;

public:
    static void globalSetup(int speed, int8_t transmit_pin = -1, int8_t receive_pin = -1, int8_t ptt_pin = -1, bool ptt_inverted = false) {
        RadioInitializer::initialize(speed, transmit_pin, receive_pin, ptt_pin, ptt_inverted);
    }

    bool haveRawMessage() {
        uint8_t buf[VW_MAX_MESSAGE_LEN];
        uint8_t buflen = VW_MAX_MESSAGE_LEN;

        if (vw_have_message()) {
            _microsec = micros(); // Track the microseconds at which the signal was received
            if (vw_get_message(buf, &buflen)) {
                memcpy(_rawBuffer, buf, buflen);
                _rawBufferLen = buflen;
                _have_structure = true;
                return true;
            }
        }
        return false;
    }

    void getRawBuffer(uint8_t* buffer, uint8_t &length) const {
        memcpy(buffer, _rawBuffer, _rawBufferLen);
        length = _rawBufferLen;
    }

    T getStructure() {
        LkArraylize<T> T_o;
        _retStructure = T_o.deArraylize(_rawBuffer);
        _have_structure = false;
        return _retStructure;
    }

    void sendStructure(const T& value) {
        uint8_t buf[sizeof(T)];
        LkArraylize<T> T_o;
        T_o.arraylize(buf, value);

        vw_send(buf, sizeof(buf));
        vw_wait_tx();
    }

    unsigned long getMicrosec() const {
        return _microsec;
    }
};

#endif // LK_RADIOSTRUCTURE_H


