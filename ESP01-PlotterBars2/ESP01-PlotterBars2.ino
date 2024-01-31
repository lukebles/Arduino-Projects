#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>

const int numPoints = 30;  // Numero di punti nell'array
int points_cont[numPoints];  // Array per memorizzare le coordinate
int currentIndex = 0;  // Indice corrente nell'array

unsigned long lastDataTime = 0;  // Tempo dell'ultimo dato ricevuto

// ===================================
// change ssid and password as needed
// ===================================
const char* ssid = "sid";
const char* password = "pw12345678";

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
    handleSerialMessage(message);
    lastDataTime = millis();  // Aggiorna il tempo dell'ultimo dato ricevuto
  }

if (millis() - lastDataTime > 100) {
    points_cont[currentIndex] = -1; // Assumi che nessun dato sia stato ricevuto
    currentIndex = (currentIndex + 1) % numPoints; // Avanza l'indice circolarmente
    sendCoordinatesToClient();
    lastDataTime = millis();  // Resetta il timer
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    Serial.println((char*)payload);
  }

  if (type == WStype_CONNECTED) {
    // Invia i messaggi memorizzati al client appena connesso
    //sendTextMessagesToClient();
  }
}

void handleSerialMessage(String message) {
  bool isNumeric = true;

  if (isNumeric) {
    int value = message.toInt();  // Converti la stringa in un numero intero
    points_cont[currentIndex] = value;  // Aggiungi il valore all'array
    currentIndex = (currentIndex + 1) % numPoints; // Avanza l'indice circolarmente

    // Aggiorna il grafico ad ogni ricezione
    sendCoordinatesToClient();
  }
}

void sendCoordinatesToClient() {
  String script = "drawPoints([";
  for (int i = 0; i < numPoints; i++) {
    int index = (currentIndex + i) % numPoints;  // Calcola l'indice circolare
    int y = points_cont[index];
    script += String(y) + ",";
  }
  script.remove(script.length() - 1);  // Rimuove l'ultima virgola
  script += "]);";
  webSocket.broadcastTXT(script);
}

String getHTML() {
  String html = "<!DOCTYPE html><html>";
  html += "<head>";
  html += getHead();
  html += "</head>";
  html += "<body>";
  html += "<svg id='graph' width='125' height='125' style='border:1px solid black'></svg>";
  html += "<div id='debugConsole'></div>";
  html += "<script>";
  html += "var webSocket = new WebSocket('ws://' + window.location.hostname + ':81/');";
  html += "webSocket.onmessage = function(event) {";
  html += "  eval(event.data);";  // Esegue lo script ricevuto
  html += "};";
  html += getScript();
  html += "</script>";
  html += "</body>";
  html += "</html>";
  return html;
}

// initializeSliders --> updateSlider --> updateBackgroundColor
String getScript() {
  String html = R"(

function drawPoints(points) {
  var svg = document.getElementById('graph');
  svg.innerHTML = ''; // Pulisci il grafico prima di disegnare nuove barre
  var svgWidth = svg.clientWidth;
  var svgHeight = svg.clientHeight;

  // Calcola la scala per il valore y (ad esempio, se i valori massimi sono 1023)
  var yScale = svgHeight / 1023;

  // Larghezza di ogni barra
  var barWidth = svgWidth / 30;

  // Disegna le barre
  for (var i = 0; i < points.length; i++) {
    var x = barWidth * i;
    var y;
    var height;
    var barColor;

    // Se il dato è undefined o null, disegna una barra grigia di altezza massima
    if (points[i] === -1) {
      y = 0;
      height = svgHeight;
      barColor = 'gray';
    } else {
      y = svgHeight - (points[i] * yScale);
      height = points[i] * yScale;
      barColor = 'blue'; // Colore della barra se il dato è presente
    }

    // Crea e aggiungi la barra
    var rect = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
    rect.setAttribute('x', x);
    rect.setAttribute('y', y);
    rect.setAttribute('width', barWidth);
    rect.setAttribute('height', height);
    rect.setAttribute('fill', barColor);
    svg.appendChild(rect);
  }
}
    )";
  return html;
}

String getHead() {
  String html = R"(
<title>Points</title>
<style>
.message {
  color: blue;
  font-weight: bold;
}
#graph, #debugConsole {
  display: inline-block; /* Imposta entrambi gli elementi su inline-block */
  vertical-align: top; /* Allinea verticalmente gli elementi in alto */
}      

#debugConsole {
  width: 640px;";
  max-height: 6000px;
  border: 1px solid black;";
  overflow-y: auto;
  white-space: pre-wrap;
}
</style>
  )";
  return html;
}