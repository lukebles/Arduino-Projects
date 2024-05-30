#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer_Generic.h>
#include <LittleFS.h>
#include <TimeLib.h>
#include "serial_packet_handler.h"

#define MAX_DATA_POINTS 31

const char* ssid = "sid2";
const char* password = "pw12345678";
const float LIGHT_TO_POWER = 1.0;

AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

struct Data {
    uint16_t activePower;
    uint16_t reactivePower;
    uint16_t lightPower;
    char timestamp[20]; // dd/mm/yyyy hh:mm:ss
};

Data dataPoints[MAX_DATA_POINTS];
uint8_t dataIndex = 0;

void formatTimestamp(char* buffer, size_t len) {
    snprintf(buffer, len, "%02d/%02d/%04d %02d:%02d:%02d", day(), month(), year(), hour(), minute(), second());
}

void handleRoot(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
}

void handleWebSocketMessage(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    if (type == WStype_TEXT) {
        String message = (char*)payload;
        if (message.equals("getData")) {
            String json = "[";
            for (int i = 0; i < MAX_DATA_POINTS; i++) {
                json += "{\"activePower\":" + String(dataPoints[i].activePower) +
                        ",\"reactivePower\":" + String(dataPoints[i].reactivePower) +
                        ",\"lightPower\":" + String(dataPoints[i].lightPower) +
                        ",\"timestamp\":\"" + String(dataPoints[i].timestamp) + "\"}";
                if (i < MAX_DATA_POINTS - 1) json += ",";
            }
            json += "]";
            webSocket.sendTXT(num, json);
        } else {
            // Parsing del messaggio per impostare l'ora
            struct tm tm;
            if (sscanf(message.c_str(), "%d-%d-%d %d:%d:%d",
                       &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                       &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
                tm.tm_year -= 1900; // Gli anni partono da 1900
                tm.tm_mon -= 1; // I mesi partono da 0
                tm.tm_isdst = 0; // Nessun DST

                time_t t = mktime(&tm);
                setTime(t);

                // Conferma dell'impostazione dell'ora
                webSocket.sendTXT(num, "Time updated successfully");
            } else {
                webSocket.sendTXT(num, "Invalid time format");
            }
        }
    }
}

void notifyClients() {
    String json = "{\"activePower\":" + String(dataPoints[dataIndex].activePower) +
                  ",\"reactivePower\":" + String(dataPoints[dataIndex].reactivePower) +
                  ",\"lightPower\":" + String(dataPoints[dataIndex].lightPower) +
                  ",\"timestamp\":\"" + String(dataPoints[dataIndex].timestamp) + "\"}";

    Serial.println(json);

    webSocket.broadcastTXT(json);
}

float roundToTens(float value) {
    return round(value / 10) * 10;
}

void setSystemTime(int year, int month, int day, int hour, int minute, int second) {
    setTime(hour, minute, second, day, month, year);
}

void initializeDataPoints() {
    for (int i = 0; i < MAX_DATA_POINTS; i++) {
        dataPoints[i].activePower = 0;
        dataPoints[i].reactivePower = 0;
        dataPoints[i].lightPower = 0;
        memset(dataPoints[i].timestamp, 0, sizeof(dataPoints[i].timestamp)); // Initialize the timestamp to zero
    }
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

    // Servire file statici
    server.serveStatic("/Chart.min.js", LittleFS, "/Chart.min.js");
    server.serveStatic("/skeleton.min.css", LittleFS, "/skeleton.min.css");

    // Gestire richieste HTML
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html");
    });
    server.on("/page1.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/page1.html", "text/html");
    });

    server.begin();
    
    webSocket.begin();
    webSocket.onEvent(handleWebSocketMessage);

    initializeDataPoints(); // Initialize all data points

    setSystemTime(2000, 1, 1, 0, 0, 0); // Imposta l'orario al 1/1/2000 00:00:00
}

void loop() {
    webSocket.loop();

    if (serialdatapacket_ready()) {
        DataPacket packet = read_serialdatapacket();
        float activePower = (packet.activeDiff * 3600.0) / (packet.timeDiff / 1000.0); // Wh in W
        float reactivePower = (packet.reactiveDiff * 3600.0) / (packet.timeDiff / 1000.0); 
        float lightPower = packet.lightIntensity * LIGHT_TO_POWER;

        // Arrotondamento alle decine
        activePower = roundToTens(activePower);
        reactivePower = roundToTens(reactivePower);
        lightPower = roundToTens(lightPower);

        char buffer[20];
        formatTimestamp(buffer, sizeof(buffer));
        
        dataPoints[dataIndex].activePower = activePower;
        dataPoints[dataIndex].reactivePower = reactivePower;
        dataPoints[dataIndex].lightPower = lightPower;
        strcpy(dataPoints[dataIndex].timestamp, buffer);

        notifyClients();
        dataIndex = (dataIndex + 1) % MAX_DATA_POINTS;
    }
}

