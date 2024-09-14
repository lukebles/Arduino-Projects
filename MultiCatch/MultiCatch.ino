
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
    uint16_t activeDiff;
    uint16_t reactiveDiff;
    unsigned long timeDiff;
};

struct __attribute__((packed)) packet_RadioRxEnergy {
    byte sender;
    uint16_t activeCount;
    uint16_t reactiveCount;
};

struct __attribute__((packed)) packet_RadioRxLight {
    byte sender;
    uint16_t intensity;
};

// Costanti
const byte ID_ENERGYSEND = 1; //1 Luca 11 Marco
const byte ID_LIGHTSEND = 2;
const int LED_PIN = 13;
const int RX_PIN = 11;
const int TX_PIN = 12;
const int SEGNALATORE_ACUSTICO_PIN = 6;
const byte MAX_SERIAL_RX_LEN = 30;

// Variabili globali
char serialRxString[MAX_SERIAL_RX_LEN];
bool msgSerialFromWebNexus = false;
int COUNTER = 0;

LkRadioStructure<packet_RadioRxEnergy> radio;

// =======================================================
// decommentare selezionando il segnalatore acustico usato
// =======================================================
// LUCA
// nessuna frequenza sul pin, 3 colpi di suoneria
LkBlinker allarme_segnalatore(SEGNALATORE_ACUSTICO_PIN, true);
//
// MARCO
// 3 kHz sul pin SEGNALATORE_ACUSTICO_PIN, non invertito, con 5 colpi di suoneria
// LkBlinker allarme_segnalatore(SEGNALATORE_ACUSTICO_PIN, false, 4, true, 3000); 
// =======================================================

LkMultivibrator flash_segnale_radio(30, MONOSTABLE);
LkMultivibrator flash_datetime_not_set(300, ASTABLE);

uint16_t intensitaLuminosa = 0;
int powerLimitValue;
int eepromAddress = 0;

unsigned long prevTime = 0;
uint16_t prevActiveCount = 0;
uint16_t prevReactiveCount = 0;

const int EEPROM_SIZE = 4;

// Prototipi delle funzioni
// void saveToEEPROM(int value);
// void setupHardware();
// void loadPowerLimitValue();
// void checkString();
// void handleRadioReception();
// void handleSerialInput();
// void handleEnergyData(packet_RadioRxEnergy& rcvdEnergy);
// void handleLightData(packet_RadioRxLight& rcvdLight);

// disabilita l'allarme per 5 minuti (dall'ultima ricezione di "MEGANE" )
LkMultivibrator disabCampana(5*60*1000,MONOSTABLE);
bool campanaAbilitata = true;

void setup() {
    setupHardware();
    loadPowerLimitValue();
    // Serial.println(powerLimitValue);
    radio.globalSetup(2000, TX_PIN, RX_PIN);
    flash_segnale_radio.start();
    flash_datetime_not_set.start();
    digitalWrite(LED_PIN, HIGH);

    disabCampana.stop();
}

void loop() {

  if(disabCampana.expired()){
    // sono passati i 5 minuti
    // di disattivazione della campana
    campanaAbilitata = true;
  }

  allarme_segnalatore.loop();

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
    pinMode(SEGNALATORE_ACUSTICO_PIN, OUTPUT);
    Serial.begin(115200);
    while (!Serial) {
        ; 
    }
    // EEPROM.begin(EEPROM_SIZE);
}

void loadPowerLimitValue() {
    powerLimitValue = EEPROM.read(eepromAddress) + (EEPROM.read(eepromAddress + 1) << 8);
    if (powerLimitValue == -1) {
        powerLimitValue = 3990;
        saveToEEPROM(powerLimitValue);
    }
    // Serial.println(powerLimitValue);
}

void checkString() {
  Serial.println(serialRxString);
    if (strcmp(serialRxString, "AUTOmegane") == 0){
      // disabilita la campana per 30 minuti
      campanaAbilitata = false;
      disabCampana.start();
    } else if (strncmp(serialRxString, "POWER-LIMIT=", 12) == 0) {
        powerLimitValue = atoi(serialRxString + 12);
        saveToEEPROM(powerLimitValue);
        Serial.print("Impostato valore: ");
        Serial.println(powerLimitValue);
        // Serial.println(powerLimitValue);
    } else if (strcmp(serialRxString, "ALARM-TEST") == 0) {
        allarme_segnalatore.enable(); 
    } else if (strcmp(serialRxString, "DATETIME-OK") == 0) {
        // Serial.println()
        flash_datetime_not_set.stop();
        digitalWrite(LED_PIN,LOW);
    }

}

void handleRadioReception() {
    if (radio.haveRawMessage()) {
        uint8_t rawBuffer[VW_MAX_MESSAGE_LEN];
        uint8_t rawBufferLen;
        radio.getRawBuffer(rawBuffer, rawBufferLen);
        byte sender = rawBuffer[rawBufferLen - 1];
        flash_segnale_radio.start();
        digitalWrite(LED_PIN, HIGH);

        if (sender == ID_ENERGYSEND) {
            LkArraylize<packet_RadioRxEnergy> energyConverter;
            packet_RadioRxEnergy rcvdEnergy = energyConverter.deArraylize(rawBuffer);
            handleEnergyData(rcvdEnergy);
        } else if (sender == ID_LIGHTSEND) {
            LkArraylize<packet_RadioRxLight> lightConverter;
            packet_RadioRxLight rcvdLight = lightConverter.deArraylize(rawBuffer);
            handleLightData(rcvdLight);
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

          Serial.print(" - ");
          Serial.println(powerLimitValue);


          packet_for_WebNexus packet = {activeDiff, reactiveDiff, timeDiff};
          Serial.write(0xFF);
          Serial.write((uint8_t*)&packet, sizeof(packet));
          // poi controlla se la potenza calcolata
          // supera la soglia limite
          if (power > float(powerLimitValue)) {
            if (campanaAbilitata){
              // suona la campana se
              // è abilitata
              allarme_segnalatore.enable();
            }
            Serial.println(power);
            
          }
        }
      }
    }
    prevTime = currentTime;
    prevActiveCount = rcvdEnergy.activeCount;
    prevReactiveCount = rcvdEnergy.reactiveCount;
}

void handleLightData(packet_RadioRxLight& rcvdLight) {
    intensitaLuminosa = rcvdLight.intensity;
}


