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

#include <VirtualWire.h>
#include <LkArraylize.h>

class RadioInitializer {
public:
    static void initialize(int speed, uint8_t transmit_pin, uint8_t receive_pin) {
        static bool isInitialized = false;
        if (!isInitialized) {
            vw_set_tx_pin(transmit_pin);
            vw_set_rx_pin(receive_pin);
            vw_setup(speed);
            vw_rx_start();
            isInitialized = true;
        }
    }
};

template<class T>     // C++ template syntax
class LkRadioStructure{

private:
  T _retStructure;
  bool _have_structure = false;
  unsigned long _microsec;

public:
  static void globalSetup(int speed, uint8_t transmit_pin, uint8_t receive_pin) {
        RadioInitializer::initialize(speed, transmit_pin, receive_pin);
  }

  static void globalSetup(int speed, uint8_t transmit_pin, uint8_t receive_pin, uint8_t ptt_pin, bool ptt_inverted) {
      vw_set_ptt_pin(ptt_pin);
      vw_set_ptt_inverted(ptt_inverted);   
      globalSetup(speed, transmit_pin, receive_pin);
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


  bool have_structure(){
    uint8_t buf[VW_MAX_MESSAGE_LEN];
    uint8_t buflen = VW_MAX_MESSAGE_LEN;

    if (vw_have_message()) {
      _microsec = micros(); // Track the microseconds at which the signal was received.
      if (vw_get_message(buf, &buflen)) {
        if(buflen == sizeof(T)){
          LkArraylize<T> T_o;
          _retStructure = T_o.deArraylize(buf);
          _have_structure = true;
        } 
      } 
    } 
    return _have_structure;
  }

  T getStructure(){
    _have_structure = false;
    return _retStructure;
  }
  
  void sendStructure(const T& value){
    uint8_t buf[sizeof(T)];
    LkArraylize<T> T_o;
    T_o.arraylize(buf, value);
    //
    vw_send((uint8_t *)buf, sizeof(buf));
    vw_wait_tx();
  }

  unsigned long getMicrosec() const {
    return _microsec;
  }
};

