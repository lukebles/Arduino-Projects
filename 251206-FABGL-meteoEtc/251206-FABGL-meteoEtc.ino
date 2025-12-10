/*
  FabGL WiFi + Meteo + News ANSA homepage (no flicker)

  - Connessione WiFi usando WIFI_SSID / WIFI_PASSWORD da config.h
  - Sync ora via NTP (Europa/Roma)
  - Fetch meteo (ReggioEmiliaMeteo)
  - Fetch RSS ANSA homepage (ansait_rss.xml)
  - Mostra:
      Riga 0  : data/ora locale
      Riga 1+ : WiFi, URL meteo, ultimo accesso, meteo, 3 news ANSA
        per ogni news:
          riga data/ora notizia
          una o più righe di titolo, con word-wrap
  - Aggiorna orologio ogni secondo SENZA sfarfallamento
*/

#include "fabgl.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include "config.h"   // definisce WIFI_SSID e WIFI_PASSWORD
#include <string.h>  // per strcasecmp
#include <ctype.h>  

// =====================
//  Oggetti FabGL
// =====================

fabgl::VGATextController DisplayController;
fabgl::PS2Controller     PS2Controller;
fabgl::Terminal          Terminal;

// =====================
//  NTP / Timezone
// =====================

const char* TZ_EUROPE_ROME = "CET-1CEST,M3.5.0/2,M10.5.0/3";

void setupTime()
{
  configTzTime(TZ_EUROPE_ROME, "pool.ntp.org", "time.google.com");
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
//  ANSA RSS homepage
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

  int maxTentativi = 20;           // 20 * 500ms ≈ 10 secondi
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
// mode = 0 -> h5, mode = 1 -> span[2]/small
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

// Estrae il primo numero con eventuale decimale SENZA arrotondare
// converte sempre virgola/punto in '.'
// Estrae il primo numero con eventuale decimale SENZA arrotondare
// converte sempre virgola/punto in '.'
static String firstNumberWithDecimal(String s) {
  // queste tre devono usare char, char
  s.replace('\n', ' ');
  s.replace('\r', ' ');
  s.replace('\t', ' ');

  // qui invece va bene la versione "stringa, stringa"
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
//  Utility stampa linee / word-wrap
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

// Aggiorna SOLO la riga 0 (data/ora)
void updateTimeLine()
{
  setDefaultInfoColors();   // date/orologio sempre in grigio

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

  // ANSI: ESC[row;1H (row 0-based → ANSI 1-based)
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
  int rows = Terminal.getRows();
  if (rows <= 0) rows = 25;

  setDefaultInfoColors();  // tutto info/meteo in grigio

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
    s += ' ';
    s += (char)0xF8;
    s += 'C';
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
//  setup / loop
// =====================

void setup()
{
  //Serial.begin(115200);

  PS2Controller.begin(PS2Preset::KeyboardPort0);

  DisplayController.begin();
  DisplayController.setResolution();

  Terminal.begin(&DisplayController);
  Terminal.connectLocally();
  //Terminal.setLogStream(Serial);

  Terminal.setBackgroundColor(Color::Black);
  Terminal.setForegroundColor(Color::BrightGreen);
  Terminal.clear();
  Terminal.enableCursor(false);

  Terminal.write("251208 Valk Research Ltd - ESP32 FabGL WiFi Meteo\r\n");

  connectWiFiFromConfig();
  setupTime();

  delay(2000);  // piccolo tempo al NTP

  if (fetchMeteo())
    meteoOk = true;
  else
    meteoOk = false;

  lastMeteoFetch = millis();

  updateTimeLine();
  drawStaticInfo();
  lastTimeUpdate = millis();
}

void loop()
{
  unsigned long now = millis();

  // Aggiorna SOLO la riga dell'orologio ogni secondo
  if (now - lastTimeUpdate >= 1000) {
    lastTimeUpdate = now;
    updateTimeLine();   // niente clear → niente sfarfallio
  }

  // Ogni METEO_REFRESH_MS aggiorna sia meteo che news ANSA
  if (WiFi.status() == WL_CONNECTED &&
      now - lastMeteoFetch >= METEO_REFRESH_MS) {

    lastMeteoFetch = now;

    if (fetchMeteo())
      meteoOk = true;
    else
      meteoOk = false;


    drawStaticInfo();
  }
}
