// Programma per ESP01 "A" che agisce come AP e WebSocket Server
#include <ESP8266WiFi.h>
#include <WebSocketsServer_Generic.h>

const char* ssid = "hter";
const char* password = "12345678";

#define T_SALE 2
#define T_SCENDE 0
#define T_COSTANTE 1

WebSocketsServer webSocket = WebSocketsServer(81);

// Struttura per ricevere i dati
struct packet_caldaia {
    byte sender; // 1 byte
    uint16_t sanitariaCount; // 2 bytes
    uint16_t termoCount; // 2 bytes
    byte statoSanitaria; // 0=discesa 1=costante 2=salita
    byte statoTermo; // 0=discesa 1=costante 2=salita
    int16_t ambienteC;
    int16_t sanitCaldaC;
    int16_t termoCaldaC;
} receivedPacket;

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_BINARY && length == sizeof(packet_caldaia)) {
    memcpy(&receivedPacket, payload, sizeof(packet_caldaia));
    Serial.print("Sender: ");
    Serial.println(receivedPacket.sender);
    Serial.print("Sanitaria Count: ");
    Serial.println(receivedPacket.sanitariaCount);
    Serial.print("Termo Count: ");
    Serial.println(receivedPacket.termoCount);
    Serial.print("Stato Sanitaria: ");
    Serial.println(getStatus(receivedPacket.statoSanitaria));
    Serial.print("Stato Termo: ");
    Serial.println(getStatus(receivedPacket.statoTermo));
    Serial.print("Temperatura Ambiente: ");
    Serial.println(receivedPacket.ambienteC/100.0);
    Serial.print("Temperatura Sanitaria Caldaia: ");
    Serial.println(receivedPacket.sanitCaldaC/100.0);
    Serial.print("Temperatura Termo Caldaia: ");
    Serial.println(receivedPacket.termoCaldaC/100.0);
  }
}

int findBestWiFiChannel() {
  int bestChannel = 1;
  int bestScore = 255;

  for (int channel = 1; channel <= 13; channel++) {
    WiFi.disconnect();
    delay(100);
    WiFi.begin(ssid, password, channel);
    delay(500);

    int n = WiFi.scanNetworks();
    int score = 0;
    for (int i = 0; i < n; ++i) {
      if (WiFi.channel(i) == channel) {
        score++;
      }
    }
    if (score < bestScore) {
      bestScore = score;
      bestChannel = channel;
    }
  }

  return bestChannel;
}

void setup() {
  Serial.begin(115200);

  int bestChannel = findBestWiFiChannel();
  WiFi.softAP(ssid, password, bestChannel);
  Serial.println("Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
}

void loop() {
  webSocket.loop();
}


void getStatus(byte stato){
  if (stato == T_COSTANTE){
    Serial.println("=");
  } else if (stato == T_SCENDE){
    Serial.println("<");
  } else {
    Serial.println(">");
  } 
}