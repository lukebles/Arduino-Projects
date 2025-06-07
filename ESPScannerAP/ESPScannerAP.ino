#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>
#include <ArduinoJson.h>

// Configurazione Access Point
#define AP_SSID "ESP_Scanner"
#define AP_PASS "12345678"

// Massimo numero di reti da mantenere
#define MAX_NETWORKS 100

// Struttura dati per salvare le reti
struct NetworkInfo {
  String ssid;
  int rssi;
  int channel;
  String bssid;
  String encryption;
  bool hidden;
};

NetworkInfo networks[MAX_NETWORKS];
int networkCount = 0;

// Server WebSocket
WebSocketsServer webSocket(81);

// Server Web
ESP8266WebServer server(80);

// Timer per la scansione
unsigned long lastScanTime = 0;
const unsigned long scanInterval = 5000; // 5 secondi

// --- Funzione di utilità per convertire encryptionType in stringa ---
String encryptionTypeToStr(uint8_t type) {
  // Nota: per l'ESP8266 sono spesso definiti come ENC_TYPE_*
  // Per l'ESP32, la nomenclatura può variare, es: WIFI_AUTH_OPEN, WIFI_AUTH_WPA2_PSK ecc.
  switch (type) {
    case ENC_TYPE_WEP:
      return "WEP";
    case ENC_TYPE_TKIP:
      return "WPA/PSK(TKIP)";
    case ENC_TYPE_CCMP:
      return "WPA2/PSK(CCMP)";
    case ENC_TYPE_NONE:
      return "Open";
    case ENC_TYPE_AUTO:
      return "Auto";
    default:
      return "Unknown";
  }
}

void setup() {
  Serial.begin(115200);

  // Configurazione Access Point
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.println("Access Point creato.");
  Serial.print("IP del dispositivo: ");
  Serial.println(WiFi.softAPIP());

  // Configurazione WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // Configurazione server web
  server.on("/", handleRoot);
  server.begin();
  Serial.println("Server HTTP avviato.");
}

void loop() {
  // Gestione WebSocket
  webSocket.loop();

  // Gestione server web
  server.handleClient();

  // Esecuzione scansione Wi-Fi
  if (millis() - lastScanTime > scanInterval) {
    scanNetworks();
    lastScanTime = millis();
  }
}

void scanNetworks() {
  Serial.println("Scansione reti Wi-Fi in corso...");
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("Nessuna rete trovata.");
    return;
  }

  for (int i = 0; i < n; ++i) {
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    int channel = WiFi.channel(i);
    String bssid = WiFi.BSSIDstr(i);           // MAC address in formato testo
    String encryption = encryptionTypeToStr(WiFi.encryptionType(i));
    bool hidden = WiFi.isHidden(i);

    bool found = false;
    for (int j = 0; j < networkCount; ++j) {
      if (networks[j].ssid == ssid) {
        networks[j].rssi       = rssi;    // Aggiorna RSSI
        networks[j].channel    = channel; // Aggiorna canale
        networks[j].bssid      = bssid;
        networks[j].encryption = encryption;
        networks[j].hidden     = hidden;
        found = true;
        break;
      }
    }

    if (!found && networkCount < MAX_NETWORKS) {
      networks[networkCount].ssid       = ssid;
      networks[networkCount].rssi       = rssi;
      networks[networkCount].channel    = channel;
      networks[networkCount].bssid      = bssid;
      networks[networkCount].encryption = encryption;
      networks[networkCount].hidden     = hidden;
      networkCount++;
    }
  }

  // Trasmetti la tabella aggiornata via WebSocket
  sendNetworkTable();

  // Cancella i risultati della scansione per risparmiare memoria
  WiFi.scanDelete();
}

void sendNetworkTable() {
  // Creiamo un JSON e lo mandiamo a tutti i client via WebSocket
  StaticJsonDocument<8192> doc;
  JsonArray array = doc.createNestedArray("networks");
  for (int i = 0; i < networkCount; ++i) {
    JsonObject obj = array.createNestedObject();
    obj["ssid"]       = networks[i].ssid;
    obj["rssi"]       = networks[i].rssi;
    obj["channel"]    = networks[i].channel;
    obj["bssid"]      = networks[i].bssid;
    obj["encryption"] = networks[i].encryption;
    obj["hidden"]     = networks[i].hidden ? "Yes" : "No";
  }

  String jsonString;
  serializeJson(doc, jsonString);
  webSocket.broadcastTXT(jsonString);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_TEXT) {
    String message = String((char*)payload).substring(0, length);
    Serial.printf("Messaggio ricevuto da client %d: %s\n", num, message.c_str());

    if (message == "RESET") {
      // Resetta la tabella
      networkCount = 0;
      Serial.println("Tabella resettata.");
      sendNetworkTable();
    }
  }
}

// Uso la soluzione "classica" per evitare problemi con i Raw String Literal
const char MAIN_page[] PROGMEM = 
"<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"  <title>WiFi Scanner</title>\n"
"  <script>\n"
"    var ws;\n"
"\n"
"    function connectWebSocket() {\n"
"      ws = new WebSocket(\"ws://\" + location.host + \":81/\");\n"
"      ws.onopen = function() {\n"
"        console.log(\"WebSocket connesso.\");\n"
"      };\n"
"      ws.onmessage = function(event) {\n"
"        console.log(\"Messaggio ricevuto:\", event.data);\n"
"        try {\n"
"          var data = JSON.parse(event.data);\n"
"          var networks = data.networks;\n"
"          var table = document.getElementById(\"networks\");\n"
"          table.innerHTML = \"<tr><th>SSID</th><th>RSSI</th><th>Canale</th><th>BSSID</th><th>Enc</th><th>Hidden</th></tr>\";\n"
"          for (var i = 0; i < networks.length; i++) {\n"
"            var row = table.insertRow(-1);\n"
"            row.insertCell(0).innerHTML = networks[i].ssid;\n"
"            row.insertCell(1).innerHTML = networks[i].rssi;\n"
"            row.insertCell(2).innerHTML = networks[i].channel;\n"
"            row.insertCell(3).innerHTML = networks[i].bssid;\n"
"            row.insertCell(4).innerHTML = networks[i].encryption;\n"
"            row.insertCell(5).innerHTML = networks[i].hidden;\n"
"          }\n"
"        } catch (error) {\n"
"          console.error(\"Errore nel parsing JSON:\", error);\n"
"        }\n"
"      };\n"
"      ws.onerror = function(error) {\n"
"        console.error(\"Errore WebSocket:\", error);\n"
"      };\n"
"      ws.onclose = function() {\n"
"        console.log(\"WebSocket chiuso. Riconnessione in corso...\");\n"
"        setTimeout(connectWebSocket, 2000); // Riconnetti automaticamente\n"
"      };\n"
"    }\n"
"\n"
"    function resetTable() {\n"
"      ws.send(\"RESET\");\n"
"    }\n"
"\n"
"    window.onload = connectWebSocket;\n"
"  </script>\n"
"</head>\n"
"<body>\n"
"  <h1>WiFi Scanner</h1>\n"
"  <button onclick=\"resetTable()\">Reset Tabella</button>\n"
"  <table id=\"networks\" border=\"1\">\n"
"    <tr><th>SSID</th><th>RSSI</th><th>Canale</th><th>BSSID</th><th>Enc</th><th>Hidden</th></tr>\n"
"  </table>\n"
"</body>\n"
"</html>\n";

void handleRoot() {
  server.send(200, "text/html", MAIN_page);
}