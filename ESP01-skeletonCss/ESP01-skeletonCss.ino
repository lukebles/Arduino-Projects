#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer_Generic.h>
#include <LittleFS.h>

const char* ssid = "sid2";
const char* password = "pw12345678";

AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

void setup() {
  Serial.begin(115200);
  
  // Configurazione come Access Point
  WiFi.softAP(ssid, password);
  Serial.println("Access Point configurato");
  
  // Inizializza LittleFS
  if (!LittleFS.begin()) {
    Serial.println("Errore nella inizializzazione di LittleFS");
    return;
  }

  // Servire il file CSS
  server.on("/skeleton.min.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/skeleton.min.css", "text/css");
  });

  // Servire le pagine HTML
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });
  server.on("/page1.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/page1.html", "text/html");
  });
  server.on("/page2.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/page2.html", "text/html");
  });
  server.on("/page3.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/page3.html", "text/html");
  });
  server.on("/page4.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/page4.html", "text/html");
  });
  server.on("/page5.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/page5.html", "text/html");
  });

  server.begin();
  Serial.println("Server HTTP avviato");

  // Inizializza WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket avviato");
}

void loop() {
  webSocket.loop();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  // Gestisci gli eventi WebSocket
}
