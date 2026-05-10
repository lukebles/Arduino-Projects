#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <RH_ASK.h>
#include <SPI.h>   // richiesto da RH_ASK

// =====================================================
// CONFIG RADIO
// =====================================================
const int RX_PIN   = 21;     // pin dati ricevitore radio
const int TX_PIN   = -1;     // non usato
const int PTT_PIN  = -1;     // non usato
const bool PTT_INV = false;
const int SPEED    = 2000;

// RH_ASK(speed, rxPin, txPin, pttPin, pttInverted)
RH_ASK driver(SPEED, RX_PIN, TX_PIN, PTT_PIN, PTT_INV);

// =====================================================
// CONFIG WIFI AP
// =====================================================
const char* AP_PASSWORD = "12345678";   // minimo 8 caratteri
const bool  AP_HIDDEN   = false;
const int   AP_MAX_CONN = 4;

// Canale WiFi persistente
int wifiChannel = 6;

// =====================================================
// WEB SERVER + PREFERENCES
// =====================================================
WebServer server(80);
Preferences prefs;

// =====================================================
// DEVICE ID
// =====================================================
const uint8_t DEVICE_ID_A = 3;   // Porta A
const uint8_t DEVICE_ID_B = 4;   // Porta B

// =====================================================
// STATO PORTE
// true  = aperta
// false = chiusa
// =====================================================
bool doorAOpen = false;
bool doorBOpen = false;

bool doorAValid = false;
bool doorBValid = false;

// =====================================================
// STATO SSID / CONTATORE
// =====================================================
String currentSSID = "";
uint8_t stateCounter = 0;   // 0..9

// =====================================================
// UTILITY HTML
// =====================================================
String htmlHeader(const String& title) {
  String s;
  s += "<!DOCTYPE html><html><head><meta charset='utf-8'>";
  s += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  s += "<title>" + title + "</title>";
  s += "<style>";
  s += "body{font-family:Arial,sans-serif;max-width:700px;margin:30px auto;padding:0 15px;line-height:1.5;}";
  s += "input,button{font-size:18px;padding:8px;}";
  s += ".box{border:1px solid #ccc;padding:16px;border-radius:8px;}";
  s += ".ok{color:green;font-weight:bold;}";
  s += ".warn{color:#b06a00;font-weight:bold;}";
  s += "</style></head><body>";
  return s;
}

// =====================================================
// DECODIFICA BYTE RADIO
// formato: xxx0000y
// bit 7..5 = device ID
// bit 0    = stato porta (1 aperta, 0 chiusa)
// =====================================================
uint8_t getDeviceId(uint8_t cmd) {
  return (cmd >> 5) & 0x07;
}

bool getDoorOpen(uint8_t cmd) {
  return (cmd & 0x01) != 0;
}

// =====================================================
// CANALE WIFI
// =====================================================
bool isValidWifiChannel(int ch) {
  return (ch >= 1 && ch <= 11);
}

void loadWifiChannel() {
  prefs.begin("bagno-radio", true);
  int ch = prefs.getInt("wifiChannel", 6);
  prefs.end();

  if (isValidWifiChannel(ch)) {
    wifiChannel = ch;
  } else {
    wifiChannel = 6;
  }
}

void saveWifiChannel(int ch) {
  prefs.begin("bagno-radio", false);
  prefs.putInt("wifiChannel", ch);
  prefs.end();
  wifiChannel = ch;
}

// =====================================================
// CONTATORE STATO 0..9
// =====================================================
void loadStateCounter() {
  prefs.begin("bagno-radio", true);
  int c = prefs.getInt("stateCount", 0);
  prefs.end();

  if (c < 0 || c > 9) {
    stateCounter = 0;
  } else {
    stateCounter = (uint8_t)c;
  }
}

void saveStateCounter() {
  prefs.begin("bagno-radio", false);
  prefs.putInt("stateCount", stateCounter);
  prefs.end();
}

void incrementStateCounter() {
  stateCounter++;
  if (stateCounter > 9) {
    stateCounter = 0;
  }
  saveStateCounter();
}

// =====================================================
// STATO PORTE -> LETTERE
// A = aperta
// C = chiusa
// X = sconosciuta
// =====================================================
char stateChar(bool valid, bool open) {
  if (!valid) return 'X';
  return open ? 'A' : 'C';
}

// =====================================================
// COSTRUZIONE SSID
// formato: OfficeLink_0_CA
// =====================================================
String buildSSID() {
  char a = stateChar(doorAValid, doorAOpen);
  char b = stateChar(doorBValid, doorBOpen);

  char buf[32];
  snprintf(buf, sizeof(buf), "OfficeLink_%u_%c%c", stateCounter, a, b);
  return String(buf);
}

// =====================================================
// AVVIO / RIAVVIO AP
// =====================================================
bool startAP(const String& ssid) {
  WiFi.softAPdisconnect(true);
  delay(200);

  bool ok = WiFi.softAP(
    ssid.c_str(),
    AP_PASSWORD,
    wifiChannel,
    AP_HIDDEN,
    AP_MAX_CONN
  );

  if (ok) {
    currentSSID = ssid;
  }

  return ok;
}

void updateAPIfNeeded(const String& newSSID) {
  if (newSSID == currentSSID) return;
  startAP(newSSID);
}

// =====================================================
// PAGINA PRINCIPALE
// =====================================================
void handleRoot() {
  String html = htmlHeader("ESP32 Porte");
  html += "<div class='box'>";
  html += "<h1>ESP32 Porte</h1>";
  html += "<p><b>SSID attuale:</b> " + currentSSID + "</p>";
  html += "<p><b>Canale WiFi:</b> " + String(wifiChannel) + "</p>";
  html += "<p><b>Contatore stato:</b> " + String(stateCounter) + "</p>";

  html += "<p><b>Porta A (ID 3):</b> ";
  html += doorAValid ? (doorAOpen ? "APERTA" : "CHIUSA") : "SCONOSCIUTA";
  html += "</p>";

  html += "<p><b>Porta B (ID 4):</b> ";
  html += doorBValid ? (doorBOpen ? "APERTA" : "CHIUSA") : "SCONOSCIUTA";
  html += "</p>";

  html += "<p><a href='/canale.html'>Configura canale WiFi</a></p>";
  html += "</div></body></html>";

  server.send(200, "text/html; charset=utf-8", html);
}

// =====================================================
// PAGINA CANALE WIFI
// =====================================================
void handleCanalePage() {
  String html = htmlHeader("Canale WiFi");
  html += "<div class='box'>";
  html += "<h1>Imposta canale WiFi</h1>";
  html += "<p>Canale attuale: <b>" + String(wifiChannel) + "</b></p>";
  html += "<form method='POST' action='/setcanale'>";
  html += "<label for='canale'>Canale (1-11):</label><br><br>";
  html += "<input type='number' id='canale' name='canale' min='1' max='11' value='" + String(wifiChannel) + "'>";
  html += "<button type='submit'>Salva</button>";
  html += "</form>";
  html += "<p style='margin-top:20px;'><a href='/'>Torna alla home</a></p>";
  html += "</div></body></html>";

  server.send(200, "text/html; charset=utf-8", html);
}

// =====================================================
// SALVATAGGIO CANALE
// =====================================================
void handleSetCanale() {
  if (!server.hasArg("canale")) {
    server.send(400, "text/plain; charset=utf-8", "Parametro 'canale' mancante");
    return;
  }

  int newChannel = server.arg("canale").toInt();

  if (!isValidWifiChannel(newChannel)) {
    String html = htmlHeader("Errore");
    html += "<div class='box'>";
    html += "<p class='warn'>Canale non valido. Inserisci un valore da 1 a 11.</p>";
    html += "<p><a href='/canale.html'>Indietro</a></p>";
    html += "</div></body></html>";
    server.send(400, "text/html; charset=utf-8", html);
    return;
  }

  bool changed = (newChannel != wifiChannel);
  saveWifiChannel(newChannel);

  if (changed) {
    startAP(currentSSID);
  }

  String html = htmlHeader("Canale salvato");
  html += "<div class='box'>";
  html += "<p class='ok'>Canale WiFi salvato correttamente.</p>";
  html += "<p>Nuovo canale: <b>" + String(wifiChannel) + "</b></p>";
  html += "<p>SSID attuale: <b>" + currentSSID + "</b></p>";
  html += "<p><a href='/canale.html'>Torna alla pagina canale</a></p>";
  html += "<p><a href='/'>Vai alla home</a></p>";
  html += "</div></body></html>";

  server.send(200, "text/html; charset=utf-8", html);
}

// =====================================================
// GESTIONE BYTE RICEVUTO
// - aggiorna solo il device ricevuto
// - mantiene l'ultimo stato noto dell'altro
// - incrementa il contatore solo se c'è un vero cambio
// - stampa su seriale solo se arriva un pacchetto valido
// =====================================================
void handleCommandByte(uint8_t cmd) {
  uint8_t deviceId = getDeviceId(cmd);
  bool doorOpen = getDoorOpen(cmd);

  bool recognized = false;
  bool changed = false;

  if (deviceId == DEVICE_ID_A) {
    recognized = true;
    if (!doorAValid || doorAOpen != doorOpen) {
      doorAOpen = doorOpen;
      doorAValid = true;
      changed = true;
    }
  }
  else if (deviceId == DEVICE_ID_B) {
    recognized = true;
    if (!doorBValid || doorBOpen != doorOpen) {
      doorBOpen = doorOpen;
      doorBValid = true;
      changed = true;
    }
  }

  if (!recognized) {
    return;
  }

  if (changed) {
    incrementStateCounter();
  }

  String newSSID = buildSSID();

  Serial.print("Ricevuto ID=");
  Serial.print(deviceId);
  Serial.print(" stato=");
  Serial.print(doorOpen ? "APERTA" : "CHIUSA");
  Serial.print(" -> SSID: ");
  Serial.println(newSSID);

  if (changed) {
    updateAPIfNeeded(newSSID);
  }
}

void setup() {
  Serial.begin(9600);
  delay(2000);
  Serial.println();

  loadWifiChannel();
  loadStateCounter();

  if (!isValidWifiChannel(wifiChannel)) {
    wifiChannel = 6;
  }

  WiFi.mode(WIFI_AP);

  String initialSSID = buildSSID();
  startAP(initialSSID);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/canale.html", HTTP_GET, handleCanalePage);
  server.on("/setcanale", HTTP_POST, handleSetCanale);
  server.begin();

  driver.init();
}

void loop() {
  server.handleClient();

  uint8_t buf[8];
  uint8_t buflen = sizeof(buf);

  if (driver.recv(buf, &buflen)) {
    if (buflen >= 1) {
      handleCommandByte(buf[0]);
    }
  }
}