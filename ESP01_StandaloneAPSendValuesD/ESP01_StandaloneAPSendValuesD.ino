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
    
    // Genera valori casuali e prepara il payload JSON
    String jsonPayload = "{\"values\":[";
    for (int i = 0; i < 400; i++) {
      int randomNumber = random(10, 100); // Genera un numero casuale tra 10 e 99
      jsonPayload += String(randomNumber);
      if (i < 399) jsonPayload += ",";
    }
    jsonPayload += "]}";

    // Invia il payload JSON a tutti i client connessi
    webSocket.broadcastTXT(jsonPayload);
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
  String html = R"=====(
<!DOCTYPE html>
<html>
<head>
<title>Tabella di Valori Casuali</title>
</head>
<body>
<h1>Tabella di Valori Casuali</h1>
<table id="valuesTable" border="1">
)=====";

  // Crea le righe e le celle della tabella
  for (int row = 0; row < 20; row++) {
    html += "<tr>";
    for (int col = 0; col < 20; col++) {
      html += "<td id='cell-" + String(row*20 + col) + "'>0</td>";
    }
    html += "</tr>";
  }

  html += R"=====(
</table>
<script>
var ws = new WebSocket('ws://' + location.hostname + ':81/');
ws.onmessage = function(event) {
  var data = JSON.parse(event.data);
  if(data.values) {
    for(var i = 0; i < data.values.length; i++) {
      document.getElementById('cell-' + i).textContent = data.values[i];
    }
  }
};
</script>
</body>
</html>
)=====";

  return html;
}
