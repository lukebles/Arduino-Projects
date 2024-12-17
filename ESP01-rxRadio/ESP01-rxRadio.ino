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

//======
#include <LkRadioStructureEx.h>

//
// ********************************************
// ============== codice per ESP01 ============
// ********************************************
//

const int LED_PIN = 2; // GPIO2 per ESP01
const int RX_PIN = 0; // GPIO0 dove è collegato il modulo radio RX
const int TX_PIN = -1; // non è presente nessuno modulo radio TX


const bool LEDPIN_HIGH = LOW;
const bool LEDPIN_LOW = HIGH;


LkRadioStructureEx<packet_RadioRxEnergy> radio;


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



