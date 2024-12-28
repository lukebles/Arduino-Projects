
// ====== STRUTTURA DEI DIALOGHI ==========
// handle_energy_counters     -> seriale -> pacchetto energia a WebNexus
// checkString          <- seriale <- comandi inviati da WebNexus
// handleRadioReception <- radio   <- discrimina con l'ID il pacchetto radio ricevuto
//                                    ed invia coerentemente la struttura a WebNexus

///////////////////////////////////////////////
// la personalizzazione Luca/Marco riguarda
// - ID_ENERGYSEND
// - blink_allarme
//////////////////////////////////////////////

#include <LkRadioStructure.h>
#include <LkBlinker.h>
#include <EEPROM.h>

// Definizioni costanti
#define DEBUG 0 // 0 off 1 on
#define LED_PIN 13
#define RX_PIN 11
#define TX_PIN 12
#define SEGNALATORE_ACUSTICO_PIN 6
#define MAX_SERIAL_RX_LEN 30
#define EEPROM_SIZE 4
#define MAX_DIFFERENCE 99
#define MAX_TIME_DIFFERENCE_MS 300000

// Costanti per i sender
#define SENDER_ENERGY 1
#define SENDER_H2O_CALORIE 99
#define SENDER_H2O_SANITARIA 100
#define SENDER_H2O_TERMOSIFONI 101
#define SENDER_H2O_STATO 102
#define SENDER_METEO_TEMP 106
#define SENDER_METEO_UMIDITA 107
#define SENDER_METEO_PRESSIONE 108

const char associazioni[10][27] = {
    "H20 contatori calorie t/s",
    "H2O Sanitaria calda/fredda",
    "H2O termo calda/fredda",
    "H2O stato termo/sanitaria",
    " ",
    " ",
    " ",
    "Meteo temperatura",
    "Meteo Umidita",
    "Meteo Pressione"
};

const int inizio_valori_sender = 99;

// valori float esatti
struct __attribute__((packed)) StructureA {
  byte sender;   // 1 byte
  float valueA;  // 4 bytes
};

// valori float approssimati a 2 digit
// 100, 101, 102 (temperature caldaia)
struct __attribute__((packed)) StructureB {
  byte sender;      // 1 byte
  int16_t valueB;  // 2 bytes
  int16_t valueC;  // 2 bytes
};

// contatori (ID 1, 99)
struct __attribute__((packed)) StructureC {
  byte sender;     // 1 byte
  uint16_t valueD; // 2 bytes
  uint16_t valueE; // 2 bytes
};

// (ID 102) variazioni caldaia
struct __attribute__((packed)) StructureD {
  byte sender;   // 1 byte
  byte valueF;   // 1 byte
  byte valueG;   // 1 byte
};

// ================


struct __attribute__((packed)) pk_wnexus_B {
  int16_t valueB;
  int16_t valueC;
};

struct __attribute__((packed)) pk_wnexus_D {
  byte valueF;
  byte valueG;
};

struct __attribute__((packed)) pk_wnexus_X {
  uint16_t diffA;
  uint16_t diffB;
  unsigned long diffTime;
};


// Variabili globali
char serialRxString[MAX_SERIAL_RX_LEN];
bool msgSerialFromWebNexus = false;
int COUNTER = 0;

LkRadioStructure<StructureB> radio;

// =======================================================
// decommentare selezionando il segnalatore acustico usato
// =======================================================
// LUCA
// nessuna frequenza sul pin, 3 colpi di suoneria
LkBlinker blink_allarme(SEGNALATORE_ACUSTICO_PIN, true);
//
// MARCO
// 3 kHz sul pin SEGNALATORE_ACUSTICO_PIN, non invertito, con 5 colpi di suoneria
// LkBlinker blink_allarme(SEGNALATORE_ACUSTICO_PIN, false, 4, true, 3000);
// =======================================================

LkMultivibrator tmr_flash_segnale_radio(30, MONOSTABLE);
LkMultivibrator tmr_flash_datetime_not_set(300, ASTABLE);
LkMultivibrator tmr_disab_allarme(5 * 60 * 1000, MONOSTABLE);
bool campanaAbilitata = true;

int powerLimitValue;
int eepromAddress = 0;



// Gestione stati precedenti
struct Previous {
    bool initialized;
    unsigned long timer;
    uint16_t valueB;
    uint16_t valueC;

    Previous() : initialized(false), timer(0), valueB(0), valueC(0) {}
};

#if DEBUG
#define prt(x) Serial.print(x)
#define prtn(x) Serial.println(x)
#else
#define prt(x)
#define prtn(x)
#endif

// il dato è valido se la differenza di conteggi e di tempo trascorso
// sono 'sensati'
bool dataIsValid(unsigned long prevTimer, uint16_t prevvalueB, uint16_t prevvalueC, uint16_t valueB, uint16_t valueC) {
  if (prevTimer != 0) {
    unsigned long currentTime = millis();
    unsigned long diffTime = currentTime - prevTimer;
    uint16_t diffA = valueB - prevvalueB;
    uint16_t diffB = valueC - prevvalueC;

    if (diffA < MAX_DIFFERENCE) {
      if (diffB < MAX_DIFFERENCE) {
        if (diffTime < MAX_TIME_DIFFERENCE_MS) {
          prtn("\tvalido");
          return true;
        }
      }
    }
  }
  prtn("\tnon valido");
  return false;
}

void sendToWebNexus(byte sender, uint8_t* data, size_t length) {
  Serial.write(&sender, 1); // Invio del sender come singolo byte

  // Invio della lunghezza come un singolo byte
  byte lengthByte = (byte)length; // Assumi che la lunghezza sia sempre <= 255
  Serial.write(&lengthByte, 1);

  // Invio dei dati
  Serial.write(data, length); 
}


void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(SEGNALATORE_ACUSTICO_PIN, OUTPUT);
  // seriale
  Serial.begin(115200);
  delay(1500);
  prtn("Avvio");
  //
  powerLimitValue = EEPROM.read(eepromAddress) + (EEPROM.read(eepromAddress + 1) << 8);
  prt("Limite di potenza (da EEPROM): ");
  prtn(powerLimitValue);
  if ((powerLimitValue <= 100) || (powerLimitValue >= 5000)) {
    prtn("eseguita correzione del limite di potenza");
    powerLimitValue = 3990;
    saveToEEPROM(powerLimitValue);
  }
  prt("Limite di potenza impostata: ");
  prtn(powerLimitValue);
  //
  radio.globalSetup(2000, TX_PIN, RX_PIN);
  //
  tmr_flash_segnale_radio.start();
  tmr_flash_datetime_not_set.start();
  tmr_disab_allarme.stop();
  //
  digitalWrite(LED_PIN, HIGH);
}

void loop() {
  blink_allarme.loop();
  //
  if (tmr_disab_allarme.expired()) {
    campanaAbilitata = true;
    prtn("Campana abilitata");
  }
  if (tmr_flash_segnale_radio.expired()) {
    digitalWrite(LED_PIN, LOW);
  }
  if (tmr_flash_datetime_not_set.expired()) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  //
  handleRadioReception();
  //
  if (msgSerialFromWebNexus) {
    serialRxString[COUNTER] = '\0';
    prtn(serialRxString);
    if (strcmp(serialRxString, "AUTOmegane") == 0) {
      campanaAbilitata = false;
      tmr_disab_allarme.start();
    } else if (strncmp(serialRxString, "POWER-LIMIT=", 12) == 0) {
      powerLimitValue = atoi(serialRxString + 12);
      saveToEEPROM(powerLimitValue);
      prt("Impostato valore: ");
      prtn(powerLimitValue);
    } else if (strcmp(serialRxString, "ALARM-TEST") == 0) {
      blink_allarme.enable();
    } else if (strcmp(serialRxString, "DATETIME-OK") == 0) {
      tmr_flash_datetime_not_set.stop();
      digitalWrite(LED_PIN, LOW);
    }
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
        serialRxString[COUNTER] = (char)theByte;
        COUNTER++;
      }
    }
  }
}

void saveToEEPROM(int value) {
  EEPROM.write(eepromAddress, value & 0xFF);
  EEPROM.write(eepromAddress + 1, (value >> 8) & 0xFF);
}

// Funzione principale per processare i pacchetti ricevuti via radio
void processRadioPacket(byte sender, uint8_t* rawBuffer, uint8_t rawBufferLen) {
  unsigned long currentTime = millis();
  prt("Identificativo ");
  prt(sender);
  prt(" (");
  if (sender > 1){
    prt(associazioni[sender - inizio_valori_sender]);
    prtn(")");
  } else {
    prtn("ENEL energia elettrica)");
  }
  switch (sender) {
    case SENDER_ENERGY: {
      handleEnergyPacket(rawBuffer, currentTime);
      break;
    }
    case SENDER_H2O_CALORIE:{
      handleGasPacket(rawBuffer, currentTime);
      break;
    }
    case SENDER_H2O_SANITARIA:
    case SENDER_H2O_TERMOSIFONI:{
      handleGasTemperature(sender, rawBuffer, currentTime);
      break;
    }
    case SENDER_H2O_STATO:{
      handleGasStatus(rawBuffer, currentTime);
      break;
    }
    case SENDER_METEO_TEMP:
    case SENDER_METEO_UMIDITA:
    case SENDER_METEO_PRESSIONE: {
      handleMeteoPacket(sender, rawBuffer, currentTime);
      break;
    }
    default: {
      prtn("Sender non riconosciuto");
      break;
    }
  }
}


// Gestione dei pacchetti energia
void handleEnergyPacket(uint8_t* rawBuffer, unsigned long currentTime) {
  static Previous prevValues;

  LkArraylize<StructureB> radioData;
  StructureB packet = radioData.deArraylize(rawBuffer);

  if (!prevValues.initialized) {
    prtn("\tScarto la prima ricezione per inizializzare");
    prevValues.initialized = true;
  } else if (dataIsValid(prevValues.timer, prevValues.valueB,
                         prevValues.valueC, packet.valueB, packet.valueC)) {
    // Calcolo e invio differenze
    pk_wnexus_X packetToSend = {
      packet.valueB - prevValues.valueB,
      packet.valueC - prevValues.valueC,
      currentTime - prevValues.timer
    };
    sendToWebNexus(SENDER_ENERGY, (uint8_t*)&packetToSend, sizeof(packetToSend));

    // Controllo del limite di potenza
    float power = (packetToSend.diffA * 3600.0) / (packetToSend.diffTime / 1000.0);
    prt("\tPotenza istantanea (W): ");
    prtn(power);
    if (power > float(powerLimitValue)) {
      if (campanaAbilitata) {
        blink_allarme.enable();
        prtn("\tAllarme! Limite superato.");
      } else {
        prtn("\tLimite superato, ma campana disabilitata.");
      }
    }
  }

  // Aggiorna i valori precedenti
  prevValues.timer = currentTime;
  prevValues.valueB = packet.valueB;
  prevValues.valueC = packet.valueC;
}

// Gestione dei pacchetti gas
void handleGasPacket(uint8_t* rawBuffer, unsigned long currentTime) {
  static Previous prevValues;

  LkArraylize<StructureB> radioData;
  StructureB packet = radioData.deArraylize(rawBuffer);

  if (!prevValues.initialized) {
    prtn("\tScarto la prima ricezione per inizializzare");
    prevValues.initialized = true;
  } else if (dataIsValid(prevValues.timer, prevValues.valueB,
                         prevValues.valueC, packet.valueB, packet.valueC)) {
    // Calcolo e invio differenze
    pk_wnexus_X packetToSend = {
      packet.valueB - prevValues.valueB,
      packet.valueC - prevValues.valueC,
      currentTime - prevValues.timer
    };
    sendToWebNexus(SENDER_H2O_CALORIE, (uint8_t*)&packetToSend, sizeof(packetToSend));
  }

  // Aggiorna i valori precedenti
  prevValues.timer = currentTime;
  prevValues.valueB = packet.valueB;
  prevValues.valueC = packet.valueC;
}

// Gestione dei pacchetti meteo
void handleMeteoPacket(byte sender, uint8_t* rawBuffer, unsigned long currentTime) {
  LkArraylize<StructureA> radioData;
  StructureA packet = radioData.deArraylize(rawBuffer);

  sendToWebNexus(sender, (uint8_t*)&packet.valueA, sizeof(packet.valueA));
  prt("\tDati meteo ricevuti: ");
  prtn(packet.valueA);
}

// Gestione dei pacchetti stato-gas
void handleGasStatus(uint8_t* rawBuffer, unsigned long currentTime) {
  LkArraylize<StructureD> radioData;
  StructureD packet = radioData.deArraylize(rawBuffer);
  pk_wnexus_D packetToSend = {
    packet.valueF,
    packet.valueG,
  };
  sendToWebNexus(SENDER_H2O_STATO, (uint8_t*)&packetToSend, sizeof(packetToSend));
  //
  prt("\tDati stato ricevuti: ");
  prt(packet.valueF);
  prt(" - ");
  prtn(packet.valueG);
}

// Gestione dei pacchetti temperatura-gas
void handleGasTemperature(byte sender, uint8_t* rawBuffer, unsigned long currentTime) {
  LkArraylize<StructureB> radioData;
  StructureB packet = radioData.deArraylize(rawBuffer);
  pk_wnexus_B packetToSend = {
    packet.valueB,
    packet.valueC,
  };
  sendToWebNexus(sender, (uint8_t*)&packetToSend, sizeof(packetToSend));
  //
  prt("\tDati temperatura ricevuti: ");
  prt(packet.valueB);
  prt(" - ");
  prtn(packet.valueC);
}

// Funzione per la ricezione radio
void handleRadioReception() {
  if (radio.haveRawMessage()) {
    uint8_t rawBuffer[VW_MAX_MESSAGE_LEN];
    uint8_t rawBufferLen;
    radio.getRawBuffer(rawBuffer, rawBufferLen);

    // Il sender è sempre l'ultimo byte del pacchetto
    byte sender = rawBuffer[rawBufferLen - 1];
    tmr_flash_segnale_radio.start();
    digitalWrite(LED_PIN, HIGH);

    // Processa il pacchetto ricevuto
    processRadioPacket(sender, rawBuffer, rawBufferLen);
  }
}



