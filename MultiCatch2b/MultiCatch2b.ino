#include <LkRadioStructure.h>
#include <LkBlinker.h>

const int LED_PIN = 13;
const int RX_PIN = 11;
const int TX_PIN = 12;
const int BADENIA_PIN = 6;

struct __attribute__((packed)) EnergyDataRx {
    byte sender; // 1 byte
    uint16_t activeCount; // 2 bytes
    uint16_t reactiveCount; // 2 bytes
};

struct __attribute__((packed)) LightDataRx {
    byte sender; // 1 byte
    uint16_t intensity; // 2 bytes
};


const int MAX_MSG_LEN = 100; // Definisci la lunghezza massima del messaggio
char msgFromPcSerial[MAX_MSG_LEN]; // Buffer per memorizzare il messaggio ricevuto
bool msgFromPcSerialisComplete = false; // Flag per indicare che il messaggio è completo
int COUNTER = 0; // Contatore per la posizione corrente nel buffer

LkRadioStructure<EnergyDataRx> radio;

// Variabili globali per il calcolo della potenza istantanea
unsigned long prevTime = 0;
uint16_t prevActiveCount = 0;
uint16_t prevReactiveCount = 0;

LkBlinker allarme_badenia(BADENIA_PIN,true);

uint16_t intensitaLuminosa = 0;

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
}

void loop() {
    allarme_badenia.loop();
    //
    if (radio.haveRawMessage()) {
        uint8_t rawBuffer[VW_MAX_MESSAGE_LEN];
        uint8_t rawBufferLen;
        radio.getRawBuffer(rawBuffer, rawBufferLen);
        byte sender = rawBuffer[rawBufferLen - 1]; // Il mittente è l'ultimo byte

        if (sender == 1) {
            // Dati elettrici
            LkArraylize<EnergyDataRx> energyConverter;
            EnergyDataRx rcvdEnergy = energyConverter.deArraylize(rawBuffer);

            unsigned long currentTime = millis();
            if (prevTime != 0) { // Evita il primo calcolo
              unsigned long timeDiff = currentTime - prevTime;
              uint16_t activeDiff = rcvdEnergy.activeCount - prevActiveCount;
              uint16_t reactiveDiff = rcvdEnergy.reactiveCount - prevReactiveCount;

              // invio su seriale per ESP01 del riassunto dei dati ricevuti
              // Il tipo Serial.write accetta un puntatore a uint8_t, 
              // quindi è necessario fare il cast corretto e assicurarsi 
              // che i tipi dei dati siano coerenti.
              Serial.write((uint8_t*)&activeDiff, sizeof(activeDiff));
              Serial.write((uint8_t*)&reactiveDiff, sizeof(reactiveDiff));
              Serial.write((uint8_t*)&timeDiff, sizeof(timeDiff));
              Serial.write((uint8_t*)&intensitaLuminosa, sizeof(intensitaLuminosa));

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
                  }                     
                }
              }
            }
            // Aggiorna i valori precedenti
            prevTime = currentTime;
            prevActiveCount = rcvdEnergy.activeCount;
        } else if (sender == 2) {
            // Dati luminosi
            LkArraylize<LightDataRx> lightConverter;
            LightDataRx rcvdLight = lightConverter.deArraylize(rawBuffer);
            // l'intensita luminosa viene assegnata ad una variabile globale
            intensitaLuminosa = rcvdLight.intensity;
        }
    }
}

bool freshData(uint16_t activeDiff, uint16_t reactiveDiff, unsigned long timeDiff){
  if (activeDiff < 3600) {
    if (timeDiff < 3600000) {
      return true;
    }    
  }
  return false;
}