#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>
#include <vector>
std::vector<int> points_cont; // Per memorizzare le coordinate

// ===================================
// change ssid and password as needed
// ===================================
const char* ssid = "atheros";    
const char* password = "pw12345";  

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

void setup() {
  Serial.begin(115200);

  //
  // ============================
  // uncomment the lines below 
  // to select the desired WiFi operation type
  // ============================
  //
  // --------------------------
  // ** AP **
  // (creates a Wifi Access Point)
  // --------------------------
  WiFi.softAP(ssid, password);
  // --------------------------
  //
  // --------------------------
  // ** STATION **
 
  // --------------------------
  //

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
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    Serial.println((char*)payload);
  }
}

void handleSerialMessage(String message) {
  int value = message.toInt(); // Converti la stringa in un numero intero
  points_cont.push_back(value); // Aggiungi il valore al buffer
  
  // Mantieni solo gli ultimi 22 elementi nel buffer
  if (points_cont.size() > 22) {
    points_cont.erase(points_cont.begin());
  }
  
  String debugMessage = "Received: " + message;
  String scriptDebug = "addDebugMessage('" + debugMessage + "');";
  webSocket.broadcastTXT(scriptDebug); // Invia il messaggio di debug al client

  // Aggiorna il grafico ad ogni ricezione
  sendCoordinatesToClient();
}



void sendCoordinatesToClient() {
  String script = "drawPoints([";

  // Determina la dimensione della finestra dei dati (ad esempio, 22 punti)
  int windowSize = points_cont.size() < 22 ? points_cont.size() : 22;

  for (int i = 0; i < windowSize; i++) {
    // Calcola l'indice del valore da utilizzare
    int index = points_cont.size() - windowSize + i;

    // Ottieni il valore dalla posizione calcolata
    int y = points_cont[index];

    // Assicurati che il valore sia valido prima di aggiungerlo
    if (y >= 0) { // o qualsiasi altra condizione di validità che desideri
      //int x = i; // Usa l'indice come coordinata X
      script += "[" + String(y) + "],";
    }
  }

  script += "]);";
  webSocket.broadcastTXT(script);
}




String getHTML() {
  String html = "<!DOCTYPE html><html>";
  html += "<head>";
  html += getHead();
  html += "</head>";
  html += "<body>";
  html += "<svg id='graph' width='500' height='500' style='border:1px solid black'></svg>";
  // debug
  html += "<textarea id='debugConsole' rows='10' cols='50' readonly></textarea>";
  
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
  function addDebugMessage(message) {
    var debugConsole = document.getElementById('debugConsole');
    debugConsole.value += message + '\n';
    debugConsole.scrollTop = debugConsole.scrollHeight; // Scorre automaticamente in basso
  }
  function drawPoints(points) {
    var svg = document.getElementById('graph');
    svg.innerHTML = ''; // Pulisci il grafico prima di disegnare nuovi punti e linee
    var svgWidth = svg.clientWidth;
    var svgHeight = svg.clientHeight;
    
    // Calcola la scala per il valore y (ad esempio, se i valori massimi sono 1023)
    var yScale = svgHeight / 1023;

    // Disegna le linee e i punti
    for (var i = 0; i < points.length - 1; i++) {
        // Calcola le coordinate x e y
        var x1 = (svgWidth / 22) * (i);
        var y1 = svgHeight - (points[i][0] * yScale);

        // Crea e aggiungi il punto
        var circle = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
        circle.setAttribute('cx', x1);
        circle.setAttribute('cy', y1);
        circle.setAttribute('r', 5);
        circle.setAttribute('fill', 'red');
        svg.appendChild(circle);
    }
  }
    )";
  return html;
}

String getHead() {
  String html = R"(
    <title>Points</title>
    <style>
    </style>)";
  return html;
}