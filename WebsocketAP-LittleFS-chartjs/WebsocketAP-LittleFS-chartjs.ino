#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer_Generic.h>
#include "LittleFS.h"

const char* ssid = "sid2";           // Sostituisci con il nome della tua rete
const char* password = "pw12345678";  // Sostituisci con la tua password

AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // aspetta la connessione
  }

  if(!LittleFS.begin()){
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
  
  WiFi.softAP(ssid, password);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/main.html", "text/html");
  });

  server.on("/Chart.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/Chart.min.js", "application/javascript");
  });

  server.begin();
  Serial.println();
  Serial.println("HTTP server started");

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}
 
void loop() {
  // Non è necessario chiamare server.handleClient() con ESPAsyncWebServer
  webSocket.loop();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    Serial.println((char*)payload);
  }
}
