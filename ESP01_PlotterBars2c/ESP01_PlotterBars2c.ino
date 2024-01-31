#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>

/*

Questo è l'ultimo progetto che ho fatto con ESP-01
Al momento ogni volta che riceve un dato dalla seriale viene ricaricato completamente
il grafico e la console con i dati
Bisognerebbe invece che questa operazione di caricamento complessivo avvenga
solo quando il browser si collega la prima volta mentre una volta che è collegato
sia il grafico che il contenuto del testo devono essere aggiornati con l'ultimo dato
disponibile, spostando i dati più vecchi di una posizione.
In questo modo si velocizza drasticamente l'aggiornamento della pagina

*/


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
    String hexString = "";

    // Itera ogni carattere della stringa
    for (int i = 0; i < message.length(); i++) {
      // Converte il carattere in esadecimale e lo aggiunge alla stringa hexString
      char buf[3]; // Array per contenere la coppia esadecimale e il carattere terminatore
      sprintf(buf, "%02X", message[i]); // Converte il carattere in esadecimale
      hexString += String(buf); // Aggiunge la rappresentazione esadecimale alla stringa finale
    }

    String script = "newSerialData('" + hexString + "')";
    webSocket.broadcastTXT(script);
  }

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

var sizeArray = 30;
var arrayDateTime = new Array(sizeArray); 
var arrayMessaggi = new Array(sizeArray);

function newSerialData(data) {
  // Aggiunge la data e l'ora corrente all'array
  var now = new Date();

  arrayDateTime.push(now);

  // Aggiunge il messaggio ricevuto all'array
  arrayMessaggi.push(data);

  // Verifica se la lunghezza degli array supera 30
  if (arrayDateTime.length > sizeArray) {
    // Rimuove l'elemento più vecchio
    arrayDateTime.shift();
  }
  if (arrayMessaggi.length > sizeArray) {
    // Rimuove l'elemento più vecchio
    arrayMessaggi.shift();
  }
  printArraysToConsole();
  plotDraw();
}

function printArraysToConsole() {
  // Ottiene l'elemento della console di debug
  var debugConsole = document.getElementById('debugConsole');

  // Pulisce la console prima di stampare i nuovi dati
  debugConsole.innerHTML = '';

  // Itera sugli array e aggiunge il contenuto alla console di debug
  for (var i = 0; i < arrayDateTime.length; i++) {
    var dateTime = arrayDateTime[i];

    // Data formattata
    var formattedDateTime = dateTime.getFullYear() + '-' +
        ('0' + (dateTime.getMonth() + 1)).slice(-2) + '-' +
        ('0' + dateTime.getDate()).slice(-2) + ' ' +
        ('0' + dateTime.getHours()).slice(-2) + ':' +
        ('0' + dateTime.getMinutes()).slice(-2) + ':' +
        ('0' + dateTime.getSeconds()).slice(-2) + '.' +
        ('00' + dateTime.getMilliseconds()).slice(-3);

      // Calcolo della differenza temporale in millisecondi
      var timeDifference = '';
      if (i < arrayDateTime.length - 1) {
        timeDifference = arrayDateTime[i + 1] - arrayDateTime[i]; 
      }
      var timeDiffElement = '<span class="time-diff">' + timeDifference + ' ms</span>';

    var message = arrayMessaggi[i];
    var testo = hexStringToAscii(message);
    
    // Crea una nuova riga per ogni coppia di data/ora e messaggio
    
    debugConsole.innerHTML += '<div>' + formattedDateTime + ': <span style = "font-family: monospace;">' + message + '</span> ' + testo + ' <span style = "color: red;">' + timeDiffElement + '</span></div>';
  }
}

function hexStringToAscii(str) {
  var asciiStr = '';
  var nonPrintableChar = '?'; // Carattere per rappresentare caratteri non stampabili

  for (var i = 0; i < str.length; i += 2) {
    var hex = str.substr(i, 2);
    var charCode = parseInt(hex, 16);
    
    // Controlla se il carattere è visualizzabile
    if (charCode >= 32 && charCode <= 126) {
      // Aggiunge il carattere ASCII corrispondente
      asciiStr += String.fromCharCode(charCode);
    } else {
      // Sostituisce con il carattere non stampabile
      asciiStr += nonPrintableChar;
    }
  }
  return asciiStr;
}

function plotDraw(){
  var svg = document.getElementById('graph');
  svg.innerHTML = ''; // Pulisci il grafico prima di disegnare nuove barre
  var svgWidth = svg.clientWidth;
  var svgHeight = svg.clientHeight;

  // Calcola la scala per il valore y (ad esempio, se i valori massimi sono 1023)
  var yScale = svgHeight / 1023;

  // Larghezza di ogni barra
  var barWidth = svgWidth / 30;

  // Disegna le barre
  for (var i = 0; i < arrayMessaggi.length; i++) {
    var x = barWidth * i;
    var y;
    var height;
    var barColor;
    var numero = Number(hexStringToAscii(arrayMessaggi[i]));

    // Se il dato è undefined o null, disegna una barra grigia di altezza massima
    if (numero === -1) {
      y = 0;
      height = svgHeight;
      barColor = 'gray';
    } else {
      y = svgHeight - (numero * yScale);
      height = numero * yScale;
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