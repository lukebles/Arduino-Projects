#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>
#include <vector>
std::vector<int> points_cont;  // Per memorizzare le coordinate
std::vector<String> textMessages; // Vettore per memorizzare i messaggi testuali


unsigned long lastDataTime = 0;  // Tempo dell'ultimo dato ricevuto

// ===================================
// change ssid and password as needed
// ===================================
const char* ssid = "sid";
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
    lastDataTime = millis();  // Aggiorna il tempo dell'ultimo dato ricevuto
  }

  // Controlla se è passato più di un secondo dall'ultimo dato
  if (millis() - lastDataTime > 100) {
    // Assumi che nessun dato sia stato ricevuto e aggiungi null a points_cont
    points_cont.push_back(-1); // i valori ricevuti devono essere tutti posiivi -1 segnala una anomalia
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
    sendTextMessagesToClient();
  }
}

void handleSerialMessage(String message) {
  Serial.println(message);
  //String debugMessage = "Received: " + message;
  String scriptDebug = "addDebugMessage('" + message + "');";
  webSocket.broadcastTXT(scriptDebug);  // Invia il messaggio di debug al client

  bool isNumeric = true;

  // Controlla se ogni carattere nella stringa è una cifra
  for (unsigned int i = 0; i < message.length(); i++) {
    if (!isDigit(message[i])) {
      isNumeric = false;
      break;
    }
  }

  // Procedi solo se la stringa è numerica
  if (isNumeric) {
    int value = message.toInt();   // Converti la stringa in un numero intero
    points_cont.push_back(value);  // Aggiungi il valore al buffer

    // Mantieni solo gli ultimi 30 elementi nel buffer
    if (points_cont.size() > 30) {
      points_cont.erase(points_cont.begin());
    }

    ///////////////////////////

  // Aggiungi il messaggio al vettore
  textMessages.push_back(message);

  // Mantieni solo gli ultimi N messaggi (ad esempio, 30)
  if (textMessages.size() > 30) {
    textMessages.erase(textMessages.begin());
  }    

    // Aggiorna il grafico ad ogni ricezione
    sendCoordinatesToClient();
  }
}

void sendTextMessagesToClient() {
  for (const String& message : textMessages) {
    String scriptMessage = "addDebugMessage('" + message + "');";
    webSocket.broadcastTXT(scriptMessage);
  }
}


void sendCoordinatesToClient() {
  String script = "drawPoints([";

  for (int i = 0; i < 30; i++) {
    // Calcola l'indice del valore da utilizzare
    //int index = points_cont.size() - windowSize + i;

    // Ottieni il valore dalla posizione calcolata
    int y = points_cont[i];

    // Assicurati che il valore sia valido prima di aggiungerlo
    script += "" + String(y) + ",";
  }
  // rimuove ultima virgola
  script.remove(script.length() - 1);

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
var lastMessageTime = 0; // Inizializza la variabile all'avvio

function addDebugMessage(message) {
  var debugConsole = document.getElementById('debugConsole');
  var now = new Date();
  var formattedDateTime = now.getFullYear() + '-' +
    ('0' + (now.getMonth() + 1)).slice(-2) + '-' +
    ('0' + now.getDate()).slice(-2) + ' ' +
    ('0' + now.getHours()).slice(-2) + ':' +
    ('0' + now.getMinutes()).slice(-2) + ':' +
    ('0' + now.getSeconds()).slice(-2) + '.' +
    ('00' + now.getMilliseconds()).slice(-3);

  // Calcola la differenza di tempo in millisecondi
  var timeDifference = now - lastMessageTime;
  lastMessageTime = now; // Aggiorna il tempo dell'ultimo messaggio
  var timeDiffElement = '<span class="time-diff">' + timeDifference + ' ms</span>';
    
  // Convertire il messaggio in esadecimale
  var hexMessage = '';
  for (var i = 0; i < message.length; i++) {
      hexMessage += message.charCodeAt(i).toString(16) + ' ';
  }

  // Crea un elemento HTML per il messaggio
  var messageElement = '<span class="message">' + message + '</span>';

  // Aggiungi la data, l'ora e il messaggio formattato
  debugConsole.innerHTML += '' + formattedDateTime + ' ' + messageElement + ' ' + hexMessage + ' ' + timeDiffElement + '<br>';

  // Scorre automaticamente in basso
  debugConsole.scrollTop = debugConsole.scrollHeight;

  // Limita a 30 messaggi
  var messages = debugConsole.innerHTML.split('<br>');
  if (messages.length > 31) { // 31 perché l'ultimo split crea un elemento vuoto
    debugConsole.innerHTML = messages.slice(-31).join('<br>');
  }
}

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