#include <Arduino.h>
#include <LittleFS.h>
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

void generateData() {
    time_t baseTime = now(); // Data di partenza: 3 Giugno 2024 ore 18:00
    time_t incrementTimeInstant = 10; // Incremento di 10 secondi per ogni DataInstant
    time_t incrementTimeHour = 3600; // Incremento di 1 ora per ogni DataEnergyHours
    time_t incrementTimeDay = 24*incrementTimeHour; // Incremento di 1 giorno per ogni DataEnergyDays

    for (int i = 0; i < MAX_DATA_POINTS; i++) {
        // Genera dati per DataInstant
        istantPoints[i].activePower = random(0, 3000);
        istantPoints[i].reactivePower = random(0, 501);
        istantPoints[i].timestamp = baseTime + (i * incrementTimeInstant);
        istantPoints[i].timeDiff = 8765;

        // Genera dati per DataEnergyHours
        hoursPoints[i].timestampH = baseTime + (i * incrementTimeHour);
        hoursPoints[i].diff_a = random(100, 1001);
        hoursPoints[i].diff_r = random(10, 301);

        // Genera dati per DataEnergyDays
        daysPoints[i].timestampD = baseTime + (i * incrementTimeDay);
        daysPoints[i].diff_a = random(1000, 16001);
        daysPoints[i].diff_r = random(200, 1001);
    }

}

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }

    setTime(18, 0, 0, 1, 5, 2024); // Imposta la data e l'ora al 1 maggio 2024, 18:00
    
    //generateData();
    //saveData();

    if (!LittleFS.begin()) {
        Serial.println("An error has occurred while mounting LittleFS");
        return;
    }

// Controllo dello spazio disponibile
    // FSInfo fs_info;
    // LittleFS.info(fs_info);
    // Serial.print("Total space: ");
    // Serial.println(fs_info.totalBytes);
    // Serial.print("Used space: ");
    // Serial.println(fs_info.usedBytes);
    
    //////////////////// CARICA I DATI SALVATI //////////////////
    EEPROM.begin(EEPROM_SIZE); 
    
    int powLimit = EEPROM.read(0) + (EEPROM.read(0 + 1) << 8);
    if (powLimit == -1) {
        powLimit = 3990;
        EEPROM.write(0, powLimit & 0xFF);
        EEPROM.write(0 + 1, (powLimit >> 8) & 0xFF);
        EEPROM.commit(); 
    }
    // Serial.print("riavvio: ");
    // Serial.println(powLimit);
    setPowerLimitValue(powLimit); // lo salva per la parte HTML

    loadData();

    // prende come data/ora l'ultimo dato presente
    // sui dati istantanei
    time_t t = istantPoints[MAX_DATA_POINTS-1].timestamp;
    setTime(t);

    //////////// CARICA I VALORI DI CONFIGURAZIONE //////////////
    // Inizializza alcuni valori di configurazione
    // strcpy(configuraz[0].key, "WiFiSSID");
    // strcpy(configuraz[0].value, "YourSSID");
    // strcpy(configuraz[1].key, "WiFiPassword");
    // strcpy(configuraz[1].value, "YourPassword");

    // // Salva la configurazione
    // saveConfig();

    // // Carica la configurazione
    // loadConfig();

    setupWiFi();
    setupWebServer();
    setupWebSocket();

    //initializeDataPoints();
    //setSystemTime(2000, 1, 1, 0, 0, 0);
}

void loop() {
    webSocket.loop();

    // se sono passati più di 27 secondi dall'ultima ricezione
    // radio, l'ultimo dato in memoria riguardante la potenza
    // istantanea non è più affidabile
    if(radioCheck.expired()){
      setAffidabilitaDato(false);
    }

    if ((millis() - lastSaveTime > SAVE_INTERVAL)) {
        saveData();
        lastSaveTime = millis();
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
        //Serial.println(packet.activeDiff);
        fillTable_hours(packet.activeDiff,packet.reactiveDiff);
        // == riempie i dati giornalieri ==
        fillTable_days(packet.activeDiff,packet.reactiveDiff);
        // === riempie i dati istantanei ===
        fillTable_istant(packet.activeDiff,packet.reactiveDiff, packet.timeDiff);
        //
        notifyClients();
        
    }
}
