/*
Compilare su ESP01s usando l'opzione FLASH SIZE 1M - FS: 256KB - OTA 374KB)
per installare il filesystem usare Upload Little FS tramite SHIFT-CMD-P

Il programma serve per testare unicamente la parte radio
e vedere se il lampeggio viene ricevuto senza saltarne alcuno
*/

#include <Arduino.h>
//======
#include <LkRadioStructureEx.h>
#include <LkBlinker.h>

struct __attribute__((packed)) packet_RadioRxEnergy {
    byte sender;
    uint16_t activeCount;
    uint16_t reactiveCount;
};

const byte ID_ENERGYSEND = 1; //1 Luca 11 Marco
const int LED_PIN = 2; // GPIO2 per ESP01
const int RX_PIN = 0; // GPIO0 dove è collegato il modulo radio RX
const int TX_PIN = -1; // non è presente nessuno modulo radio TX
const bool PIN_INVERTED = true;

const bool LEDPIN_HIGH = LOW;
const bool LEDPIN_LOW = HIGH;

unsigned long prevTime = 0;
uint16_t prevActiveCount = 0;
uint16_t prevReactiveCount = 0;

LkMultivibrator timer_radioCheck(27000,MONOSTABLE);
LkMultivibrator timer_flashSegnaleRadio(30, MONOSTABLE);

LkRadioStructureEx<packet_RadioRxEnergy> radio;

void setup() {
  
    Serial.begin(115200);
    while (!Serial) {
        ; 
    }
    Serial.println();
    Serial.println("Start");
    // led pin
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LEDPIN_LOW);
    delay(600);
    digitalWrite(LED_PIN, LEDPIN_LOW);

    radio.globalSetup(2000, TX_PIN, RX_PIN);
    //
    timer_flashSegnaleRadio.start();

}

void loop() {
  // flash che un segnale radio è stato ricevuto
  if (timer_flashSegnaleRadio.expired()) {
      digitalWrite(LED_PIN, LEDPIN_LOW);
  }
  // controllo del segnale radio
  handleRadioReception();
}

void handleRadioReception() {
    if (radio.haveRawMessage()) {
        uint8_t rawBuffer[VW_MAX_MESSAGE_LEN];
        uint8_t rawBufferLen;
        radio.getRawBuffer(rawBuffer, rawBufferLen);
        byte sender = rawBuffer[rawBufferLen - 1];
        timer_flashSegnaleRadio.start();
        digitalWrite(LED_PIN, LEDPIN_HIGH);
        // via radio è stato ricevuto qualcosa
        if (sender == ID_ENERGYSEND) {
            // il pacchetto è quello di EnergySend (1=Luca, 11=Marco)
            LkArraylize<packet_RadioRxEnergy> energyConverter;
            packet_RadioRxEnergy rcvdEnergy = energyConverter.deArraylize(rawBuffer);
            handleEnergyData(rcvdEnergy);
        }
    }
}

void handleEnergyData(packet_RadioRxEnergy& rcvdEnergy) {
    unsigned long currentTime = millis();
    if (prevTime != 0) {
      unsigned long timeDiff = currentTime - prevTime;
      uint16_t activeDiff = rcvdEnergy.activeCount - prevActiveCount;
      uint16_t reactiveDiff = rcvdEnergy.reactiveCount - prevReactiveCount;

      Serial.print(activeDiff);
      Serial.print(" - ");
      Serial.print(timeDiff);

      if (activeDiff < 3600) {
        if (timeDiff < 3600000){
          float power = (activeDiff * 3600.0) / (timeDiff / 1000.0);
 
          Serial.print(" - ");
          Serial.print(power);

        }
      }
    }
    prevTime = currentTime;
    prevActiveCount = rcvdEnergy.activeCount;
    prevReactiveCount = rcvdEnergy.reactiveCount;
}
