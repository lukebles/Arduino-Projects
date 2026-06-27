/*
  FabGL WiFi + News RSS configurabili (no flicker)

  - Configurazione WiFi da pagina web, senza config.h
  - Se il pulsante RSS è premuto all'accensione, apre un Access Point di configurazione
  - Sync ora via NTP (Europa/Roma) quando il WiFi è disponibile
  - Feed RSS modificabili da pagina web, con valori di default nel codice
  - Mostra:
      Riga 0  : data/ora locale
      Riga 1+ : news RSS
        per ogni news:
          riga data/ora notizia
          una o più righe di titolo, con word-wrap
  - Aggiorna orologio ogni secondo SENZA sfarfallamento
*/

// istruzioni di compilazione:
// ================================
// esp32 by Espressif Systems: 2.0.4
// Tools → Board → ESP32 Arduino → ESP32 Dev Module
// FabGL 1.0.9
/*
Board: ESP32 Dev Module
Upload Speed: 921600 oppure 115200
CPU Frequency: 240 MHz
Flash Frequency: 80 MHz
Flash Mode: QIO
Flash Size: 4MB
Partition Scheme: Default 4MB with spiffs
PSRAM: Disabled
Core Debug Level: None
Port: la porta USB del tuo ESP32
*/

#include "fabgl.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include <string.h>   // strcasecmp
#include <ctype.h>    // ispunct
#include <Bounce2.h>

#include "LkWiFiConfigPortal.h"

// =====================
//  Configurazione generale
// =====================

const char* TZ_EUROPE_ROME = "CET-1CEST,M3.5.0/2,M10.5.0/3";

const uint32_t FETCH_REFRESH_MS = 5UL * 60UL * 1000UL;  // ogni quanto ricaricare lo stesso feed
const unsigned long RSS_SELECTION_SETTLE_MS = 2000;     // attesa dopo cambio STAZIONE_RSS

const int MAX_NEWS = 20;

// =====================
//  Pulsante cambio RSS
// =====================

const int BTN_NEXT_RSS = 33;
Bounce2::Button btnNextRSS = Bounce2::Button();

// =====================
//  Oggetti principali
// =====================

fabgl::VGATextController DisplayController;
fabgl::PS2Controller     PS2Controller;
fabgl::Terminal          Terminal;
LkWiFiConfigPortal       WiFiPortal;

// =====================
//  Feed RSS di default
// =====================

// Questi sono i valori di default.
// Da web puoi modificarli su: http://IP_ESP32/rss.html
const LkRSSFeed DEFAULT_RSS_FEEDS[] = {
  { "IL FATTO QUOTIDIANO ", "https://www.ilfattoquotidiano.it/feed/"},
  { "THE GUARDIAN", "https://www.theguardian.com/world/rss"},
  { "NEW YORK TIMES", "https://rss.nytimes.com/services/xml/rss/nyt/World.xml"},
  { "ANSA CRONACA", "https://www.ansa.it/sito/notizie/cronaca/cronaca_rss.xml" },
  { "ANSA HOME",    "https://www.ansa.it/sito/ansait_rss.xml" },
  { "ANSA MONDO",   "https://www.ansa.it/sito/notizie/mondo/mondo_rss.xml" },
};

const int NUM_DEFAULT_RSS_FEEDS = sizeof(DEFAULT_RSS_FEEDS) / sizeof(DEFAULT_RSS_FEEDS[0]);

// Cambia questa variabile da 0 al numero di feed RSS configurati - 1.
// Il pulsante collegato a BTN_NEXT_RSS incrementa questo indice.
volatile int STAZIONE_RSS = 0;

// =====================
//  Stato runtime
// =====================

int currentFeedIndex = 0;
int requestedFeedIndex = -1;
unsigned long requestedFeedChangeTime = 0;

unsigned long lastFetch = 0;
unsigned long lastClockUpdate = 0;

bool wifiTimeReady = false;
bool firstFetchAfterWiFi = false;

uint32_t lastRSSConfigVersion = 0;

String currentFeedLabel = "";
String newsTitles[MAX_NEWS];
String newsDates[MAX_NEWS];

// =====================
//  Colori
// =====================

static const Color COLORS[] = {
  Color::Black,
  Color::BrightWhite,
  Color::BrightGreen,
  Color::BrightYellow,
  Color::BrightCyan,
  Color::BrightRed,
  Color::BrightBlue,
  Color::BrightMagenta,
};

constexpr int NUM_COLORS = sizeof(COLORS) / sizeof(COLORS[0]);
constexpr int FORBIDDEN_PAIRS = 15;
constexpr int NUM_ALLOWED_COMBOS = NUM_COLORS * (NUM_COLORS - 1) - FORBIDDEN_PAIRS;

// =====================
//  Prototipi
// =====================

void setupTime();
void handleNextRSSButton();
void handleRSSStationSelection();
void handleRSSConfigChangedFromWeb();
void fetchCurrentRSS();
void fetchRSSNews(const char* url, const char* label);
void updateTimeLine();
void drawStaticInfo();
void setDefaultInfoColors();
int getRSSFeedCount();

static String between(const String& s, const String& open, const String& close, int from = 0);
static String stripCDATA(String s);
static void decodeHtmlEntities(String &s);
static void normalizeUtf8Punctuation(String &s);
static String toDisplayCharset(String s);
static String shortenPubDate(String s);
static String cleanNewsTitle(const String &input);
bool parsePubDateToEpoch(const String &in, time_t &out);

// =====================
//  NTP / Timezone
// =====================

void setupTime()
{
  configTzTime(TZ_EUROPE_ROME, "pool.ntp.org", "time.google.com");
}

// =====================
//  Gestione feed RSS
// =====================

int getRSSFeedCount()
{
  int n = WiFiPortal.rssFeedCount();
  return n > 0 ? n : 1;
}

int clampFeedIndex(int index)
{
  int n = getRSSFeedCount();

  if (index < 0)
    return 0;

  if (index >= n)
    return n - 1;

  return index;
}

void fetchCurrentRSS()
{
  currentFeedIndex = clampFeedIndex(currentFeedIndex);

  String url = WiFiPortal.rssUrl(currentFeedIndex);
  String label = WiFiPortal.rssLabel(currentFeedIndex);

  if (url.length() == 0) {
    currentFeedLabel = "RSS non configurato";
    return;
  }

  fetchRSSNews(url.c_str(), label.c_str());
}

void refreshCurrentRSSOnScreen()
{
  if (WiFi.status() != WL_CONNECTED)
    return;

  fetchCurrentRSS();
  lastFetch = millis();
  updateTimeLine();
  drawStaticInfo();
}

void handleRSSConfigChangedFromWeb()
{
  uint32_t v = WiFiPortal.rssConfigVersion();

  if (v == lastRSSConfigVersion)
    return;

  lastRSSConfigVersion = v;

  STAZIONE_RSS = clampFeedIndex(STAZIONE_RSS);
  currentFeedIndex = STAZIONE_RSS;
  requestedFeedIndex = STAZIONE_RSS;
  requestedFeedChangeTime = millis();

  refreshCurrentRSSOnScreen();
}

void handleRSSStationSelection()
{
  unsigned long now = millis();

  int selected = clampFeedIndex(STAZIONE_RSS);

  // Se STAZIONE_RSS era fuori range, corregge anche la variabile globale.
  if (selected != STAZIONE_RSS)
    STAZIONE_RSS = selected;

  // Se il valore richiesto è cambiato, avvia il tempo di stabilizzazione.
  if (selected != requestedFeedIndex) {
    requestedFeedIndex = selected;
    requestedFeedChangeTime = now;
    return;
  }

  // Se è già visualizzato, non fare nulla.
  if (requestedFeedIndex == currentFeedIndex)
    return;

  // Aspetta che la selezione resti stabile.
  if (now - requestedFeedChangeTime < RSS_SELECTION_SETTLE_MS)
    return;

  currentFeedIndex = requestedFeedIndex;
  refreshCurrentRSSOnScreen();
}

void handleNextRSSButton()
{
  static unsigned long pressStart = 0;
  static bool alreadyHandled = false;

  btnNextRSS.update();

  if (btnNextRSS.isPressed()) {

    if (pressStart == 0)
      pressStart = millis();

    if (!alreadyHandled && millis() - pressStart >= 500) {

      int n = getRSSFeedCount();

      if (n > 0) {
        STAZIONE_RSS++;

        if (STAZIONE_RSS >= n)
          STAZIONE_RSS = 0;
      }

      alreadyHandled = true;
    }

  } else {
    pressStart = 0;
    alreadyHandled = false;
  }
}


// =====================
//  Colori terminale
// =====================

bool isForbiddenPair(Color fg, Color bg)
{
  return (
    (fg == Color::BrightYellow && bg == Color::BrightCyan) ||
    (fg == Color::BrightCyan  && bg == Color::BrightYellow) ||
    (fg == Color::BrightGreen && bg == Color::BrightCyan) ||
    (fg == Color::BrightCyan  && bg == Color::BrightGreen) ||
    (fg == Color::BrightRed && bg == Color::BrightMagenta) ||
    (fg == Color::BrightMagenta  && bg == Color::BrightRed) ||
    (fg == Color::BrightYellow && bg == Color::BrightGreen) ||
    (fg == Color::BrightGreen  && bg == Color::BrightYellow) ||
    (fg == Color::Black && bg == Color::BrightWhite) ||
    (fg == Color::BrightWhite && bg == Color::BrightYellow) ||
    (fg == Color::BrightYellow && bg == Color::BrightWhite) ||
    (fg == Color::BrightWhite && bg == Color::BrightGreen) ||
    (fg == Color::BrightGreen && bg == Color::BrightWhite) ||
    (fg == Color::BrightWhite && bg == Color::BrightCyan) ||
    (fg == Color::BrightCyan && bg == Color::BrightWhite)
  );
}

void setDefaultInfoColors()
{
  Terminal.setForegroundColor(Color::White);
  Terminal.setBackgroundColor(Color::Black);
}

void applyNewsColor(const String &text)
{
  uint16_t h = 0;

  for (int i = 0; i < text.length(); ++i)
    h = h * 31 + (uint8_t)text[i];

  int targetIndex = h % NUM_ALLOWED_COMBOS;
  int k = 0;

  Color chosenFG = Color::BrightYellow;
  Color chosenBG = Color::Black;

  for (int fi = 0; fi < NUM_COLORS; ++fi) {
    for (int bi = 0; bi < NUM_COLORS; ++bi) {
      if (fi == bi)
        continue;

      Color fg = COLORS[fi];
      Color bg = COLORS[bi];

      if (isForbiddenPair(fg, bg))
        continue;

      if (k == targetIndex) {
        chosenFG = fg;
        chosenBG = bg;
        goto done;
      }

      ++k;
    }
  }

done:
  Terminal.setForegroundColor(chosenFG);
  Terminal.setBackgroundColor(chosenBG);
}

// =====================
//  Parsing / normalizzazione testo
// =====================

static String between(const String& s, const String& open, const String& close, int from)
{
  int a = s.indexOf(open, from);
  if (a < 0)
    return "";

  a += open.length();

  int b = s.indexOf(close, a);
  if (b < 0)
    return "";

  return s.substring(a, b);
}

static String stripCDATA(String s)
{
  s.trim();

  if (s.startsWith("<![CDATA[")) {
    s.remove(0, 9);

    int pos = s.indexOf("]]>");
    if (pos >= 0)
      s = s.substring(0, pos);
  }

  s.trim();
  return s;
}

static void decodeHtmlEntities(String &s)
{
  s.replace("&amp;", "&");
  s.replace("&lt;", "<");
  s.replace("&gt;", ">");
  s.replace("&quot;", "\"");
  s.replace("&nbsp;", " ");
  s.replace("&#39;", "'");
  s.replace("&#039;", "'");
  s.replace("&#8220;", "'");
  s.replace("&#8221;", "'");
  s.replace("&#8216;", "'");
  s.replace("&#8217;", "'");
  s.replace("&#8211;", "-");
  s.replace("&#8230;", "...");

  char b3[2] = { (char)0xB3, '\0' };
  s.replace("&#124;", b3);
}

static void normalizeUtf8Punctuation(String &s)
{
  s.replace("\xE2\x80\x99", "'");
  s.replace("\xE2\x80\x98", "'");
  s.replace("\xE2\x80\x9C", "\"");
  s.replace("\xE2\x80\x9D", "\"");
  s.replace("\xE2\x80\x93", "-");
  s.replace("\xE2\x80\x94", "-");
  s.replace("\xE2\x80\xA6", "...");
}

static inline void replaceUtf8WithByte(String &s, const char *utf8, uint8_t b)
{
  char out[2];
  out[0] = (char)b;
  out[1] = '\0';
  s.replace(utf8, out);
}

static String toDisplayCharset(String s)
{
  replaceUtf8WithByte(s, "à", 0x85);
  replaceUtf8WithByte(s, "è", 0x8A);
  replaceUtf8WithByte(s, "é", 0x82);
  replaceUtf8WithByte(s, "ì", 0x8D);
  replaceUtf8WithByte(s, "ò", 0x95);
  replaceUtf8WithByte(s, "ù", 0x97);

  replaceUtf8WithByte(s, "À", 0x85);
  replaceUtf8WithByte(s, "È", 0x8A);
  replaceUtf8WithByte(s, "É", 0x90);
  replaceUtf8WithByte(s, "Ì", 0x8D);
  replaceUtf8WithByte(s, "Ò", 0x95);
  replaceUtf8WithByte(s, "Ù", 0x97);

  replaceUtf8WithByte(s, "ç", 0x87);
  replaceUtf8WithByte(s, "Ç", 0x80);

  return s;
}

static String shortenPubDate(String s)
{
  s.trim();

  int comma = s.indexOf(',');
  if (comma >= 0 && s.length() > comma + 2)
    s = s.substring(comma + 2);

  int lastColon = s.lastIndexOf(':');
  if (lastColon >= 0 && lastColon + 3 <= (int)s.length())
    s = s.substring(0, lastColon + 3);

  s.trim();
  return s;
}

bool parsePubDateToEpoch(const String &in, time_t &out)
{
  String s = in;
  s.trim();

  int comma = s.indexOf(',');
  if (comma >= 0 && s.length() > comma + 1)
    s = s.substring(comma + 1);

  s.trim();

  int lastSpace = s.lastIndexOf(' ');
  if (lastSpace > 0 && lastSpace + 1 < (int)s.length()) {
    char tzFirst = s[lastSpace + 1];

    if (tzFirst == '+' || tzFirst == '-') {
      s = s.substring(0, lastSpace);
      s.trim();
    }
  }

  int day, year, hh, mm, ss;
  char monStr[4] = {0, 0, 0, 0};

  if (sscanf(s.c_str(), "%d %3s %d %d:%d:%d", &day, monStr, &year, &hh, &mm, &ss) != 6)
    return false;

  const char* mons[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };

  int month = 0;
  for (int i = 0; i < 12; ++i) {
    if (strcasecmp(monStr, mons[i]) == 0) {
      month = i + 1;
      break;
    }
  }

  if (month == 0)
    return false;

  struct tm t;
  memset(&t, 0, sizeof(t));

  t.tm_year = year - 1900;
  t.tm_mon  = month - 1;
  t.tm_mday = day;
  t.tm_hour = hh;
  t.tm_min  = mm;
  t.tm_sec  = ss;
  t.tm_isdst = -1;

  time_t ep = mktime(&t);
  if (ep == (time_t)-1)
    return false;

  out = ep;
  return true;
}

String cleanNewsTitle(const String &input)
{
  String s = input;

  for (int i = 0; i < s.length(); ++i) {
    char c = s[i];

    if (c == '\r' || c == '\n' || c == '\t')
      s[i] = ' ';
  }

  String out;
  out.reserve(s.length());

  int n = s.length();

  for (int i = 0; i < n; ) {
    while (i < n && s[i] == ' ')
      ++i;

    if (i >= n)
      break;

    int start = i;

    while (i < n && s[i] != ' ')
      ++i;

    int end = i;
    String token = s.substring(start, end);

    int ts = 0;
    int te = token.length();

    while (ts < te && ispunct((unsigned char)token[ts]))
      ++ts;

    while (te > ts && ispunct((unsigned char)token[te - 1]))
      --te;

    String core = token.substring(ts, te);
    core.trim();

    if (core != "DIRETTA" && core != "LIVE" && core != "VIDEO") {
      if (out.length() > 0)
        out += ' ';

      out += token;
    }
  }

  out.trim();
  return out;
}

// =====================
//  Download RSS
// =====================

void clearNewsDefaults()
{
  for (int i = 0; i < MAX_NEWS; ++i) {
    newsTitles[i] = "news non disponibili";
    newsDates[i]  = "";
  }
}

void fetchRSSNews(const char* url, const char* label)
{
  currentFeedLabel = label;
  clearNewsDefaults();

  if (WiFi.status() != WL_CONNECTED)
    return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  http.setConnectTimeout(10000);
  http.useHTTP10(true);

  http.begin(client, url);
  http.setUserAgent("ESP32/Arduino");
  http.addHeader("Accept", "application/rss+xml,application/xml,text/xml;q=0.9,*/*;q=0.8");
  http.addHeader("Accept-Encoding", "identity");
  http.addHeader("Connection", "close");

  int code = http.GET();

  if (code != HTTP_CODE_OK) {
    http.end();
    return;
  }

  WiFiClient &stream = http.getStream();

  String titlesTmp[MAX_NEWS];
  String datesDispTmp[MAX_NEWS];
  time_t epochsTmp[MAX_NEWS];

  int count = 0;
  bool insideItem = false;
  String itemBlock;

  while ((stream.connected() || stream.available()) && count < MAX_NEWS) {
    String line = stream.readStringUntil('\n');

    if (!line.length() && !stream.available())
      break;

    String lineTrim = line;
    lineTrim.trim();

    if (lineTrim.length() == 0)
      continue;

    if (!insideItem) {
      int posItem = lineTrim.indexOf("<item");

      if (posItem >= 0) {
        insideItem = true;
        itemBlock = lineTrim.substring(posItem);
        itemBlock += '\n';
      }
    } else {
      itemBlock += lineTrim;
      itemBlock += '\n';

      if (lineTrim.indexOf("</item>") >= 0) {
        String title = between(itemBlock, "<title>", "</title>");
        String pubDate = between(itemBlock, "<pubDate>", "</pubDate>");

        if (title.length() > 0) {
          title = stripCDATA(title);
          decodeHtmlEntities(title);
          normalizeUtf8Punctuation(title);
          title = toDisplayCharset(title);
          title.trim();
          title = cleanNewsTitle(title);

          if (title.length() == 0)
            title = "titolo vuoto";
        } else {
          title = "titolo mancante";
        }

        time_t ep = 0;
        String dispDate = "";

        if (pubDate.length() > 0) {
          String raw = stripCDATA(pubDate);
          raw = toDisplayCharset(raw);

          if (parsePubDateToEpoch(raw, ep))
            dispDate = shortenPubDate(raw);
          else
            dispDate = "data non valida";
        } else {
          dispDate = "data non disponibile";
        }

        titlesTmp[count] = title;
        datesDispTmp[count] = dispDate;
        epochsTmp[count] = ep;
        count++;

        insideItem = false;
        itemBlock = "";
      }
    }
  }

  http.end();

  if (count == 0)
    return;

  int idx[MAX_NEWS];
  for (int i = 0; i < count; ++i)
    idx[i] = i;

  for (int i = 0; i < count - 1; ++i) {
    for (int j = i + 1; j < count; ++j) {
      if (epochsTmp[idx[j]] > epochsTmp[idx[i]]) {
        int tmp = idx[i];
        idx[i] = idx[j];
        idx[j] = tmp;
      }
    }
  }

  for (int k = 0; k < count; ++k) {
    int src = idx[k];
    newsTitles[k] = titlesTmp[src];
    newsDates[k] = datesDispTmp[src];
  }

  for (int k = count; k < MAX_NEWS; ++k) {
    newsTitles[k] = "";
    newsDates[k] = "";
  }
}

// =====================
//  Stampa terminale
// =====================

void printLine(int row, const String &text)
{
  int cols = Terminal.getColumns();
  if (cols <= 0)
    cols = 80;

  char esc[16];
  snprintf(esc, sizeof(esc), "\x1b[%d;1H", row + 1);
  Terminal.write(esc);

  Terminal.write(text.c_str());

  int len = text.length();
  for (int i = len; i < cols; ++i)
    Terminal.write(' ');
}

int printWrapped(int startRow, const String &text)
{
  int cols = Terminal.getColumns();
  int rows = Terminal.getRows();

  if (cols <= 0)
    cols = 80;

  if (rows <= 0)
    rows = 25;

  int pos = 0;
  int row = startRow;

  while (pos < text.length() && row < rows) {
    int remaining = text.length() - pos;
    int take = remaining < cols ? remaining : cols;
    int end = pos + take;

    if (end < (int)text.length()) {
      int lastSpace = -1;

      for (int i = pos; i < end; ++i) {
        if (text[i] == ' ')
          lastSpace = i;
      }

      if (lastSpace > pos + 5)
        end = lastSpace;
    }

    String line = text.substring(pos, end);
    line.trim();

    printLine(row, line);
    row++;

    pos = end;

    while (pos < text.length() && text[pos] == ' ')
      pos++;
  }

  return row;
}

// =====================
//  Orologio e schermata
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

void updateTimeLine()
{
  setDefaultInfoColors();

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 200)) {
    printLine(0, "Data/ora non disponibile");
    return;
  }

  char buf[96];
  snprintf(buf, sizeof(buf), "%s %d %s %d %02d:%02d:%02d",
           giorniSettimana[timeinfo.tm_wday],
           timeinfo.tm_mday,
           mesiAnno[timeinfo.tm_mon],
           1900 + timeinfo.tm_year,
           timeinfo.tm_hour,
           timeinfo.tm_min,
           timeinfo.tm_sec);

  String baseLine = String(buf);

  int cols = Terminal.getColumns();
  if (cols <= 0)
    cols = 80;

  String title = toDisplayCharset(currentFeedLabel);
  title.trim();

  int baseLen = baseLine.length();
  int titleLen = title.length();

  if (titleLen > 0) {
    int maxTitle = cols - (baseLen + 1);

    if (maxTitle <= 0) {
      title = "";
      titleLen = 0;
    } else if (titleLen > maxTitle) {
      title = title.substring(0, maxTitle);
      titleLen = title.length();
    }
  }

  String line = baseLine;

  if (titleLen > 0) {
    int spaces = cols - baseLen - titleLen;
    if (spaces < 1)
      spaces = 1;

    for (int i = 0; i < spaces; ++i)
      line += ' ';

    line += title;
  }

  printLine(0, line);
}


void drawStaticInfo()
{
  int row = 1;
  int rows = Terminal.getRows();

  if (rows <= 0)
    rows = 25;

  setDefaultInfoColors();

  if (WiFi.status() != WL_CONNECTED)
    printLine(row++, "                          WiFi non connesso");

  String refDate = "";

  for (int i = 0; i < MAX_NEWS && row < rows; ++i) {
    setDefaultInfoColors();

    String raw = newsDates[i];
    if (raw.length() == 0)
      raw = "data non disponibile";

    String toPrint = raw;

    int lastSp = raw.lastIndexOf(' ');
    if (lastSp > 0 && lastSp < (int)raw.length() - 1 &&
        raw != "data non disponibile" &&
        raw != "data non valida") {

      String datePart = raw.substring(0, lastSp);
      String timePart = raw.substring(lastSp + 1);
      timePart.trim();

      if (refDate.length() == 0) {
        refDate = datePart;
        toPrint = raw;
      } else if (datePart == refDate) {
        toPrint = timePart;
      } else {
        refDate = datePart;
        toPrint = raw;
      }
    }

    printLine(row++, toPrint);
    if (row >= rows)
      break;

    applyNewsColor(newsTitles[i]);
    row = printWrapped(row, newsTitles[i]);

    if (row >= rows)
      break;
  }

  setDefaultInfoColors();
}

// =====================
//  setup / loop
// =====================

void setup()
{
  pinMode(BTN_NEXT_RSS, INPUT_PULLUP);
  delay(80);

  btnNextRSS.attach(BTN_NEXT_RSS, INPUT_PULLUP);
  btnNextRSS.interval(80);
  btnNextRSS.setPressedState(LOW);

  PS2Controller.begin(PS2Preset::KeyboardPort0);

  DisplayController.begin();
  DisplayController.setResolution();

  Terminal.begin(&DisplayController);
  Terminal.connectLocally();
  Terminal.setBackgroundColor(Color::Black);
  Terminal.setForegroundColor(Color::BrightGreen);
  Terminal.clear();
  Terminal.enableCursor(false);

  Terminal.write("251208 Valk Research Ltd - ESP32 FabGL WiFi - NEWS \r\n");

  bool forceConfigMode = digitalRead(BTN_NEXT_RSS) == LOW;
  if (forceConfigMode)
    Terminal.write("Modalita configurazione: AP ESP32-RSS-SETUP, pagina http://192.168.4.1/config.html\r\n");

  WiFiPortal.setRSSDefaults(DEFAULT_RSS_FEEDS, NUM_DEFAULT_RSS_FEEDS);
  WiFiPortal.begin(forceConfigMode);
  lastRSSConfigVersion = WiFiPortal.rssConfigVersion();

  if (WiFiPortal.isConnected()) {
    setupTime();
    wifiTimeReady = true;
    delay(2000);
  }

  STAZIONE_RSS = clampFeedIndex(STAZIONE_RSS);
  currentFeedIndex = STAZIONE_RSS;
  requestedFeedIndex = STAZIONE_RSS;
  requestedFeedChangeTime = millis();

  fetchCurrentRSS();
  lastFetch = millis();

  updateTimeLine();
  drawStaticInfo();

  lastClockUpdate = millis();
}

void loop()
{
  unsigned long now = millis();

  WiFiPortal.handleClient();

  // Se il WiFi viene configurato dopo l'avvio, sincronizza ora e forza un primo fetch.
  if (WiFiPortal.isConnected() && !wifiTimeReady) {
    setupTime();
    wifiTimeReady = true;
    firstFetchAfterWiFi = true;
  }

  if (firstFetchAfterWiFi && WiFiPortal.isConnected()) {
    firstFetchAfterWiFi = false;
    refreshCurrentRSSOnScreen();
  }

  handleRSSConfigChangedFromWeb();
  handleNextRSSButton();
  handleRSSStationSelection();

  // Aggiorna SOLO la riga dell'orologio ogni secondo.
  if (now - lastClockUpdate >= 1000) {
    lastClockUpdate = now;
    updateTimeLine();
  }

  // Aggiorna periodicamente le news restando sullo stesso feed.
  if (WiFi.status() == WL_CONNECTED && now - lastFetch >= FETCH_REFRESH_MS)
    refreshCurrentRSSOnScreen();
}
