#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>
#include <TimeLib.h>

const char* ssid = "sid";
const char* password = "pw12345678";

uint8_t righe = 20;
uint8_t colonne = 15;

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// Definisci una dimensione massima per l'HTML
const unsigned int HTML_SIZE = 15000;
unsigned long lastTime = 0; 


void sendRandomLength() {
  char message[300]; // Assicurati che sia abbastanza grande.
  int len = 0; // Usata per tenere traccia della lunghezza attuale del messaggio.

  len += snprintf(message + len, sizeof(message) - len, "["); // Inizia l'array JSON.

  for(int i = 0; i <= righe; i++) {
    if (i > 0) {
      len += snprintf(message + len, sizeof(message) - len, ","); // Aggiungi virgola.
    }
    int width = random(5, 31); // Genera un numero casuale per la larghezza.
    len += snprintf(message + len, sizeof(message) - len, "%d", width); // Aggiungi il numero.
  }

  len += snprintf(message + len, sizeof(message) - len, "]"); // Chiude l'array JSON.

  webSocket.broadcastTXT(message); // Invia il messaggio.
}



// codice HTML per la pagina principale
char* getHTMLmain() {
  static char html[HTML_SIZE];
  memset(html, 0, HTML_SIZE); // Pulisce l'array

  // Parte iniziale dell'HTML
  strcat(html, "<!DOCTYPE html>"
    "<html lang=\"it\">"
    "<head>"
    "<meta charset=\"UTF-8\">"
    "<title>Tabella con SVG e Pulsante</title>"
    "<style>"
    "table, th, td {border: 1px solid black; border-collapse: collapse; padding: 0}"
    "th, td {padding: 10px; text-align: center;}"
    ".svg-cell {padding: 0; margin: 0;}"
    "</style>"
    "</head><body>"
    "<table>");

  // Crea le righe e le celle della tabella
  char cell[200]; // Aumenta la dimensione se necessario
  for (int row = 0; row < righe; row++) {
    strcat(html, "<tr>");
    for (int col = 0; col < colonne; col++) {
      if (col == 1) {        
        sprintf(cell, "<td class='svg-cell'>"
        "<svg id='svg-%d' width='25' height='25'>"
        "<circle cx='12.5' cy='12.5' r='12.5' fill='red' />"
        "</svg>"
        "</td>", row + 1);
        strcat(html, cell);
      } else {
        sprintf(cell, "<td id='cell-%d'></td>", row * colonne + col);
        strcat(html, cell);
      }
    }
    strcat(html, "</tr>");
  }

  // Script JavaScript
  strcat(html, "<script>"
    // apertura socket BROWSER --> ESP01 
    "var webSocket = new WebSocket('ws://' + window.location.hostname + ':81/');"
    // inserimento pulsante e aggiunta dell'eventListener
    "function inserisciPulsante(idCella, testoPulsante) {"
      "var cella = document.getElementById(idCella);"
      "var pulsante = document.createElement(\"button\");"
      "pulsante.addEventListener('click', function() {"
        "var now = new Date();"
        "var dateTimeString = now.getFullYear() + '-' + "
          "('0' + (now.getMonth() + 1)).slice(-2) + '-' + "
          "('0' + now.getDate()).slice(-2) + ' ' + "
          "('0' + now.getHours()).slice(-2) + ':' + "
          "('0' + now.getMinutes()).slice(-2) + ':' + "
          "('0' + now.getSeconds()).slice(-2);"
        "webSocket.send(dateTimeString);"
        "pulsante.textContent = 'Data/Ora Aggiornata';"
      "});" // Chiusura della funzione e dell'addEventListener
      "pulsante.innerText = testoPulsante;"
      "cella.appendChild(pulsante);"
    "}"); // Chiusura della funzione inserisciPulsante


  // Aggiungi il pulsante specifico
  char script[100];
  sprintf(script, "inserisciPulsante('cell-%d', 'Clicca Qui');", colonne);
  strcat(html, script);

  // Fine dell'HTML
  strcat(html, 
    "webSocket.onmessage = function(event) {"
      "var widths = JSON.parse(event.data);" // Parse l'array JSON ricevuto.
      "widths.forEach(function(width, index) {"
        "var svgId = 'svg-' + index;" // Costruisce l'ID dell'SVG basato sull'indice.
        "var svg = document.getElementById(svgId);"
        "if (svg) {"
          "svg.innerHTML = '';" // Pulisce l'SVG.
          "var rect = document.createElementNS('http://www.w3.org/2000/svg', 'rect');"
          "rect.setAttribute('width', width);"// Imposta la larghezza in base al valore dell'array.
          "rect.setAttribute('height', '10');"
          "rect.setAttribute('fill', 'blue');"
          "svg.appendChild(rect);"
        "}"
      "});"
    "};"
  "</script>"
  "</body>"
  "</html>");

  return html;
}

void setup() {
  Serial.begin(115200);
  while (!Serial){;}
  WiFi.softAP(ssid, password);
  Serial.println("Access Point Started");

  // Configurazione server
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", getHTMLmain());
  });

  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  webSocket.loop();
  server.handleClient();

  unsigned long currentTime = millis();
  if (currentTime - lastTime > 100) { // 1000 ms = 1 secondo
    lastTime = currentTime;
    sendRandomLength();
  }  
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    // Copia il payload in un array di char terminato da null
    char dateTime[length + 1];
    strncpy(dateTime, (char *)payload, length);
    dateTime[length] = '\0'; // Assicurati che la stringa sia terminata correttamente

    // Estrai ogni componente della data e ora dalla stringa
    int year, month, day, hour, minute, second;
    sscanf(dateTime, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);

    // Ajust for the year offset in tmElements_t
    tmElements_t tm;
    tm.Year = year - 1970;
    tm.Month = month;
    tm.Day = day;
    tm.Hour = hour;
    tm.Minute = minute;
    tm.Second = second;

    time_t t = makeTime(tm); // Converti in time_t
    setTime(t); // Imposta l'ora del sistema

    Serial.print("Data/Ora impostata: ");
    Serial.print(day);
    Serial.print("/");
    Serial.print(month);
    Serial.print("/");
    Serial.print(year);
    Serial.print(" ");
    Serial.print(hour);
    Serial.print(":");
    Serial.print(minute);
    Serial.print(":");
    Serial.println(second);


  }
}


