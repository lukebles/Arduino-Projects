/*
  WebNexus ESP32 - ESP32 core 3.3.7 ready
  VERSIONE RADIO DIRETTA

  Mantiene la logica WEB del progetto ESP32 originale.
  Sostituisce soltanto la sorgente dati:
    prima: Seriale con DataPacket già pronto
    ora:   Radio -> contatori cumulativi -> diff -> stessa logica originale


Board: ESP32 Dev Module
Core ESP32: 3.3.7
Upload Speed: 921600 oppure 460800 se dà problemi
CPU Frequency: 240 MHz
Flash Frequency: 80 MHz
Flash Mode: QIO
Flash Size: 4MB
Partition Scheme: Default 4MB with spiffs oppure una partizione con almeno 1MB per filesystem
PSRAM: Disabled
Arduino Runs On: Core 1
Events Run On: Core 1
Erase All Flash Before Sketch Upload: Enabled solo al primo caricamento o se hai problemi

ESP32 core 3.3.7
AsyncTCP
ESPAsyncWebServer
ArduinoJson
TimeLib
LkMultivibrator
LkRadioStructure_RH
RadioHead / RH_ASK, se richiesto dalla tua LkRadioStructure_RH.h
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
#include "LkRadioStructure_RH.h"

#include <memory>
#include <math.h>
#include <time.h> // la data/ora la prende da internet

#include "config.h"

// ====================== CONFIG ======================

#define DEBUG 1   // 0 = debug spento, 1 = debug acceso

#if DEBUG
  #define dbgBegin(baud) Serial.begin(baud)
  #define prt(...)       Serial.print(__VA_ARGS__)
  #define prtn(...)      Serial.println(__VA_ARGS__)
  #define prtf(...)      Serial.printf(__VA_ARGS__)
#else
  #define dbgBegin(baud)
  #define prt(...)
  #define prtn(...)
  #define prtf(...)
#endif

#ifndef LED_BUILTIN
  #define LED_BUILTIN 2
#endif

static const uint32_t WIFI_RETRY_INTERVAL_MS = 10000;
static uint32_t lastWiFiRetry = 0;
static bool wifiWasConnected = false;


static const uint32_t RADIO_TIMEOUT_MS = 27000;
static const uint32_t SAVE_INTERVAL_MS = 8UL * 60UL * 60UL * 1000UL; // 8 ore
static const uint32_t WS_CLEANUP_MS    = 5000;

static const int MAX_DATA_POINTS = 31;
static const uint8_t ID_ENERGYSEND = 0x0B;

// ====================== RADIO =======================
struct DummyPayload {
  uint8_t dummy;
};

using RxRadio = LkRadioStructure<DummyPayload>;
RxRadio radio;

// ====================== DATA STRUCTURES =============
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

static uint32_t lastSaveTime   = 0;
static uint32_t lastWsCleanup  = 0;

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

#define WNX_MAGIC 0x31584E57UL  // 'WNX1' little-endian

// ===== stato contatori cumulativi, come su ESP01s =====
static uint32_t prevTime = 0;
static uint16_t prevActiveCount = 0;
static uint16_t prevReactiveCount = 0;

// ====================== HELPERS ======================
static float roundToTens(float v) {
  return roundf(v / 10.0f) * 10.0f;
}

static const char* wday3(int wday) {
  static const char* a[] = {"DOM","LUN","MAR","MER","GIO","VEN","SAB"};
  if (wday < 1 || wday > 7) return "---";
  return a[wday - 1];
}

static const char* mon3(int m) {
  static const char* a[] = {"GEN","FEB","MAR","APR","MAG","GIU","LUG","AGO","SET","OTT","NOV","DIC"};
  if (m < 1 || m > 12) return "---";
  return a[m - 1];
}

static void formatTime_radio(time_t t, char* b, size_t n) {
  if (t == 0) {
    snprintf(b, n, "--/--/---- --:--:--");
    return;
  }
  snprintf(b, n, "%s %02d/%02d/%04d %02d:%02d:%02d",
           wday3(weekday(t)), day(t), month(t), year(t), hour(t), minute(t), second(t));
}

static void formatTime_istant(time_t t, char* b, size_t n) {
  if (t == 0) {
    snprintf(b, n, "--:--:--");
    return;
  }
  snprintf(b, n, "%02d:%02d:%02d", hour(t), minute(t), second(t));
}

static void formatTime_hours(time_t t, char* b, size_t n) {
  if (t == 0) {
    snprintf(b, n, "--/--/---- --:--");
    return;
  }
  snprintf(b, n, "%s %02d/%02d/%04d %02d:%02d",
           wday3(weekday(t)), day(t), month(t), year(t), hour(t), 0);
}

static void formatTime_days(time_t t, char* b, size_t n) {
  if (t == 0) {
    snprintf(b, n, "-- --- ---- ---");
    return;
  }
  snprintf(b, n, "%02d %s %04d %s", day(t), mon3(month(t)), year(t), wday3(weekday(t)));
}

static void setAffidabilitaDato(bool v) { ultimoDatoRadioAffidabile = v; }
static void setPotenza(int v) { potenza = v; }

static bool setupTimeFromInternet() {
  // Italia: CET/CEST automatico
  configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3",
               "pool.ntp.org",
               "time.google.com",
               "time.cloudflare.com");

  prt("Sincronizzazione ora NTP");

  struct tm timeinfo;
  uint32_t start = millis();

  while (!getLocalTime(&timeinfo) && millis() - start < 15000) {
    delay(500);
    prt(".");
  }

  prtn();

  if (!getLocalTime(&timeinfo)) {
    prtn("NTP fallito");
    return false;
  }

  time_t t = mktime(&timeinfo);
  setTime(t);

  prt("Ora NTP impostata: ");
  prtn(asctime(&timeinfo));

  return true;
}

// ====================== WIFI (AP) ====================
static int findBestChannel(int n) {
  int channels[13] = {0};

  for (int i = 0; i < n; i++) {
    int ch = WiFi.channel(i);
    if (ch >= 1 && ch <= 13) {
      channels[ch - 1]++;
    }
  }

  int best = 1;
  int minN = channels[0];

  for (int i = 1; i < 13; i++) {
    if (channels[i] < minN) {
      minN = channels[i];
      best = i + 1;
    }
  }

  prtf("Best channel: %d (nets=%d)\n", best, minN);
  return best;
}

static void handleWiFiConnection() {
  wl_status_t st = WiFi.status();

  if (st == WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, HIGH);

    if (!wifiWasConnected) {
      wifiWasConnected = true;
      prtn("WiFi connesso");
      prt("IP ESP32: ");
      prtn(WiFi.localIP());
    }

    return;
  }

  digitalWrite(LED_BUILTIN, LOW);

  if (wifiWasConnected) {
    wifiWasConnected = false;
    prtn("WiFi disconnesso");
  }

  if (millis() - lastWiFiRetry >= WIFI_RETRY_INTERVAL_MS) {
    lastWiFiRetry = millis();

    prt("Ritento connessione WiFi a: ");
    prtn(WIFI_SSID);

    WiFi.disconnect(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

static void setupWiFiSTA() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  prt("Avvio connessione WiFi a: ");
  prtn(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWiFiRetry = millis();
}


// ====================== RING BUFFERS ==================
static void ring_push_instant(uint16_t diff_a, uint16_t diff_r, uint32_t timediff_ms) {
  float dt_s = (float)timediff_ms / 1000.0f;
  if (dt_s <= 0.001f) return;

  float activePower   = roundToTens((diff_a * 3600.0f) / dt_s);
  float reactivePower = roundToTens((diff_r * 3600.0f) / dt_s);

  if (activePower < 0) activePower = 0;
  if (reactivePower < 0) reactivePower = 0;
  if (activePower > 65535.0f) activePower = 65535.0f;
  if (reactivePower > 65535.0f) reactivePower = 65535.0f;

  istantPoints[ist_head].activePower   = (uint16_t)activePower;
  istantPoints[ist_head].reactivePower = (uint16_t)reactivePower;
  istantPoints[ist_head].timestamp     = now();
  istantPoints[ist_head].timeDiff      = timediff_ms;

  setPotenza((int)activePower);

  ist_head++;
  if (ist_head >= MAX_DATA_POINTS) {
    ist_head = 0;
    ist_full = true;
  }
}

static void ring_push_hours(uint16_t diff_a, uint16_t diff_r) {
  time_t cnow = now();
  int cy = year(cnow), cm = month(cnow), cd = day(cnow), ch = hour(cnow);

  int count = hr_full ? MAX_DATA_POINTS : hr_head;
  for (int i = 0; i < count; i++) {
    int idx = hr_full ? (hr_head + i) % MAX_DATA_POINTS : i;
    time_t t = hoursPoints[idx].timestampH;

    if (t != 0 && year(t) == cy && month(t) == cm && day(t) == cd && hour(t) == ch) {
      hoursPoints[idx].diff_a += diff_a;
      hoursPoints[idx].diff_r += diff_r;
      return;
    }
  }

  hoursPoints[hr_head].timestampH = cnow;
  hoursPoints[hr_head].diff_a = diff_a;
  hoursPoints[hr_head].diff_r = diff_r;

  hr_head++;
  if (hr_head >= MAX_DATA_POINTS) {
    hr_head = 0;
    hr_full = true;
  }
}

static void ring_push_days(uint16_t diff_a, uint16_t diff_r) {
  time_t cnow = now();
  int cy = year(cnow), cm = month(cnow), cd = day(cnow);

  int count = dy_full ? MAX_DATA_POINTS : dy_head;
  for (int i = 0; i < count; i++) {
    int idx = dy_full ? (dy_head + i) % MAX_DATA_POINTS : i;
    time_t t = daysPoints[idx].timestampD;

    if (t != 0 && year(t) == cy && month(t) == cm && day(t) == cd) {
      daysPoints[idx].diff_a += diff_a;
      daysPoints[idx].diff_r += diff_r;
      return;
    }
  }

  daysPoints[dy_head].timestampD = cnow;
  daysPoints[dy_head].diff_a = diff_a;
  daysPoints[dy_head].diff_r = diff_r;

  dy_head++;
  if (dy_head >= MAX_DATA_POINTS) {
    dy_head = 0;
    dy_full = true;
  }
}

// ====================== RADIO INPUT ===================
static bool decodeEnergyCounts(const uint8_t* data, uint8_t len,
                               uint16_t& activeCount,
                               uint16_t& reactiveCount) {
  if (len < 5) return false;

  // Ultimi 5 byte: countR hi/lo, countA hi/lo, sender
  const uint8_t* p = data + (len - 5);
  uint8_t sender = p[4];

  if (sender != ID_ENERGYSEND) return false;

  reactiveCount = ((uint16_t)p[0] << 8) | p[1];
  activeCount   = ((uint16_t)p[2] << 8) | p[3];

  return true;
}

static void processEnergyCounts(uint16_t activeCount, uint16_t reactiveCount) {
  uint32_t currentTime = millis();

  if (prevTime != 0) {
    uint32_t timeDiff = currentTime - prevTime;
    uint16_t activeDiff = activeCount - prevActiveCount;
    uint16_t reactiveDiff = reactiveCount - prevReactiveCount;

    prt("activeCount=");
    prt(activeCount);
    prt(" reactiveCount=");
    prt(reactiveCount);
    prt(" activeDiff=");
    prt(activeDiff);
    prt(" reactiveDiff=");
    prt(reactiveDiff);
    prt(" timeDiff=");
    prtn(timeDiff);

    // stessa logica ESP01s
    if (activeDiff < 3600) {
      if (timeDiff < 3600000) {
        setAffidabilitaDato(true);
        radioCheck.start();

        ring_push_hours(activeDiff, reactiveDiff);
        ring_push_days(activeDiff, reactiveDiff);
        ring_push_instant(activeDiff, reactiveDiff, timeDiff);

        ws_broadcast_last();
      }
    }
  }

  prevTime = currentTime;
  prevActiveCount = activeCount;
  prevReactiveCount = reactiveCount;
}

// ====================== PERSISTENCE ===================
struct DataFileHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  uint32_t payloadLen;
  uint32_t checksum;
};

static uint32_t simpleChecksum(const uint8_t* p, size_t n) {
  uint32_t s = 0;
  for (size_t i = 0; i < n; i++) {
    s = (s + p[i]) * 1664525u + 1013904223u;
  }
  return s;
}

struct Payload {
  DataInstant     inst[MAX_DATA_POINTS];
  DataEnergyHours hours[MAX_DATA_POINTS];
  DataEnergyDays  days[MAX_DATA_POINTS];
  uint8_t ist_head; bool ist_full;
  uint8_t hr_head;  bool hr_full;
  uint8_t dy_head;  bool dy_full;
};

static void saveData() {
  Payload payload{};

  memcpy(payload.inst,  istantPoints, sizeof(istantPoints));
  memcpy(payload.hours, hoursPoints,  sizeof(hoursPoints));
  memcpy(payload.days,  daysPoints,   sizeof(daysPoints));

  payload.ist_head = ist_head; payload.ist_full = ist_full;
  payload.hr_head  = hr_head;  payload.hr_full  = hr_full;
  payload.dy_head  = dy_head;  payload.dy_full  = dy_full;

  const uint8_t* pb = reinterpret_cast<const uint8_t*>(&payload);
  const uint32_t plen = (uint32_t)sizeof(payload);

  DataFileHeader h{};
  h.magic = WNX_MAGIC;
  h.version = 1;
  h.reserved = 0;
  h.payloadLen = plen;
  h.checksum = simpleChecksum(pb, plen);

  File f = LittleFS.open("/data.bin", "w");
  if (!f) {
    prtn("saveData: open fail");
    return;
  }

  size_t w1 = f.write(reinterpret_cast<const uint8_t*>(&h), sizeof(h));
  size_t w2 = f.write(pb, plen);
  f.close();

  if (w1 != sizeof(h) || w2 != plen) {
    prtn("saveData: short write");
    return;
  }

  prtn("Data saved");
}

static void loadData() {
  File f = LittleFS.open("/data.bin", "r");
  if (!f) {
    prtn("loadData: missing");
    return;
  }

  DataFileHeader h{};
  if (f.read(reinterpret_cast<uint8_t*>(&h), sizeof(h)) != sizeof(h)) {
    prtn("loadData: bad header");
    f.close();
    return;
  }

  if (h.magic != WNX_MAGIC || h.version != 1) {
    prtn("loadData: wrong magic/version");
    f.close();
    return;
  }

  if (h.payloadLen != sizeof(Payload)) {
    prtn("loadData: payload size mismatch");
    f.close();
    return;
  }

  std::unique_ptr<uint8_t[]> buf(new uint8_t[h.payloadLen]);
  if (!buf) {
    prtn("loadData: oom");
    f.close();
    return;
  }

  int r = f.read(buf.get(), h.payloadLen);
  f.close();

  if (r != (int)h.payloadLen) {
    prtn("loadData: short read");
    return;
  }

  uint32_t cs = simpleChecksum(buf.get(), h.payloadLen);
  if (cs != h.checksum) {
    prtn("loadData: checksum mismatch");
    return;
  }

  Payload* p = reinterpret_cast<Payload*>(buf.get());

  memcpy(istantPoints, p->inst,  sizeof(istantPoints));
  memcpy(hoursPoints,  p->hours, sizeof(hoursPoints));
  memcpy(daysPoints,   p->days,  sizeof(daysPoints));

  ist_head = p->ist_head; ist_full = p->ist_full;
  hr_head  = p->hr_head;  hr_full  = p->hr_full;
  dy_head  = p->dy_head;  dy_full  = p->dy_full;

  prtn("Data loaded");
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
  int count = ist_full ? MAX_DATA_POINTS : (int)ist_head;
  if (count <= 0) return;

  int last = (ist_head == 0) ? (MAX_DATA_POINTS - 1) : (ist_head - 1);

  char ts[20], tslong[32];
  formatTime_istant(istantPoints[last].timestamp, ts, sizeof(ts));
  formatTime_radio(istantPoints[last].timestamp, tslong, sizeof(tslong));

  StaticJsonDocument<192> doc;
  doc["activePower"]   = istantPoints[last].activePower;
  doc["reactivePower"] = istantPoints[last].reactivePower;
  doc["timeDiff"]      = istantPoints[last].timeDiff;
  doc["timestamp"]     = ts;
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

  char ts[20], tslong[32];
  formatTime_istant(istantPoints[last].timestamp, ts, sizeof(ts));
  formatTime_radio(istantPoints[last].timestamp, tslong, sizeof(tslong));

  StaticJsonDocument<192> doc;
  doc["activePower"]   = istantPoints[last].activePower;
  doc["reactivePower"] = istantPoints[last].reactivePower;
  doc["timeDiff"]      = istantPoints[last].timeDiff;
  doc["timestamp"]     = ts;
  doc["timestampLong"] = tslong;

  char out[256];
  size_t n = serializeJson(doc, out, sizeof(out));
  ws.textAll(out, n);
}

static void ws_send_instant_array(AsyncWebSocketClient* c) {
  StaticJsonDocument<8192> doc;
  JsonArray arr = doc.to<JsonArray>();

  int count = ist_full ? MAX_DATA_POINTS : (int)ist_head;

  for (int i = 0; i < MAX_DATA_POINTS; i++) {
    int idx;
    if (count < MAX_DATA_POINTS) {
      idx = i;
    } else {
      idx = (ist_head + i) % MAX_DATA_POINTS;
    }

    char ts[20];
    formatTime_istant(istantPoints[idx].timestamp, ts, sizeof(ts));

    JsonObject o = arr.createNestedObject();
    o["activePower"]   = istantPoints[idx].activePower;
    o["reactivePower"] = istantPoints[idx].reactivePower;
    o["timeDiff"]      = istantPoints[idx].timeDiff;
    o["timestamp"]     = ts;
  }

  String out;
  out.reserve(4096);
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
    o["activeEnergy"]   = hoursPoints[idx].diff_a;
    o["reactiveEnergy"] = hoursPoints[idx].diff_r;
    o["timestamp"]      = ts;
  }

  String out;
  out.reserve(4096);
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
    o["activeEnergy"]   = daysPoints[idx].diff_a;
    o["reactiveEnergy"] = daysPoints[idx].diff_r;
    o["timestamp"]      = ts;
  }

  String out;
  out.reserve(4096);
  serializeJson(arr, out);
  c->text(out);
}

// ====================== HTTP API ======================
static void http_send_json(AsyncWebServerRequest* req, const String& payload) {
  AsyncWebServerResponse* r = req->beginResponse(200, "application/json; charset=utf-8", payload);
  r->addHeader("Cache-Control", "no-store");
  req->send(r);
}

static void setupHttpApi() {
  server.on("/api/powerLimit", HTTP_GET, [](AsyncWebServerRequest* req) {
    StaticJsonDocument<96> doc;
    doc["powerLimit"] = powerLimitValue;
    String out;
    serializeJson(doc, out);
    http_send_json(req, out);
  });

  server.on("/api/last", HTTP_GET, [](AsyncWebServerRequest* req) {
    int count = ist_full ? MAX_DATA_POINTS : (int)ist_head;
    if (count <= 0) {
      req->send(204);
      return;
    }

    int last = (ist_head == 0) ? (MAX_DATA_POINTS - 1) : (ist_head - 1);

    char ts[20], tslong[32];
    formatTime_istant(istantPoints[last].timestamp, ts, sizeof(ts));
    formatTime_radio(istantPoints[last].timestamp, tslong, sizeof(tslong));

    StaticJsonDocument<192> doc;
    doc["activePower"]   = istantPoints[last].activePower;
    doc["reactivePower"] = istantPoints[last].reactivePower;
    doc["timeDiff"]      = istantPoints[last].timeDiff;
    doc["timestamp"]     = ts;
    doc["timestampLong"] = tslong;

    String out;
    serializeJson(doc, out);
    http_send_json(req, out);
  });

  server.on("/api/instant", HTTP_GET, [](AsyncWebServerRequest* req) {
    StaticJsonDocument<8192> doc;
    JsonArray arr = doc.to<JsonArray>();

    int count = ist_full ? MAX_DATA_POINTS : (int)ist_head;

    for (int i = 0; i < MAX_DATA_POINTS; i++) {
      int idx = (count >= MAX_DATA_POINTS) ? ((ist_head + i) % MAX_DATA_POINTS) : i;

      char ts[20];
      formatTime_istant(istantPoints[idx].timestamp, ts, sizeof(ts));

      JsonObject o = arr.createNestedObject();
      o["activePower"]   = istantPoints[idx].activePower;
      o["reactivePower"] = istantPoints[idx].reactivePower;
      o["timeDiff"]      = istantPoints[idx].timeDiff;
      o["timestamp"]     = ts;
    }

    String out;
    serializeJson(arr, out);
    http_send_json(req, out);
  });

  server.on("/api/hours", HTTP_GET, [](AsyncWebServerRequest* req) {
    StaticJsonDocument<8192> doc;
    JsonArray arr = doc.to<JsonArray>();

    for (int i = 0; i < MAX_DATA_POINTS; i++) {
      int idx = hr_full ? ((hr_head + i) % MAX_DATA_POINTS) : i;

      char ts[26];
      formatTime_hours(hoursPoints[idx].timestampH, ts, sizeof(ts));

      JsonObject o = arr.createNestedObject();
      o["activeEnergy"]   = hoursPoints[idx].diff_a;
      o["reactiveEnergy"] = hoursPoints[idx].diff_r;
      o["timestamp"]      = ts;
    }

    String out;
    serializeJson(arr, out);
    http_send_json(req, out);
  });

  server.on("/api/days", HTTP_GET, [](AsyncWebServerRequest* req) {
    StaticJsonDocument<8192> doc;
    JsonArray arr = doc.to<JsonArray>();

    for (int i = 0; i < MAX_DATA_POINTS; i++) {
      int idx = dy_full ? ((dy_head + i) % MAX_DATA_POINTS) : i;

      char ts[26];
      formatTime_days(daysPoints[idx].timestampD, ts, sizeof(ts));

      JsonObject o = arr.createNestedObject();
      o["activeEnergy"]   = daysPoints[idx].diff_a;
      o["reactiveEnergy"] = daysPoints[idx].diff_r;
      o["timestamp"]      = ts;
    }

    String out;
    serializeJson(arr, out);
    http_send_json(req, out);
  });
}

// ====================== WS EVENT ======================
static void onWsEvent(AsyncWebSocket* serverPtr,
                      AsyncWebSocketClient* client,
                      AwsEventType type,
                      void* arg,
                      uint8_t* data,
                      size_t len) {
  (void)serverPtr;

  if (type == WS_EVT_CONNECT) {
    ws_send_last(client);
    return;
  }

  if (type != WS_EVT_DATA) return;

  AwsFrameInfo* info = reinterpret_cast<AwsFrameInfo*>(arg);
  if (!info->final || info->index != 0 || info->len != len) return;
  if (info->opcode != WS_TEXT) return;

  String msg;
  msg.reserve(len);
  msg.concat(reinterpret_cast<const char*>(data), len);

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
    prtn(msg);

  } else if (msg.startsWith("setTime:")) {
    String s = msg.substring(8);

    tm tmv{};
    if (sscanf(s.c_str(), "%d-%d-%d %d:%d:%d",
               &tmv.tm_year, &tmv.tm_mon, &tmv.tm_mday,
               &tmv.tm_hour, &tmv.tm_min, &tmv.tm_sec) == 6) {
      tmv.tm_year -= 1900;
      tmv.tm_mon  -= 1;
      tmv.tm_isdst = 0;

      time_t t = mktime(&tmv);
      if (t > 0) {
        setTime(t);
        client->text("Time updated successfully");
        prtn("DATETIME-OK");
      }
    }

  } else if (msg.startsWith("ALARM-TEST")) {
    prtn(msg);

  } else if (msg == "SAVE") {
    saveData();
  }
}

// ====================== WEB SERVER ====================
static void setupWebServer() {
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  server.on("/megane.html", HTTP_GET, [](AsyncWebServerRequest* req) {
    String responseText = String(potenza) + "-" + String(ultimoDatoRadioAffidabile ? 1 : 0);
    req->send(200, "text/plain; charset=utf-8", responseText);
    prtn("AUTOmegane");
  });

  setupHttpApi();

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.begin();
}

// ====================== SETUP/LOOP ====================
void setup() {
  dbgBegin(9600);

  uint32_t serialWaitStart = millis();
  while (!Serial && (millis() - serialWaitStart < 2000)) {
    delay(10);
  }

  setupWiFiSTA();

  if (!setupTimeFromInternet()) {
    setTime(18, 0, 0, 1, 5, 2024);

    int count = ist_full ? MAX_DATA_POINTS : (int)ist_head;
    if (count > 0) {
      int last = (ist_head == 0) ? (MAX_DATA_POINTS - 1) : (ist_head - 1);
      if (istantPoints[last].timestamp > 1700000000) {
        setTime(istantPoints[last].timestamp);
      }
    }
  }

  setupWebServer();

  if (!LittleFS.begin(true)) {
    prtn("LittleFS mount failed");
    return;
  }

  prefs.begin("webnexus", false);
  powerLimitValue = prefs.getInt("powLimit", 3990);

  loadData();

  {
    int count = ist_full ? MAX_DATA_POINTS : (int)ist_head;
    if (count > 0) {
      int last = (ist_head == 0) ? (MAX_DATA_POINTS - 1) : (ist_head - 1);
      if (istantPoints[last].timestamp > 1700000000) {
        setTime(istantPoints[last].timestamp);
      }
    }
  }

  setupWiFiSTA();
  setupWebServer();

  RxRadio::globalSetup(
    2000,
    22,
    21,
    23,
    false
  );

  prtn("Radio in ascolto...");

  lastSaveTime = millis();
  lastWsCleanup = millis();
}

void loop() {
  handleWiFiConnection();

  if (millis() - lastWsCleanup >= WS_CLEANUP_MS) {
    ws.cleanupClients();
    lastWsCleanup = millis();
  }

  if (radioCheck.expired()) {
    setAffidabilitaDato(false);
  }

  if (millis() - lastSaveTime > SAVE_INTERVAL_MS) {
    saveData();
    lastSaveTime = millis();
  }

  if (radio.haveRawMessage()) {
    uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
    uint8_t len = 0;

    radio.getRawBuffer(buf, len);

    prt("RAW len=");
    prt(len);
    prt(" HEX=");
    for (uint8_t i = 0; i < len; i++) {
      if (buf[i] < 0x10) prt('0');
      prt(buf[i], HEX);
      prt(' ');
    }
    prtn();

    uint16_t activeCount = 0;
    uint16_t reactiveCount = 0;

    if (decodeEnergyCounts(buf, len, activeCount, reactiveCount)) {
      processEnergyCounts(activeCount, reactiveCount);
    }
  }

  delay(1);
}