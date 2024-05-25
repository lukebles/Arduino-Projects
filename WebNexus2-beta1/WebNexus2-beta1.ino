#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer_Generic.h>
#include "LittleFS.h"
#include <TimeLib.h>

const char* ssid = "sid2";           // Sostituisci con il nome della tua rete
const char* password = "pw12345678";  // Sostituisci con la tua password

AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

unsigned long lastTime = 0;
unsigned long timerDelay = 2000; // 2 secondi
unsigned long hourStartTime = 0;

struct DataPoint {
  int value1;
  int value2;
  String timestamp;
};

#define NRBARRE 30

DataPoint dataPoints[NRBARRE];
DataPoint hourlyAverages[NRBARRE];
int dataIndex = 0;
int hourIndex = 0;

int hourlyValue1Sum = 0;
int hourlyValue2Sum = 0;
int hourlyCount = 0;

//////////////

bool newData = false;
int serialBufferIndex = 0;
#define MAX_TEXT_LENGTH 40           // Imposta la dimensione del buffer secondo le necessità
char serialBuffer[MAX_TEXT_LENGTH];  // Buffer per i dati seriali
////
#define MAXINDEX NRBARRE - 1
struct TabellaUltimiDatiSeriali {
  uint16_t anno;
  uint8_t mese;
  uint8_t giorno;
  uint8_t ora;
  uint8_t minuto;
  uint8_t secondo;
  uint16_t deltaContatoreA;
  uint16_t deltaContatoreR; 
  unsigned long ms_deltaT; // millisecondi trascorsi dalla precedente ricezione
};
TabellaUltimiDatiSeriali serData[NRBARRE];
unsigned long previous_time_ENEL = 0;
//
uint16_t prevCountA, prevCountR;
bool primoAvvio=true;

////////////////////////////////
// ricezione messaggi su seriale
////////////////////////////////
// vengono inviati da "MultiCatch" solo i valori inerenti 
// ai conteggi energia elettrica
void checkSerialData() {
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
  // se l'intero messaggio è stato ricevuto
  // lo elabora e inserisce i dati nella coda a scorrimento
  if (newData) {
    unsigned long adesso = millis();
    uint8_t vID; // non usato
    uint16_t vCountA, vCountR;
    //
    int result = sscanf(serialBuffer, "%2hhx%4hx%4hx", &vID, &vCountA, &vCountR);
    vCountA = (vCountA >> 8) | (vCountA << 8); // scambia i due bytes che compongono unit16_t
    vCountR = (vCountR >> 8) | (vCountR << 8);
    /////////////////////
    // scorrimento coda
    /////////////////////
    for (int i = 1; i < NRBARRE; i++) {
      serData[i - 1] = serData[i];
    }
    // =========== inserimento ===========
    bool valido = false;
    //
    if (primoAvvio) {
      primoAvvio = false;
      prevCountA = vCountA;
      prevCountR = vCountR;
      previous_time_ENEL = adesso;
    } else {
      // calcolo dei tre "delta": valido anche per passaggio
      // per lo zero
      unsigned long deltaTms = adesso - previous_time_ENEL;
      uint16_t deltaCountA = vCountA - prevCountA;
      uint16_t deltaCountR = vCountR - prevCountR;
      // assegnazione dei valori "precedenti" con quelli attuali
      prevCountA = vCountA;
      prevCountR = vCountR;
      previous_time_ENEL = adesso;
      // controllo che siano dati sensati
      // la precedente ricezione radio deve risalire a meno di un'ora fa 
      if (deltaTms < 3600000) {
        valido = true;
      }
      // la differenza di conteggi deve essere inferiore a 3600
      // considera che normalmente deltacount è dell'ordine dei 0-12 conteggi
      // in un lasso di tempo di circa 9 secondi
      if (deltaCountA < 3600) {
        valido = true;
      }
      if (deltaCountR < 3600) {
        valido = true;
      }
      // se tutto è valido memorizzo
      if (valido){
        time_t t = now();
        tmElements_t tm; // struttura
        breakTime(t, tm);
        serData[MAXINDEX].anno = tmYearToCalendar(tm.Year); // Converti l'anno da 1970 epoch
        serData[MAXINDEX].mese = tm.Month;
        serData[MAXINDEX].giorno = tm.Day;
        serData[MAXINDEX].ora = tm.Hour;
        serData[MAXINDEX].minuto = tm.Minute;
        serData[MAXINDEX].secondo = tm.Second;
        serData[MAXINDEX].deltaContatoreA = deltaCountA;
        serData[MAXINDEX].deltaContatoreR = deltaCountR;
        serData[MAXINDEX].ms_deltaT = deltaTms;        
      }
    }
    //////////////////////////////////////////
    // Resetta il buffer e preparati per la prossima lettura
    ///////////////////////////////////////////
    memset(serialBuffer, 0, MAX_TEXT_LENGTH); 
    newData = false; 
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // aspetta la connessione
  }

  if(!LittleFS.begin()){
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
  
  WiFi.softAP(ssid, password);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/main.html", "text/html");
  });

  server.on("/Chart.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/Chart.min.js", "application/javascript");
  });

  // Endpoint per ottenere i dati memorizzati
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "[";
    for (int i = 0; i < NRBARRE; i++) {
      int index = (dataIndex + i) % NRBARRE;
      json += "{\"value1\":" + String(dataPoints[index].value1) + ",\"value2\":" + String(dataPoints[index].value2) + ",\"timestamp\":\"" + dataPoints[index].timestamp + "\"}";
      if (i < 29) {
        json += ",";
      }
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  // Endpoint per ottenere le medie orarie memorizzate
  server.on("/hourlyAverages", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "[";
    for (int i = 0; i < NRBARRE; i++) {
      int index = (hourIndex + i) % NRBARRE;
      json += "{\"value1\":" + String(hourlyAverages[index].value1) + ",\"value2\":" + String(hourlyAverages[index].value2) + ",\"timestamp\":\"" + hourlyAverages[index].timestamp + "\"}";
      if (i < 29) {
        json += ",";
      }
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  server.begin();
  Serial.println();
  Serial.println("HTTP server started");

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  checkSerialData();
  //
  webSocket.loop();
  // Invia una coppia di valori casuali ogni due secondi
  unsigned long currentTime = millis();
  if (currentTime - lastTime >= timerDelay) {
    lastTime = currentTime;

    // Genera valori casuali
    int value1 = random(0, 100);
    int value2 = random(0, 100);
    String timestamp = getTimeStamp();

    // Aggiorna l'array dei punti dati
    dataPoints[dataIndex] = {value1, value2, timestamp};
    dataIndex = (dataIndex + 1) % NRBARRE;

    // Aggiorna i valori per la media oraria
    hourlyValue1Sum += value1;
    hourlyValue2Sum += value2;
    hourlyCount++;

    // Controlla se è trascorsa un'ora
    if (hourHasPassed()) {
      // Calcola le medie orarie
      int avgValue1 = hourlyValue1Sum / hourlyCount;
      int avgValue2 = hourlyValue2Sum / hourlyCount;
      String hourTimestamp = getHourTimeStamp();

      // Memorizza le medie orarie
      hourlyAverages[hourIndex] = {avgValue1, avgValue2, hourTimestamp};
      hourIndex = (hourIndex + 1) % NRBARRE;

      // Resetta i contatori orari
      hourlyValue1Sum = 0;
      hourlyValue2Sum = 0;
      hourlyCount = 0;

      // Invia le medie orarie ai client WebSocket
      String hourlyMessage = "{\"timestamp\":\"" + hourTimestamp + "\",\"value1\":" + String(avgValue1) + ",\"value2\":" + String(avgValue2) + "}";
      webSocket.broadcastTXT(hourlyMessage);
    }

    // Crea una stringa con i valori separati da una virgola
    String message = String(value1) + "," + String(value2);

    // Invia il messaggio a tutti i client WebSocket connessi
    webSocket.broadcastTXT(message);
  }
}

bool hourHasPassed() {
  time_t now = time(nullptr);
  struct tm* p_tm = localtime(&now);
  return (p_tm->tm_min == 0 && p_tm->tm_sec < 2); // Controlla se è l'inizio di una nuova ora
}

String getTimeStamp() {
  // Funzione che restituisce l'ora attuale come stringa
  time_t now = time(nullptr);
  struct tm* p_tm = localtime(&now);
  char buffer[16];
  sprintf(buffer, "%02d:%02d:%02d", p_tm->tm_hour, p_tm->tm_min, p_tm->tm_sec);
  return String(buffer);
}

String getHourTimeStamp() {
  // Funzione che restituisce l'ora attuale arrotondata all'ora più vicina come stringa
  time_t now = time(nullptr);
  struct tm* p_tm = localtime(&now);
  char buffer[6];
  sprintf(buffer, "%02d:00", p_tm->tm_hour);
  return String(buffer);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    Serial.println((char*)payload);
  }
}
