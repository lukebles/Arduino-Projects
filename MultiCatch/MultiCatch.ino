#include <LkRadioStructure.h>
#include <LkBlinker.h>
#include <LkHexBytes.h>

struct __attribute__((packed)) packet_for_WebNexus {
    uint16_t activeDiff;
    uint16_t reactiveDiff;
    unsigned long timeDiff;
};

const byte ID_ENERGYSEND = 1; 
const byte ID_LIGHTSEND = 2;

const int LED_PIN = 13;
const int RX_PIN = 11;
const int TX_PIN = 12;
const int BADENIA_PIN = 6;
const int PIEZO_CAPSULA_PIN = 7;

struct __attribute__((packed)) packet_RadioRxEnergy {
    byte sender; // 1 byte
    uint16_t activeCount; // 2 bytes
    uint16_t reactiveCount; // 2 bytes
};

struct __attribute__((packed)) packet_RadioRxLight {
    byte sender; // 1 byte
    uint16_t intensity; // 2 bytes
};

const uint8_t MAX_SERIAL_RX_LEN = LkHexBytes::MaxByteArraySize * 2; 
char hexString[MAX_SERIAL_RX_LEN]; // Buffer per memorizzare il messaggio ricevuto
bool msgFromPcSerialisComplete = false; // Flag per indicare che il messaggio è completo
int COUNTER = 0; // Contatore per la posizione corrente nel buffer

LkRadioStructure<packet_RadioRxEnergy> radio;

// Variabili globali per il calcolo della potenza istantanea
unsigned long prevTime = 0;
uint16_t prevActiveCount = 0;
uint16_t prevReactiveCount = 0;

LkBlinker allarme_badenia(BADENIA_PIN,true);
LkBlinker allarme_piezo(PIEZO_CAPSULA_PIN, false, 4, true, 3000); // 3 kHz sul pin 12, non invertito, con 5 lampeggi
LkMultivibrator spia_segnale_radio(30,MONOSTABLE);

uint16_t intensitaLuminosa = 0;

// codici che se vengono ricevuti sulla seriale
// come primo carattere, eseguono una operazione
#define VIA_RADIO 0xAA
#define SUON_BADENIA 0x01

void setup() {
    pinMode(LED_PIN, OUTPUT);
    pinMode(BADENIA_PIN, OUTPUT);
    //
    Serial.begin(115200);
    while (!Serial) {
        ; 
    }
    //
    radio.globalSetup(2000, TX_PIN, RX_PIN);  
    //
    spia_segnale_radio.start();
    digitalWrite(LED_PIN, HIGH);
}

void loop() {
  allarme_badenia.loop();

  if (spia_segnale_radio.expired()){
    digitalWrite(LED_PIN, LOW);
  }
  // =====================
  // RICEZIONI RADIO
  // =====================
  if (radio.haveRawMessage()) {
    uint8_t rawBuffer[VW_MAX_MESSAGE_LEN];
    uint8_t rawBufferLen;
    radio.getRawBuffer(rawBuffer, rawBufferLen);
    byte sender = rawBuffer[rawBufferLen - 1]; // Il mittente è l'ultimo byte
    // segnala che è stato ricevuto un segnale radio
    spia_segnale_radio.start();
    digitalWrite(LED_PIN,HIGH);
    //
    if (sender == ID_ENERGYSEND) {
      // ==============
      // Dati elettrici
      // ==============
      LkArraylize<packet_RadioRxEnergy> energyConverter;
      packet_RadioRxEnergy rcvdEnergy = energyConverter.deArraylize(rawBuffer);

      unsigned long currentTime = millis();
      if (prevTime != 0) { // Evita il primo calcolo
        unsigned long timeDiff = currentTime - prevTime;
        uint16_t activeDiff = rcvdEnergy.activeCount - prevActiveCount;
        uint16_t reactiveDiff = rcvdEnergy.reactiveCount - prevReactiveCount;
        
        // ====================
        // invio dati a WebNexus
        // ====================
        packet_for_WebNexus packet = {activeDiff, reactiveDiff, timeDiff};
        Serial.write(0xFF); // Byte di sincronizzazione
        Serial.write((uint8_t*)&packet, sizeof(packet));

        // ================
        // allarme badenia
        // ================
        // controlla che i dati siano sensati
        // normalmente activeDiff è dell'ordine da 0-12
        // e timeDiff è dell'ordine di 9000
        if (activeDiff < 3600) {
          if (timeDiff < 3600000) {
            // Calcola la potenza istantanea in watt
            float power = (activeDiff * 3600.0) / (timeDiff / 1000.0); // Wh in W
            // Controlla se la potenza supera la soglia
            if (power > 3990.0) {
              allarme_badenia.enable(); 
              allarme_piezo.enable();
            }                     
          }
        }
      } 
      // Aggiorna i valori precedenti
      prevTime = currentTime;
      prevActiveCount = rcvdEnergy.activeCount;
      prevReactiveCount = rcvdEnergy.reactiveCount;
    } else if (sender == 2) {
      // ==============
      // Dati luminosi
      // ==============
      LkArraylize<packet_RadioRxLight> lightConverter;
      packet_RadioRxLight rcvdLight = lightConverter.deArraylize(rawBuffer);
      //
      intensitaLuminosa = rcvdLight.intensity;
    }

  }

  // ========================================================
  // RICEZIONI DALLA PORTA SERIALE (WebNexus)
  // ******** ANCORA DA SVILUPPARE ***********
  // =========================================================
  if (msgFromPcSerialisComplete) {
    // viene creato un array di bytes di (30) bytes
    byte byteArray[LkHexBytes::MaxByteArraySize];
    // esegue la conversione hexString --> byteArray
    LkHexBytes::hexCharacterStringToBytes(byteArray, hexString);
    // ora ne analizza il contenuto
    if(byteArray[0] == VIA_RADIO){
      // -----------------------------
      // MUST BE SENT BY RADIO
      // -----------------------------
      // each pair of characters equals 1 byte
      uint8_t n_bytes = COUNTER >> 1; 
      // the first character is removed from the count, which is only an 
      // serial communication identifier
      n_bytes -= 1;
      char msg[n_bytes];
      for (int i = 1; i <= n_bytes; i++){
        msg[i-1]=byteArray[i]; 
      }
      vw_send((uint8_t *)msg, n_bytes);
      vw_wait_tx(); // Wait until the whole message is gone
    } else {
      if(byteArray[0] == SUON_BADENIA){
        allarme_badenia.enable(); 
        allarme_piezo.enable();
      }
    }
    // clear the string:
    COUNTER = 0;
    ///msgFromPcSerial[MaxSerialMessageLen] = {0};
    msgFromPcSerialisComplete = false;    
  }  
}

void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    byte theByte = Serial.read();  
    // add it to the msgFromPcSerial:
    // if the incoming character is a newline, 
    // set a flag so the main loop can
    // do something about it:
    if (theByte == '\n') {
      // il \n non viene aggiunto alla stringa
      msgFromPcSerialisComplete = true;
      //Serial.flush();
      //Serial.println();
    } else {
      msgFromPcSerialisComplete = false;
      // only ASCII characters are added without the commands
      // to avoid shattering
      // pairs of characters must be sent to send values from 0 to 255
      // '00' ... 'FF' and terminated with \n (single control character equivalent to 10 in decimal 
      // or to 0a in hexadecimal)
      // example of sending: '0A12BF0067\n'
      if((theByte > 31) && (theByte < 127)){
        hexString[COUNTER]=(char) theByte;
        //Serial.print(msgFromPcSerial[COUNTER]);
        COUNTER++;
      }
    }
  }
}

