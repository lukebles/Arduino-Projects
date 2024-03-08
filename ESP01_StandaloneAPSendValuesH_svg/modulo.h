#ifndef MODULO_H
#define MODULO_H

#include "incomune.h"
#include "pagineweb.h"
#include "taborario.h"
#include "tabgiorni.h"

// Configurazioni WiFi
const char* ssid = "sid";
const char* password = "pw12345678";

int ptIdxArray = TAB_ROWS-1;  // Numero corrente di valori nell'array

uint8_t ID;
uint16_t CONTATOREa, CONTATOREb;

unsigned long lastTime = 0;

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
Tabella myk[TAB_ROWS];


bool primoAvvio = true;

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
    fillHorsColumns(myk[ptIdxArray].deltaContatoreA); // riempie le colonne data ora
    filldaysColumns(myk[ptIdxArray].deltaContatoreA);
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

  server.on("/setTime", HTTP_GET, []() {
    server.send(200, "text/html", getHTMLsetTime());
  });

  server.on("/getEnergyHours", HTTP_GET, []() {
    char htmlTable[MAX_HTML_SIZE];
    generateHTMLTable(htmlTable);
    server.send(200, "text/html", htmlTable);
  });

  server.on("/getEnergyDays", HTTP_GET, []() {
    char htmlTable[MAX_HTML_SIZE];
    generateHTMLTableDays(htmlTable);
    server.send(200, "text/html", htmlTable);
  });

  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

#endif  // MODULO_H
