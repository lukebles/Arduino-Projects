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
const unsigned int HTML_SIZE = 10000;

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
    "table, th, td {border: 1px solid black; border-collapse: collapse;}"
    "th, td {padding: 10px; text-align: center;}"
    "svg {width: 125px; height: 125px;}"
    "</style>"
    "</head><body>"
    "<table>");

  // Crea le righe e le celle della tabella
  for (int row = 0; row < righe; row++) {
    strcat(html, "<tr>");
    for (int col = 0; col < colonne; col++) {
      char cell[50];
      sprintf(cell, "<td id='cell-%d'></td>", row * colonne + col);
      strcat(html, cell);
    }
    strcat(html, "</tr>");
  }

  // Script JavaScript
  strcat(html, "<script>"
    // apertura socket BROWSER --> ESP01 
    "var webSocket = new WebSocket('ws://' + window.location.hostname + ':81/');"
    //
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
  strcat(html, "</script>"
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


