#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer_Generic.h>
#include <LittleFS.h>
#include <time.h>
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
    time_t timestamp;
};

Data dataPoints[MAX_DATA_POINTS];
uint8_t dataIndex = 0;

void handleRoot(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
}

void handleWebSocketMessage(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (type == WStype_TEXT) {
        String message = (char*)payload;
        if (message.equals("getData")) {
            String json = "[";
            for (int i = 0; i < MAX_DATA_POINTS; i++) {
                json += "{\"activePower\":" + String(dataPoints[i].activePower) +
                        ",\"reactivePower\":" + String(dataPoints[i].reactivePower) +
                        ",\"lightPower\":" + String(dataPoints[i].lightPower) +
                        ",\"timestamp\":" + String(dataPoints[i].timestamp) + "}";
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
                struct timeval now = { .tv_sec = t, .tv_usec = 0 };
                settimeofday(&now, NULL);

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
                  ",\"timestamp\":" + String(dataPoints[dataIndex].timestamp) + "}";

        Serial.println(json);

    webSocket.broadcastTXT(json);
}

float roundToTens(float value) {
    return round(value / 10) * 10;
}

void setSystemTime(int year, int month, int day, int hour, int minute, int second) {
    struct tm tm;
    tm.tm_year = year - 1900; // Anno dal 1900
    tm.tm_mon = month - 1; // Mese (gennaio è 0)
    tm.tm_mday = day; // Giorno del mese
    tm.tm_hour = hour; // Ore
    tm.tm_min = minute; // Minuti
    tm.tm_sec = second; // Secondi
    tm.tm_isdst = 0; // Nessun DST

    time_t t = mktime(&tm);
    struct timeval now = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&now, NULL);
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
  // server.serveStatic("/", LittleFS, "/index.html");
  // server.serveStatic("/page1.html", LittleFS, "/page1.html");
  // server.serveStatic("/page2.html", LittleFS, "/page2.html");
  // server.serveStatic("/page3.html", LittleFS, "/page3.html");
  // server.serveStatic("/page4.html", LittleFS, "/page4.html");
  // server.serveStatic("/page5.html", LittleFS, "/page5.html");


    // server.serveStatic("/Chart.min.js", LittleFS, "/Chart.min.js");
  
    // server.on("/skeleton.min.css", HTTP_GET, [](AsyncWebServerRequest *request){
    //   request->send(LittleFS, "/skeleton.min.css", "text/css");
    // });

    // Servire le pagine HTML
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(LittleFS, "/index.html", "text/html");
    });
    server.on("/page1.html", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(LittleFS, "/page1.html", "text/html");
    });
    // server.on("/page2.html", HTTP_GET, [](AsyncWebServerRequest *request){
    //   request->send(LittleFS, "/page2.html", "text/html");
    // });
    // server.on("/page3.html", HTTP_GET, [](AsyncWebServerRequest *request){
    //   request->send(LittleFS, "/page3.html", "text/html");
    // });
    // server.on("/page4.html", HTTP_GET, [](AsyncWebServerRequest *request){
    //   request->send(LittleFS, "/page4.html", "text/html");
    // });
    // server.on("/page5.html", HTTP_GET, [](AsyncWebServerRequest *request){
    //   request->send(LittleFS, "/page5.html", "text/html");
    // });
  
    server.begin();
    
    webSocket.begin();
    webSocket.onEvent(handleWebSocketMessage);

    for (int i = 0; i < MAX_DATA_POINTS; i++) {
        dataPoints[i] = {0, 0, 0, 0}; // Initialize all data points to zero
    }

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

        dataPoints[dataIndex] = {activePower, reactivePower, lightPower, time(nullptr)};
        notifyClients();
        dataIndex = (dataIndex + 1) % MAX_DATA_POINTS;
    }
}