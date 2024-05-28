#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer_Generic.h>
#include <LittleFS.h>
#include <time.h>
#include "DataPacketHandler.h"

#define MAX_DATA_POINTS 31

const char* ssid = "sid2";
const char* password = "pw12345678";

AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

struct Data {
    uint16_t activeDiff;
    uint16_t reactiveDiff;
    uint16_t intensitaLuminosa;
    unsigned long timeDiff;
    time_t timestamp;
};

Data dataPoints[MAX_DATA_POINTS];
uint8_t dataIndex = 0;

void handleRoot(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
}

void handleWebSocketMessage(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (type == WStype_TEXT) {
        if (strcmp((char*)payload, "getData") == 0) {
            String json = "[";
            for (int i = 0; i < MAX_DATA_POINTS; i++) {
                json += "{\"activeDiff\":" + String(dataPoints[i].activeDiff) +
                        ",\"reactiveDiff\":" + String(dataPoints[i].reactiveDiff) +
                        ",\"intensitaLuminosa\":" + String(dataPoints[i].intensitaLuminosa) +
                        ",\"timeDiff\":" + String(dataPoints[i].timeDiff) +
                        ",\"timestamp\":" + String(dataPoints[i].timestamp) + "}";
                if (i < MAX_DATA_POINTS - 1) json += ",";
            }
            json += "]";
            webSocket.sendTXT(num, json);
        }
    }
}

void notifyClients() {
    String json = "{\"activeDiff\":" + String(dataPoints[dataIndex].activeDiff) +
                  ",\"reactiveDiff\":" + String(dataPoints[dataIndex].reactiveDiff) +
                  ",\"intensitaLuminosa\":" + String(dataPoints[dataIndex].intensitaLuminosa) +
                  ",\"timeDiff\":" + String(dataPoints[dataIndex].timeDiff) +
                  ",\"timestamp\":" + String(dataPoints[dataIndex].timestamp) + "}";

        Serial.println(json);

    webSocket.broadcastTXT(json);
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }

    if (!LittleFS.begin()) {
        Serial.println("An error has occurred while mounting LittleFS");
        return;
    }

    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);

    server.on("/", HTTP_GET, handleRoot);
    server.serveStatic("/Chart.min.js", LittleFS, "/Chart.min.js");
    server.begin();
    
    webSocket.begin();
    webSocket.onEvent(handleWebSocketMessage);

    for (int i = 0; i < MAX_DATA_POINTS; i++) {
        dataPoints[i] = {0, 0, 0, 0}; // Initialize all data points to zero
    }

    //configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // Configure time from NTP server
}

void loop() {
    webSocket.loop();

    if (serialdatapacket_ready()) {
        DataPacket packet = read_serialdatapacket();

        time_t now = time(nullptr);
        dataPoints[dataIndex] = {packet.activeDiff, packet.reactiveDiff, packet.intensitaLuminosa, packet.timeDiff,now};
        notifyClients();
        dataIndex = (dataIndex + 1) % MAX_DATA_POINTS;
    }
}