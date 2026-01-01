/*
  FabGL WiFi + News ANSA homepage (no flicker)

  - Connessione WiFi usando WIFI_SSID / WIFI_PASSWORD da config.h
  - Sync ora via NTP (Europa/Roma)
  - Fetch RSS ANSA homepage (ansait_rss.xml)
  - Mostra:
      Riga 0  : data/ora locale
      Riga 1+ : WiFi, ultimo accesso, news ANSA
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
//  Tempistiche Aggiornamenti
// =====================

const uint32_t FETCH_REFRESH_MS = 1UL * 60UL * 1000UL; // durata schermata notizie
const int RSS_BAR_LEN = 20;   // barra di 20 caratteri

unsigned long lastFetch    = 0;
unsigned long lastClockUpdate    = 0;

// =====================
//  ANSA RSS multipli
// =====================

struct AnsaFeed {
  const char* label;  // testo da mostrare in alto a destra
  const char* url;    // URL del feed RSS
};

const AnsaFeed ANSA_FEEDS[] = {
  { "IL FATTO QUOTIDIANO ", "https://www.ilfattoquotidiano.it/feed/"},
  { "THE GUARDIAN", "https://www.theguardian.com/world/rss"},
  { "NEW YORK TIMES", "https://rss.nytimes.com/services/xml/rss/nyt/World.xml"},
  { "ANSA CRONACA", "https://www.ansa.it/sito/notizie/cronaca/cronaca_rss.xml" },
  { "ANSA HOME",    "https://www.ansa.it/sito/ansait_rss.xml" },
  { "ANSA MONDO",   "https://www.ansa.it/sito/notizie/mondo/mondo_rss.xml" },
};

const int NUM_ANSA_FEEDS = sizeof(ANSA_FEEDS) / sizeof(ANSA_FEEDS[0]);
int currentFeedIndex = 0;

// Label del feed attualmente visualizzato (per la riga in alto a destra)
String currentFeedLabel = "";


const int   MAX_NEWS = 20;
String      newsTitles[MAX_NEWS];
String      newsDates[MAX_NEWS];

// ======= colori ==========

struct Combo {
  Color fg;
  Color bg;
};

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

// Combinazioni totali fg!=bg: 7 * 6 = 42
// Ne escludiamo 2: (Green,Cyan) e (Cyan,Green) → 40 valide
const int FORBIDDEN_PAIRS = 15;
constexpr int NUM_ALLOWED_COMBOS = NUM_COLORS * (NUM_COLORS - 1) - FORBIDDEN_PAIRS; 

// --- Pulsante "RSS successivo" ---
const int BTN_NEXT_RSS = 33;        // pin fisico del pulsante (collegato a GND)

// quanto tempo il segnale deve rimanere stabile per essere considerato valido
const unsigned long BTN_DEBOUNCE_MS    = 40;   // 40 ms bastano
// tempo minimo tra due pressioni valide (per evitare doppi click)
const unsigned long BTN_MIN_INTERVAL_MS = 600; // 0,6 s

bool btnLastStableState  = HIGH;     // con INPUT_PULLUP: "non premuto" = HIGH
bool btnLastReading      = HIGH;
unsigned long btnLastChangeTime = 0;



void handleNextRSSButton() {
  static unsigned long lastPressTime = 0;  // ultima pressione accettata

  unsigned long now = millis();
  int reading = digitalRead(BTN_NEXT_RSS);

  // Se cambia la lettura “grezza”, resetto il timer di stabilità
  if (reading != btnLastReading) {
    btnLastReading     = reading;
    btnLastChangeTime  = now;
  }

  // Se il segnale resta stabile per almeno BTN_DEBOUNCE_MS
  if (now - btnLastChangeTime >= BTN_DEBOUNCE_MS) {

    // È cambiato lo stato stabile?
    if (reading != btnLastStableState) {
      btnLastStableState = reading;

      // Fronte di discesa: HIGH -> LOW (pulsante premuto)
      if (btnLastStableState == LOW) {

        // Controllo anche l'intervallo minimo tra pressioni
        if (now - lastPressTime >= BTN_MIN_INTERVAL_MS) {
          lastPressTime = now;

          // QUI avviene un solo click "logico"
          if (WiFi.status() == WL_CONNECTED) {
            goToNextRSSFeed();
          }
        }
      }
    }
  }
}


void goToNextRSSFeed() {
  unsigned long now = millis();
  lastFetch = now;   // così la barra si resetta subito

  // passa al feed successivo (0 → 1 → 2 → ... → 0)
  currentFeedIndex = (currentFeedIndex + 1) % NUM_ANSA_FEEDS;

  fetchAnsaNews(ANSA_FEEDS[currentFeedIndex].url,
                ANSA_FEEDS[currentFeedIndex].label);

  drawStaticInfo();   // ridisegna schermo (titoli ecc.)
}


String buildRSSProgressBar() {
  String bar;
  bar.reserve(RSS_BAR_LEN + 1);

  // uno spazio per separare dall'orario
  bar += ' ';

  // Se non abbiamo ancora fatto un fetch, barra vuota
  if (FETCH_REFRESH_MS == 0 || lastFetch == 0) {
    for (int i = 0; i < RSS_BAR_LEN; ++i)
      bar += ' ';
    return bar;
  }

  unsigned long now = millis();
  unsigned long elapsed = now - lastFetch;

  // protezione overflow (nel caso millis faccia il giro)
  if ((long)elapsed < 0)
    elapsed = 0;

  if (elapsed > FETCH_REFRESH_MS)
    elapsed = FETCH_REFRESH_MS;

  float ratio = (float) elapsed / (float) FETCH_REFRESH_MS;
  int filled = (int) (ratio * RSS_BAR_LEN + 0.5f);
  if (filled < 0) filled = 0;
  if (filled > RSS_BAR_LEN) filled = RSS_BAR_LEN;

  for (int i = 0; i < RSS_BAR_LEN; ++i) {
    // puoi cambiare '#' con '=' o altro carattere se preferisci
    bar += (i < filled) ? '#' : ' ';
  }

  return bar;
}


bool isForbiddenPair(Color fg, Color bg) {
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


String cleanNewsTitle(const String &input) {
  String s = input;

  // CR/LF/TAB -> spazio
  for (int i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (c == '\r' || c == '\n' || c == '\t')
      s[i] = ' ';
  }

  String out;
  out.reserve(s.length());

  int n = s.length();
  for (int i = 0; i < n; ) {
    // salta spazi
    while (i < n && s[i] == ' ')
      ++i;
    if (i >= n) break;

    // token [start,end)
    int start = i;
    while (i < n && s[i] != ' ')
      ++i;
    int end = i;

    String token = s.substring(start, end);

    // togli punteggiatura a inizio/fine per il confronto
    int ts = 0;
    int te = token.length();
    while (ts < te && ispunct((unsigned char)token[ts]))
      ++ts;
    while (te > ts && ispunct((unsigned char)token[te - 1]))
      --te;

    String core = token.substring(ts, te);
    core.trim();

    String coreUpper = core;
    //coreUpper.toUpperCase();

    // filtra parole indesiderate
    if (coreUpper != "DIRETTA" &&
        coreUpper != "LIVE"    &&
        coreUpper != "VIDEO") {

      if (out.length() > 0)
        out += ' ';       // singolo spazio
      out += token;       // mantieni il token (con eventuale punteggiatura)
    }
  }

  out.trim();
  return out;
}

void setDefaultInfoColors() {
  Terminal.setForegroundColor(Color::White);  
  Terminal.setBackgroundColor(Color::Black);
}

void applyNewsColor(const String &text) {
  // hash semplice del testo (come avevi tu)
  uint16_t h = 0;
  for (int i = 0; i < text.length(); ++i) {
    h = h * 31 + (uint8_t)text[i];
  }

  // riduco l’hash al numero di combinazioni valide
  int targetIndex = h % NUM_ALLOWED_COMBOS;

  // Scorro tutte le coppie fg/bg, saltando:
  //  - fg == bg
  //  - BrightGreen/BrightCyan in qualsiasi ordine
  int k = 0;
  Color chosenFG = Color::BrightYellow; // default qualsiasi
  Color chosenBG = Color::Black;

  for (int fi = 0; fi < NUM_COLORS; ++fi) {
    for (int bi = 0; bi < NUM_COLORS; ++bi) {
      if (fi == bi)
        continue;  // niente fg == bg

      Color fg = COLORS[fi];
      Color bg = COLORS[bi];

      if (isForbiddenPair(fg, bg))
        continue;  // salta verde/ciano e ciano/verde

      if (k == targetIndex) {
        chosenFG = fg;
        chosenBG = bg;
        goto done; // usciamo da entrambi i for
      }
      ++k;
    }
  }

done:
  Terminal.setForegroundColor(chosenFG);
  Terminal.setBackgroundColor(chosenBG);
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

  int maxTentativi = 120;           // 120 * 500ms ≈ 60 secondi
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
//  Helper parsing HTML 
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

// =====================
//  RSS ANSA helpers
// =====================

// Rimuove <![CDATA[ ... ]]> se presente
static String stripCDATA(String s) {
  s.trim();
  if (s.startsWith("<![CDATA[")) {
    s.remove(0, 9); // "<![CDATA["
    int pos = s.indexOf("]]>");
    if (pos >= 0) {
      s = s.substring(0, pos);
    }
  }
  s.trim();
  return s;
}

// Decodifica alcune entity HTML basiche
static void decodeHtmlEntities(String &s) {
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
  // &#124; -> byte singolo 0xB3
  char b3[2] = { (char)0xB3, '\0' };
  s.replace("&#124;", b3);
}

// Sostituisce vocali accentate italiane con ASCII
// static void foldItalianAccents(String &s) {
//   s.replace("à", "a");
//   s.replace("è", "e");
//   s.replace("é", "e");
//   s.replace("ì", "i");
//   s.replace("ò", "o");
//   s.replace("ù", "u");

//   s.replace("À", "A");
//   s.replace("È", "E");
//   s.replace("É", "E");
//   s.replace("Ì", "I");
//   s.replace("Ò", "O");
//   s.replace("Ù", "U");

//   s.replace("ç", "c");
//   s.replace("Ç", "C");
// }

// Sostituisce "token" UTF-8 con un singolo byte (codepage/charset target)
static inline void replaceUtf8WithByte(String &s, const char *utf8, uint8_t b) {
  char out[2];
  out[0] = (char)b;  // byte di destinazione (anche > 0x7F)
  out[1] = '\0';
  s.replace(utf8, out);
}

// Sostituisce vocali accentate italiane con ASCII/codepage
static void foldItalianAccents(String &s) {
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
}


// Restituisce versione solo-ASCII: elimina byte >= 0x80 dopo aver “appiattito” le accentate
static String toASCII(String s) {
  foldItalianAccents(s);
  return s;
}

// Accorcia il pubDate RSS (es: "Sun, 07 Dec 2025 08:31:00 +0100")
// in qualcosa tipo "07 Dec 2025 08:31"
static String shortenPubDate(String s) {
  s.trim();
  // Se è nel formato standard "DDD, DD MMM YYYY HH:MM:SS +ZZZZ"
  // prendiamo da posizione dopo la virgola fino a prima degli ultimi 6 caratteri
  int comma = s.indexOf(',');
  if (comma >= 0 && s.length() > comma + 2) {
    s = s.substring(comma + 2); // salta "Sun, "
  }
  // togliamo timezone e i secondi se presente
  // cerchiamo l'ultima ':' e tagliamo dopo i minuti
  int lastColon = s.lastIndexOf(':');
  if (lastColon >= 0 && lastColon + 3 <= (int)s.length()) {
    // "HH:MM:SS ..." → mantieni "HH:MM"
    s = s.substring(0, lastColon + 3); // include minuti
  }
  s.trim();
  return s;
}

// Converte un pubDate RSS tipo "Sun, 07 Dec 2025 08:31:00 +0100"
// in un time_t (epoch). Ritorna true se OK.
bool parsePubDateToEpoch(const String &in, time_t &out)
{
  String s = in;
  s.trim();

  // Rimuovi il giorno della settimana ("Sun, ")
  int comma = s.indexOf(',');
  if (comma >= 0 && s.length() > comma + 1) {
    s = s.substring(comma + 1);
  }
  s.trim(); // ora es: "07 Dec 2025 08:31:00 +0100"

  // Rimuovi eventuale timezone finale ("+0100")
  int lastSpace = s.lastIndexOf(' ');
  if (lastSpace > 0 && lastSpace + 1 < (int) s.length()) {
    // se dopo l'ultimo spazio troviamo un '+' o '-' lo consideriamo timezone
    char tzFirst = s[lastSpace + 1];
    if (tzFirst == '+' || tzFirst == '-') {
      s = s.substring(0, lastSpace);
      s.trim();
    }
  }

  // Ora atteso: "07 Dec 2025 08:31:00"
  int day, year, hh, mm, ss;
  char monStr[4] = {0, 0, 0, 0};

  if (sscanf(s.c_str(), "%d %3s %d %d:%d:%d",
             &day, monStr, &year, &hh, &mm, &ss) != 6) {
    return false;
  }

  // Mappa mese "Dec" -> 12
  const char* mons[] = {
    "Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec"
  };
  int month = 0;
  for (int i = 0; i < 12; ++i) {
    if (strcasecmp(monStr, mons[i]) == 0) {
      month = i + 1;
      break;
    }
  }
  if (month == 0) return false;

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
  if (ep == (time_t)-1) {
    return false;
  }

  out = ep;
  return true;
}

// Estrae fino a MAX_NEWS titoli + date dai primi <item> del feed ANSA
void fetchAnsaNews(const char* url, const char* label)
{
  // memorizzo il nome del feed corrente (per mostrarlo in alto a destra)
  currentFeedLabel = label;

  // opzionale: puoi lasciare o togliere questo reset
  for (int i = 0; i < MAX_NEWS; ++i) {
    newsTitles[i] = "news non disponibili";
    newsDates[i]  = "";
  }

  if (WiFi.status() != WL_CONNECTED)
    return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  http.setConnectTimeout(10000);
  http.useHTTP10(true);   // aiuta in alcuni casi con stream

  http.begin(client, url);

  // ========== user agent ====
  http.setUserAgent("ESP32/Arduino");
  http.addHeader("Accept", "application/rss+xml,application/xml,text/xml;q=0.9,*/*;q=0.8");
  http.addHeader("Accept-Encoding", "identity");
  http.addHeader("Connection", "close");
  // ============================

  int code = http.GET();

  //Serial.printf("Fetch RSS [%s] -> HTTP %d\r\n", label, code);

  if (code != HTTP_CODE_OK) {
    http.end();
    return;
  }

  // --- lettura a streaming, NIENTE getString() ---
  WiFiClient &s = http.getStream();

  String titlesTmp[MAX_NEWS];
  String datesDispTmp[MAX_NEWS];
  time_t epochsTmp[MAX_NEWS];

  int count = 0;
  bool insideItem = false;
  String itemBlock;

  while ((s.connected() || s.available()) && count < MAX_NEWS) {

    String line = s.readStringUntil('\n');
    if (!line.length() && !s.available()) {
      // niente più dati
      break;
    }

    // lavoro con una versione "trimmed" per cercare i tag
    String lineTrim = line;
    lineTrim.trim();
    if (lineTrim.length() == 0)
      continue;

    if (!insideItem) {
      // cerco l'inizio di <item ...>
      int posItem = lineTrim.indexOf("<item");
      if (posItem >= 0) {
        insideItem = true;
        itemBlock  = lineTrim.substring(posItem);
        itemBlock += '\n';
      }
    } else {
      // siamo già dentro un item, accumulo
      itemBlock += lineTrim;
      itemBlock += '\n';

      int posEnd = lineTrim.indexOf("</item>");
      if (posEnd >= 0) {
        // abbiamo un item completo in itemBlock: PARSING

        String title   = between(itemBlock, "<title>", "</title>");
        String pubDate = between(itemBlock, "<pubDate>", "</pubDate>");

        // --- pulizia titolo ---
        if (title.length() > 0) {
          title = stripCDATA(title);
          decodeHtmlEntities(title);
          normalizeUtf8Punctuation(title);
          title = toASCII(title);
          title.trim();
          title = cleanNewsTitle(title);
          if (title.length() == 0)
            title = "titolo vuoto";
        } else {
          title = "titolo mancante";
        }

        // --- pulizia data/epoch ---
        time_t ep = 0;
        String dispDate = "";

        if (pubDate.length() > 0) {
          String raw = pubDate;
          raw = stripCDATA(raw);
          raw = toASCII(raw);
          if (parsePubDateToEpoch(raw, ep)) {
            dispDate = shortenPubDate(raw);
          } else {
            dispDate = "data non valida";
          }
        } else {
          dispDate = "data non disponibile";
          ep = 0;
        }

        if (count < MAX_NEWS) {
          titlesTmp[count]    = title;
          datesDispTmp[count] = dispDate;
          epochsTmp[count]    = ep;
          count++;
        }

        // pronto per l’eventuale prossimo <item>
        insideItem = false;
        itemBlock  = "";
      }
    }
  }

  http.end();

  //Serial.printf("RSS [%s]: trovate %d news\r\n", label, count);

  // se non ho trovato nessuna news valida, esco lasciando i default
  if (count == 0)
    return;

  // ===============================
  // ordinamento per epoch decrescente
  // ===============================

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

  int outN = (count < MAX_NEWS) ? count : MAX_NEWS;

  for (int k = 0; k < outN; ++k) {
    int src = idx[k];
    newsTitles[k] = titlesTmp[src];
    newsDates[k]  = datesDispTmp[src];
  }

  // se vuoi “pulire” le eventuali posizioni residue:
  for (int k = outN; k < MAX_NEWS; ++k) {
    newsTitles[k] = "";
    newsDates[k]  = "";
  }
}

// =====================
//  Utility stampa linee / word-wrap
// =====================

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

// Stampa text su più righe (a partire da startRow) spezzando per parole.
// Ritorna la prima riga libera successiva.
int printWrapped(int startRow, const String &text)
{
  int cols = Terminal.getColumns();
  int rows = Terminal.getRows();
  if (cols <= 0) cols = 80;
  if (rows <= 0) rows = 25;

  String s = text;
  int pos = 0;
  int row = startRow;

  while (pos < s.length() && row < rows) {
    int remaining = s.length() - pos;
    int take = remaining < cols ? remaining : cols;

    // tentiamo di spezzare all'ultimo spazio entro "take"
    int end = pos + take;
    if (end < (int)s.length()) {
      int lastSpace = -1;
      for (int i = pos; i < end; ++i) {
        if (s[i] == ' ')
          lastSpace = i;
      }
      if (lastSpace > pos + 5) {  // evita spezzare troppo presto
        end = lastSpace;          // spezza prima dell'ultimo spazio
      }
    }

    String line = s.substring(pos, end);
    line.trim();
    printLine(row, line);
    row++;

    pos = end;
    // salta eventuali spazi successivi
    while (pos < s.length() && s[pos] == ' ')
      pos++;
  }

  return row;
}

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
  setDefaultInfoColors();   // date/orologio e label feed in bianco su nero

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

  String timeStr = String(buf);

  int cols = Terminal.getColumns();
  if (cols <= 0) cols = 80;

  // Usa la label del feed corrente, “appiattita” in ASCII
  String title = toASCII(currentFeedLabel);
  title.trim();

  // *** NUOVO: aggiungo la barra dopo la data/ora ***
  String bar = buildRSSProgressBar();

  // base: data/ora + barra
  String baseLine = timeStr + bar;

  int L1 = baseLine.length();   // lunghezza (data/ora + barra)
  int L2 = title.length();

  // Se non ci sta, accorcia il titolo
  if (L2 > 0) {
    int maxTitle = cols - (L1 + 1); // almeno 1 spazio tra barra e titolo
    if (maxTitle <= 0) {
      // non c'è spazio per il titolo
      title = "";
      L2 = 0;
    } else if (L2 > maxTitle) {
      L2 = maxTitle;
      title = title.substring(0, L2);
    }
  }

  String line = baseLine;

  if (title.length() > 0) {
    // vogliamo che il titolo finisca all'ultima colonna
    int spaces = cols - L1 - L2;
    if (spaces < 1) spaces = 1;
    for (int i = 0; i < spaces; ++i)
      line += ' ';
    line += title;
  }

  printLine(0, line);
}

// Riscrive tutte le righe statiche (WiFi, ANSA)
void drawStaticInfo()
{
  int row = 1;
  int rows = Terminal.getRows();
  if (rows <= 0) rows = 25;

  setDefaultInfoColors();  // tutto info in grigio

  // Riga 1: WiFi
  if (WiFi.status() == WL_CONNECTED) {
    //String s = "Connesso a SSID ";
    //s += WiFi.SSID();
    //s += " con IP ";
    //s += WiFi.localIP().toString();
    // printLine(row++, s);
  } else {
    printLine(row++, "                          WiFi non connesso");
  }



   // News ANSA
  String refDate = "";  // data completa (es. "7 Dec 2025") della prima notizia

  for (int i = 0; i < MAX_NEWS && row < rows; ++i) {

    // --- DATA/ORA NOTIZIA --- (in grigio)
    setDefaultInfoColors();
    String raw = newsDates[i];
    if (raw.length() == 0)
      raw = "data non disponibile";

    String toPrint = raw;

    // Cerchiamo l'ULTIMO spazio: tutto prima = data, tutto dopo = ora
    int lastSp = raw.lastIndexOf(' ');
    if (lastSp > 0 && lastSp < (int)raw.length() - 1 &&
        raw != "data non disponibile" &&
        raw != "data non valida") {

      String datePart = raw.substring(0, lastSp);       // es. "7 Dec 2025"
      String timePart = raw.substring(lastSp + 1);      // es. "08:31" o "08:31:00"
      timePart.trim();

      if (refDate.length() == 0) {
        // Prima notizia: memorizzo la data e stampo data+ora
        refDate = datePart;
        toPrint = raw;
      } else {
        if (datePart == refDate) {
          // Stessa data della prima: mostra solo l'ora
          toPrint = timePart;
        } else {
          // Data diversa: aggiorno refDate e mostro data+ora
          refDate = datePart;
          toPrint = raw;
        }
      }
    }

    printLine(row++, toPrint);
    if (row >= rows) break;

    // --- TITOLO NOTIZIA --- (colorato)
    applyNewsColor(newsTitles[i]);
    row = printWrapped(row, newsTitles[i]);
    if (row >= rows) break;

    // Torno al "grigio" per le righe successive
    setDefaultInfoColors();
  }
}

// Normalizza punteggiatura UTF-8 in caratteri semplici (o byte codepage)
static void normalizeUtf8Punctuation(String &s) {
  // ’  (E2 80 99) -> '
  s.replace("\xE2\x80\x99", "'");

  // opzionali ma utilissimi nei feed:
  // ‘  (E2 80 98) -> '
  s.replace("\xE2\x80\x98", "'");

  // “  (E2 80 9C) -> "
  s.replace("\xE2\x80\x9C", "\"");

  // ”  (E2 80 9D) -> "
  s.replace("\xE2\x80\x9D", "\"");

  // – (E2 80 93) -> -
  s.replace("\xE2\x80\x93", "-");

  // — (E2 80 94) -> -
  s.replace("\xE2\x80\x94", "-");

  // … (E2 80 A6) -> ...
  s.replace("\xE2\x80\xA6", "...");
}


// =====================
//  setup / loop
// =====================

void setup()
{
  //Serial.begin(115200);

  pinMode(BTN_NEXT_RSS, INPUT_PULLUP);

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

  Terminal.write("251208 Valk Research Ltd - ESP32 FabGL WiFi - NEWS \r\n");

  connectWiFiFromConfig();
  setupTime();

  delay(2000);  // piccolo tempo al NTP


  // Primo fetch ANSA: parte dal feed 0 (ANSA HOME)
  currentFeedIndex = 0;
  fetchAnsaNews(ANSA_FEEDS[currentFeedIndex].url,
                ANSA_FEEDS[currentFeedIndex].label);


  lastFetch = millis();

  updateTimeLine();
  drawStaticInfo();
  lastClockUpdate = millis();
}

void loop()
{
  unsigned long now = millis();

  // gestisce il pulsante per andare al prossimo RSS
  handleNextRSSButton();

  // Aggiorna SOLO la riga dell'orologio ogni secondo
  if (now - lastClockUpdate >= 1000) {
    lastClockUpdate = now;
    updateTimeLine();   // niente clear → niente sfarfallio
  }

  // Ogni FETCH_REFRESH_MS passa al feed successivo e aggiorna le news
  if (WiFi.status() == WL_CONNECTED &&
      now - lastFetch >= FETCH_REFRESH_MS) {

    goToNextRSSFeed();
  }


}
