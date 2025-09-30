#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

const char* SSID = "";
const char* PASS = "";
const char* URL  = "https://www.reggioemiliameteo.it/stazioni/tab-live/stazioni-meteo-tab-live-20.html";





void scaricaHTML() {
  WiFiClientSecure client;
  client.setInsecure(); // meglio usare una CA con client.setCACert(...)

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // segui 301/302
  http.setTimeout(15000);           // 15s per lettura (aumenta se serve)
  http.setConnectTimeout(10000);    // 10s connessione

  http.begin(client, URL);

  // Alcuni server rifiutano richieste senza UA o con gzip:
  http.setUserAgent("ESP32/Arduino");
  http.addHeader("Accept", "text/html,application/xhtml+xml");
  http.addHeader("Accept-Encoding", "identity"); // niente gzip
  http.addHeader("Connection", "close");

  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    String html = http.getString();
    // ... parsing come da esempio precedente ...
  } else {
    Serial.printf("HTTP err: %d\n", code);
  }
  http.end();
}

// --- utility semplici di parsing ---
static String between(const String& s, const String& open, const String& close, int from = 0) {
  int a = s.indexOf(open, from); if (a < 0) return "";
  a += open.length();
  int b = s.indexOf(close, a);   if (b < 0) return "";
  return s.substring(a, b);
}

static int findTag(const String& s, const String& tag, int from = 0) {
  return s.indexOf("<" + tag, from);
}

// Ritorna il contenuto del k-esimo tag <tag>...</tag> (k=1-based) all'interno di "scope"
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

// Estrae il testo desiderato dalla riga trIndex (1-based) e colonna 3
// mode = 0 -> cerca <h5>, mode = 1 -> cerca <span>[2]/<small>
static String extractFromRow(const String& tableHtml, int trIndex, int mode) {
  // trova tr[trIndex]
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

  // prendi td[3] dentro alla tr
  // estrai tutti i td come blocchi
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
    if (tdCount == 3) {
      td3 = trBlock.substring(tdStartEnd + 1, tdEnd);
      break;
    }
    tdPos = tdEnd + 5;
  }
  if (td3.isEmpty()) return "";

  if (mode == 0) {
    // <h5>...</h5>
    String t = kthTagText(td3, "h5", 1);
    t.trim();
    return t;
  } else {
    // <span>[2]/<small>
    // prendi il 2° <span>...</span>
    String span2 = kthTagText(td3, "span", 2);
    if (span2.isEmpty()) return "";
    String small = kthTagText(span2, "small", 1);
    small.trim();
    return small;
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(SSID, PASS);
  Serial.print("WiFi...");
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\nConnesso.");
  scaricaHTML();
  HTTPClient http;
  http.begin(URL);
  int code = http.GET();
  if (code != 200) {
    Serial.printf("HTTP err: %d\n", code);
    return;
  }
  String html = http.getString();
  http.end();

  // Isola il primo <table>...</table> (come nel tuo XPath)
  String table = between(html, "<table", "</table>");
  if (table.isEmpty()) {
    Serial.println("Tabella non trovata.");
    return;
  }
  // rimuovi eventuale apertura attributata fino a '>'
  int gt = table.indexOf('>');
  if (gt >= 0) table = table.substring(gt + 1);

  // Estrazioni richieste:
  String v1 = extractFromRow(table, 1, 0); // /tr[1]/td[3]/h5
  String v2 = extractFromRow(table, 2, 0); // /tr[2]/td[3]/h5
  String v3 = extractFromRow(table, 3, 1); // /tr[3]/td[3]/span[2]/small
  String v4 = extractFromRow(table, 4, 1); // /tr[4]/td[3]/span[2]/small
  String v5 = extractFromRow(table, 5, 0); // /tr[5]/td[3]/h5
  String v6 = extractFromRow(table, 6, 0); // /tr[6]/td[3]/h5

  Serial.println("Risultati:");
  Serial.println(v1);
  Serial.println(v2);
  Serial.println(v3);
  Serial.println(v4);
  Serial.println(v5);
  Serial.println(v6);
}

void loop() {}
