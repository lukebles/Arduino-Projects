#include <Arduino.h>
#include <LittleFS.h>
#include "wifi_setup.h"
#include "web_server_setup.h"
#include "websocket_handler.h"
#include "data_handling.h"
#include "time_management.h"
#include "serial_packet_handler.h"

AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
Data dataPoints[MAX_DATA_POINTS];
uint8_t dataIndex = 0;
const float LIGHT_TO_POWER = 1.0;


#define TABLE_SIZE 31 // 24 ore memorizzate

struct DateTimeValue {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    unsigned long attiva; 
    unsigned long reattiva; 
    unsigned long luminosita; 
};
DateTimeValue table[TABLE_SIZE];
int currentIndex = -1; // Indica l'indice corrente, -1 se la tabella è vuota

void fillHorsColumns(unsigned long attiva, unsigned long reattiva,unsigned long luminosita) {
    time_t cnow = now();
    uint16_t cyear = year(cnow);
    uint8_t cmonth = month(cnow);
    uint8_t cday = day(cnow);
    uint8_t chour = hour(cnow);

    for (int i = 0; i <= currentIndex; i++) {
        if (table[i].year == cyear && table[i].month == cmonth &&
            table[i].day == cday && table[i].hour == chour ) {
            table[i].attiva += attiva;
            table[i].reattiva += reattiva;
            table[i].luminosita += luminosita;
            return;
        }
    }

    if (currentIndex + 1 == TABLE_SIZE) {
        for (int i = 0; i < TABLE_SIZE - 1; i++) {
            table[i] = table[i + 1];
        }
        currentIndex--;
    }

    currentIndex++;
    table[currentIndex] = {cyear, cmonth, cday, chour, attiva, reattiva, luminosita};

}



void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }

    if (!LittleFS.begin()) {
        Serial.println("An error has occurred while mounting LittleFS");
        return;
    }

    setupWiFi();
    setupWebServer();
    setupWebSocket();

    initializeDataPoints();
    setSystemTime(2000, 1, 1, 0, 0, 0);
}

void loop() {
    webSocket.loop();

    if (serialdatapacket_ready()) {
        DataPacket packet = read_serialdatapacket();
        float activePower = (packet.activeDiff * 3600.0) / (packet.timeDiff / 1000.0);
        float reactivePower = (packet.reactiveDiff * 3600.0) / (packet.timeDiff / 1000.0);
        float lightPower = packet.lightIntensity * LIGHT_TO_POWER;

        activePower = roundToTens(activePower);
        reactivePower = roundToTens(reactivePower);
        lightPower = roundToTens(lightPower);

        dataPoints[dataIndex].activePower = activePower;
        dataPoints[dataIndex].reactivePower = reactivePower;
        dataPoints[dataIndex].lightPower = lightPower;
        dataPoints[dataIndex].timestamp = getTimestamp();

        notifyClients();

        dataIndex = dataIndex + 1;
        cycleDataPoints();
    }
}
