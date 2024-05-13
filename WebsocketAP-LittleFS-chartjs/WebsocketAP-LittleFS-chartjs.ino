#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>
#include "LittleFS.h"
 
const char* ssid = "sid2";           // Sostituisci con il nome della tua rete
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


    server.on("/", HTTP_GET, handleRoot);
    server.on("/Chart.min.js", HTTP_GET, handleChartJS);
    server.begin();

    Serial.println();
    Serial.println("HTTP server started");

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
}
 
void loop() {
  server.handleClient();

}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    Serial.println((char*)payload);
  }
}

void handleRoot() {
    File file = LittleFS.open("/main.html", "r");
    if (!file) {
        server.send(404, "text/plain", "File not found");
        return;
    }
    size_t sent = server.streamFile(file, "text/html");
    file.close();
}

void handleChartJS() {
    File file = LittleFS.open("/Chart.min.js", "r");
    if (!file) {
        server.send(404, "text/plain", "File not found");
        return;
    }
    size_t sent = server.streamFile(file, "application/javascript");
    file.close();
}

