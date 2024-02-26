#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>
#include <TimeLib.h>
#include <stdlib.h> // Include la libreria per accesso a atoi()

const char* ssid = "sid";
const char* password = "pw12345678";

#define TAB_ROWS  20
#define TAB_COLUMNS 15

long values[TAB_ROWS]; // Array per memorizzare i valori
int numValues = 0; // Numero attuale di valori nell'array

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// Definisci una dimensione massima per l'HTML
const unsigned int HTML_SIZE = 15000;
unsigned long lastTime = 0; 

#define MAX_TEXT_LENGTH 40 // Imposta la dimensione del buffer secondo le necessità
char serialBuffer[MAX_TEXT_LENGTH]; // Buffer per i dati seriali
int serialBufferIndex = 0; // Indice per il posizionamento nel buffer
bool newData = false; // Flag per indicare la disponibilità di nuovi dati

void sendArrayValues() {
  char message[300]; // modificare se necessario
  int len = 0; // Usata per tenere traccia della lunghezza attuale del messaggio.
  len += snprintf(message + len, sizeof(message) - len, "["); // Inizia l'array JSON.
  for(int i = 0; i < TAB_ROWS; i++) {
    if (i > 0) {
      len += snprintf(message + len, sizeof(message) - len, ","); // Aggiungi virgola.
    }
    int width = values[i];//random(5, 31); // Genera un numero casuale per la larghezza.
    len += snprintf(message + len, sizeof(message) - len, "%d", width); // Aggiungi il numero.
  }
  len += snprintf(message + len, sizeof(message) - len, "]"); // Chiude l'array JSON.
  //Serial.println(message);
  webSocket.broadcastTXT(message); // Invia il messaggio.
}

void addValue(long newValue) {
  if (numValues < TAB_ROWS) {
    // Se c'è spazio, aggiungi il valore all'ultimo indice disponibile
    values[numValues++] = newValue;
  } else {
    // Se l'array è pieno, sposta tutti gli elementi di una posizione verso l'inizio
    for(int i = 1; i < TAB_ROWS; i++) {
      values[i-1] = values[i];
    }
    // Aggiungi il nuovo valore all'ultimo indice
    values[TAB_ROWS - 1] = newValue;
  }
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
    "table, th, td {border: 0; border-collapse: collapse; padding: 0}"
    "th, td {padding: 0; margin: 0; text-align: right;}"
    ".svg-cell {padding: 0; margin: 0;}"
    "</style>"
    "</head><body>"
    "<table>");

  // Crea le righe e le celle della tabella
  char cell[200]; // Aumenta la dimensione se necessario
  for (int row = 0; row < TAB_ROWS; row++) {
    strcat(html, "<tr>");
    for (int col = 0; col < TAB_COLUMNS; col++) {
      if (col == 1) {        
        sprintf(cell, "<td class='svg-cell'>"
        "<svg id='svg-%d-%d' width='60' height='10'>"
        "</svg>"
        "</td>", col, row);
        strcat(html, cell);
      } else {
        sprintf(cell, "<td id='cell-%d-%d'></td>", col,row);
        strcat(html, cell);
      }
    }
    strcat(html, "</tr>");
  }

  //Serial.println(html);

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
  sprintf(script, "inserisciPulsante('cell-0-1', 'Clicca Qui');");
  strcat(html, script);

  // Fine dell'HTML
  strcat(html, 
    "webSocket.onmessage = function(event) {"
      "var widths = JSON.parse(event.data);" // Parse l'array JSON ricevuto.
      "var maxWidth = 60;" // Larghezza massima in pixel per il rettangolo SVG.
      "var maxValue = Math.max(...widths.map(Math.abs));" // Trova il valore massimo nell'array.       
      "widths.forEach(function(width, index) {"
        "var absWidth = Math.abs(width);"
        "var scaledWidth = (absWidth / maxValue) * maxWidth;" // Calcola la larghezza scalata.
        "var svgId = 'svg-1-' + index;" // Costruisce l'ID dell'SVG basato sull'indice.
        "var cellId = 'cell-2-' + index;"
        "var svg = document.getElementById(svgId);"
        "var cell = document.getElementById(cellId);"
        "if (svg) {"
          "svg.innerHTML = '';" // Pulisce l'SVG.
          "if (isFinite(width)){" 
            // solo se è un numero ne crea il grafico
            "var rect = document.createElementNS('http://www.w3.org/2000/svg', 'rect');"
            "rect.setAttribute('width', scaledWidth);"// Imposta la larghezza in base al valore dell'array.
            "rect.setAttribute('height', '10');"
            // Assegna il colore basato sul valore.
            "var fillColor = getColorBasedOnValue(width);"
            //
            "rect.setAttribute('fill', fillColor);"
            "svg.appendChild(rect);"
          "}"
          // aggiunta del testo
          "cell.textContent = width;"
        "}"
      "});"
    "};"
    "function getColorBasedOnValue(value) {"
      "if (value < 0) return 'gray';" // Numeri negativi.
      "else if (value < 250) return 'green';"
      "else if (value < 500) return 'yellow';"
      "else if (value < 1000) return 'orange';"
      "else if (value < 2000) return 'red';"
      "else if (value < 3000) return 'blue';"
      "else return 'purple';" // Valori >= 3000.
    "}"
  "</script>"
  "</body>"
  "</html>");

  return html;
}

void setup() {
  Serial.begin(115200);
  while (!Serial){;}

  serialBuffer[0] = '\0';

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

uint8_t ID;
uint16_t CONTATOREa, CONTATOREb;

// Funzione per invertire l'ordine dei byte di un uint16_t
uint16_t swapBytes(uint16_t value) {
    return (value >> 8) | (value << 8);
}

void loop() {
  webSocket.loop();
  server.handleClient();

  serialDataReady(); // Controlla e accumula i dati seriali
  if (newData) { // Se sono disponibili nuovi dati

    int result = sscanf(serialBuffer, "%2hhx%4hx%4hx", &ID, &CONTATOREa, &CONTATOREb);

    // Inversione dell'ordine dei byte per CONTATOREa e CONTATOREb
    CONTATOREa = swapBytes(CONTATOREa);

    addValue(CONTATOREa);

    sendArrayValues();

    //Serial.println(receivedNumber);
    memset(serialBuffer, 0, MAX_TEXT_LENGTH); // Resetta il buffer e preparati per la prossima lettura
    newData = false; // Resetta il flag di nuovi dati
  }
}

void serialDataReady() {
  while (Serial.available() > 0) {
    char receivedChar = Serial.read(); // Legge il carattere in arrivo
    if (receivedChar == '\n') { // Se il carattere è una nuova linea
      serialBuffer[serialBufferIndex] = '\0'; // Termina la stringa nel buffer
      serialBufferIndex = 0; // Resetta l'indice per la prossima lettura
      newData = true; // Imposta il flag di nuovi dati disponibili
    } else if (serialBufferIndex < MAX_TEXT_LENGTH - 1) { // Se c'è spazio nel buffer
      serialBuffer[serialBufferIndex++] = receivedChar; // Aggiungi al buffer
      serialBuffer[serialBufferIndex] = '\0'; // Assicura che il buffer sia sempre una stringa valida
    }
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


