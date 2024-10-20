// Programma per ESP01 "B" che si connette ad "A" e tenta la riconnessione periodicamente
#include <ESP8266WiFi.h>
#include <WebSocketsClient_Generic.h>

const char* ssid = "hter";
const char* password = "12345678";

#include <LkMultivibrator.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Imposta il pin al quale è collegato il bus OneWire
#define ONE_WIRE_BUS 2

// Inizializza il bus OneWire e la libreria DallasTemperature
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

const int LED_PIN = 13;
const int TRANSMIT_PIN = 12;

LkMultivibrator timerRadioTx(30000,ASTABLE);

#define SANITARIA_calda 0
#define SANITARIA_fredda 4
#define TERMO_calda 2
#define TERMO_fredda 3
#define AMBIENTE 1

#define T_SALE 2
#define T_SCENDE 0
#define T_COSTANTE 1

const int maxSensors = 5;
DeviceAddress sensoreID[maxSensors];

float previousTemp[5];
byte stato[5];

WebSocketsClient webSocket;

// Struttura per inviare i dati
struct packet_caldaia {
    byte sender; // 1 byte
    uint16_t sanitariaCount; // 2 bytes
    uint16_t termoCount; // 2 bytes
    byte statoSanitaria; // 0=discesa 1=costante 2=salita
    byte statoTermo; // 0=discesa 1=costante 2=salita
    int16_t ambienteC;
    int16_t sanitCaldaC;
    int16_t termoCaldaC;
} sendPacket;

unsigned long lastReconnectAttempt = 0;

float totaleDifferenzeSanitaria = 0;
float totaleDifferenzeTermo = 0;

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("Disconnected from WebSocket");
      break;
    case WStype_CONNECTED:
      Serial.println("Connected to WebSocket");
      break;
    case WStype_TEXT:
      Serial.printf("Message received: %s\n", payload);
      break;
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");

  webSocket.begin("192.168.4.1", 81, "/");
  webSocket.onEvent(webSocketEvent);

  sensors.requestTemperatures();
  previousTemp[SANITARIA_calda] = sensors.getTempCByIndex(SANITARIA_calda);
  previousTemp[TERMO_calda] = sensors.getTempCByIndex(TERMO_calda);
  previousTemp[AMBIENTE] = sensors.getTempCByIndex(AMBIENTE);
  //
  sendPacket.sender = 99;
  sendPacket.sanitariaCount = 0;
  sendPacket.termoCount = 0;
  sendPacket.statoSanitaria = T_COSTANTE;
  sendPacket.statoTermo = T_COSTANTE;
  sendPacket.ambienteC = (int16_t)(sensors.getTempCByIndex(AMBIENTE)*100);
  sendPacket.sanitCaldaC = (int16_t)(sensors.getTempCByIndex(SANITARIA_calda)*100);
  sendPacket.termoCaldaC = (int16_t)(sensors.getTempCByIndex(TERMO_calda)*100);
}

void loop() {
  webSocket.loop();


    // ==================================
  // ogni 30 secondi invia il resoconto
  if (timerRadioTx.expired()){
    sensors.requestTemperatures();
        // ==============================
    // temperature in salita/discesa?
    getStatus(SANITARIA_calda);
    getStatus(TERMO_calda);
    getStatus(AMBIENTE);
        // =====================================
    // cacolo differenze tra CALDA e FREDDA
    float diffSanitaria  = sensors.getTempCByIndex(SANITARIA_calda) - sensors.getTempCByIndex(SANITARIA_fredda); 
    float diffTermo = sensors.getTempCByIndex(TERMO_calda) - sensors.getTempCByIndex(TERMO_fredda);
    //
    totaleDifferenzeSanitaria += diffSanitaria;
    totaleDifferenzeTermo += diffTermo;
    // impedisce che venga superato il valore float 65535.0
    totaleDifferenzeSanitaria = chkFloat(totaleDifferenzeSanitaria);
    totaleDifferenzeTermo = chkFloat(totaleDifferenzeTermo);
        // ===========================
    // riempimento deti da inviare
    dataToSend.sanitariaCount = (int16_t)round(totaleDifferenzeSanitaria);
    dataToSend.termoCount = = (int16_t)round(totaleDifferenzeTermo);
    dataToSend.statoSanitaria = stato[SANITARIA_calda];
    dataToSend.statoTermo = stato[TERMO_calda];
    dataToSend.ambienteC = (int16_t)(sensors.getTempCByIndex(AMBIENTE)*100);
    dataToSend.sanitCaldaC = (int16_t)(sensors.getTempCByIndex(SANITARIA_calda)*100);
    dataToSend.termoCaldaC = (int16_t)(sensors.getTempCByIndex(TERMO_calda)*100);
    // ==============
    // invio dei dati
    webSocket.sendBIN((uint8_t*)&sendPacket, sizeof(sendPacket));
    Serial.println("Dati inviati: Pacchetto struttura");
    lastSendTime = millis();
  }


  // Tentativo di riconnessione periodica
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) { // Tentativo ogni 5 secondi
      lastReconnectAttempt = now;
      Serial.println("Tentativo di riconnessione al WiFi...");
      WiFi.begin(ssid, password);
    }
  }

  // Se il WiFi è connesso e WebSocket non è connesso, tenta la riconnessione
  if (WiFi.status() == WL_CONNECTED && !webSocket.isConnected()) {
    Serial.println("Tentativo di riconnessione al WebSocket...");
    webSocket.begin();
  }

}
