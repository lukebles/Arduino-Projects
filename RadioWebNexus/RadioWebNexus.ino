/*
Compilare su ESP01s usando l'opzione FLASH SIZE 1M - FS: 256KB - OTA 374KB)
per installare il filesystem usare Upload Little FS tramite SHIFT-CMD-P
*/

// 04/10/2024 - INTERROTTO LO SVILUPPO
// Motivazione: la funzione WebSocket.loop() provoca la perdita occasionale di pacchetti radio,
// rendendo instabile la ricezione. A causa di questo problema, ho deciso di continuare 
// lo sviluppo separato di WebNexus e MultiCatch.
//
// Nota: il progetto attuale è comunque funzionante, ma richiede ottimizzazioni nella parte 
// relativa a data_handling.h, dove vengono utilizzate le stringhe. Ogni tanto, 
// circa ogni due settimane, il sistema si blocca, probabilmente a causa di problemi nella gestione delle stringhe.


#include <Arduino.h>
#include <LittleFS.h>
#include "wifi_setup.h"
#include "web_server_setup.h"
#include "websocket_handler.h"
#include "data_handling.h"
#include "time_management.h"
#include "serial_packet_handler.h"
//======
#include <LkRadioStructureEx.h>
#include <LkBlinker.h>
#include <EEPROM.h>
//
// ********************************************
// ============== codice per ESP01 ============
// ********************************************
//
// la EEPROM contiene la potenza-limite che fa scattare 
// l'allarma (badenia tramite relè (Luca) oppure capsula piezo (Marco))
const int EEPROM_SIZE = 4;
const byte ID_ENERGYSEND = 1; //1 Luca 11 Marco
const int LED_PIN = 2; // GPIO2 per ESP01
const int RX_PIN = 0; // GPIO0 dove è collegato il modulo radio RX
const int TX_PIN = -1; // non è presente nessuno modulo radio TX
const int SEGNALATORE_ACUSTICO_PIN = 3; // GPIO3 campana (Luca) - capsula piezo (Marco)
const bool PIN_INVERTED = true;

const bool LEDPIN_HIGH = LOW;
const bool LEDPIN_LOW = HIGH;
uint16_t intensitaLuminosa = 0;
int powerLimitValue;
int eepromAddress = 0;

unsigned long prevTime = 0;
uint16_t prevActiveCount = 0;
uint16_t prevReactiveCount = 0;
//
// ho necessità di sapere se l'ultimo dato che riguarda la potenza 
// istantanea che ho in memoria è ancora valida oppure no
// Se sono passati più di 27 secondi dall'ultima ricezione del segnale
// radio significa che l'ultimo dato ricevuto non è più affidabile
LkMultivibrator timer_radioCheck(27000,MONOSTABLE);
// =======================================================
// decommentare selezionando il segnalatore acustico usato
// =======================================================
// LUCA
// nessuna frequenza sul pin, 3 colpi di suoneria
LkBlinker blinker_segnalatore(SEGNALATORE_ACUSTICO_PIN, PIN_INVERTED);
//
// MARCO
// 3 kHz sul pin SEGNALATORE_ACUSTICO_PIN, non invertito, con 5 colpi di suoneria
// LkBlinker blinker_segnalatore(SEGNALATORE_ACUSTICO_PIN, false, 4, true, 3000); 
// =======================================================
LkMultivibrator timer_flashSegnaleRadio(30, MONOSTABLE);
LkMultivibrator timer_flashDatetimeNotSet(300, ASTABLE);
// disabilita l'allarme per 5 minuti (dall'ultima ricezione di "MEGANE" )
LkMultivibrator timer_sospendiCampana(5*60*1000,MONOSTABLE);
bool campanaAbilitata = true;
// =========== RICORDA ===========================================
// la persdonalizzzazione Luca/Marco riguarda
// - il nome della stazione wifi
// - il file di configurazione "config.js" nella cartella /data
// ===============================================================
// salvataggio automatico dello storico dei consumi
#define SAVE_INTERVAL 28800000 // 8 ore in millisecondi
unsigned long lastSaveTime = 0;
//
AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
// array di strutture dati
DataInstant istantPoints[MAX_DATA_POINTS];
DataEnergyHours hoursPoints[MAX_DATA_POINTS];
DataEnergyDays daysPoints[MAX_DATA_POINTS];

LkRadioStructureEx<packet_RadioRxEnergy> radio;

// Prototipi delle funzioni
// void saveToEEPROM(int value);
// void setupHardware();
// void loadPowerLimitValue();
// void checkString();
// void handleRadioReception();
// void handleSerialInput();
// void handleEnergyData(packet_RadioRxEnergy& rcvdEnergy);
// void handleLightData(packet_RadioRxLight& rcvdLight);

void setup() {
  
    EEPROM.begin(EEPROM_SIZE); 
    setupHardware();
    //
    // velocità di ricezione sperimenale
    // il trasmettitore ATMEGA328P trasmette a 2000 con VirtualWire
    // il ricevutore ESP01 con VirtualWireEx necessita di adattamento
    // 1990 limite estremo basso / 2005 estremo alto 
    // media 1997
    radio.globalSetup(2000, TX_PIN, RX_PIN);
    //
    timer_flashSegnaleRadio.start();
    timer_flashDatetimeNotSet.start();
    timer_sospendiCampana.stop();
    //
    setTime(18, 0, 0, 1, 5, 2024); // Imposta la data e l'ora al 1 maggio 2024, 18:00
    //
    if (!LittleFS.begin()) {
        Serial.println("An error has occurred while mounting LittleFS");
        return;
    }

    //
    loadData();
    //
    // prende come data/ora l'ultimo dato presente
    // sui dati istantanei
    time_t t = istantPoints[MAX_DATA_POINTS-1].timestamp;
    setTime(t);
    //
    setupWiFi();
    setupWebServer();
    setupWebSocket();

    blinker_segnalatore.begin();
    
    // pinMode(SEGNALATORE_ACUSTICO_PIN, OUTPUT);
    // digitalWrite(SEGNALATORE_ACUSTICO_PIN,HIGH);
    // delay(500);
    // digitalWrite(SEGNALATORE_ACUSTICO_PIN,LOW);
    // delay(500);
    // digitalWrite(SEGNALATORE_ACUSTICO_PIN,HIGH);
    // delay(500);
    // digitalWrite(SEGNALATORE_ACUSTICO_PIN,LOW);
    // delay(500);        
}

void loop() {
  webSocket.loop();
  // se sono passati più di 27 secondi dall'ultima ricezione
  // radio, l'ultimo dato in memoria riguardante la potenza
  // istantanea non è più affidabile
  if(timer_radioCheck.expired()){
    setAffidabilitaDato(false);
  }
  // è il momeno di salvare i dati riguardanti il consumo?
  if ((millis() - lastSaveTime > SAVE_INTERVAL)) {
      saveData();
      lastSaveTime = millis();
  }      
  // debbo smettere di non far suonare la campana (o capssula piezo)
  if(timer_sospendiCampana.expired()){
    // sono passati i minuti
    // di disattivazione della campana
    campanaAbilitata = true;
  }
  // lampeggio
  blinker_segnalatore.loop();
  // flash che un segnale radio è stato ricevuto
  if (timer_flashSegnaleRadio.expired()) {
      digitalWrite(LED_PIN, LEDPIN_LOW);
  }
  // flash che dice che la data e l'ora non sono state impostate
  if (timer_flashDatetimeNotSet.expired()) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      // Serial.println("Data ora non impostata");
  }
  // controllo del segnale radio
  handleRadioReception();
}

void setupHardware() {
    // seriale
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
    //
    loadPowerLimitValue();
}

void loadPowerLimitValue() {
    powerLimitValue = EEPROM.read(eepromAddress) + (EEPROM.read(eepromAddress + 1) << 8);
    if (powerLimitValue <= 0) {
        // Serial.println("Non leggo dalla EEPROM");
        powerLimitValue = 3990;
        EEPROM.write(eepromAddress, powerLimitValue & 0xFF);
        EEPROM.write(eepromAddress + 1, (powerLimitValue >> 8) & 0xFF);
        EEPROM.commit();
    }
    // Serial.println(powerLimitValue);
}

// void checkString() {
//   Serial.println(serialRxString);
//     if (strcmp(serialRxString, "AUTOmegane") == 0){
//       // disabilita la campana per 30 minuti
//       campanaAbilitata = false;
//       disabCampana.start();
//     } else if (strncmp(serialRxString, "POWER-LIMIT=", 12) == 0) {
//         powerLimitValue = atoi(serialRxString + 12);
//         saveToEEPROM(powerLimitValue);
//         Serial.print("Impostato valore: ");
//         Serial.println(powerLimitValue);
//         // Serial.println(powerLimitValue);
//     } else if (strcmp(serialRxString, "ALARM-TEST") == 0) {
//         allarme_segnalatore.enable(); 
//     } else if (strcmp(serialRxString, "DATETIME-OK") == 0) {
//         // Serial.println()
//         flash_datetime_not_set.stop();
//         digitalWrite(LED_PIN,LEDPIN_LOW);
//     }

// }

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

          // tiene aggiornato il fatto che l'ultima
          // potenxa letta sia effettivamente un dato aggiornato
          // oppure no
          setAffidabilitaDato(true);
          
          // === riempie i dati orari ===
          fillTable_hours(activeDiff,reactiveDiff);
          // == riempie i dati giornalieri ==
          fillTable_days(activeDiff,reactiveDiff);
          // === riempie i dati istantanei ===
          fillTable_istant(activeDiff,reactiveDiff, timeDiff);
          //
          notifyClients();

          // poi controlla se la potenza calcolata
          // supera la soglia limite
          if (power > float(powerLimitValue)) {
            if (campanaAbilitata){
              // suona la campana se
              // è abilitata
              blinker_segnalatore.enable();
            }
            // Serial.println(power);
            
          }
        }
      }
    }
    prevTime = currentTime;
    prevActiveCount = rcvdEnergy.activeCount;
    prevReactiveCount = rcvdEnergy.reactiveCount;
}



