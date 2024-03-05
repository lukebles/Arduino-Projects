#ifndef MODULO_H
#define MODULO_H

const char* javascript = R"====(
  // Apertura socket BROWSER --> ESP01 
  var webSocket = new WebSocket('ws://' + window.location.hostname + ':81/');
  // Funzione per inserire un pulsante e aggiungere l'eventListener
  function inserisciPulsante(idCella, testoPulsante) {
    var cella = document.getElementById(idCella);
    var pulsante = document.createElement('button');    
    pulsante.addEventListener('click', function() {
      var now = new Date();
      var dateTimeString = now.getFullYear() + '-' + 
        ('0' + (now.getMonth() + 1)).slice(-2) + '-' + 
        ('0' + now.getDate()).slice(-2) + ' ' + 
        ('0' + now.getHours()).slice(-2) + ':' + 
        ('0' + now.getMinutes()).slice(-2) + ':' + 
        ('0' + now.getSeconds()).slice(-2);      
      webSocket.send(dateTimeString); // <<------- PULSANTE BROWSER -> ESP-01
      pulsante.textContent = 'Data/Ora Aggiornata';
    });
    pulsante.innerText = testoPulsante;
    cella.appendChild(pulsante);
  }
  // Inserimento pulsante
  inserisciPulsante('cell-0-1', 'Clicca Qui');
  // ==============================================
  // Gestione del messaggio ricevuto via WebSocket
  // ==============================================
  webSocket.onmessage = function(event) {
    var data = parseData(event.data);
    updateTable(data);
    insertSvgRectangles(data);
  };
  // Analizzare i dati ricevuti
  function parseData(dataString) {
    var rows = dataString.split(';').filter(function(row) {
        return row.length > 0;
    });
    return rows.map(function(row) {
      var values = row.split(',');
      return {
          //chiave: valore 
          deltaContatoreA: values[0],
          anno: values[1],
          mese: values[2],
          giorno: values[3],
          ora: values[4],
          minuto: values[5],
          secondo: values[6],
          ms_deltaT: values[7],
          potenza: values[8],
          messaggio: values[9]
      };
    }
    );
  }
  function updateTable(data) {
    var table = document.querySelector('table');
    data.forEach(function(row, rowIndex) {
      var tableRow = table.rows[rowIndex];
      var colIndex = 2;
      for (var key in row) {
          tableRow.cells[colIndex].innerText = row[key];
          colIndex++;
      }
    });
  }
  function insertSvgRectangles(data) {
    var maxPotenza = Math.max.apply(Math, data.map(function(row) {
      return row.potenza;
    })); // Trova il valore massimo di potenza senza arrow function
    var scaleFactor = 60 / maxPotenza; // Calcola il fattore di scala

    const table = document.querySelector('table');
    data.forEach(function(row, rowIndex) {
      // Assicurati che la riga della tabella esista
      var tableRow = table.rows[rowIndex];

      // Assumi che la seconda colonna debba contenere il rettangolo SVG
      var cell = tableRow.cells[1]; // Le celle sono indicizzate a partire da 0

      // Crea l'elemento SVG
      var svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
      svg.setAttribute('width', '61'); // Larghezza fissa dell'SVG
      svg.setAttribute('height', '20'); // Altezza fissa dell'SVG

      // Crea il rettangolo SVG
      var rect = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
      var rectWidth = row.potenza * scaleFactor + 3 ;
      rect.setAttribute('width', rectWidth); // Usa "potenza" per definire la larghezza
      rect.setAttribute('height', '20'); // Altezza fissa del rettangolo
      rect.setAttribute('fill', getColorBasedOnValue(row.potenza)); // Colore di riempimento del rettangolo

      // Aggiungi il rettangolo all'SVG, e l'SVG alla cella della tabella
      svg.appendChild(rect);
      cell.innerHTML = ''; // Pulisci il contenuto precedente della cella
      cell.appendChild(svg);
    });
  }
  function getColorBasedOnValue(value) {
    if (value < 0) return 'gray'; // Numeri negativi.
    else if (value < 250) return 'green';
    else if (value < 500) return 'yellow';
    else if (value < 1000) return 'orange';
    else if (value < 2000) return 'red';
    else if (value < 3000) return 'blue';
    else return 'purple'; // Valori >= 3000.
  }
)====";

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>
#include <TimeLib.h>

// Configurazioni WiFi
const char* ssid = "sid";
const char* password = "pw12345678";

#define TAB_ROWS 14
#define TAB_COLUMNS 15

const unsigned int HTML_SIZE = 12000;
long values[TAB_ROWS];  // Array per memorizzare i valori

int ptIdxArray = 0;  // Numero corrente di valori nell'array

uint8_t ID;
uint16_t CONTATOREa, CONTATOREb;

unsigned long lastTime = 0;

#define MAX_TEXT_LENGTH 40           // Imposta la dimensione del buffer secondo le necessità
char serialBuffer[MAX_TEXT_LENGTH];  // Buffer per i dati seriali
int serialBufferIndex = 0;           // Indice per il posizionamento nel buffer
bool newData = false;                // Flag per indicare la disponibilità di nuovi dati

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

unsigned long previous_time_ENEL = 0;

struct Tabella {
  uint16_t contatoreA;
  uint16_t contatoreR;
  uint16_t deltaContatoreA;
  uint16_t deltaContatoreR;
  uint16_t anno;
  uint8_t mese;
  uint8_t giorno;
  uint8_t ora;
  uint8_t minuto;
  uint8_t secondo;
  unsigned long ms_deltaT;
  int potenza;
  char messaggio[11];
};
Tabella myk[20];

bool primoAvvio = true;

// codice HTML per la pagina principale
char* getHTMLmain() {
  static char html[HTML_SIZE];
  memset(html, 0, HTML_SIZE);  // Pulisce l'array
  // ========================
  // Parte iniziale dell'HTML
  // ========================
  strcat(html,
         "<!DOCTYPE html>"
         "<html lang=\"it\">"
         "<head>"
         "<meta charset=\"UTF-8\">"
         "<title>Tabella con SVG e Pulsante</title>"
         "<style>"
         "table, th, td {border: 1px solid black;; border-collapse: collapse; padding: 0}"
         "th, td {padding: 0; margin: 0; text-align: right;}"
         ".svg-cell {padding: 0; margin: 0;}"
         "</style>"
         "</head>"
         "<body>"
         "<table>");

  // ========
  // tabella
  // ========
  char cell[200];  // Aumenta la dimensione se necessario
  for (int row = 0; row < TAB_ROWS; row++) {
    strcat(html, "<tr>");
    for (int col = 0; col < TAB_COLUMNS; col++) {
      if (col == 1) {
        sprintf(cell, "<td class='svg-cell'>"
                      "<svg id='svg-%d-%d' width='60' height='10'>"
                      "</svg>"
                      "</td>",
                col, row);
        strcat(html, cell);
      } else {
        sprintf(cell, "<td id='cell-%d-%d'></td>", col, row);
        strcat(html, cell);
      }
    }
    strcat(html, "</tr>");
  }
  // ==================
  // Script JavaScript
  // ==================
  strcat(html, "<script>");
  strcat(html, javascript);
  strcat(html, "</script>"
               "</body>"
               "</html>");
  return html;
}

// "function updateTable(data) {"
//     "var table = document.querySelector('table');"
//     "var maxWidth = 60;" // Larghezza massima in pixel per il rettangolo SVG.
//     // Calcola il valore massimo di "potenza" tra tutte le righe per la scalatura.
//     "var maxValue = Math.max.apply(Math, data.map(function(row) {"
//         "return Math.abs(row.potenza);"
//     "}));"
//     "data.forEach(function(row, rowIndex) {"
//         "var tableRow = table.rows[rowIndex] || table.insertRow(rowIndex);"
//         "var cell = tableRow.cells[2];" // Colonna per il rettangolo SVG.
//         "var svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');"
//         "svg.setAttribute('width', maxWidth);"
//         "svg.setAttribute('height', '10');"
//         "cell.innerHTML = '';" // Pulisci la cella.
//         "cell.appendChild(svg);"
//         "if (isFinite(row.potenza)) {"
//             "var absPotenza = Math.abs(row.potenza);"
//             "var scaledWidth = (absPotenza / maxValue) * maxWidth;"
//             "var rect = document.createElementNS('http://www.w3.org/2000/svg', 'rect');"
//             "rect.setAttribute('width', scaledWidth);"
//             "rect.setAttribute('height', '10');"
//             "rect.setAttribute('fill', getColorBasedOnValue(row.potenza));"
//             "svg.appendChild(rect);"
//         "}"
//         // Aggiungi il testo con il valore di "potenza" nella cella successiva o nella stessa cella a seconda della tua preferenza.
//         "cell.textContent = row.potenza;"
//     "});"
// "}"
// "function getColorBasedOnValue(value) {"
//     "if (value < 0) return 'gray';"
//     "else if (value < 250) return 'green';"
//     "else if (value < 500) return 'yellow';"
//     "else if (value < 1000) return 'orange';"
//     "else if (value < 2000) return 'red';"
//     "else if (value < 3000) return 'blue';"
//     "else return 'purple';"
// "}"

// Funzione per invertire l'ordine dei byte di un uint16_t
uint16_t swapBytes(uint16_t value) {
  return (value >> 8) | (value << 8);
}


int calcolaPotenza(unsigned long diffEnergia, unsigned long diffmillis) {
  float delta_tempo_sec = float(diffmillis) / 1000.0;
  float delta_energia_wh = float(diffEnergia);
  float potenza = 3600.0 / float(delta_tempo_sec) * float(delta_energia_wh);
  // approssima al centinaio
  int potenza_approx = static_cast<int>((potenza + 50) / 100) * 100;
  return potenza_approx;
}

void inserisci(unsigned long adesso, uint16_t CONTATOREa, uint16_t CONTATOREb) {
  // riempimento valori tabella
  myk[ptIdxArray].contatoreA = CONTATOREa;
  myk[ptIdxArray].contatoreR = CONTATOREb;
  // myk[ptIdxArray].deltaContatoreA -- VIENE FATTA SOTTO
  // myk[ptIdxArray].deltaContatoreR -- VIENE FATTA SOTTO
  myk[ptIdxArray].anno = year();
  myk[ptIdxArray].mese = month();
  myk[ptIdxArray].giorno = day();
  myk[ptIdxArray].ora = hour();
  myk[ptIdxArray].minuto = minute();
  myk[ptIdxArray].secondo = second();
  // myk[ptIdxArray].ms_deltaT -- VIENE FATTA SOTTO
  //
  // copio (troncando se necessario) il messaggio di testo
  strncpy(myk[ptIdxArray].messaggio, serialBuffer, sizeof(myk[ptIdxArray].messaggio) - 1);
  myk[ptIdxArray].messaggio[sizeof(myk[ptIdxArray].messaggio) - 1] = '\0';
  //
  // myk[ptIdxArray].potenza -- VIENE FATTA SOTTO
  //
  if (primoAvvio) {
    primoAvvio = false;
    //
    myk[ptIdxArray].ms_deltaT = 0;
    myk[ptIdxArray].deltaContatoreA = 0;
    myk[ptIdxArray].deltaContatoreR = 0;
    myk[ptIdxArray].potenza = 0;

  } else {
    myk[ptIdxArray].ms_deltaT = adesso - previous_time_ENEL;
    myk[ptIdxArray].deltaContatoreA = myk[ptIdxArray].contatoreA - myk[ptIdxArray - 1].contatoreA;
    myk[ptIdxArray].deltaContatoreR = myk[ptIdxArray].contatoreR - myk[ptIdxArray - 1].contatoreR;
    //
    myk[ptIdxArray].potenza = calcolaPotenza(myk[ptIdxArray].deltaContatoreA, myk[ptIdxArray].ms_deltaT);
    //
  }
}

void sendArrayJs() {
  char buffer[20000];  // Aumenta la dimensione se necessario, a seconda della dimensione totale dei dati.
  strcpy(buffer, "");  // Inizializza il buffer con una stringa vuota.

  for (int i = 0; i < TAB_ROWS; i++) {
    char temp[1000];  // Buffer temporaneo per un singolo elemento della struttura.
    // Serializza tutti i campi di un elemento della struttura in una stringa.
    sprintf(temp, "%u,%u,%u,%u,%u,%u,%u,%lu,%d,%s;",
            myk[i].deltaContatoreA,
            myk[i].anno, myk[i].mese, myk[i].giorno, myk[i].ora, myk[i].minuto, myk[i].secondo,
            myk[i].ms_deltaT, myk[i].potenza, myk[i].messaggio);
    strcat(buffer, temp);  // Aggiungi la stringa serializzata al buffer principale.
  }
  // Invia array tramite webSocket
  webSocket.broadcastTXT(buffer);
}


///////////////////////////////////////////////////
// aggiunta dati sotto forma di coda a scorrimento
///////////////////////////////////////////////////
void addValue(char* serialBuffer) {
  unsigned long adesso = millis();
  // estrae i valori ENEL
  int result = sscanf(serialBuffer, "%2hhx%4hx%4hx", &ID, &CONTATOREa, &CONTATOREb);
  // Inversione dell'ordine dei byte
  CONTATOREa = swapBytes(CONTATOREa);
  CONTATOREb = swapBytes(CONTATOREb);

  if (ptIdxArray < TAB_ROWS - 1) {
    inserisci(adesso, CONTATOREa, CONTATOREb);
    ptIdxArray++;
  } else {
    // attivazione scorrimento
    for (int i = 1; i < TAB_ROWS; i++) {
      myk[i - 1] = myk[i];
    }
    //
    inserisci(adesso, CONTATOREa, CONTATOREb);
    ptIdxArray = TAB_ROWS - 1;
  }
  previous_time_ENEL = adesso;
  //
  sendArrayJs();
}

// =================================
// eventi provenienti dal browser
// =================================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    // copio il payload in un array di char terminato da null
    char dateTime[length + 1];
    strncpy(dateTime, (char*)payload, length);
    dateTime[length] = '\0';  //la stringa deve essere terminata correttamente

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

    time_t t = makeTime(tm);  // Converte in time_t
    setTime(t);               // Imposta l'ora del sistema
  }
  if(type == WStype_CONNECTED) {
    // Quando un client si connette, invia l'array.
    sendArrayJs();
  }
}

void setupWiFi() {
  Serial.begin(115200);
  while (!Serial) { ; }
  WiFi.softAP(ssid, password);
  Serial.println();
  Serial.println("Access Point Started");

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", getHTMLmain());
  });

  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

#endif  // MODULO_H
