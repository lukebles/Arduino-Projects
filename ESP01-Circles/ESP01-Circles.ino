

#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>
#include <vector>
std::vector<std::pair<int, int>> coordinates; // Per memorizzare le coordinate


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
  // (connection to an existing wifi network)
  // --------------------------
  // WiFi.begin(ssid, password); 
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.print(".");
  // }
  // Serial.println("");
  // Serial.print("Connected to ");
  // Serial.println(ssid);
  // Serial.print("IP address: ");
  // Serial.println(WiFi.localIP());
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
  if (message == "FINE") {
    sendCoordinatesToClient();
    coordinates.clear(); // Pulisci le coordinate dopo l'invio
  } else {
    int commaIndex = message.indexOf(',');
    if (commaIndex != -1) {
      int x = message.substring(0, commaIndex).toInt();
      int y = message.substring(commaIndex + 1).toInt();
      coordinates.push_back(std::make_pair(x, y)); // Memorizza la coppia di coordinate
    }
  }
}


void sendCoordinatesToClient() {
  String script = "drawPoints([";
  for (const auto& coord : coordinates) {
    script += "[" + String(coord.first) + ", " + String(coord.second) + "],";
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
    svg.innerHTML = ''; // Pulisci il grafico prima di disegnare nuovi punti e linee
    // Disegna le linee
    for (var i = 0; i < points.length - 1; i++) {
        var line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
        line.setAttribute('x1', points[i][0]);
        line.setAttribute('y1', points[i][1]);
        line.setAttribute('x2', points[i + 1][0]);
        line.setAttribute('y2', points[i + 1][1]);
        line.setAttribute('stroke', 'black');
        svg.appendChild(line);
    }
    // Disegna i punti
    points.forEach(function(point) {
        var circle = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
        circle.setAttribute('cx', point[0]);
        circle.setAttribute('cy', point[1]);
        circle.setAttribute('r', 5);
        circle.setAttribute('fill', 'red');
        svg.appendChild(circle);
    });
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


