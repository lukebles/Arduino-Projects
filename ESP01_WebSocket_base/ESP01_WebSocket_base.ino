#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>

const char* ssid = "sid";           // Sostituisci con il nome della tua rete
const char* password = "pwd12345";  // Sostituisci con la tua password

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

void setup() {
  Serial.begin(115200);
  WiFi.softAP(ssid, password);

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", getHTML());
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

String getHTML() {
  String html = "<!DOCTYPE html><html><body>"
                "<button id='button'>Invia ID</button>"
                "<input type='text' id='textBox' placeholder='Testo ricevuto'>"
                "<script>"
                "var webSocket = new WebSocket('ws://' + window.location.hostname + ':81/');"
                "webSocket.onmessage = function(event) {"
                "  document.getElementById('textBox').value = event.data;"
                "};"
                "document.getElementById('button').addEventListener('click', function() {"
                "  webSocket.send('buttonClicked');"
                "});"
                "</script>"
                "</body></html>";
  return html;
}