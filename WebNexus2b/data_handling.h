#ifndef DATA_HANDLING_H
#define DATA_HANDLING_H

#include <Arduino.h>
#include <TimeLib.h>
#include <WebSocketsServer_Generic.h>

#define MAX_DATA_POINTS 31

struct Data {
    uint16_t activePower;
    uint16_t reactivePower;
    uint16_t lightPower;
    uint32_t timestamp;
};

extern Data dataPoints[MAX_DATA_POINTS];
extern uint8_t dataIndex;
extern WebSocketsServer webSocket;

uint32_t getTimestamp() {
    return now();
}

void formatTime(uint32_t timestamp, char* buffer, size_t len) {
    snprintf(buffer, len, "%02d/%02d/%04d %02d:%02d:%02d", day(timestamp), month(timestamp), year(timestamp), hour(timestamp), minute(timestamp), second(timestamp));
}

void initializeDataPoints() {
    for (int i = 0; i < MAX_DATA_POINTS; i++) {
        dataPoints[i].activePower = 0;
        dataPoints[i].reactivePower = 0;
        dataPoints[i].lightPower = 0;
        dataPoints[i].timestamp = 0;
    }
}

void sendDataToClient(uint8_t num) {
    String json = "[";
    char buffer[20];
    for (int i = 0; i < MAX_DATA_POINTS; i++) {
        formatTime(dataPoints[i].timestamp, buffer, sizeof(buffer));
        json += "{\"activePower\":" + String(dataPoints[i].activePower) +
                ",\"reactivePower\":" + String(dataPoints[i].reactivePower) +
                ",\"lightPower\":" + String(dataPoints[i].lightPower) +
                ",\"timestamp\":\"" + String(buffer) + "\"}";
        if (i < MAX_DATA_POINTS - 1) json += ",";
    }
    json += "]";
    webSocket.sendTXT(num, json);
}

void notifyClients() {
    char buffer[20];
    formatTime(dataPoints[dataIndex].timestamp, buffer, sizeof(buffer));
    String json = "{\"activePower\":" + String(dataPoints[dataIndex].activePower) +
                  ",\"reactivePower\":" + String(dataPoints[dataIndex].reactivePower) +
                  ",\"lightPower\":" + String(dataPoints[dataIndex].lightPower) +
                  ",\"timestamp\":\"" + String(buffer) + "\"}";

    Serial.println(json);
    webSocket.broadcastTXT(json);
}

float roundToTens(float value) {
    return round(value / 10) * 10;
}

void cycleDataPoints(){
  if (dataIndex == MAX_DATA_POINTS) {
      // Sposta tutti i dataPoints verso sinistra di 1
      for (int i = 1; i < MAX_DATA_POINTS; i++) {
          dataPoints[i - 1] = dataPoints[i];
      }
      // Ripristina l'ultimo indice per consentire l'inserimento del nuovo valore
      dataIndex = MAX_DATA_POINTS - 1;
  }
}

#endif // DATA_HANDLING_H
