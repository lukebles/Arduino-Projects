#include <ESP8266WiFi.h>
#include <WebSocketsServer_Generic.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <RH_ASK.h>

// Configurazione Access Point
const char* ssid = "rxd";
const char* password = "12345678";

// Configurazione WebSocket
WebSocketsServer webSocket = WebSocketsServer(81);

// Configurazione Web Server
ESP8266WebServer server(80);

// Configurazione RadioHead
RH_ASK driver(2000, 0); // 2000 bps, GPIO0

String messageBuffer = ""; // Buffer per accumulare i messaggi da inviare tramite WebSocket

void setup() {
  // Avvia LittleFS
  if (!LittleFS.begin()) {
    webSocketPrint("Failed to mount LittleFS\n");
    return;
  }

  // Configurazione come Access Point
  WiFi.softAP(ssid, password);

  IPAddress myIP = WiFi.softAPIP();
  webSocketPrint("AP IP address: " + myIP.toString() + "\n");
  
  // Inizializzazione WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  // Configurazione del server web
  server.on("/", handleRoot);
  server.begin();

  // Inizializzazione RadioHead
  if (!driver.init()) {
    webSocketPrint("Initialization of RadioHead failed\n");
    while (1);
  }
}

void loop() {
  // Controllo se è stato ricevuto un messaggio
  uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
  uint8_t buflen = sizeof(buf);

  if (driver.recv(buf, &buflen)) {
    // Converti il messaggio in esadecimale
    String hexMessage = "";
    for (int i = 0; i < buflen; i++) {
      hexMessage += String(buf[i], HEX);
    }
    
    // Invia il messaggio via WebSocket
    webSocketPrint("Received: " + hexMessage + "\n");
  }
  
  // Gestione WebSocket
  webSocket.loop();
  server.handleClient();  // Gestione delle richieste del server
}

// Funzione per reindirizzare le stampe seriali via WebSocket
void webSocketPrint(String message) {
  messageBuffer += message;
  if (webSocket.connectedClients() > 0) {
    webSocket.broadcastTXT(messageBuffer);
    messageBuffer = ""; // Svuota il buffer dopo aver inviato i messaggi
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  // Eventualmente gestisci eventi del WebSocket
}

// Funzione per gestire la richiesta della pagina root
void handleRoot() {
  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Failed to open file");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}
