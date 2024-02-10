#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>
#include <LkArraylize.h>
#include "LkHexBytes.h" 
#include <TimeLib.h>

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
#define NUM_STRINGS 20 // dimensione dell'array
#define STRING_LENGTH 40 // lunghezza massima della stringa
#define GRAFICO_LENGTH 24 // lunghezza massima della stringa

char myStrings[NUM_STRINGS][STRING_LENGTH];
unsigned long myStringDiffTimes[NUM_STRINGS];

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

bool orarioAggiornato = false;

uint8_t arrayEnergiaMinuti[GRAFICO_LENGTH];

#include <cstring> // Per usare strcpy e strncpy

const int MAX_STRINGHE = NUM_STRINGS; // Numero massimo di stringhe
const int MAX_LUNGHEZZA = STRING_LENGTH; // Lunghezza massima di ogni stringa

char arrayStringhe[MAX_STRINGHE][MAX_LUNGHEZZA] = {0}; // Inizializza l'array di stringhe
int indiceCorrente = 0; // Indice per tracciare l'ultima posizione usata nell'array

void aggiungiStringa(const String& nuovaStringa) {
    if (indiceCorrente < MAX_STRINGHE - 1) {
        // Se non abbiamo ancora raggiunto l'ultimo indice, procediamo normalmente.
        strncpy(arrayStringhe[indiceCorrente], nuovaStringa.c_str(), MAX_LUNGHEZZA - 1);
        arrayStringhe[indiceCorrente][MAX_LUNGHEZZA - 1] = '\0'; // Assicura terminazione della stringa
        indiceCorrente++;
    } else {
        // Se abbiamo raggiunto l'ultimo indice, dobbiamo scorrere tutto l'array verso sinistra
        for (int i = 1; i < MAX_STRINGHE; i++) {
            // Copia la stringa dalla posizione i alla posizione i-1
            strncpy(arrayStringhe[i - 1], arrayStringhe[i], MAX_LUNGHEZZA);
        }
        // Aggiungi la nuova stringa all'ultima posizione dell'array
        strncpy(arrayStringhe[MAX_STRINGHE - 1], nuovaStringa.c_str(), MAX_LUNGHEZZA - 1);
        arrayStringhe[MAX_STRINGHE - 1][MAX_LUNGHEZZA - 1] = '\0'; // Assicura terminazione della stringa
        // Nota: l'indiceCorrente resta invariato perché l'array è pieno e lavoriamo sempre sull'ultima posizione.
    }
}


int approssimaAlCentinaio(float valore) {
    return static_cast<int>((valore + 50) / 100) * 100;
}

// dal contenuto dei due array di C++
// genera il testo da inviare alla funzione Javascript
// [ valori primo array ENERGIA ],[ valori secondo array DATA ORA POTENZA]
String generaParametriJavascript(){
  String jsArray1 = "[";
  for(int i = 0; i < GRAFICO_LENGTH; ++i) {
      jsArray1 += "'" + String(arrayEnergiaMinuti[i]) + "'";
      if(i < 29) jsArray1 += ",";
  }
  jsArray1 += "]";

  String jsArray2 = "[";
  for(int i = 0; i < NUM_STRINGS; ++i) {
      jsArray2 += "'" +String(arrayStringhe[i]) + "'";
      if(i < 19) jsArray2 += ",";
  }
  jsArray2 += "]";

  return jsArray1 + "," + jsArray2;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // aspetta la connessione della porta seriale
  }

  WiFi.softAP(ssid, password);
  
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", getHTML());
  });

  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  tmElements_t tm;
  
}


void loop() {
  server.handleClient();
  webSocket.loop();

  if (Serial.available() > 0) {
    String strSeriale = Serial.readStringUntil('\n');
    // la lunghezza del dato sulla seriale deve essere adeguata a DataPacket
    if (strSeriale.length() == (sizeof(DataPacket)-1) * 2){ // -1 per il terminatore
      // 10 caratteri tipo "01FA4388FE" + 1 (terminatore \0)
      char chararray[sizeof(DataPacket) * 2 + 1];
      // 5 caratteri + 1 per il terminatore nullo
      byte bytearray[sizeof(DataPacket)];
      // converte la stringa in un array di caratteri char
      strncpy(chararray, strSeriale.c_str(), sizeof(chararray) - 1);
      // aggiunge il terminatore stringa null \0
      chararray[sizeof(chararray) - 1] = '\0'; 
      // converto l'array di caratteri in un array di bytes
      LkHexBytes::hexCharacterStringToBytes(bytearray, chararray);
      // dichiaro la classe 
      //LkArraylize<DataPacket> dearraylizator;
      // ricompongo i dati secondo la struttura
      //DataPacket packet_rx = dearraylizator.deArraylize(bytearray);
      uint16_t contatoreWHA = (bytearray[2] << 8) | bytearray[1];
      // se il mittente è 1 (ENEL)
      if (bytearray[0] == 1){
        // se è la prima ricezione
        if (previous_time_ENEL == 0){
          // riempie i valori precedenti con quelli attuali
          previous_time_ENEL = millis();
          previous_countActiveWh = contatoreWHA;
        } else {
          // .. altimenti esegue le differenze valori attuali - precedenti
          //
          // verificato: funziona anche la differenza 
          // durante il passaggio da 4294967295 a 0, 1, 2, ... eccetera
          unsigned long diffmillis = millis() - previous_time_ENEL;
          // verificato: funziona anche la differenza 
          // durante il passaggio da 65535 a 0, 1, 2, ... eccetera
          unsigned long diffEnergia = contatoreWHA - previous_countActiveWh;
          // calcoli della potenza dall'energia e dal tempo
          float delta_tempo_sec = float(diffmillis)/1000.0;
          float delta_energia_wh = float(diffEnergia);
          float potenzaAttiva = 3600.0 / float(delta_tempo_sec) * float(delta_energia_wh);
          // aggiornamento degli array
          int approssimata = approssimaAlCentinaio(potenzaAttiva);

          // aggiornamento dei dati 'precedenti'
          previous_time_ENEL = millis();
          previous_countActiveWh = contatoreWHA;

          // se l'orario è aggiornato
          // memorizza l'energia dell'ultima ora
          if (orarioAggiornato){

            // riempimento array 1

            arrayEnergiaMinuti[minute()/2] += diffEnergia;

            // rimpimento array 2
            String stringa = String(year()) + " " + String(month()) + " " + 
                String(day()) + " " + String(hour()) + " " + 
                String(minute()) + " " + String(second()) + " -> ";
            aggiungiStringa(stringa + String(approssimata)+ " (" + diffmillis +"ms)");

            // aggiornamento della pagina web
            // -------------------------NON VISUALIZZA SU WEB ----------------------------
            String script = "riempiConsoleDebug(" + generaParametriJavascript() + ")";
            webSocket.broadcastTXT(script);
            //Serial.println(script);

          }

        }
      }
    }
  }
}


void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    String dateTime = String((char *)payload).substring(0, length);
    
    // Estrai ogni componente della data e ora dalla stringa
    int year, month, day, hour, minute, second;
    year = dateTime.substring(0, 4).toInt();
    month = dateTime.substring(5, 7).toInt();
    day = dateTime.substring(8, 10).toInt();
    hour = dateTime.substring(11, 13).toInt();
    minute = dateTime.substring(14, 16).toInt();
    second = dateTime.substring(17, 19).toInt();

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
    // Serial.println("Data e ora aggiornate via WebSocket");
    orarioAggiornato = true;
  }  
}

String getHTML() {
  String html = "<!DOCTYPE html><html>";
  html += "<head>";
  html += getHead();
  html += "</head>";
  html += "<body>";
  html += "<svg id='graph' width='125' height='125' style='border:1px solid black'></svg>";
  html += "<div id='debugConsole'></div>";
  html += "<button id='updateTime'>Aggiorna Data/Ora</button>";
  html += "<script>";
  html += "var webSocket = new WebSocket('ws://' + window.location.hostname + ':81/');";
  html += "webSocket.onmessage = function(event) {";
  html += "  eval(event.data);";  // Esegue lo script ricevuto
  html += "};";

  html += R"====(
var updateTimeButton = document.getElementById('updateTime');
updateTimeButton.addEventListener('click', function() {
    var now = new Date();
    var dateTimeString = now.getFullYear() + '-' + 
                          ('0' + (now.getMonth() + 1)).slice(-2) + '-' + 
                          ('0' + now.getDate()).slice(-2) + ' ' + 
                          ('0' + now.getHours()).slice(-2) + ':' + 
                          ('0' + now.getMinutes()).slice(-2) + ':' + 
                          ('0' + now.getSeconds()).slice(-2);
    webSocket.send(dateTimeString);
    // Disabilita il pulsante una volta cliccato
    updateTimeButton.disabled = true;
    // Cambia il testo del pulsante per indicare che il comando è stato inviato
    updateTimeButton.textContent = 'Data/Ora Aggiornata';
});
  )====";

  html += getScript();
  html += "</script>";
  html += "</body>";
  html += "</html>";
  return html;
}

// initializeSliders --> updateSlider --> updateBackgroundColor
String getScript() {
  String html = R"(

function riempiConsoleDebug(array1, array2) {
    // Ottieni il div della console di debug tramite il suo id
    var consoleDebug = document.getElementById('debugConsole');

    // Pulisci il contenuto precedente della console di debug
    consoleDebug.innerHTML = '';

    // Itera su ogni elemento 
    array2.forEach(function(elemento) {
        // Crea un elemento p (paragrafo) per ogni elemento dell'array
        var paragrafo = document.createElement('p');
        // Assegna il testo del paragrafo all'elemento corrente dell'array
        paragrafo.textContent = elemento;
        // stile
        paragrafo.classList.add("paragrafoCompatto");
        // Aggiungi il paragrafo al div della console di debug
        consoleDebug.appendChild(paragrafo);
    });
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

.paragrafoCompatto {
    margin-top: 0px;
    margin-bottom: 0px;
}
</style>
  )";
  return html;
}