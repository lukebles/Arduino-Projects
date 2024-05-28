#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer_Generic.h>
#include "LittleFS.h"
#include <time.h>

const char* ssid = "sid2";           // Sostituisci con il nome della tua rete
const char* password = "pw12345678";  // Sostituisci con la tua password

AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

unsigned long lastTime = 0;
unsigned long timerDelay = 2000; // 2 secondi

struct DataPoint {
  int value1;
  int value2;
  String timestamp;
};

DataPoint dataPoints[30];
int dataIndex = 0;


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
    for (int i = 0; i < 30; i++) {
      int index = (dataIndex + i) % 30;
      json += "{\"value1\":" + String(dataPoints[index].value1) + ",\"value2\":" + String(dataPoints[index].value2) + ",\"timestamp\":\"" + dataPoints[index].timestamp + "\"}";
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
    dataIndex = (dataIndex + 1) % 30;

    // Crea una stringa con i valori separati da una virgola
    String message = String(value1) + "," + String(value2);

    // Invia il messaggio a tutti i client WebSocket connessi
    webSocket.broadcastTXT(message);
  }
}


String getTimeStamp() {
  // Funzione che restituisce l'ora attuale come stringa
  time_t now = time(nullptr);
  struct tm* p_tm = localtime(&now);
  char buffer[16];
  sprintf(buffer, "%02d:%02d:%02d", p_tm->tm_hour, p_tm->tm_min, p_tm->tm_sec);
  return String(buffer);
}


void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    Serial.println((char*)payload);
  }
}
