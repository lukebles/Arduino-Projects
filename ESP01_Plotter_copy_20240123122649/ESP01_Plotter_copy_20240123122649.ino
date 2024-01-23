#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>
#include <vector>
std::vector<int> points_cont;  // Per memorizzare le coordinate

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

    // Mantieni solo gli ultimi 128 elementi nel buffer
    if (points_cont.size() > 128) {
      points_cont.erase(points_cont.begin());
    }

    // Aggiorna il grafico ad ogni ricezione
    sendCoordinatesToClient();
  }
}

void sendCoordinatesToClient() {
  String script = "drawPoints([";


  for (int i = 0; i < 128; i++) {
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
  //script = "drawPoints([100,200,300,400,500,600,700,800,900,1000,100,200,300,400,500,600,700,800,900,1000,300,400]);";

  webSocket.broadcastTXT(script);
}

String getHTML() {
  String html = "<!DOCTYPE html><html>";
  html += "<head>";
  html += getHead();
  html += "</head>";
  html += "<body>";
  html += "<svg id='graph' width='500' height='500' style='border:1px solid black'></svg>";
  html += "<div><textarea id='debugConsole' rows='30' cols='80' readonly></textarea></div>";
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
      // Ottenere la data e l'ora attuali con i millisecondi
      var now = new Date();
      var formattedDateTime = now.getFullYear() + '-' +
        ('0' + (now.getMonth() + 1)).slice(-2) + '-' +
        ('0' + now.getDate()).slice(-2) + ' ' +
        ('0' + now.getHours()).slice(-2) + ':' +
        ('0' + now.getMinutes()).slice(-2) + ':' +
        ('0' + now.getSeconds()).slice(-2) + '.' +
        ('00' + now.getMilliseconds()).slice(-3);      
      // Convertire il messaggio in esadecimale
      var hexMessage = '';
      for (var i = 0; i < message.length; i++) {
          hexMessage += message.charCodeAt(i).toString(16) + ' ';
      }
      // Aggiungere la data, l'ora e il messaggio in esadecimale alla console
      debugConsole.value += '' + formattedDateTime + ' ' + ' ' + message + ' ' + hexMessage + '\n';
      // Scorre automaticamente in basso
      debugConsole.scrollTop = debugConsole.scrollHeight;
  }
  function drawPoints(points) {
    var svg = document.getElementById('graph');
    svg.innerHTML = ''; // Pulisci il grafico prima di disegnare nuovi punti e linee
    var svgWidth = svg.clientWidth;
    var svgHeight = svg.clientHeight;
    
    // Calcola la scala per il valore y (ad esempio, se i valori massimi sono 1023)
    var yScale = svgHeight / 1023;

    // Disegna le linee e i punti
    for (var i = 0; i <= 127; i++) {
        // Calcola le coordinate x e y
        var x1 = (svgWidth / 128) * i;
        var y1 = svgHeight - (points[i] * yScale);
        var x2 = (svgWidth / 128) * (i + 1);
        var y2 = svgHeight - (points[i + 1] * yScale);

        if ( i < 127 ) {
          // Crea e aggiungi la linea
          var line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
          line.setAttribute('x1', x1);
          line.setAttribute('y1', y1);
          line.setAttribute('x2', x2);
          line.setAttribute('y2', y2);
          line.setAttribute('stroke', 'black');
          svg.appendChild(line);
        }

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
