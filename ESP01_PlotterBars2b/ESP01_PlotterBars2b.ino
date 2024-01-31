#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>

const int numPoints = 30;  // Numero di punti nell'array
String points_cont[numPoints];  // Array per memorizzare le coordinate
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
}

void handleSerialMessage(String message) {
  sistemaArray();
  points_cont[currentIndex] =  message; 
  sendCoordinatesToClient();
}

void sistemaArray(){
  if (currentIndex == numPoints - 1){
    // sono arrivato alla cima
    // quindi scalo i vaori in modo da fare posto all'ultimo valore
    for (int x = 0; x < numPoints - 1; x++) {
      points_cont[x] = points_cont[x + 1];
    }
  } else {
    // sto ancora creando l'array
    currentIndex += 1;
  }
}

void sendCoordinatesToClient() {
  String script = "plotGraph([";
  for (int i = 0; i < numPoints; i++) {
    script += points_cont[i] + ",";
  }
  script.remove(script.length() - 1);  // Rimuove l'ultima virgola
  script += "]);";
  webSocket.broadcastTXT(script);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
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

var arrayDate = new Array(30); 
var giaRicevuto = false;

function setDateToMessages(messages){
  if (messages.length == 30){
    if (giaRicevuto == false) {
      // è la prima volta che vedo la lunghezza 29
      // quindi mi comport normalmente
      arrayDate[messages.length - 1] = new Date();
      // prendo nota che questo sarà l'ultimo
      // che non sposterà gli elementi dell'array
      giaRicevuto = true;
    } else {
      // Sposta tutti gli elementi indietro di un indice
      for (var i = 0; i < 29; i++) {
        arrayDate[i] = arrayDate[i + 1];
      }
      arrayDate[29] = new Date();
    }
  } else {
    // il numero di messaggi è ancora basso
    arrayDate[messages.length - 1] = new Date();
  }

  var debugConsole = document.getElementById('debugConsole');
  debugConsole.innerHTML = "";
  for (var m = 0; m < messages.length; m++){
      var message = messages[m];

      // Conversione del messaggio in esadecimale
      var hexMessage = '';
      for (var i = 0; i < message.length; i++) {
        hexMessage += message.charCodeAt(i).toString(16) + ' ';
      }

      // Data formattata
      var formattedDateTime = arrayDate[m].getFullYear() + '-' +
          ('0' + (arrayDate[m].getMonth() + 1)).slice(-2) + '-' +
          ('0' + arrayDate[m].getDate()).slice(-2) + ' ' +
          ('0' + arrayDate[m].getHours()).slice(-2) + ':' +
          ('0' + arrayDate[m].getMinutes()).slice(-2) + ':' +
          ('0' + arrayDate[m].getSeconds()).slice(-2) + '.' +
          ('00' + arrayDate[m].getMilliseconds()).slice(-3);

      // Calcolo della differenza temporale in millisecondi
      var timeDifference = '';
      if (m < messages.length - 1) {
        timeDifference = arrayDate[m + 1] - arrayDate[m]; // Correzione qui
      }
      var timeDiffElement = '<span class="time-diff">' + timeDifference + ' ms</span>';

      // Creazione di un elemento HTML per il messaggio
      var messageElement = '<span class="message">' + message + '</span>';

      debugConsole.innerHTML += formattedDateTime + ' ' + messageElement + ' ' + hexMessage + ' ' + timeDiffElement + '<br>';
  }


}

function plotGraph(messages) {

  setDateToMessages(messages);

  var svg = document.getElementById('graph');
  svg.innerHTML = ''; // Pulisci il grafico prima di disegnare nuove barre
  var svgWidth = svg.clientWidth;
  var svgHeight = svg.clientHeight;

  // Calcola la scala per il valore y (ad esempio, se i valori massimi sono 1023)
  var yScale = svgHeight / 1023;

  // Larghezza di ogni barra
  var barWidth = svgWidth / 30;

  // Disegna le barre
  for (var i = 0; i < messages.length; i++) {
    var x = barWidth * i;
    var y;
    var height;
    var barColor;

    // Se il dato è undefined o null, disegna una barra grigia di altezza massima
    if (messages[i] === -1) {
      y = 0;
      height = svgHeight;
      barColor = 'gray';
    } else {
      y = svgHeight - (messages[i] * yScale);
      height = messages[i] * yScale;
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