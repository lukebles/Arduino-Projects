//#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>

const char* ssid = "oscillo";
const char* password = "osc12345";

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// Funzione per inviare la pagina web
void handleRoot() {
  String htmlPage = R"=====(
<!DOCTYPE html>
<html>
<head>
    <title>Grafico SVG Dinamico</title>
    <style>
        svg { border: 1px solid black; }
    </style>
</head>
<body>
    <svg id='grafico' width='500' height='500'></svg>

    <script>
        document.addEventListener('DOMContentLoaded', function() {
            var svg = document.getElementById('grafico');
            var larghezza = svg.width.baseVal.value;
            var altezza = svg.height.baseVal.value;

            function creaPunto(x, y) {
                var cerchio = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
                cerchio.setAttribute('cx', x);
                cerchio.setAttribute('cy', y);
                cerchio.setAttribute('r', 5);
                cerchio.setAttribute('fill', 'red');
                return cerchio;
            }

            function creaLinea(x1, y1, x2, y2) {
                var linea = document.createElementNS('http://www.w3.org/2000/svg', 'line');
                linea.setAttribute('x1', x1);
                linea.setAttribute('y1', y1);
                linea.setAttribute('x2', x2);
                linea.setAttribute('y2', y2);
                linea.setAttribute('stroke', 'black');
                return linea;
            }

            function aggiornaGrafico() {
                while (svg.firstChild) {
                    svg.removeChild(svg.firstChild);
                }

                var xPrecedente, yPrecedente;

                for (var i = 0; i < 10; i++) {
                    var x = Math.random() * larghezza;
                    var y = Math.random() * altezza;

                    svg.appendChild(creaPunto(x, y));

                    if (i > 0) {
                        svg.appendChild(creaLinea(xPrecedente, yPrecedente, x, y));
                    }

                    xPrecedente = x;
                    yPrecedente = y;
                }
            }

            setInterval(aggiornaGrafico, 1000);
        });
    </script>
</body>
</html>
)=====";
  server.send(200, "text/html", htmlPage);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  // Gestisci qui gli eventi WebSocket se necessario
}


void setup() {
  Serial.begin(115200);
  
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("IP Address: ");
  Serial.println(myIP);

  server.on("/", handleRoot);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  static String incomingData = ""; // Stringa per accumulare i dati ricevuti
  webSocket.loop();
  server.handleClient();

  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    webSocket.broadcastTXT(data);
  }
}


