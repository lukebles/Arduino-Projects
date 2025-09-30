#include "config.h" // Contiene WIFI_SSID, WIFI_PASSWORD (e se vuoi altri token)
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <LiquidCrystal.h>
#include <time.h>
#include "esp_sntp.h"

// ================== CONFIG ==================
#define LCD_COLS 16
#define LCD_ROWS 2
LiquidCrystal lcd(13, 12, 14, 27, 26, 25);

// Meteo (HTTPS)
const char* METEO_URL  = "https://www.reggioemiliameteo.it/stazioni/tab-live/stazioni-meteo-tab-live-20.html";
const uint32_t METEO_REFRESH_MS = 10UL * 60UL * 1000UL; // 10 minuti

// Ora legale EU (Roma) e NTP
const char* TZ_EUROPE_ROME = "CET-1CEST,M3.5.0/2,M10.5.0/3";
const uint32_t NTP_SYNC_INTERVAL_MS = 120UL * 60UL * 1000UL; // 120 minuti

// ================== STATI ==================
bool internetOK = false;
unsigned long lastNTPTry = 0;
unsigned long lastLine1Update = 0;
const uint32_t LINE1_INTERVAL_MS = 1000;

// Scroller riga 2
String line2Text = "Inizializzo dati meteo...";
String line2Padded = "";
int line2Idx = 0;
unsigned long lastScroll = 0;
const uint32_t SCROLL_INTERVAL_MS = 350;

// Meteo
unsigned long lastMeteoFetch = 0;
bool meteoOk = false;

// ================== UTILS ==================
void onTimeSync(struct timeval* tv) { internetOK = true; }

void showWaitingAnimation(uint8_t dots) {
  lcd.setCursor(0, 0); lcd.print("Attesa Internet ");
  lcd.setCursor(0, 1);
  String s = "";
  for (uint8_t i=0;i<dots;i++) s += '.';
  while (s.length() < LCD_COLS) s += ' ';
  lcd.print(s);
}

void connectWiFiNonBlocking(unsigned long timeout_ms = 8000) {
  if (WiFi.status() == WL_CONNECTED) { internetOK = true; return; }
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeout_ms) {
    delay(100);
  }
  internetOK = (WiFi.status() == WL_CONNECTED);
}

void connectWiFiUntilConnected() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint8_t dots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    showWaitingAnimation((dots % 4) + 1); // animazione "...."
    dots++;
    delay(400);
  }
  internetOK = true;
  lcd.clear();
}

void setupTime() {
  sntp_set_time_sync_notification_cb(onTimeSync);
  sntp_set_sync_interval(NTP_SYNC_INTERVAL_MS);
  configTzTime(TZ_EUROPE_ROME, "pool.ntp.org", "time.google.com");
}

bool trySyncNow() {
  if (WiFi.status() != WL_CONNECTED) { internetOK = false; return false; }
  configTzTime(TZ_EUROPE_ROME, "pool.ntp.org", "time.google.com");
  struct tm t;
  if (getLocalTime(&t, 2000)) { internetOK = true; return true; }
  internetOK = false; return false;
}

// --- parsing helpers ---
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

  // td[3]
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

// Sanificazione minima: rimuove &deg; (temperatura/dew point), toglie &nbsp; e tag
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

// Estrae il primo numero (con eventuale decimale), mantiene la virgola se presente,
// altrimenti converte il punto in virgola. Non arrotonda, solo tronca alla parte presente.
// Estrae il primo numero e rimuove sempre la virgola, unendo le parti
static String firstNumberRemoveComma(String s) {
  s.replace('\n', ' ');
  s.replace('\r', ' ');
  s.replace('\t', ' ');
  s.replace("&nbsp;", " ");
  s.replace(".", ",");  // normalizza eventuali punti in virgola

  String out;
  bool started = false;

  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (!started) {
      if ((c == '+' || c == '-' || (c >= '0' && c <= '9'))) {
        started = true;
        if (c != '+') out += c; // ignora il segno +
      }
    } else {
      if ((c >= '0' && c <= '9')) {
        out += c;
      } else if (c == ',') {
        // ignora la virgola → concatena direttamente i numeri
      } else {
        break;
      }
    }
  }

  return out;
}



// Estrae il primo numero e lo arrotonda all'intero più vicino (per T, UR, vento, pressione)
static String firstNumberRoundedInt(String s) {
  s.replace('\n', ' ');
  s.replace('\r', ' ');
  s.replace('\t', ' ');
  s.replace("&nbsp;", " ");
  s.replace(",", ".");  // consentiamo virgola italiana

  String buf;
  bool started = false;
  bool hasDot = false;

  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (!started) {
      if (c == '-' || (c >= '0' && c <= '9')) {
        started = true;
        buf += c;
      }
    } else {
      if (c >= '0' && c <= '9') {
        buf += c;
      } else if (c == '.' && !hasDot) {
        hasDot = true;
        buf += c;
      } else {
        break;
      }
    }
  }

  if (buf.length() == 0) return String("");
  float f = buf.toFloat();
  long n = lroundf(f);
  return String(n);
}

// pad a sinistra con zeri fino a 'width'
static String leftPadZeros(String s, int width) {
  while (s.length() < width) s = "0" + s;
  return s;
}

// --- Funzione principale meteo --------------------------------------------
bool fetchMeteoAndBuildLine() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure(); // per semplicità
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
  if (code != HTTP_CODE_OK) { http.end(); return false; }
  String html = http.getString();
  http.end();

  // Isola primo <table>...</table>
  String table = between(html, "<table", "</table>");
  if (table.isEmpty()) return false;
  int gt = table.indexOf('>');
  if (gt >= 0) table = table.substring(gt + 1);

  // Estrazioni
  String v1 = extractFromRow(table, 1, 0); // Temperatura (h5)
  String v2 = extractFromRow(table, 2, 0); // Umidità (h5)
  String v3 = extractFromRow(table, 3, 1); // Vento (span[2]/small)
  String v4 = extractFromRow(table, 4, 1); // Pioggia (span[2]/small)
  String v5 = extractFromRow(table, 5, 0); // Pressione (h5)
  // String v6 = extractFromRow(table, 6, 0); // Punto di rugiada (se servisse)

  // Sanifica: rimuovi &deg; da temperatura (e da dew point se lo usassimo)
  v1 = sanitize(v1, true);
  v2 = sanitize(v2, false);
  v3 = sanitize(v3, false);
  v4 = sanitize(v4, false);
  v5 = sanitize(v5, false);

  // Numeri
  String TT   = leftPadZeros(firstNumberRoundedInt(v1), 2); // °C -> intero 2 cifre
  String UU   = leftPadZeros(firstNumberRoundedInt(v2), 2); // %  -> intero 2 cifre
  String VV   = leftPadZeros(firstNumberRoundedInt(v3), 2); // vento -> intero 2 cifre
  String PPmm = firstNumberRemoveComma(v4);                   // pioggia -> MANTIENE LA VIRGOLA
  String SSSS = leftPadZeros(firstNumberRoundedInt(v5), 4); // pressione -> 4 cifre

  // Costruisci riga compatta (senza unità per stare in 16 char quando possibile)
  // Formato: "TT UU SSSS VV PP" (PP=pioggia con virgola)
  String msg = TT + " " + UU + " " + SSSS + " " + VV + " " + PPmm;

  // Se troppo corta, pad; per marquee aggiungo qualche spazio
  line2Text = msg;
  line2Padded = line2Text + "   ";
  line2Idx = 0;

  return true;
}

void drawLine2() {
  String src = line2Padded;
  if (src.length() <= LCD_COLS) {
    String pad = src;
    while (pad.length() < LCD_COLS) pad += ' ';
    lcd.setCursor(0, 1);
    lcd.print(pad);
    return;
  }
  String doubled = src + src;
  String window = doubled.substring(line2Idx, line2Idx + LCD_COLS);
  lcd.setCursor(0, 1);
  lcd.print(window);
  line2Idx = (line2Idx + 1) % src.length();
}

// ================== SETUP/LOOP ==================
void setup() {
  Serial.begin(115200);
  lcd.begin(LCD_COLS, LCD_ROWS);
  lcd.clear();

  // Connessione BLOCKING con animazione fino a OK
  connectWiFiUntilConnected();

  // NTP
  setupTime();
  lastNTPTry = millis() - NTP_SYNC_INTERVAL_MS; // forza una sync presto

  // Primo fetch meteo
  if (fetchMeteoAndBuildLine()) {
    meteoOk = true;
  } else {
    meteoOk = false;
    line2Text = "no meteo";
    line2Padded = line2Text + "   ";
    line2Idx = 0;
  }
  lastMeteoFetch = millis();
}

void loop() {
  unsigned long now = millis();

  // —— Gestione connettività —— //
  if (WiFi.status() != WL_CONNECTED) {
    internetOK = false;
    // Mostra no internet in riga 2 e ritenta la connessione (non blocco troppo)
    if (line2Text != "no internet") {
      line2Text = "no internet";
      line2Padded = line2Text + "   ";
      line2Idx = 0;
    }
    connectWiFiNonBlocking(3000);
  } else {
    internetOK = true;
  }

  // —— Sync NTP periodica —— //
  if (now - lastNTPTry >= NTP_SYNC_INTERVAL_MS) {
    lastNTPTry = now;
    if (WiFi.status() == WL_CONNECTED) {
      trySyncNow();
    }
  }

  // —— Prima riga: orologio (1 Hz) —— //
  if (now - lastLine1Update >= LINE1_INTERVAL_MS) {
    lastLine1Update = now;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 10)) {
      // Non abbiamo l'ora valida (mai sincronizzata): placeholder
      lcd.setCursor(0, 0); lcd.print("LUN 00/00 00:00");
    } else {
      char line1[17];
      strftime(line1, sizeof(line1), "%a %d/%m %H:%M", &timeinfo);
      if      (strncmp(line1, "Mon", 3) == 0) memcpy(line1, "LUN", 3);
      else if (strncmp(line1, "Tue", 3) == 0) memcpy(line1, "MAR", 3);
      else if (strncmp(line1, "Wed", 3) == 0) memcpy(line1, "MER", 3);
      else if (strncmp(line1, "Thu", 3) == 0) memcpy(line1, "GIO", 3);
      else if (strncmp(line1, "Fri", 3) == 0) memcpy(line1, "VEN", 3);
      else if (strncmp(line1, "Sat", 3) == 0) memcpy(line1, "SAB", 3);
      else if (strncmp(line1, "Sun", 3) == 0) memcpy(line1, "DOM", 3);

      lcd.setCursor(0, 0); lcd.print(line1);
      // Lampeggio dei due punti
      lcd.setCursor(12, 0); lcd.print((timeinfo.tm_sec % 2 == 0) ? ":" : " ");
    }
  }

  // —— Seconda riga: scroll / static —— //
  // if (now - lastScroll >= SCROLL_INTERVAL_MS) {
  //   lastScroll = now;
  //   drawLine2();
  // }
  lcd.setCursor(0, 1);
  String s = line2Text;
  while (s.length() < LCD_COLS) s += ' ';  // padding per pulire
  lcd.print(s);


  // —— Refresh meteo ogni 10 minuti —— //
  if (internetOK) {
    if (now - lastMeteoFetch >= METEO_REFRESH_MS) {
      lastMeteoFetch = now;
      if (fetchMeteoAndBuildLine()) {
        meteoOk = true;
      } else {
        meteoOk = false;
        line2Text = "no meteo";
        line2Padded = line2Text + "   ";
        line2Idx = 0;
      }
    }
  }
}
