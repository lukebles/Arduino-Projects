
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

struct __attribute__((packed)) radiopacket_A {
  byte sender;     // 1 byte
  uint16_t wordA;  // 2 bytes
  uint16_t wordB;  // 2 bytes
};

struct __attribute__((packed)) radiopacket_B {
  byte sender;  // 1 byte
  byte byteA;   // 1 byte
  byte byteB;   // 1 byte
};

// ================

struct __attribute__((packed)) packet_wn_C {
  uint16_t diffA;
  uint16_t diffB;
  unsigned long diffTime;
};

struct __attribute__((packed)) packet_wn_A {
  int16_t tempA_C;
  int16_t tempB_C;
};

struct __attribute__((packed)) packet_wn_B {
  byte byteA;
  byte byteB;
};


// Costanti
const byte ID_ENERGYSEND = 1;  //1 Luca 11 Marco
const int LED_PIN = 13;
const int RX_PIN = 11;
const int TX_PIN = 12;  // non usato
const int SEGNALATORE_ACUSTICO_PIN = 6;
const byte MAX_SERIAL_RX_LEN = 30;

// Variabili globali
char serialRxString[MAX_SERIAL_RX_LEN];
bool msgSerialFromWebNexus = false;
int COUNTER = 0;

LkRadioStructure<radiopacket_A> radio;

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
const int EEPROM_SIZE = 4;
int eepromAddress = 0;


#define VALORI_ELETTRICI 0
#define VALORI_CALDAIA 1
struct Previous {
  bool primaricezione[2];
  unsigned long timer[2];
  uint16_t wordA[2];
  uint16_t wordB[2];
  
  // Costruttore con valori di default
  Previous() {
    primaricezione[VALORI_ELETTRICI] = primaricezione[VALORI_CALDAIA] = 0;
    timer[VALORI_ELETTRICI] = timer[VALORI_CALDAIA] = 0;
    wordA[VALORI_ELETTRICI] = wordA[VALORI_CALDAIA] = 0;
    wordB[VALORI_ELETTRICI] = wordB[VALORI_CALDAIA] = 0;
  }
};


// differenza tra due contatori
// per definire il conteggio "sensato"
#define MAX_DIFFERENCE 99 
#define MAX_TIME_DIFFERENCE_MS 300000

#define DEBUG 1

#if DEBUG
#define prt(x) Serial.print(x)
#define prtn(x) Serial.println(x)
#else
#define prt(x)
#define prtn(x)
#endif

// il dato è valido se la differenza di conteggi e di tempo trascorso
// sono 'sensati'
bool dataIsValid(unsigned long prevTimer, uint16_t prevwordA, uint16_t prevwordB, uint16_t wordA, uint16_t wordB) {
  if (prevTimer != 0) {
    unsigned long currentTime = millis();
    unsigned long diffTime = currentTime - prevTimer;
    uint16_t diffA = wordA - prevwordA;
    uint16_t diffB = wordB - prevwordB;

    if (diffA < MAX_DIFFERENCE) {
      if (diffB < MAX_DIFFERENCE) {
        if (diffTime < MAX_TIME_DIFFERENCE_MS) {
          prtn("valido");
          return true;
        }
      }
    }
  }
  prtn("non valido");
  return false;
}

void write2webnexus(const uint8_t* data, size_t length) {
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

void handleRadioReception() {
  if (radio.haveRawMessage()) {
    uint8_t rawBuffer[VW_MAX_MESSAGE_LEN];
    uint8_t rawBufferLen;
    radio.getRawBuffer(rawBuffer, rawBufferLen);
    // il mittente è sempre nell'ultimo byte del messaggio
    // inpendentemente dalla lunghezza
    byte sender = rawBuffer[rawBufferLen - 1];
    // azzera il timer per il lampeggio del led
    // relativo alla segnalazione di un pacchetto ricevuto
    tmr_flash_segnale_radio.start();
    digitalWrite(LED_PIN, HIGH);
    //
    prtn("");
    prt("Identificativo mittente ricevuto via radio: ");
    prtn(sender);
    if ((sender == ID_ENERGYSEND) || (sender == 99)) {
      byte iden;
      if (sender == ID_ENERGYSEND){
        iden = VALORI_ELETTRICI;
      } else {
        iden = VALORI_CALDAIA;
      }

      static Previous prevv_values;
      unsigned long currentTime = millis();
      // energia elettrica: contatori
      LkArraylize<radiopacket_A> the_radio_data;
      radiopacket_A packet = the_radio_data.deArraylize(rawBuffer);
      //
      if (prevv_values.primaricezione[iden] == 0){ 
        prevv_values.primaricezione[iden] = 1;
        prtn("\tScarto la prima ricezione per inizializzare correttamente le differenze");
      } else {
        prt("\tControllo se il dato ricevuto è valido: ");
        if (dataIsValid(prevv_values.timer[iden], prevv_values.wordA[iden], prevv_values.wordB[iden], packet.wordA, packet.wordB)) {
          //
          unsigned long diffTime = currentTime - prevv_values.timer[iden];
          uint16_t diffA = packet.wordA - prevv_values.wordA[iden];
          uint16_t diffB = packet.wordB - prevv_values.wordB[iden];
          //
          packet_wn_C packet_2_wnx = { diffA, diffB, diffTime };
          // invio a webnexus
          // il messaggio è anticipato da un ID equivalente al sender
          // ricevuto via radio
          write2webnexus(sender, 1);
          write2webnexus((uint8_t*)&packet_2_wnx, sizeof(packet_2_wnx));
          prt("\t");
          prt("Valori inviati a webnexus (diffA, diffB, diffTime(ms)): ");
          prt(packet_2_wnx.diffA);
          prt("\t");
          prt(packet_2_wnx.diffB);
          prt("\t");
          prtn(packet_2_wnx.diffTime);
          if (sender == ID_ENERGYSEND){
            // controllo potenza ricevuta se supera il limite
            float power = (diffA * 3600.0) / (diffTime / 1000.0);
            prt("\tPotenza istantanea (attiva) W: ");
            prt(power);
            prt("\tLimite di potenza (W): ");
            prtn(powerLimitValue);
            if (power > float(powerLimitValue)) {
              if (campanaAbilitata) {
                blink_allarme.enable();
                prtn("\t\tAllarme");
              } else {
                prtn("\t\tAllarme, ma suoneria disabilitata");
              }
            }
          }
        }
      }
      //
      prevv_values.timer[iden] = currentTime;
      prevv_values.wordA[iden] = packet.wordA;
      prevv_values.wordB[iden] = packet.wordB;
    } else if ((sender == 100) or (sender == 101)) {
      // gas (caldaia): temperatura acqua sanitaria
      LkArraylize<radiopacket_A> the_radio_data;
      radiopacket_A packet = the_radio_data.deArraylize(rawBuffer);
      packet_wn_A packet_2_wnx = { packet.wordA, packet.wordB };
      write2webnexus(sender, 1);
      write2webnexus((uint8_t*)&packet_2_wnx, sizeof(packet_2_wnx));
    } else if (sender == 102) {
      LkArraylize<radiopacket_B> the_radio_data;
      radiopacket_B packet = the_radio_data.deArraylize(rawBuffer);
      packet_wn_B packet_2_wnx = { packet.byteA, packet.byteB };
      write2webnexus(sender, 1);
      write2webnexus((uint8_t*)&packet_2_wnx, sizeof(packet_2_wnx));
    }
  }
}

