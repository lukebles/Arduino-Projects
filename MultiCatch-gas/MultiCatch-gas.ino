
// ====== STRUTTURA DEI DIALOGHI ==========
// handleEnergyData     -> seriale -> pacchetto energia a WebNexus
// checkString          <- seriale <- comandi inviati da WebNexus
// handleRadioReception <- radio   <- discrimina con l'ID il pacchetto radio ricevuto
//                                    ed invia coerentemente la struttura a WebNexus

///////////////////////////////////////////////
// la personalizzazione Luca/Marco riguarda
// - ID_ENERGYSEND
// - allarme_segnalatore
//////////////////////////////////////////////


#include <LkRadioStructure.h>
#include <LkBlinker.h>
#include <EEPROM.h>

// Strutture dei pacchetti
struct __attribute__((packed)) packet_for_WebNexus {
    uint16_t termoDiff;
    uint16_t sanitariaDiff;
    unsigned long timeDiff;
};



struct __attribute__((packed)) packet_contatori {
    byte sender; // 1 byte
    uint16_t sanitariaCount; // 2 bytes
    uint16_t termoCount; // 2 bytes
};

struct __attribute__((packed)) packet_sanitaria_termo {
    byte sender; // 1 byte
    byte stato; // 1byte 0=discesa 1=costante 2=salita
    int16_t temperaturaCaldaC; // 2byte
};

struct __attribute__((packed)) packet_ambiente {
    byte sender; // 1 byte
    int16_t temperaturaC; // 2 byte
};

LkRadioStructure<packet_contatori> radio_contatori;
LkRadioStructure<packet_sanitaria_termo> radio_sanitaria;
LkRadioStructure<packet_ambiente> radio_ambiente;

// Costanti
const int LED_PIN = 13;
const int RX_PIN = 11;
const int TX_PIN = 12;
const byte MAX_SERIAL_RX_LEN = 30;

// Variabili globali
char serialRxString[MAX_SERIAL_RX_LEN];
bool msgSerialFromWebNexus = false;
int COUNTER = 0;

LkMultivibrator flash_segnale_radio(30, MONOSTABLE);
LkMultivibrator flash_datetime_not_set(300, ASTABLE);

int eepromAddress = 0;

unsigned long prevTime = 0;
uint16_t prevActiveCount = 0;
uint16_t prevReactiveCount = 0;

const int EEPROM_SIZE = 4;

void setup() {
    setupHardware();
    // Serial.println(powerLimitValue);
    radio_contatori.globalSetup(2000, TX_PIN, RX_PIN);
    flash_segnale_radio.start();
    flash_datetime_not_set.start();
    digitalWrite(LED_PIN, HIGH);
}

void loop() {

 
  if (flash_segnale_radio.expired()) {
      digitalWrite(LED_PIN, LOW);
  }
  if (flash_datetime_not_set.expired()) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      // Serial.println("Data ora non impostata");
  }
  handleRadioReception();
  if (msgSerialFromWebNexus) {
      serialRxString[COUNTER] = '\0';
      checkString();
      COUNTER = 0;
      msgSerialFromWebNexus = false;
  }
}

void serialEvent() {
    while (Serial.available()) {
        byte theByte = Serial.read();
        if (theByte == '\n') {
            msgSerialFromWebNexus = true;
        } else {
            msgSerialFromWebNexus = false;
            if ((theByte > 31) && (theByte < 127)) {
                serialRxString[COUNTER] = (char) theByte;
                COUNTER++;
            }
        }
    }
}

void saveToEEPROM(int value) {
    EEPROM.write(eepromAddress, value & 0xFF);
    EEPROM.write(eepromAddress + 1, (value >> 8) & 0xFF);
    //Serial.println("Scrittura su EEPROM");
}

void setupHardware() {
    pinMode(LED_PIN, OUTPUT);
    Serial.begin(115200);
    while (!Serial) {
        ; 
    }
    // EEPROM.begin(EEPROM_SIZE);
}


void checkString() {
  Serial.println(serialRxString);
    if (strcmp(serialRxString, "AUTOmegane") == 0){
    } else if (strncmp(serialRxString, "POWER-LIMIT=", 12) == 0) {
        Serial.print("Impostato valore: ");
        // Serial.println(powerLimitValue);
    } else if (strcmp(serialRxString, "ALARM-TEST") == 0) {
    } else if (strcmp(serialRxString, "DATETIME-OK") == 0) {
        // Serial.println()
        flash_datetime_not_set.stop();
        digitalWrite(LED_PIN,LOW);
    }

}

void handleRadioReception() {

 if (radio_contatori.haveRawMessage()) {
        uint8_t rawBuffer[VW_MAX_MESSAGE_LEN];
        uint8_t rawBufferLen;
        radio_contatori.getRawBuffer(rawBuffer, rawBufferLen);
        if (rawBufferLen == 5) {
            // Deserializza il messaggio nel tipo corretto di struttura
            LkArraylize<packet_contatori> converter;
            packet_contatori ds_contatori = converter.deArraylize(rawBuffer);
            if (ds_contatori.sender == 99){
                handleEnergyData(ds_contatori);
                Serial.println();
                Serial.print("Sanitaria count: ");
                Serial.println(ds_contatori.sanitariaCount);
                Serial.print("Termo count:     ");
                Serial.println(ds_contatori.termoCount);  // Print with 10 decimal places
                flash_segnale_radio.start();
                digitalWrite(LED_PIN, HIGH);

            }
        } else if (rawBufferLen == 4) {
            LkArraylize<packet_sanitaria_termo> converter;
            packet_sanitaria_termo ds_sanitaria_termo = converter.deArraylize(rawBuffer);
            if (ds_sanitaria_termo.sender == 100){
                Serial.println();
                Serial.print("Sanitaria Temp: ");
                Serial.println(ds_sanitaria_termo.temperaturaCaldaC);
                Serial.print("Sanitaria Stato: ");
                Serial.println(ds_sanitaria_termo.stato);  // Print with 10 decimal places
                flash_segnale_radio.start();
                digitalWrite(LED_PIN, HIGH);


            } else if (ds_sanitaria_termo.sender == 101){
                Serial.println();
                Serial.print("Termo Temp: ");
                Serial.println(ds_sanitaria_termo.temperaturaCaldaC);
                Serial.print("Termo Stato: ");
                Serial.println(ds_sanitaria_termo.stato);  // Print with 10 decimal places
                flash_segnale_radio.start();
                digitalWrite(LED_PIN, HIGH);
            }           
        } else if (rawBufferLen == 3) {
            LkArraylize<packet_ambiente> converter;
            packet_ambiente ds_ambiente = converter.deArraylize(rawBuffer);
            if (ds_ambiente.sender == 102){
                Serial.println();
                Serial.print("Ambiente Temp: ");
                Serial.println(ds_ambiente.temperaturaC);
                flash_segnale_radio.start();
                digitalWrite(LED_PIN, HIGH);
            }                    
        }
    }








    // if (radio.haveRawMessage()) {


    //     uint8_t rawBuffer[VW_MAX_MESSAGE_LEN];
    //     uint8_t rawBufferLen;
    //     radio.getRawBuffer(rawBuffer, rawBufferLen);
    //     byte sender = rawBuffer[rawBufferLen - 1];
    //     flash_segnale_radio.start();
    //     digitalWrite(LED_PIN, HIGH);

    //     if (sender == ID_ENERGYSEND) {
    //         LkArraylize<packet_RadioRxEnergy> energyConverter;
    //         packet_RadioRxEnergy rcvdEnergy = energyConverter.deArraylize(rawBuffer);
    //         handleEnergyData(rcvdEnergy);
    //     } else if (sender == ID_LIGHTSEND) {
    //         LkArraylize<packet_RadioRxLight> lightConverter;
    //         packet_RadioRxLight rcvdLight = lightConverter.deArraylize(rawBuffer);
    //         handleLightData(rcvdLight);
    //     }
    // }
}

void handleEnergyData(packet_contatori& rcvdEnergy) {
    unsigned long currentTime = millis();
    if (prevTime != 0) {
      unsigned long timeDiff = currentTime - prevTime;
      uint16_t activeDiff = rcvdEnergy.termoCount - prevActiveCount;
      uint16_t reactiveDiff = rcvdEnergy.sanitariaCount - prevReactiveCount;

      Serial.print(activeDiff);
      Serial.print(" - ");
      Serial.print(timeDiff);

      // i valori qui indicati servono a capire se l'ultima ricezione
      // del segnale radio è recente oppure no...
      // (una differenza massima di 3600 conteggi - normali sono una decina)
      // (intervallo di tempo massim 1 ora - normale 9 secondi circa)
      if (activeDiff < 3600) {
        if (timeDiff < 3600000){
          float power = (activeDiff * 3600.0) / (timeDiff / 1000.0);
          // e se tutto è ok...
          //
          // invia un pacchetto sulla seriale per WebNexus anticipando
          // il valore 0xFF che serve come sincronizzaione (o discriminare
          // il tipo di messaggio inviato. al momento serve solo per 
          // sincronizzazione)

          Serial.print(" - ");
          Serial.print(power);

          packet_for_WebNexus packet = {activeDiff, reactiveDiff, timeDiff};
          Serial.write(0xFF);
          Serial.write((uint8_t*)&packet, sizeof(packet));

        }
      }
    }
    prevTime = currentTime;
    prevActiveCount = rcvdEnergy.termoCount;
    prevReactiveCount = rcvdEnergy.sanitariaCount;
}


