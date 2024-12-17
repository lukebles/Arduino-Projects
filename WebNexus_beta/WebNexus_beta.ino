#include <Arduino.h>
#include <LittleFS.h>

// ===============
#define DEBUG 1
// ===============
#if DEBUG
#define prt(x) Serial.print(x)
#define prtn(x) Serial.println(x)
#else
#define prt(x)
#define prtn(x)
#endif

#include "wifi_setup.h"
#include "web_server_setup.h"
#include "websocket_handler.h"
#include "data_handling.h"
#include "time_management.h"
#include "serial_packet_handler.h"
#include <LkMultivibrator.h>

// ho necessità di sapere se l'ultimo dato che riguarda la potenza 
// istantanea che ho in memoria è ancora valida oppure no
// Se sono passati più di 27 secondi dall'ultima ricezione del segnale
// radio significa che l'ultimo dato ricevuto non è più affidabile
LkMultivibrator radioCheck(27000,MONOSTABLE);

// la persdonalizzzazione Luca/Marco riguarda
// - il nome della stazione wifi
// - il file di configurazione "config.js" nella cartella /data

#define SAVE_INTERVAL 28800000 // 8 ore in millisecondi
unsigned long lastSaveTime = 0;

AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

DataInstant istantPoints[MAX_DATA_POINTS];
DataEnergyHours hoursPoints[MAX_DATA_POINTS];
DataEnergyDays daysPoints[MAX_DATA_POINTS];

const int EEPROM_SIZE = 4;

void setup() {
    Serial.begin(115200);
    delay(1500);
    prtn("");
    prtn("Avvio...");
    setTime(18, 0, 0, 1, 5, 2024); // Imposta la data e l'ora al 1 maggio 2024, 18:00
    prtn("IMpostata data ora a valore di default");
    if (!LittleFS.begin()) {
        prtn("An error has occurred while mounting LittleFS");
        return;
    }
    //
    EEPROM.begin(EEPROM_SIZE); 
    int powLimit = EEPROM.read(0) + (EEPROM.read(0 + 1) << 8);
    if (powLimit == -1) {
        powLimit = 3990;
        EEPROM.write(0, powLimit & 0xFF);
        EEPROM.write(0 + 1, (powLimit >> 8) & 0xFF);
        EEPROM.commit(); 
    }
    setPowerLimitValue(powLimit); // lo salva per la parte HTML
    prt("Limite di potenza recuperato da EEPROM: ");
    prtn("powLimit");
    //
    loadData();
    //
    setupWiFi();
    setupWebServer();
    setupWebSocket();
    //
    prtn("Avvio completato.");
}

void loop() {
    webSocket.loop();

    // se sono passati più di 27 secondi dall'ultima ricezione
    // radio, l'ultimo dato in memoria riguardante la potenza
    // istantanea non è più affidabile
    if(radioCheck.expired()){
      setAffidabilitaDato(false);
      prtn("dati di potenza non aggiornati (ricezione radio manca da vari secondi)");
    }

    if ((millis() - lastSaveTime > SAVE_INTERVAL)) {
        saveData();
        lastSaveTime = millis();
        prtn("Le tabelle elaborate finora sono state salvate su LittleFS");
    }    

    if (serialdatapacket_ready()) {

        // tiene aggiornato il fatto che l'ultima
        // potenxa letta sia effettivamente un dato aggiornato
        // oppure no
        setAffidabilitaDato(true);
        radioCheck.start();
        
        // riceve il pacchetto
        DataPacket packet = read_serialdatapacket();
        // === riempie i dati orari ===
        fillTable_hours(packet.activeDiff,packet.reactiveDiff);
        // == riempie i dati giornalieri ==
        fillTable_days(packet.activeDiff,packet.reactiveDiff);
        // === riempie i dati istantanei ===
        fillTable_istant(packet.activeDiff,packet.reactiveDiff, packet.timeDiff);
        //
        notifyClients();
        
    }
}
