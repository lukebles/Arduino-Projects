#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>

const char* ssid = "sid";
const char* password = "pw12345678";

uint8_t righe = 20;
uint8_t colonne = 12;

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// codice HTML per la pagina principale
String getHTMLmain(){
  String html = R"====(
<!DOCTYPE html>
<html lang="it">
<head>
    <meta charset="UTF-8">
    <title>Tabella con SVG e Pulsante</title>
    <style>
        table, th, td {
            border: 1px solid black;
            border-collapse: collapse;
        }
        th, td {
            padding: 10px;
            text-align: center;
        }
        svg {
            width: 125px;
            height: 125px;
        }
    </style>
</head>
<body>

<table>

)====";

  // Crea le righe e le celle della tabella
  for (int row = 0; row < righe; row++) {
    html += "<tr>";
    // la numerazione parte dalla colonna 1
    for (int col = 0; col < colonne; col++) {
      html += "<td id='cell-" + String(row*colonne + col) + "'></td>";
    }
    html += "</tr>";
  }


  html += R"====(
<script>
  function inserisciPulsante(idCella, testoPulsante) {
    var cella = document.getElementById(idCella);
    var pulsante = document.createElement("button");
    pulsante.innerText = testoPulsante;
    cella.appendChild(pulsante);
  }
  )====";

  html += "inserisciPulsante('cell-'" + String(colonne) + ", 'Clicca Qui');";

  html += R"====(
</script>

</body>
</html>
  )====";

  return html;
}

void setup() {
  Serial.begin(115200);
  while (!Serial){;}
  WiFi.softAP(ssid, password);
  Serial.println("Access Point Started");

  // quando si raggiunge la pagina principale del browser...
  server.on("/", HTTP_GET, []() {
    // viene generato il codice HTML corrispondente
    server.send(200, "text/html", getHTMLmain());
  });

  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  webSocket.loop();
  server.handleClient();
  // qui ci va il codice 
  // ESP01 -> WEBSERVER
  //
  //  webSocket.broadcastTXT(jsonPayload);
}


void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  // qui ci va il codice
  // WEBSERVER -> ESP01
}


