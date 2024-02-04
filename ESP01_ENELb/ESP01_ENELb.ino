#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>
#include <LkArraylize.h>
#include "LkHexBytes.h" 

struct DataPacket{
  uint8_t sender;
  uint16_t countActiveWh;
  uint16_t countReactiveWh;
};

// ===================================
// change ssid and password as needed
// ===================================
const char* ssid = "sid";
const char* password = "pw12345678";

uint16_t previous_countActiveWh = 0;
unsigned long previous_time_ENEL = 0;
#define NUM_STRINGS 30 // dimensione dell'array
#define STRING_LENGTH 30 // lunghezza massima della stringa

char myStrings[NUM_STRINGS][STRING_LENGTH];
unsigned long myStringDiffTimes[NUM_STRINGS];

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
    String miaStringa = Serial.readStringUntil('\n');
    // controlla se i primi due caratteri della stringa
    // sono "01" che identifica i dati ENEL
    if (strncmp(myCharArray, "01", 2) == 0) {
      // converto la stringa in array di caratteri
      // il codice è una stringa di 10 caratteri tipo "01FA4388FE"
      char hexString[11];
      byte barray[6]; // 5 caratteri + 1 per il terminatore nullo
      String miaStringa = Serial.readStringUntil('\n');
      strncpy(hexString, miaStringa.c_str(), sizeof(hexString) - 1);
      hexString[sizeof(hexString) - 1] = '\0'; // terminazione stringa

      LkHexBytes::hexCharacterStringToBytes(barray, hexString);
      // valore numerico del contatore
      uint16_t contatoreWHA = (barray[2] << 8) | barray[1];

      //////////////////////////////////////
      if (previous_time_ENEL == 0){
        // prima ricezione
        previous_time_ENEL = millis();
        previous_countActiveWh = contatoreWHA;
      } else {
        // funziona anche la differenza durante il passaggio da 4294967295 a 0, 1, 2, ... eccetera
        unsigned long diffmillis = millis() - previous_time_ENEL;
        previous_time_ENEL = millis();
        // funziona anche la differenza durante il passaggio da 65535 a 0, 1, 2, ... eccetera
        unsigned long diffEnergia = contatoreWHA - previous_countActiveWh;
        previous_countActiveWh = contatoreWHA;
        // calcoli
        float delta_tempo_sec = float(diffmillis)/1000.0;
        float delta_energia_wh = float(diffEnergia);
        // determinazione della potenza attiva istantanea
        float potenzaAttiva = 3600.0 / float(delta_tempo_sec) * float(delta_energia_wh);

      }
      //////////////////////////////////////

      String script = "newSerialData('" + miaStringa + "-" + String(contatoreWHA) + "')";
      webSocket.broadcastTXT(script);
    }
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

    var testo = arrayMessaggi[i];
    
    // Crea una nuova riga per ogni coppia di data/ora e messaggio
    
    debugConsole.innerHTML += '<div>' + formattedDateTime + ': ' + testo + ' ' + timeDiffElement + '</div>';
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