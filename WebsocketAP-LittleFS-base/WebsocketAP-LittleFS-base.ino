#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>
#include "LittleFS.h"
 
const char* ssid = "sid";           // Sostituisci con il nome della tua rete
const char* password = "pw12345678";  // Sostituisci con la tua password

ESP8266WebServer server(80);
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

  server.on("/", HTTP_GET, []() {
    // Cerca il file main.html
    File file = LittleFS.open("/main.html", "r");
    if (!file) {
      server.send(500, "text/plain", "Errore nel leggere il file HTML");
      return;
    }
    
    String html = file.readString();
    file.close();
    server.send(200, "text/html", html);
  });

  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}
 
void loop() {
  server.handleClient();
  webSocket.loop();

  if (Serial.available() > 0) {
    String message = Serial.readStringUntil('\n');
    webSocket.broadcastTXT(message);
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    Serial.println((char*)payload);
  }
}
