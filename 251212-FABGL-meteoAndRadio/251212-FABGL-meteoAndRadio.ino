/*
  FabGL WiFi + Meteo + Dati Radio via Serial + ASCII test (no flicker)

  - Connessione WiFi usando WIFI_SSID / WIFI_PASSWORD da config.h (timeout 1 minuto)
  - Sync ora via NTP (Europa/Roma)
  - Fetch meteo (ReggioEmiliaMeteo)
  - Ricezione dati via SERIAL da Arduino UNO (che fa da ponte RadioHead)
    Formato: linea di coppie esadecimali, es. "0B12345663\n"
  - Mostra:
      Riga 0               : data/ora locale
      Riga 1-?             : WiFi, URL meteo, ultimo accesso, meteo
      Riga RADIO_START_ROW : sezione "Dati radio 433 MHz"
                              - Potenza attiva
                              - Potenza reattiva
                              - Caldaia 1
                              - Caldaia 2
                              - Temperatura
                              - Umidità
                              - Pressione
        Ogni riga mostra valore, data/ora ultima ricezione e Δms fra le ultime due
      Ultime ~16 righe     : tabella ASCII 0x00–0xFF (codice hex + carattere)

- ATTIVA     : XXXXX (cont) [AAAA-MM-GG HH:MM:SS, JJJJJJ ms]
- REATTIVA   : XXXXX (cont) [AAAA-MM-GG HH:MM:SS, JJJJJJ ms]
- CALDAIA 1  : XXXXX (lumi) [AAAA-MM-GG HH:MM:SS, JJJJJJ ms]
- CALDAIA 2  : XXXXX (cont) [AAAA-MM-GG HH:MM:SS, JJJJJJ ms]
- TEMPERATURA: XXXXX C      [AAAA-MM-GG HH:MM:SS, JJJJJJ ms]
- UMIDITA    : XXXXX %      [AAAA-MM-GG HH:MM:SS, JJJJJJ ms]
- PRESSIONE  : XXXXX hPa    [AAAA-MM-GG HH:MM:SS, JJJJJJ ms]
*/

#include <Arduino.h>
#include "fabgl.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include "config.h"   // definisce WIFI_SSID e WIFI_PASSWORD
#include <string.h>
#include <ctype.h>
#include <math.h>

// =====================
//  Oggetti FabGL
// =====================

fabgl::VGATextController DisplayController;
fabgl::PS2Controller     PS2Controller;
fabgl::Terminal          Terminal;

// =====================
//  Serial2 per dati radio (da Arduino UNO)
// =====================

HardwareSerial RadioSerial(2);  // UART2

const int UART_RX_PIN = 27;  // collegato al TX di Arduino UNO
const int UART_TX_PIN = 26;  // non usato, ma richiesto da begin()

// Buffer per accumulare la linea esadecimale
String radioLineBuffer;

// =====================
//  NTP / Timezone
// =====================

const char* TZ_EUROPE_ROME = "CET-1CEST,M3.5.0/2,M10.5.0/3";

void setupTime()
{
  configTzTime(TZ_EUROPE_ROME, "pool.ntp.org", "time.google.com");
}

// Restituisce "YYYY-MM-DD HH:MM:SS" o "sconosciuto"
String getNowDateTimeString() {
  struct tm ti;
  if (getLocalTime(&ti, 200)) {
    char buf[32];
    int anno = 1900 + ti.tm_year;
    snprintf(buf, sizeof(buf),
             "%04d-%02d-%02d %02d:%02d:%02d",
             anno, ti.tm_mon + 1, ti.tm_mday,
             ti.tm_hour, ti.tm_min, ti.tm_sec);
    return String(buf);
  }
  return "sconosciuto";
}

// =====================
//  Meteo (HTTPS)
// =====================

const char* METEO_URL  = "https://www.reggioemiliameteo.it/stazioni/tab-live/stazioni-meteo-tab-live-20.html";
const uint32_t METEO_REFRESH_MS = 10UL * 60UL * 1000UL; // 10 minuti

String meteoTemp     = "";
String meteoUmid     = "";
String meteoPress    = "";
String meteoVento    = "";
String meteoPioggia  = "";
String lastAccessStr = "mai";

bool   meteoOk            = false;
unsigned long lastMeteoFetch    = 0;
unsigned long lastTimeUpdate    = 0;

// =====================
//  Colori di default (info/meteo)
// =====================

void setDefaultInfoColors() {
  Terminal.setForegroundColor(Color::White);
  Terminal.setBackgroundColor(Color::Black);
}

// =====================
//  Connessione WiFi
// =====================

void connectWiFiFromConfig()
{
  Terminal.write("\r\nConnessione WiFi in corso...\r\n");

  WiFi.disconnect(true, true);
  delay(200);
  WiFi.mode(WIFI_STA);

  Terminal.printf("SSID: %s\r\n", WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // 1 minuto di tempo: 120 tentativi * 500 ms = 60 s
  int maxTentativi = 120;
  while (WiFi.status() != WL_CONNECTED && maxTentativi-- > 0) {
    Terminal.write(".");
    Terminal.flush();
    delay(500);
  }

  Terminal.write("\r\n");

  if (WiFi.status() == WL_CONNECTED) {
    Terminal.write("connesso\r\n");
    Terminal.printf("IP: %s\r\n", WiFi.localIP().toString().c_str());
  } else {
    Terminal.write("connessione fallita\r\n");
  }
}

// =====================
//  Helper parsing HTML meteo
// =====================

static String between(const String& s, const String& open, const String& close, int from = 0) {
  int a = s.indexOf(open, from); if (a < 0) return "";
  a += open.length();
  int b = s.indexOf(close, a);   if (b < 0) return "";
  return s.substring(a, b);
}

static String kthTagText(const String& scope, const String& tag, int k) {
  int pos = 0, count = 0;
  while (true) {
    int start = scope.indexOf("<" + tag, pos);
    if (start < 0) return "";
    int gt = scope.indexOf('>', start);
    if (gt < 0) return "";
    int end = scope.indexOf("</" + tag + ">", gt + 1);
    if (end < 0) return "";
    count++;
    if (count == k) {
      return scope.substring(gt + 1, end);
    }
    pos = end + tag.length() + 3;
  }
}

// Estrae testo da tr[trIndex]/td[3]/(h5 oppure span[2]/small)
static String extractFromRow(const String& tableHtml, int trIndex, int mode) {
  int pos = 0;
  String trBlock;
  for (int i = 1; i <= trIndex; ++i) {
    int trStart = tableHtml.indexOf("<tr", pos);
    if (trStart < 0) return "";
    int trStartEnd = tableHtml.indexOf('>', trStart);
    if (trStartEnd < 0) return "";
    int trEnd = tableHtml.indexOf("</tr>", trStartEnd + 1);
    if (trEnd < 0) return "";
    trBlock = tableHtml.substring(trStartEnd + 1, trEnd);
    pos = trEnd + 5;
  }

  int tdPos = 0, tdCount = 0;
  String td3;
  while (true) {
    int tdStart = trBlock.indexOf("<td", tdPos);
    if (tdStart < 0) break;
    int tdStartEnd = trBlock.indexOf('>', tdStart);
    if (tdStartEnd < 0) break;
    int tdEnd = trBlock.indexOf("</td>", tdStartEnd + 1);
    if (tdEnd < 0) break;
    tdCount++;
    if (tdCount == 3) { td3 = trBlock.substring(tdStartEnd + 1, tdEnd); break; }
    tdPos = tdEnd + 5;
  }
  if (td3.isEmpty()) return "";

  if (mode == 0) {
    String t = kthTagText(td3, "h5", 1);
    t.trim();
    return t;
  } else {
    String span2 = kthTagText(td3, "span", 2);
    if (span2.isEmpty()) return "";
    String small = kthTagText(span2, "small", 1);
    small.trim();
    return small;
  }
}

static String sanitize(String s, bool removeDeg = false) {
  s.replace("&nbsp;", " ");
  if (removeDeg) s.replace("&deg;", "");
  while (true) {
    int a = s.indexOf('<');
    if (a < 0) break;
    int b = s.indexOf('>', a);
    if (b < 0) break;
    s.remove(a, b - a + 1);
  }
  s.trim();
  return s;
}

static String firstNumberWithDecimal(String s) {
  s.replace('\n', ' ');
  s.replace('\r', ' ');
  s.replace('\t', ' ');
  s.replace("&nbsp;", " ");

  String out;
  bool started = false;
  bool hasSep  = false;

  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];

    if (!started) {
      if (c == '+' || c == '-' || (c >= '0' && c <= '9')) {
        started = true;
        out += c;
      }
    } else {
      if (c >= '0' && c <= '9') {
        out += c;
      } else if ((c == '.' || c == ',') && !hasSep) {
        hasSep = true;
        out += '.';
      } else {
        break;
      }
    }
  }

  return out;
}

// =====================
//  Fetch meteo
// =====================

bool fetchMeteo()
{
  if (WiFi.status() != WL_CONNECTED)
    return false;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  http.setConnectTimeout(10000);

  http.begin(client, METEO_URL);
  http.setUserAgent("ESP32/Arduino");
  http.addHeader("Accept", "text/html,application/xhtml+xml");
  http.addHeader("Accept-Encoding", "identity");
  http.addHeader("Connection", "close");

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  String html = http.getString();
  http.end();

  String table = between(html, "<table", "</table>");
  if (table.isEmpty())
    return false;

  int gt = table.indexOf('>');
  if (gt >= 0)
    table = table.substring(gt + 1);

  String v1 = extractFromRow(table, 1, 0); // Temperatura
  String v2 = extractFromRow(table, 2, 0); // Umidita
  String v3 = extractFromRow(table, 3, 1); // Vento
  String v4 = extractFromRow(table, 4, 1); // Pioggia
  String v5 = extractFromRow(table, 5, 0); // Pressione

  v1 = sanitize(v1, true);
  v2 = sanitize(v2, false);
  v3 = sanitize(v3, false);
  v4 = sanitize(v4, false);
  v5 = sanitize(v5, false);

  String tStr = firstNumberWithDecimal(v1);
  String uStr = firstNumberWithDecimal(v2);
  String vStr = firstNumberWithDecimal(v3);
  String pmm  = firstNumberWithDecimal(v4);
  String pStr = firstNumberWithDecimal(v5);

  if (tStr == "" && uStr == "" && vStr == "" && pmm == "" && pStr == "")
    return false;

  meteoTemp    = tStr;
  meteoUmid    = uStr;
  meteoVento   = vStr;
  meteoPioggia = pmm;
  meteoPress   = pStr;

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 1000)) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    lastAccessStr = String(buf);
  } else {
    lastAccessStr = "sconosciuto";
  }

  return true;
}

// =====================
//  Utility stampa linee
// =====================

const char* giorniSettimana[7] = {
  "Domenica", "Lunedi", "Martedi",
  "Mercoledi", "Giovedi", "Venerdi", "Sabato"
};

const char* mesiAnno[12] = {
  "Gennaio", "Febbraio", "Marzo", "Aprile",
  "Maggio", "Giugno", "Luglio", "Agosto",
  "Settembre", "Ottobre", "Novembre", "Dicembre"
};

void printLine(int row, const String &text);

void updateTimeLine()
{
  setDefaultInfoColors();

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 200)) {
    printLine(0, "Data/ora non disponibile");
    return;
  }

  const char* giorno = giorniSettimana[timeinfo.tm_wday];
  const char* mese   = mesiAnno[timeinfo.tm_mon];
  int anno           = 1900 + timeinfo.tm_year;

  char buf[96];
  snprintf(buf, sizeof(buf), "%s %d %s %d %02d:%02d:%02d",
           giorno,
           timeinfo.tm_mday,
           mese,
           anno,
           timeinfo.tm_hour,
           timeinfo.tm_min,
           timeinfo.tm_sec);

  printLine(0, String(buf));
}

void printLine(int row, const String &text)
{
  int cols = Terminal.getColumns();
  if (cols <= 0) cols = 80;

  char esc[16];
  snprintf(esc, sizeof(esc), "\x1b[%d;1H", row + 1);
  Terminal.write(esc);

  Terminal.write(text.c_str());

  int len = text.length();
  for (int i = len; i < cols; ++i)
    Terminal.write(' ');
}

// Riscrive tutte le righe statiche (WiFi, meteo)
void drawStaticInfo()
{
  int row = 1;

  setDefaultInfoColors();

  // Riga 1: WiFi
  if (WiFi.status() == WL_CONNECTED) {
    String s = "Connesso a SSID ";
    s += WiFi.SSID();
    s += " con IP ";
    s += WiFi.localIP().toString();
    printLine(row++, s);
  } else {
    printLine(row++, "WiFi non connesso");
  }

  // Riga 2: URL meteo
  printLine(row++, String(METEO_URL));

  // Riga 3: ultimo accesso meteo
  {
    String s = "ultimo accesso: ";
    s += lastAccessStr;
    printLine(row++, s);
  }

  // Riga 4+: dati meteo
  if (meteoOk) {
    String s;

    s  = "Temperatura: ";
    s += meteoTemp;
    s += " ";
    s += (char)0xF8;
    s += "C";
    printLine(row++, s);

    s  = "Umidita: ";
    s += meteoUmid;
    s += " %";
    printLine(row++, s);

    s  = "Pressione: ";
    s += meteoPress;
    s += " hPa";
    printLine(row++, s);

    s  = "Vento: ";
    s += meteoVento;
    s += " m/s";
    printLine(row++, s);

    s  = "Precipitazioni: ";
    s += meteoPioggia;
    s += " mm";
    printLine(row++, s);
  } else {
    printLine(row++, "Dati meteo non disponibili");
  }
}

// =====================
//  RADIO: stato globale (dati arrivati da seriale)
// =====================

// Valori
uint16_t g_elettAttiva   = 0;
uint16_t g_elettReattiva = 0;
uint16_t g_caldaia1      = 0;
uint16_t g_caldaia2      = 0;
float    g_temp          = NAN;
float    g_umid          = NAN;
float    g_press         = NAN;

// flag validità
bool haveElettA   = false;
bool haveElettR   = false;
bool haveCald1    = false;
bool haveCald2    = false;
bool haveTemp     = false;
bool haveUmid     = false;
bool havePress    = false;

// timing: ultimo e penultimo aggiornamento (ms da boot)
unsigned long lastElettAMs      = 0, prevElettAMs      = 0;
unsigned long lastElettRMs      = 0, prevElettRMs      = 0;
unsigned long lastCald1Ms       = 0, prevCald1Ms       = 0;
unsigned long lastCald2Ms       = 0, prevCald2Ms       = 0;
unsigned long lastTempMs        = 0, prevTempMs        = 0;
unsigned long lastUmidMs        = 0, prevUmidMs        = 0;
unsigned long lastPressMs       = 0, prevPressMs       = 0;

// data/ora ultima ricezione per ogni campo
String timeElettA, timeElettR, timeCald1, timeCald2, timeTempStr, timeUmidStr, timePressStr;

const unsigned long ONE_HOUR_MS = 3600000UL;

// riga da cui partire per i dati radio
const int RADIO_START_ROW = 10;

// =====================
//  Helpers per timing per ogni campo
// =====================

void updateReceptionMeta(bool &haveFlag,
                         unsigned long &lastMs,
                         unsigned long &prevMs,
                         String &timeStr,
                         unsigned long nowMs)
{
  if (haveFlag) {
    prevMs = lastMs;     // sposta l'ultimo tempo nel "precedente"
  } else {
    prevMs = 0;          // prima volta: nessun delta
  }

  lastMs   = nowMs;
  timeStr  = getNowDateTimeString();
  haveFlag = true;
}


// Restituisce "[YYYY-MM-DD HH:MM:SS, Δ=xxxx ms]" oppure "[n/a]" se mai ricevuto
String formatTimingInfo(bool haveFlag,
                        unsigned long lastMs,
                        unsigned long prevMs,
                        const String &timeStr,
                        unsigned long nowMs)
{
  if (!haveFlag)
    return " [n/a]";

  String out = " [";
  out += timeStr;
  out += ", Δ=";

  if (prevMs > 0 && lastMs >= prevMs) {
    unsigned long delta = lastMs - prevMs;
    out += String(delta);
  } else {
    out += "---";
  }
  out += " ms]";
  return out;
}

// =====================
//  Disegno sezione "Dati radio"
// =====================

// =====================
//  Disegno sezione "Dati radio" — versione allineata
// =====================

// =====================
//  Disegno sezione "Dati radio" — versione allineata senza caratteri di riempimento
// =====================

// =====================
//  Disegno sezione "Dati radio" — colonne fisse
// =====================

void drawRadioInfo()
{
  unsigned long now = millis();
  int row = RADIO_START_ROW;

  setDefaultInfoColors();
  printLine(row++, "=== Dati radio 433 MHz (via seriale) ===");

  // -------- helper locale per formattare una riga --------
  auto fmt = [&](const char *label,
                 const String &rawValue,
                 const String &unit,
                 bool have,
                 unsigned long lastMs,
                 unsigned long prevMs,
                 const String &timeStr)
  {
    // label: larghezza fissa 12 caratteri, allineata a sinistra
    char lbl[13];
    snprintf(lbl, sizeof(lbl), "%-12s", label);

    // valore: max 7 caratteri, con spazi a sinistra per riempire
    String val;
    if (have && (now - lastMs <= ONE_HOUR_MS) && rawValue.length() > 0) {
      val = rawValue;
      // se più lungo di 7, tronco a destra (taglio i decimali in eccesso)
      if (val.length() > 7) {
        val = val.substring(0, 7);
      }
    } else {
      val = "";
    }
    while (val.length() < 7) {
      val = " " + val;  // padding a sinistra
    }

    // unit/descrizione: larghezza fissa 6 caratteri, padding a destra
    String u = unit;
    if (u.length() > 6) {
      u = u.substring(0, 6);
    }
    while (u.length() < 6) {
      u += " ";
    }

    // testo base: LABEL(12) + ": " + VALUE(7) + " " + UNIT(6)
    String out = String(lbl);
    out += ": ";
    out += val;
    out += " ";
    out += u;

    // timing
    if (have && (now - lastMs <= ONE_HOUR_MS)) {
      String ts = timeStr;
      if (ts.length() == 0) ts = "sconosciuto";

      out += " [";
      out += ts;
      out += ", ";

      if (prevMs > 0 && lastMs >= prevMs) {
        out += String(lastMs - prevMs);
      } else {
        out += "---";
      }
      out += " ms]";
    } else {
      out += " [n/a]";
    }

    printLine(row++, out);
  };

  // Preparo le stringhe valore, senza toccare i g_* se non abbiamo dati

  // Contatori interi
  String sAttiva    = haveElettA ? String(g_elettAttiva)   : "";
  String sReattiva  = haveElettR ? String(g_elettReattiva) : "";
  String sCald1     = haveCald1  ? String(g_caldaia1)      : "";
  String sCald2     = haveCald2  ? String(g_caldaia2)      : "";

  // Decimali (puoi cambiare 2 → 1 o 0 se vuoi meno decimali)
  String sTemp      = haveTemp   ? String(g_temp, 2)       : "";
  String sUmid      = haveUmid   ? String(g_umid, 2)       : "";
  String sPress     = havePress  ? String(g_press, 2)      : "";

  // -------- righe come da specifica --------
  // - ATTIVA     : XXXXXXX (cont) [....]
  // - REATTIVA   : XXXXXXX (cont) [....]
  // - CALDAIA 1  : XXXXXXX (lumi) [....]
  // - CALDAIA 2  : XXXXXXX (cont) [....]
  // - TEMPERATURA: XXXXXXX C      [....]
  // - UMIDITA    : XXXXXXX %      [....]
  // - PRESSIONE  : XXXXXXX hPa    [....]

  fmt("ATTIVA",     sAttiva,   "(cont)",
      haveElettA, lastElettAMs, prevElettAMs, timeElettA);

  fmt("REATTIVA",   sReattiva, "(cont)",
      haveElettR, lastElettRMs, prevElettRMs, timeElettR);

  fmt("CALDAIA 1",  sCald1,    "(lumi)",
      haveCald1, lastCald1Ms, prevCald1Ms, timeCald1);

  fmt("CALDAIA 2",  sCald2,    "(cont)",
      haveCald2, lastCald2Ms, prevCald2Ms, timeCald2);

fmt("TEMPERATURA", sTemp, String((char)0xF8) + "C",
    haveTemp, lastTempMs, prevTempMs, timeTempStr);


  fmt("UMIDITA",     sUmid,    "%",
      haveUmid, lastUmidMs, prevUmidMs, timeUmidStr);

  fmt("PRESSIONE",   sPress,   "hPa",
      havePress, lastPressMs, prevPressMs, timePressStr);
}


// =====================
//  Tabella ASCII alla fine della pagina
// =====================

void drawAsciiTest()
{
  int rows = Terminal.getRows();
  if (rows <= 0) rows = 25;

  // 0x20–0xFF → 224 caratteri, 16 per riga = 14 righe
  const int ASCII_ROWS = 15;  // 1 riga titolo + 14 righe dati
  int startRow = rows - ASCII_ROWS;
  if (startRow < RADIO_START_ROW + 8)
    startRow = RADIO_START_ROW + 8;

  int row = startRow;

  setDefaultInfoColors();
  printLine(row++, "=== ASCII test 0x20 - 0xFF ===");

  uint16_t code = 0x20;               // <-- PRIMA era 0x00

  // 14 righe x 16 caratteri = 224 valori (0x20–0xFF)
  for (int r = 0; r < 14 && code < 256; ++r) {
    String line;
    for (int i = 0; i < 16 && code < 256; ++i, ++code) {
      char buf[8];
      // Codice esadecimale
      snprintf(buf, sizeof(buf), "%02X ", code);
      line += buf;

      // Carattere visualizzabile
      char c = (char)code;
      line += c;
      line += ' ';   // spazio di separazione
    }
    printLine(row++, line);
  }
}

// =====================
//  Decodifica linea esadecimale da seriale
// =====================

uint8_t hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  c = toupper((unsigned char)c);
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return 0xFF;
}

void processRadioHexLine(const String &line)
{
  String s = line;
  s.trim();
  if (s.length() < 2) return;

  // lunghezza pari?
  if (s.length() % 2 != 0) {
    return;
  }

  const size_t MAX_RADIO_BYTES = 64;
  uint8_t buf[MAX_RADIO_BYTES];
  size_t nBytes = s.length() / 2;
  if (nBytes > MAX_RADIO_BYTES) nBytes = MAX_RADIO_BYTES;

  size_t outIndex = 0;
  for (size_t i = 0; i < nBytes; ++i) {
    char c1 = s[2 * i];
    char c2 = s[2 * i + 1];
    uint8_t hi = hexNibble(c1);
    uint8_t lo = hexNibble(c2);
    if (hi == 0xFF || lo == 0xFF) {
      return;
    }
    buf[outIndex++] = (hi << 4) | lo;
  }

  if (outIndex > 0) {
    extern void elaboraDatiBytes(const uint8_t* data, uint8_t len);
    elaboraDatiBytes(buf, (uint8_t)outIndex);
  }
}

// =====================
//  ELABORAZIONE PACCHETTO (dal buffer di byte)
// =====================

void elaboraDatiBytes(const uint8_t* data, uint8_t len) {
  if (len < 5) {
    return;
  }

  const uint8_t* payloadBytes = data + (len - 5);

  uint8_t b0 = payloadBytes[0];
  uint8_t b1 = payloadBytes[1];
  uint8_t b2 = payloadBytes[2];
  uint8_t b3 = payloadBytes[3];
  uint8_t dispositivo = payloadBytes[4];

  unsigned long nowMs = millis();

  // 0x0B: elettricità attiva / reattiva (contatori interi)
  if (dispositivo == 0x0B) {
    uint16_t valore1 = ((uint16_t)b0 << 8) | b1;  // attiva
    uint16_t valore2 = ((uint16_t)b2 << 8) | b3;  // reattiva

    g_elettAttiva   = valore1;
    g_elettReattiva = valore2;

    updateReceptionMeta(haveElettA, lastElettAMs, prevElettAMs, timeElettA, nowMs);
    updateReceptionMeta(haveElettR, lastElettRMs, prevElettRMs, timeElettR, nowMs);
  }

  // 0x63: Caldaia1 / Caldaia2 (contatori interi)
  else if (dispositivo == 0x63) {
    uint16_t valore1 = ((uint16_t)b0 << 8) | b1;
    uint16_t valore2 = ((uint16_t)b2 << 8) | b3;

    g_caldaia1 = valore1;
    g_caldaia2 = valore2;

    updateReceptionMeta(haveCald1, lastCald1Ms, prevCald1Ms, timeCald1, nowMs);
    updateReceptionMeta(haveCald2, lastCald2Ms, prevCald2Ms, timeCald2, nowMs);
  }

  // 0x6A: temperatura float (decimale)
  else if (dispositivo == 0x6A) {
    uint8_t raw[4]      = { b0, b1, b2, b3 };
    uint8_t reversed[4] = { raw[3], raw[2], raw[1], raw[0] };

    float valore_float;
    memcpy(&valore_float, reversed, sizeof(float));

    g_temp = valore_float;

    updateReceptionMeta(haveTemp, lastTempMs, prevTempMs, timeTempStr, nowMs);
  }

  // 0x6B: umidità float (decimale)
  else if (dispositivo == 0x6B) {
    uint8_t raw[4]      = { b0, b1, b2, b3 };
    uint8_t reversed[4] = { raw[3], raw[2], raw[1], raw[0] };

    float valore_float;
    memcpy(&valore_float, reversed, sizeof(float));

    g_umid = valore_float;

    updateReceptionMeta(haveUmid, lastUmidMs, prevUmidMs, timeUmidStr, nowMs);
  }

  // 0x6C: pressione float (decimale)
  else if (dispositivo == 0x6C) {
    uint8_t raw[4]      = { b0, b1, b2, b3 };
    uint8_t reversed[4] = { raw[3], raw[2], raw[1], raw[0] };

    float valore_float;
    memcpy(&valore_float, reversed, sizeof(float));

    g_press = valore_float;

    updateReceptionMeta(havePress, lastPressMs, prevPressMs, timePressStr, nowMs);
  }

  // Ogni pacchetto valido → aggiorna sezione radio + ASCII
  drawRadioInfo();
  drawAsciiTest();
}


// =====================
//  setup / loop
// =====================

unsigned long lastRadioRedraw = 0;

void setup()
{
  Serial.begin(115200);
  delay(2000);

  // UART2 per dati da Arduino UNO (hex)
  RadioSerial.begin(1200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  PS2Controller.begin(PS2Preset::KeyboardPort0);

  DisplayController.begin();
  DisplayController.setResolution();

  Terminal.begin(&DisplayController);
  Terminal.connectLocally();

  Terminal.setBackgroundColor(Color::Black);
  Terminal.setForegroundColor(Color::BrightGreen);
  Terminal.clear();
  Terminal.enableCursor(false);

  Terminal.write("Valk Research Ltd - ESP32 FabGL WiFi Meteo + RadioSerial + ASCII\r\n");

  connectWiFiFromConfig();
  setupTime();

  delay(2000);  // tempo per NTP

  if (fetchMeteo())
    meteoOk = true;
  else
    meteoOk = false;

  lastMeteoFetch = millis();

  updateTimeLine();
  drawStaticInfo();
  drawRadioInfo();
  drawAsciiTest();
  lastTimeUpdate  = millis();
  lastRadioRedraw = millis();
}

void loop()
{
  unsigned long now = millis();

  // === Lettura NON BLOCCANTE dalla seriale radio ===
  while (RadioSerial.available() > 0) {
    char c = (char)RadioSerial.read();

    if (c == '\n' || c == '\r') {
      if (radioLineBuffer.length() > 0) {
        processRadioHexLine(radioLineBuffer);
        radioLineBuffer = "";
      }
    } else {
      if (isxdigit((unsigned char)c)) {
        radioLineBuffer += (char)toupper((unsigned char)c);
      }
    }
  }

  // Aggiorna SOLO la riga dell'orologio ogni secondo
  if (now - lastTimeUpdate >= 1000) {
    lastTimeUpdate = now;
    updateTimeLine();
  }

  // Ogni METEO_REFRESH_MS aggiorna meteo
  if (WiFi.status() == WL_CONNECTED &&
      now - lastMeteoFetch >= METEO_REFRESH_MS) {

    lastMeteoFetch = now;

    if (fetchMeteo())
      meteoOk = true;
    else
      meteoOk = false;

    drawStaticInfo();
    drawRadioInfo();
    drawAsciiTest();
  }

  // Ridisegna periodicamente la sezione radio + ASCII per aggiornare time-out
  if (now - lastRadioRedraw >= 5000) {  // ogni 5s
    lastRadioRedraw = now;
    drawRadioInfo();
    drawAsciiTest();
  }
}
