#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>

const char* ssid = "sid";
const char* password = "pw12345678";

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

unsigned long previousMillis = 0; // Per tenere traccia del tempo trascorso
const long interval = 200; // Intervallo di tempo (in millisecondi)
int counter = 0; // Il contatore

void setup() {
  Serial.begin(115200);
  WiFi.softAP(ssid, password);
  Serial.println("Access Point Started");

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", getHTML());
  });

  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  webSocket.loop();
  server.handleClient();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    counter++; // Incrementa il contatore
    // Invia il valore del contatore a tutti i client connessi
    webSocket.broadcastTXT(String(counter));
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connection from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
      }
      break;
    case WStype_TEXT:
      Serial.printf("[%u] Text: %s\n", num, payload);
      // Qui puoi gestire i messaggi in arrivo se necessario
      break;
  }
}

String getHTML() {
  String html = "<!DOCTYPE html><html><head><title>Contatore WebSocket</title></script></head><body><h1>Valore del contatore: <span id='counter'>0</span></h1><script>var ws=new WebSocket('ws://'+location.hostname+':81/');ws.onmessage=function(event){document.getElementById('counter').textContent=event.data;};</script></body></html>";
  return html;
}
