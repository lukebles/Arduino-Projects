/*
  ESP32 WiFi Scanner via pagina web AP
  ------------------------------------
  - ESP32 in modalita' Access Point + Station
  - Scansione WiFi 2.4 GHz per durata impostabile da pagina web
  - Tabella ordinabile lato browser
  - Storico ultime 5 scansioni salvato in LittleFS
  - Grafici gestiti dalla pagina web con Chart.js 2.9.3 se presente in data/chart.min.js

  File da caricare in LittleFS:
  - data/index.html
  - data/chart.min.js  (Chart.js 2.9.3, opzionale ma consigliato)

  Nota:
  se chart.min.js non e' presente, la pagina usa un piccolo grafico Canvas di fallback.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

// ============================
// CONFIGURAZIONE
// ============================
const char* AP_SSID = "ESP32_WIFI_SCANNER";
const char* AP_PASS = "12345678";      // Minimo 8 caratteri. Per AP aperto usare WiFi.softAP(AP_SSID)

const uint16_t WEB_PORT = 80;
const int MAX_NETS = 30;
const int HISTORY_SIZE = 5;
const uint32_t DEFAULT_SCAN_SECONDS = 30;
const uint32_t MIN_SCAN_SECONDS = 5;
const uint32_t MAX_SCAN_SECONDS = 300;

WebServer server(WEB_PORT);

// ============================
// STRUTTURE DATI
// ============================
struct NetInfo {
  bool used;
  String ssid;
  String bssid;
  int rssiMin;
  int rssiMax;
  int rssiLast;
  uint8_t channel;
  int freqMHz;
  String encryption;
  bool isOpen;
  String width;
};

struct HistoryMeta {
  int nextIndex;
  int count;
};

NetInfo nets[MAX_NETS];
int uniqueCount = 0;

bool scanning = false;
bool scanRequestActive = false;
bool everCompleted = false;
String stateText = "Pronto";
String scanClientTime = "";
uint32_t scanDurationMs = DEFAULT_SCAN_SECONDS * 1000UL;
uint32_t scanStartMs = 0;
uint32_t scanEndMs = 0;
String currentScanJson = "{\"time\":\"\",\"duration\":0,\"durationEffective\":0,\"total\":0,\"networks\":[]}";

// ============================
// UTILITY
// ============================
String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"':  out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if ((uint8_t)c < 32) out += ' ';
        else out += c;
        break;
    }
  }
  return out;
}

String htmlEscape(const String& in) {
  String out;
  out.reserve(in.length() + 16);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += c; break;
    }
  }
  return out;
}

int freqFromChannel(uint8_t ch) {
  if (ch >= 1 && ch <= 13) return 2412 + ((int)ch - 1) * 5;
  if (ch == 14) return 2484;
  return 0;
}

String authToString(wifi_auth_mode_t auth) {
  switch (auth) {
    case WIFI_AUTH_OPEN: return "OPEN";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA-PSK";
    case WIFI_AUTH_WPA2_PSK: return "WPA2-PSK";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2-PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENTERPRISE";
    default: return String("AUTH_") + String((int)auth);
  }
}

void resetNets() {
  uniqueCount = 0;
  for (int i = 0; i < MAX_NETS; i++) {
    nets[i].used = false;
    nets[i].ssid = "";
    nets[i].bssid = "";
    nets[i].rssiMin = 127;
    nets[i].rssiMax = -127;
    nets[i].rssiLast = -127;
    nets[i].channel = 0;
    nets[i].freqMHz = 0;
    nets[i].encryption = "";
    nets[i].isOpen = false;
    nets[i].width = "n/d";
  }
}

int findByBSSID(const String& bssid) {
  for (int i = 0; i < MAX_NETS; i++) {
    if (nets[i].used && nets[i].bssid == bssid) return i;
  }
  return -1;
}

int firstFreeIndex() {
  for (int i = 0; i < MAX_NETS; i++) {
    if (!nets[i].used) return i;
  }
  return -1;
}

int weakestIndex() {
  int idx = -1;
  int weakest = 127;
  for (int i = 0; i < MAX_NETS; i++) {
    if (nets[i].used && nets[i].rssiMax < weakest) {
      weakest = nets[i].rssiMax;
      idx = i;
    }
  }
  return idx;
}

void putNetworkInSlot(int idx, int scanIndex) {
  String ssid = WiFi.SSID(scanIndex);
  if (ssid.length() == 0) ssid = "(nascosto)";
  if (ssid.length() > 32) ssid = ssid.substring(0, 32);

  String bssid = WiFi.BSSIDstr(scanIndex);
  int rssi = WiFi.RSSI(scanIndex);
  uint8_t ch = WiFi.channel(scanIndex);
  wifi_auth_mode_t auth = WiFi.encryptionType(scanIndex);

  nets[idx].used = true;
  nets[idx].ssid = ssid;
  nets[idx].bssid = bssid;
  nets[idx].rssiMin = rssi;
  nets[idx].rssiMax = rssi;
  nets[idx].rssiLast = rssi;
  nets[idx].channel = ch;
  nets[idx].freqMHz = freqFromChannel(ch);
  nets[idx].encryption = authToString(auth);
  nets[idx].isOpen = (auth == WIFI_AUTH_OPEN);
  nets[idx].width = "n/d";   // WiFi.h non espone sempre la larghezza canale in modo portabile
}

void updateNetworkSlot(int idx, int scanIndex) {
  int rssi = WiFi.RSSI(scanIndex);
  nets[idx].rssiLast = rssi;
  if (rssi < nets[idx].rssiMin) nets[idx].rssiMin = rssi;
  if (rssi > nets[idx].rssiMax) nets[idx].rssiMax = rssi;

  uint8_t ch = WiFi.channel(scanIndex);
  if (ch > 0) {
    nets[idx].channel = ch;
    nets[idx].freqMHz = freqFromChannel(ch);
  }
}

void processScanResults(int n) {
  for (int i = 0; i < n; i++) {
    String bssid = WiFi.BSSIDstr(i);
    if (bssid.length() == 0) continue;

    int idx = findByBSSID(bssid);
    if (idx >= 0) {
      updateNetworkSlot(idx, i);
      continue;
    }

    idx = firstFreeIndex();
    if (idx >= 0) {
      putNetworkInSlot(idx, i);
      uniqueCount++;
      continue;
    }

    // Se abbiamo gia' 30 reti, manteniamo le 30 con segnale massimo migliore.
    int rssi = WiFi.RSSI(i);
    int weak = weakestIndex();
    if (weak >= 0 && rssi > nets[weak].rssiMax) {
      putNetworkInSlot(weak, i);
    }
  }
}

void sortedIndexesByRSSI(int* indexes, int& count) {
  count = 0;
  for (int i = 0; i < MAX_NETS; i++) {
    if (nets[i].used) indexes[count++] = i;
  }

  for (int i = 0; i < count - 1; i++) {
    for (int j = i + 1; j < count; j++) {
      if (nets[indexes[j]].rssiMax > nets[indexes[i]].rssiMax) {
        int tmp = indexes[i];
        indexes[i] = indexes[j];
        indexes[j] = tmp;
      }
    }
  }
}

String buildScanJson() {
  int indexes[MAX_NETS];
  int count = 0;
  sortedIndexesByRSSI(indexes, count);

  uint32_t effectiveSeconds = 0;
  if (scanEndMs >= scanStartMs) effectiveSeconds = (scanEndMs - scanStartMs) / 1000UL;

  String out;
  out.reserve(12000);
  out += "{\"time\":\"";
  out += jsonEscape(scanClientTime);
  out += "\"";
  out += ",\"duration\":";
  out += String(scanDurationMs / 1000UL);
  out += ",\"durationEffective\":";
  out += String(effectiveSeconds);
  out += ",\"total\":";
  out += String(count);
  out += ",\"networks\":[";

  for (int pos = 0; pos < count; pos++) {
    int i = indexes[pos];
    if (pos > 0) out += ",";
    out += "{";
    out += "\"n\":";
    out += String(pos + 1);
    out += ",\"ssid\":\"";
    out += jsonEscape(nets[i].ssid);
    out += "\"";
    out += ",\"bssid\":\"";
    out += jsonEscape(nets[i].bssid);
    out += "\"";
    out += ",\"rssiMin\":";
    out += String(nets[i].rssiMin);
    out += ",\"rssiMax\":";
    out += String(nets[i].rssiMax);
    out += ",\"rssiLast\":";
    out += String(nets[i].rssiLast);
    out += ",\"channel\":";
    out += String(nets[i].channel);
    out += ",\"freq\":";
    out += String(nets[i].freqMHz);
    out += ",\"enc\":\"";
    out += jsonEscape(nets[i].encryption);
    out += "\"";
    out += ",\"open\":";
    out += (nets[i].isOpen ? "true" : "false");
    out += ",\"width\":\"";
    out += jsonEscape(nets[i].width);
    out += "\"";
    out += "}";
  }

  out += "]}";
  return out;
}

// ============================
// LITTLEFS / STORICO
// ============================
String readTextFile(const String& path) {
  File f = LittleFS.open(path, "r");
  if (!f) return "";
  String s = f.readString();
  f.close();
  return s;
}

bool writeTextFile(const String& path, const String& data) {
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  f.print(data);
  f.close();
  return true;
}

HistoryMeta loadHistoryMeta() {
  HistoryMeta m;
  m.nextIndex = 0;
  m.count = 0;

  String txt = readTextFile("/meta.txt");
  if (txt.length() == 0) return m;

  int comma = txt.indexOf(',');
  if (comma < 0) return m;

  m.nextIndex = txt.substring(0, comma).toInt();
  m.count = txt.substring(comma + 1).toInt();

  if (m.nextIndex < 0 || m.nextIndex >= HISTORY_SIZE) m.nextIndex = 0;
  if (m.count < 0) m.count = 0;
  if (m.count > HISTORY_SIZE) m.count = HISTORY_SIZE;
  return m;
}

void saveHistoryMeta(const HistoryMeta& m) {
  writeTextFile("/meta.txt", String(m.nextIndex) + "," + String(m.count));
}

void saveScanToHistory(const String& scanJson) {
  HistoryMeta m = loadHistoryMeta();
  String path = String("/scan") + String(m.nextIndex) + ".json";
  writeTextFile(path, scanJson);

  m.nextIndex = (m.nextIndex + 1) % HISTORY_SIZE;
  if (m.count < HISTORY_SIZE) m.count++;
  saveHistoryMeta(m);
}

String buildHistoryJson() {
  HistoryMeta m = loadHistoryMeta();
  String out;
  out.reserve(60000);
  out += "{\"scans\":[";

  bool first = true;
  for (int k = 0; k < m.count; k++) {
    int idx = (m.nextIndex - 1 - k + HISTORY_SIZE) % HISTORY_SIZE;
    String path = String("/scan") + String(idx) + ".json";
    String scan = readTextFile(path);
    if (scan.length() == 0) continue;

    if (!first) out += ",";
    out += scan;
    first = false;
  }

  out += "]}";
  return out;
}

void clearHistory() {
  for (int i = 0; i < HISTORY_SIZE; i++) {
    LittleFS.remove(String("/scan") + String(i) + ".json");
  }
  HistoryMeta m;
  m.nextIndex = 0;
  m.count = 0;
  saveHistoryMeta(m);
}

void loadLatestScanAsCurrent() {
  HistoryMeta m = loadHistoryMeta();
  if (m.count <= 0) return;
  int idx = (m.nextIndex - 1 + HISTORY_SIZE) % HISTORY_SIZE;
  String scan = readTextFile(String("/scan") + String(idx) + ".json");
  if (scan.length() > 0) {
    currentScanJson = scan;
    everCompleted = true;
    stateText = "Pronto";
  }
}

// ============================
// SCANSIONE ASINCRONA
// ============================
void startAsyncScanRequest() {
  WiFi.scanDelete();
  WiFi.scanNetworks(true, true);   // async=true, show_hidden=true
  scanRequestActive = true;
}

void finishScan() {
  scanning = false;
  scanRequestActive = false;
  scanEndMs = millis();
  stateText = "Scansione completata";
  everCompleted = true;

  currentScanJson = buildScanJson();
  saveScanToHistory(currentScanJson);
}

void handleScanState() {
  if (!scanning) return;

  if (!scanRequestActive) {
    if (millis() - scanStartMs >= scanDurationMs) {
      finishScan();
    } else {
      startAsyncScanRequest();
    }
    return;
  }

  int result = WiFi.scanComplete();

  if (result == WIFI_SCAN_RUNNING) {
    return;
  }

  if (result == WIFI_SCAN_FAILED) {
    WiFi.scanDelete();
    scanRequestActive = false;
    if (millis() - scanStartMs >= scanDurationMs) finishScan();
    return;
  }

  if (result >= 0) {
    processScanResults(result);
    WiFi.scanDelete();
    scanRequestActive = false;

    if (millis() - scanStartMs >= scanDurationMs) {
      finishScan();
    }
  }
}

// ============================
// WEB SERVER
// ============================
String contentTypeFor(const String& path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".js")) return "application/javascript";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".png")) return "image/png";
  if (path.endsWith(".ico")) return "image/x-icon";
  return "text/plain";
}

bool serveLittleFSFile(String path) {
  if (path == "/") path = "/index.html";
  if (!LittleFS.exists(path)) return false;

  File file = LittleFS.open(path, "r");
  if (!file) return false;

  server.streamFile(file, contentTypeFor(path));
  file.close();
  return true;
}

void sendNoCache() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
}

void handleRoot() {
  sendNoCache();
  if (!serveLittleFSFile("/index.html")) {
    String msg = "<html><body><h1>ESP32 WiFi Scanner</h1>";
    msg += "<p>File /index.html non trovato in LittleFS.</p>";
    msg += "<p>Carica la cartella data/ nel filesystem LittleFS.</p>";
    msg += "</body></html>";
    server.send(200, "text/html", msg);
  }
}

void handleStart() {
  sendNoCache();

  if (scanning) {
    server.send(409, "application/json", "{\"ok\":false,\"error\":\"scansione gia in corso\"}");
    return;
  }

  scanClientTime = server.arg("clientTime");
  if (scanClientTime.length() == 0) scanClientTime = "data/ora non disponibile";
  if (scanClientTime.length() > 32) scanClientTime = scanClientTime.substring(0, 32);

  uint32_t duration = server.arg("duration").toInt();
  if (duration == 0) duration = DEFAULT_SCAN_SECONDS;
  if (duration < MIN_SCAN_SECONDS) duration = MIN_SCAN_SECONDS;
  if (duration > MAX_SCAN_SECONDS) duration = MAX_SCAN_SECONDS;

  resetNets();
  scanDurationMs = duration * 1000UL;
  scanStartMs = millis();
  scanEndMs = scanStartMs;
  scanning = true;
  scanRequestActive = false;
  stateText = "Scansione in corso";

  startAsyncScanRequest();

  String resp = String("{\"ok\":true,\"state\":\"scanning\",\"duration\":") + String(duration) + "}";
  server.send(200, "application/json", resp);
}

void handleStatus() {
  sendNoCache();

  uint32_t elapsed = 0;
  uint32_t remaining = 0;
  if (scanning) {
    elapsed = (millis() - scanStartMs) / 1000UL;
    uint32_t dur = scanDurationMs / 1000UL;
    remaining = (elapsed >= dur) ? 0 : (dur - elapsed);
  }

  String state = scanning ? "scanning" : (everCompleted ? "completed" : "ready");

  String out = "{";
  out += "\"state\":\"";
  out += state;
  out += "\"";
  out += ",\"text\":\"";
  out += jsonEscape(stateText);
  out += "\"";
  out += ",\"elapsed\":";
  out += String(elapsed);
  out += ",\"remaining\":";
  out += String(remaining);
  out += ",\"duration\":";
  out += String(scanDurationMs / 1000UL);
  out += ",\"unique\":";
  out += String(uniqueCount);
  out += "}";

  server.send(200, "application/json", out);
}

void handleLast() {
  sendNoCache();
  server.send(200, "application/json", currentScanJson);
}

void handleHistory() {
  sendNoCache();
  server.send(200, "application/json", buildHistoryJson());
}

void handleClearHistory() {
  sendNoCache();
  clearHistory();
  currentScanJson = "{\"time\":\"\",\"duration\":0,\"durationEffective\":0,\"total\":0,\"networks\":[]}";
  everCompleted = false;
  stateText = "Pronto";
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleNotFound() {
  sendNoCache();
  String path = server.uri();
  if (serveLittleFSFile(path)) return;
  server.send(404, "text/plain", String("404 - File non trovato: ") + path);
}

// ============================
// SETUP / LOOP
// ============================
void setup() {
  Serial.begin(115200);
  delay(300);

  resetNets();

  if (!LittleFS.begin(true)) {
    Serial.println("Errore LittleFS");
  } else {
    Serial.println("LittleFS OK");
    loadLatestScanAsCurrent();
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);

  bool apOk = WiFi.softAP(AP_SSID, AP_PASS);
  Serial.println(apOk ? "AP avviato" : "Errore avvio AP");
  Serial.print("SSID AP: ");
  Serial.println(AP_SSID);
  Serial.print("IP AP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/start", HTTP_GET, handleStart);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/last", HTTP_GET, handleLast);
  server.on("/history", HTTP_GET, handleHistory);
  server.on("/clear-history", HTTP_GET, handleClearHistory);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("Web server avviato");
}

void loop() {
  server.handleClient();
  handleScanState();
  delay(2);
}
