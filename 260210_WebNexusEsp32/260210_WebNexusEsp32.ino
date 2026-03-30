/*
  WebNexus ESP32 - single .ino
  - AP mode (auto channel selection)
  - AsyncWebServer + AsyncWebSocket on same port 80  => ws://host/ws
  - LittleFS serve static files from /data
  - Preferences (NVS) for powerLimit
  - Ring buffers for Instant/Hour/Day
  - Binary persistence /data.bin with header + version + basic checksum
  - JSON via ArduinoJson (static buffer)
  - Fallback HTTP endpoints for old browsers:
      GET /api/instant
      GET /api/hours
      GET /api/days
      GET /api/powerLimit
      GET /api/last
*/

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <Preferences.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <ArduinoJson.h>
#include <TimeLib.h>

#include <LkMultivibrator.h>

// ====================== CONFIG ======================
static const char* AP_SSID     = "sid2";       // personalizza (Luca/Marco)
static const char* AP_PASS     = "pw12345678"; // >= 8 chars

static const uint32_t RADIO_TIMEOUT_MS = 27000;
static const uint32_t SAVE_INTERVAL_MS = 8UL * 60UL * 60UL * 1000UL; // 8 ore

static const int MAX_DATA_POINTS = 31;

// ================ DATA STRUCTURES ====================
struct DataInstant {
  uint16_t activePower;
  uint16_t reactivePower;
  time_t   timestamp;
  uint32_t timeDiff;
};

struct DataEnergyHours {
  time_t   timestampH;
  uint16_t diff_a;
  uint16_t diff_r;
};

struct DataEnergyDays {
  time_t   timestampD;
  uint16_t diff_a;
  uint16_t diff_r;
};

struct __attribute__((packed)) DataPacket {
  uint16_t activeDiff;
  uint16_t reactiveDiff;
  uint32_t timeDiff;
};

// ====================== GLOBALS ======================
static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");
static Preferences prefs;

static LkMultivibrator radioCheck(RADIO_TIMEOUT_MS, MONOSTABLE);

static uint32_t lastSaveTime = 0;

static bool ultimoDatoRadioAffidabile = true;
static int  potenza = 0;
static int  powerLimitValue = 3990;

// data arrays
static DataInstant     istantPoints[MAX_DATA_POINTS];
static DataEnergyHours hoursPoints[MAX_DATA_POINTS];
static DataEnergyDays  daysPoints[MAX_DATA_POINTS];

// Ring buffers indices
static uint8_t ist_head = 0;  static bool ist_full = false;
static uint8_t hr_head  = 0;  static bool hr_full  = false;
static uint8_t dy_head  = 0;  static bool dy_full  = false;

#define WNX_MAGIC 0x31584E57UL  // 'WNX1' in little-endian: W N X 1


// ====================== HELPERS ======================
static float roundToTens(float v) { return roundf(v / 10.f) * 10.f; }

static const char* wday3(int wday) {
  static const char* a[] = {"DOM","LUN","MAR","MER","GIO","VEN","SAB"};
  return a[(wday - 1) % 7];
}
static const char* mon3(int m) {
  static const char* a[] = {"GEN","FEB","MAR","APR","MAG","GIU","LUG","AGO","SET","OTT","NOV","DIC"};
  return a[(m - 1) % 12];
}

static void formatTime_radio(time_t t, char* b, size_t n) {
  snprintf(b, n, "%s %02d/%02d/%04d %02d:%02d:%02d",
           wday3(weekday(t)), day(t), month(t), year(t), hour(t), minute(t), second(t));
}
static void formatTime_istant(time_t t, char* b, size_t n) {
  snprintf(b, n, "%02d:%02d:%02d", hour(t), minute(t), second(t));
}
static void formatTime_hours(time_t t, char* b, size_t n) {
  snprintf(b, n, "%s %02d/%02d/%04d %02d:%02d",
           wday3(weekday(t)), day(t), month(t), year(t), hour(t), 0);
}
static void formatTime_days(time_t t, char* b, size_t n) {
  snprintf(b, n, "%02d %s %04d %s", day(t), mon3(month(t)), year(t), wday3(weekday(t)));
}

static void setAffidabilitaDato(bool v) { ultimoDatoRadioAffidabile = v; }
static void setPotenza(int v) { potenza = v; }

// ====================== WIFI (AP) ====================
static int findBestChannel(int n) {
  int channels[13] = {0};
  for (int i = 0; i < n; i++) {
    int ch = WiFi.channel(i);
    if (ch >= 1 && ch <= 13) channels[ch - 1]++;
  }
  int best = 1, minN = channels[0];
  for (int i = 1; i < 13; i++) {
    if (channels[i] < minN) { minN = channels[i]; best = i + 1; }
  }
  Serial.printf("Best channel: %d (nets=%d)\n", best, minN);
  return best;
}

static void setupWiFiAP() {
  WiFi.mode(WIFI_AP);
  int n = WiFi.scanNetworks();
  int ch = findBestChannel(n);
  WiFi.softAP(AP_SSID, AP_PASS, ch);
  Serial.printf("AP up: %s ch=%d ip=%s\n", AP_SSID, ch, WiFi.softAPIP().toString().c_str());
}

// ====================== SERIAL PACKET =================
static bool serialdatapacket_ready() {
  while (Serial.available()) {
    if (Serial.peek() == 0xFF) {
      Serial.read(); // sync
      uint32_t start = millis();
      while (Serial.available() < (int)sizeof(DataPacket)) {
        if (millis() - start > 120) return false;
      }
      return true;
    } else {
      Serial.read(); // scarta
    }
  }
  return false;
}

static DataPacket read_serialdatapacket() {
  DataPacket p{};
  if (Serial.available() >= (int)sizeof(DataPacket)) {
    Serial.readBytes((uint8_t*)&p, sizeof(p));
  }
  return p;
}

// ====================== RING BUFFERS ==================
static void ring_push_instant(uint16_t diff_a, uint16_t diff_r, uint32_t timediff_ms) {
  float dt_s = (float)timediff_ms / 1000.0f;
  if (dt_s <= 0.001f) return;

  float activePower   = roundToTens((diff_a * 3600.0f) / dt_s);
  float reactivePower = roundToTens((diff_r * 3600.0f) / dt_s);

  istantPoints[ist_head].activePower   = (uint16_t)activePower;
  istantPoints[ist_head].reactivePower = (uint16_t)reactivePower;
  istantPoints[ist_head].timestamp     = now();
  istantPoints[ist_head].timeDiff      = timediff_ms;

  setPotenza((int)activePower);

  ist_head++;
  if (ist_head >= MAX_DATA_POINTS) { ist_head = 0; ist_full = true; }
}

static void ring_push_hours(uint16_t diff_a, uint16_t diff_r) {
  time_t cnow = now();
  int cy = year(cnow), cm = month(cnow), cd = day(cnow), ch = hour(cnow);

  // cerca se esiste già la stessa ora nel ring
  int count = hr_full ? MAX_DATA_POINTS : hr_head;
  for (int i = 0; i < count; i++) {
    int idx = hr_full ? (hr_head + i) % MAX_DATA_POINTS : i;
    time_t t = hoursPoints[idx].timestampH;
    if (year(t)==cy && month(t)==cm && day(t)==cd && hour(t)==ch) {
      hoursPoints[idx].diff_a += diff_a;
      hoursPoints[idx].diff_r += diff_r;
      return;
    }
  }

  hoursPoints[hr_head].timestampH = cnow;
  hoursPoints[hr_head].diff_a = diff_a;
  hoursPoints[hr_head].diff_r = diff_r;

  hr_head++;
  if (hr_head >= MAX_DATA_POINTS) { hr_head = 0; hr_full = true; }
}

static void ring_push_days(uint16_t diff_a, uint16_t diff_r) {
  time_t cnow = now();
  int cy = year(cnow), cm = month(cnow), cd = day(cnow);

  int count = dy_full ? MAX_DATA_POINTS : dy_head;
  for (int i = 0; i < count; i++) {
    int idx = dy_full ? (dy_head + i) % MAX_DATA_POINTS : i;
    time_t t = daysPoints[idx].timestampD;
    if (year(t)==cy && month(t)==cm && day(t)==cd) {
      daysPoints[idx].diff_a += diff_a;
      daysPoints[idx].diff_r += diff_r;
      return;
    }
  }

  daysPoints[dy_head].timestampD = cnow;
  daysPoints[dy_head].diff_a = diff_a;
  daysPoints[dy_head].diff_r = diff_r;

  dy_head++;
  if (dy_head >= MAX_DATA_POINTS) { dy_head = 0; dy_full = true; }
}

// ====================== PERSISTENCE ===================
// Simple header with basic checksum to avoid loading garbage
struct DataFileHeader {
  uint32_t magic;     // 'WNX1'
  uint16_t version;   // 1
  uint16_t reserved;  // 0
  uint32_t payloadLen;
  uint32_t checksum;  // simple additive checksum
};

static uint32_t simpleChecksum(const uint8_t* p, size_t n) {
  uint32_t s = 0;
  for (size_t i = 0; i < n; i++) s = (s + p[i]) * 1664525u + 1013904223u;
  return s;
}

static void saveData() {
  // payload = arrays + ring state
  struct Payload {
    DataInstant     inst[MAX_DATA_POINTS];
    DataEnergyHours hours[MAX_DATA_POINTS];
    DataEnergyDays  days[MAX_DATA_POINTS];
    uint8_t ist_head; bool ist_full;
    uint8_t hr_head;  bool hr_full;
    uint8_t dy_head;  bool dy_full;
  } payload;

  memcpy(payload.inst,  istantPoints, sizeof(istantPoints));
  memcpy(payload.hours, hoursPoints,  sizeof(hoursPoints));
  memcpy(payload.days,  daysPoints,   sizeof(daysPoints));
  payload.ist_head = ist_head; payload.ist_full = ist_full;
  payload.hr_head  = hr_head;  payload.hr_full  = hr_full;
  payload.dy_head  = dy_head;  payload.dy_full  = dy_full;

  const uint8_t* pb = (const uint8_t*)&payload;
  const uint32_t plen = (uint32_t)sizeof(payload);

  DataFileHeader h;
  h.magic = 0x31584E57UL; // 'WNX1' little-endian safe-ish (WNX1)
  h.version = 1;
  h.reserved = 0;
  h.payloadLen = plen;
  h.checksum = simpleChecksum(pb, plen);

  File f = LittleFS.open("/data.bin", "w");
  if (!f) { Serial.println("saveData: open fail"); return; }
  f.write((const uint8_t*)&h, sizeof(h));
  f.write(pb, plen);
  f.close();
  Serial.println("Data saved");
}

static void loadData() {
  File f = LittleFS.open("/data.bin", "r");
  if (!f) { Serial.println("loadData: missing"); return; }

  DataFileHeader h{};
  if (f.read((uint8_t*)&h, sizeof(h)) != sizeof(h)) {
    Serial.println("loadData: bad header");
    f.close(); return;
  }

  if (h.magic != 0x31584E57UL || h.version != 1) {
    Serial.println("loadData: wrong magic/version");
    f.close(); return;
  }

  if (h.payloadLen > 200000) { // sanity
    Serial.println("loadData: payload too big");
    f.close(); return;
  }

  std::unique_ptr<uint8_t[]> buf(new uint8_t[h.payloadLen]);
  if (!buf) { Serial.println("loadData: oom"); f.close(); return; }

  int r = f.read(buf.get(), h.payloadLen);
  f.close();
  if (r != (int)h.payloadLen) { Serial.println("loadData: short read"); return; }

  uint32_t cs = simpleChecksum(buf.get(), h.payloadLen);
  if (cs != h.checksum) { Serial.println("loadData: checksum mismatch"); return; }

  struct Payload {
    DataInstant     inst[MAX_DATA_POINTS];
    DataEnergyHours hours[MAX_DATA_POINTS];
    DataEnergyDays  days[MAX_DATA_POINTS];
    uint8_t ist_head; bool ist_full;
    uint8_t hr_head;  bool hr_full;
    uint8_t dy_head;  bool dy_full;
  };

  if (h.payloadLen != sizeof(Payload)) {
    Serial.println("loadData: payload size mismatch (struct changed?)");
    return;
  }

  Payload* p = (Payload*)buf.get();
  memcpy(istantPoints, p->inst, sizeof(istantPoints));
  memcpy(hoursPoints,  p->hours, sizeof(hoursPoints));
  memcpy(daysPoints,   p->days, sizeof(daysPoints));
  ist_head = p->ist_head; ist_full = p->ist_full;
  hr_head  = p->hr_head;  hr_full  = p->hr_full;
  dy_head  = p->dy_head;  dy_full  = p->dy_full;

  Serial.println("Data loaded");
}

// ====================== JSON SEND =====================
static void ws_send_powerLimit(AsyncWebSocketClient* c) {
  StaticJsonDocument<96> doc;
  doc["powerLimit"] = powerLimitValue;
  char out[128];
  size_t n = serializeJson(doc, out, sizeof(out));
  c->text(out, n);
}

static void ws_send_last(AsyncWebSocketClient* c) {
  // last instant = head-1
  int count = ist_full ? MAX_DATA_POINTS : (int)ist_head;
  if (count <= 0) return;

  int last = (ist_head == 0) ? (MAX_DATA_POINTS - 1) : (ist_head - 1);
  char ts[20], tslong[30];
  formatTime_istant(istantPoints[last].timestamp, ts, sizeof(ts));
  formatTime_radio (istantPoints[last].timestamp, tslong, sizeof(tslong));

  StaticJsonDocument<192> doc;
  doc["activePower"] = istantPoints[last].activePower;
  doc["reactivePower"] = istantPoints[last].reactivePower;
  doc["timeDiff"] = istantPoints[last].timeDiff;
  doc["timestamp"] = ts;
  doc["timestampLong"] = tslong;

  char out[256];
  size_t n = serializeJson(doc, out, sizeof(out));
  c->text(out, n);
}

static void ws_broadcast_last() {
  if (ws.count() == 0) return;
  int count = ist_full ? MAX_DATA_POINTS : (int)ist_head;
  if (count <= 0) return;

  int last = (ist_head == 0) ? (MAX_DATA_POINTS - 1) : (ist_head - 1);
  char ts[20], tslong[30];
  formatTime_istant(istantPoints[last].timestamp, ts, sizeof(ts));
  formatTime_radio (istantPoints[last].timestamp, tslong, sizeof(tslong));

  StaticJsonDocument<192> doc;
  doc["activePower"] = istantPoints[last].activePower;
  doc["reactivePower"] = istantPoints[last].reactivePower;
  doc["timeDiff"] = istantPoints[last].timeDiff;
  doc["timestamp"] = ts;
  doc["timestampLong"] = tslong;

  char out[256];
  size_t n = serializeJson(doc, out, sizeof(out));
  ws.textAll(out, n);
}

static void ws_send_instant_array(AsyncWebSocketClient* c) {
  // 31 oggetti -> usa un buffer abbastanza grande
  // JSON compatto: [{"activePower":..,"reactivePower":..,"timeDiff":..,"timestamp":".."},...]
  StaticJsonDocument<8192> doc; // ok per 31 record piccoli
  JsonArray arr = doc.to<JsonArray>();

  int count = ist_full ? MAX_DATA_POINTS : (int)ist_head;
  for (int i = 0; i < MAX_DATA_POINTS; i++) {
    // se non hai ancora dati, manda comunque 31 righe coerenti
    int idx;
    if (count < MAX_DATA_POINTS) {
      idx = i; // ma qui potresti avere timestamp=0; va bene
    } else {
      idx = (ist_head + i) % MAX_DATA_POINTS; // in ordine cronologico
    }

    char ts[20];
    formatTime_istant(istantPoints[idx].timestamp, ts, sizeof(ts));

    JsonObject o = arr.createNestedObject();
    o["activePower"] = istantPoints[idx].activePower;
    o["reactivePower"] = istantPoints[idx].reactivePower;
    o["timeDiff"] = istantPoints[idx].timeDiff;
    o["timestamp"] = ts;
  }

  // stream verso client (evita buffer enormi)
  String out;
  serializeJson(arr, out);
  c->text(out);
}

static void ws_send_hours_array(AsyncWebSocketClient* c) {
  StaticJsonDocument<8192> doc;
  JsonArray arr = doc.to<JsonArray>();

  for (int i = 0; i < MAX_DATA_POINTS; i++) {
    int idx = hr_full ? (hr_head + i) % MAX_DATA_POINTS : i;
    char ts[26];
    formatTime_hours(hoursPoints[idx].timestampH, ts, sizeof(ts));

    JsonObject o = arr.createNestedObject();
    o["activeEnergy"] = hoursPoints[idx].diff_a;
    o["reactiveEnergy"] = hoursPoints[idx].diff_r;
    o["timestamp"] = ts;
  }

  String out;
  serializeJson(arr, out);
  c->text(out);
}

static void ws_send_days_array(AsyncWebSocketClient* c) {
  StaticJsonDocument<8192> doc;
  JsonArray arr = doc.to<JsonArray>();

  for (int i = 0; i < MAX_DATA_POINTS; i++) {
    int idx = dy_full ? (dy_head + i) % MAX_DATA_POINTS : i;
    char ts[26];
    formatTime_days(daysPoints[idx].timestampD, ts, sizeof(ts));

    JsonObject o = arr.createNestedObject();
    o["activeEnergy"] = daysPoints[idx].diff_a;
    o["reactiveEnergy"] = daysPoints[idx].diff_r;
    o["timestamp"] = ts;
  }

  String out;
  serializeJson(arr, out);
  c->text(out);
}

// ====================== HTTP API (fallback) ===========
static void http_send_json(AsyncWebServerRequest* req, const String& payload) {
  AsyncWebServerResponse* r = req->beginResponse(200, "application/json; charset=utf-8", payload);
  // per cache: in LAN puoi anche cacheare Chart/config; per API no
  r->addHeader("Cache-Control", "no-store");
  req->send(r);
}

static void setupHttpApi() {
  server.on("/api/powerLimit", HTTP_GET, [](AsyncWebServerRequest* req){
    StaticJsonDocument<96> doc;
    doc["powerLimit"] = powerLimitValue;
    String out; serializeJson(doc, out);
    http_send_json(req, out);
  });

  server.on("/api/last", HTTP_GET, [](AsyncWebServerRequest* req){
    int count = ist_full ? MAX_DATA_POINTS : (int)ist_head;
    if (count <= 0) { req->send(204); return; }
    int last = (ist_head == 0) ? (MAX_DATA_POINTS - 1) : (ist_head - 1);
    char ts[20], tslong[30];
    formatTime_istant(istantPoints[last].timestamp, ts, sizeof(ts));
    formatTime_radio (istantPoints[last].timestamp, tslong, sizeof(tslong));

    StaticJsonDocument<192> doc;
    doc["activePower"] = istantPoints[last].activePower;
    doc["reactivePower"] = istantPoints[last].reactivePower;
    doc["timeDiff"] = istantPoints[last].timeDiff;
    doc["timestamp"] = ts;
    doc["timestampLong"] = tslong;

    String out; serializeJson(doc, out);
    http_send_json(req, out);
  });

  server.on("/api/instant", HTTP_GET, [](AsyncWebServerRequest* req){
    StaticJsonDocument<8192> doc;
    JsonArray arr = doc.to<JsonArray>();
    int count = ist_full ? MAX_DATA_POINTS : (int)ist_head;

    for (int i = 0; i < MAX_DATA_POINTS; i++) {
      int idx = (count >= MAX_DATA_POINTS) ? ((ist_head + i) % MAX_DATA_POINTS) : i;
      char ts[20];
      formatTime_istant(istantPoints[idx].timestamp, ts, sizeof(ts));
      JsonObject o = arr.createNestedObject();
      o["activePower"] = istantPoints[idx].activePower;
      o["reactivePower"] = istantPoints[idx].reactivePower;
      o["timeDiff"] = istantPoints[idx].timeDiff;
      o["timestamp"] = ts;
    }

    String out; serializeJson(arr, out);
    http_send_json(req, out);
  });

  server.on("/api/hours", HTTP_GET, [](AsyncWebServerRequest* req){
    StaticJsonDocument<8192> doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < MAX_DATA_POINTS; i++) {
      int idx = hr_full ? ((hr_head + i) % MAX_DATA_POINTS) : i;
      char ts[26];
      formatTime_hours(hoursPoints[idx].timestampH, ts, sizeof(ts));
      JsonObject o = arr.createNestedObject();
      o["activeEnergy"] = hoursPoints[idx].diff_a;
      o["reactiveEnergy"] = hoursPoints[idx].diff_r;
      o["timestamp"] = ts;
    }
    String out; serializeJson(arr, out);
    http_send_json(req, out);
  });

  server.on("/api/days", HTTP_GET, [](AsyncWebServerRequest* req){
    StaticJsonDocument<8192> doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < MAX_DATA_POINTS; i++) {
      int idx = dy_full ? ((dy_head + i) % MAX_DATA_POINTS) : i;
      char ts[26];
      formatTime_days(daysPoints[idx].timestampD, ts, sizeof(ts));
      JsonObject o = arr.createNestedObject();
      o["activeEnergy"] = daysPoints[idx].diff_a;
      o["reactiveEnergy"] = daysPoints[idx].diff_r;
      o["timestamp"] = ts;
    }
    String out; serializeJson(arr, out);
    http_send_json(req, out);
  });
}

// ====================== WEBSOCKET EVENT ===============
static void onWsEvent(AsyncWebSocket* server,
                      AsyncWebSocketClient* client,
                      AwsEventType type,
                      void* arg,
                      uint8_t* data,
                      size_t len) {
  if (type == WS_EVT_CONNECT) {
    // opzionale: manda subito timestamp last
    ws_send_last(client);
    return;
  }
  if (type != WS_EVT_DATA) return;

  AwsFrameInfo* info = (AwsFrameInfo*)arg;
  if (!info->final || info->index != 0 || info->len != len) return;
  if (info->opcode != WS_TEXT) return;

  // message text
  String msg;
  msg.reserve(len + 1);
  for (size_t i = 0; i < len; i++) msg += (char)data[i];

  // comandi compatibili con i tuoi attuali
  if (msg == "getPowerData") {
    ws_send_instant_array(client);

  } else if (msg == "getHourEnergy") {
    ws_send_hours_array(client);

  } else if (msg == "getDaysEnergy") {
    ws_send_days_array(client);

  } else if (msg == "getPowerLimit") {
    ws_send_powerLimit(client);

  } else if (msg.startsWith("POWER-LIMIT=")) {
    powerLimitValue = msg.substring(12).toInt();
    prefs.putInt("powLimit", powerLimitValue);
    // inoltra a MultiCatch (se ti serve)
    Serial.println(msg);

  } else if (msg.startsWith("setTime:")) {
    String s = msg.substring(8);
    tm tm{};
    if (sscanf(s.c_str(), "%d-%d-%d %d:%d:%d",
               &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
      tm.tm_year -= 1900;
      tm.tm_mon  -= 1;
      tm.tm_isdst = 0;
      time_t t = mktime(&tm);
      setTime(t);
      client->text("Time updated successfully");
      Serial.println("DATETIME-OK");
    }

  } else if (msg.startsWith("ALARM-TEST")) {
    Serial.println(msg);

  } else if (msg.startsWith("SAVE")) {
    saveData();
  }
}

// ====================== WEB SERVER ====================
static void setupWebServer() {
  // static
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // endpoint “megane” come prima
  server.on("/megane.html", HTTP_GET, [](AsyncWebServerRequest* req){
    String responseText = String(potenza) + "-" + String(ultimoDatoRadioAffidabile);
    req->send(200, "text/plain; charset=utf-8", responseText);
    Serial.println("AUTOmegane");
  });

  // HTTP API fallback
  setupHttpApi();

  // WebSocket on same server/port
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.begin();
}

// ====================== SETUP/LOOP ====================
void setup() {
  Serial.begin(115200);
  while (!Serial) {;}

  // tempo iniziale (poi può essere aggiornato via WS)
  setTime(18, 0, 0, 1, 5, 2024);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
    return;
  }

  // Preferences
  prefs.begin("webnexus", false);
  powerLimitValue = prefs.getInt("powLimit", 3990);

  loadData();

  // se hai un timestamp sensato, impostalo
  // (se timestamp=0, TimeLib resta al setTime sopra)
  // Prendo l’ultimo “con senso”: preferisco last instant se presente
  // (non perfetto ma robusto)
  {
    int count = ist_full ? MAX_DATA_POINTS : (int)ist_head;
    if (count > 0) {
      int last = (ist_head == 0) ? (MAX_DATA_POINTS - 1) : (ist_head - 1);
      if (istantPoints[last].timestamp > 1700000000) { // sanity ~ 2023+
        setTime(istantPoints[last].timestamp);
      }
    }
  }

  setupWiFiAP();
  setupWebServer();

  lastSaveTime = millis();
}

void loop() {
  // AsyncWebServer non richiede loop, ma ws cleanup sì
  ws.cleanupClients();

  if (radioCheck.expired()) {
    setAffidabilitaDato(false);
  }

  if (millis() - lastSaveTime > SAVE_INTERVAL_MS) {
    saveData();
    lastSaveTime = millis();
  }

  if (serialdatapacket_ready()) {
    setAffidabilitaDato(true);
    radioCheck.start();

    DataPacket p = read_serialdatapacket();

    ring_push_hours(p.activeDiff, p.reactiveDiff);
    ring_push_days (p.activeDiff, p.reactiveDiff);
    ring_push_instant(p.activeDiff, p.reactiveDiff, p.timeDiff);

    ws_broadcast_last();
  }
}
