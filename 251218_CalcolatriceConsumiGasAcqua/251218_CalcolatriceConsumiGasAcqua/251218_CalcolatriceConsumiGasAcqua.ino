#include <WiFi.h>
#include <time.h>
#include <Keypad.h>
#include <LiquidCrystal.h>
#include <FS.h>
#include <LittleFS.h>
#include "config.h"


// Italia (Roma) + DST automatico
// Nota: ESP32 Arduino supporta POSIX TZ string
const char* TZ_INFO = "CET-1CEST,M3.5.0/2,M10.5.0/3";

// -------------------- PIN LCD (6 FILI) --------------------
LiquidCrystal lcd(4, 16, 17, 5, 18, 19);

// -------------------- KEYPAD 4x4 --------------------
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// Cambia questi pin secondo il tuo cablaggio
byte rowPins[ROWS] = {26, 25, 33, 32}; // R1..R4
byte colPins[COLS] = {27, 14, 12, 13}; // C1..C4

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// -------------------- STORAGE --------------------
static const char* LOG_PATH = "/log.csv";
// formato riga: epoch;TYPE;value_milli;price_milli
// es: 1734512345;GAS;1234567;1450

// -------------------- UTIL NUMERI --------------------
// Usiamo "millesimi" per evitare float:
// 12.345 -> 12345
// (max 3 decimali nel UI)
bool parseToMilli(const String& s, int64_t& outMilli) {
  if (s.length() == 0) return false;
  bool seenDot = false;
  int dotPos = -1;
  for (int i=0;i<s.length();i++){
    char c = s[i];
    if (c == '.') { if (seenDot) return false; seenDot=true; dotPos=i; continue; }
    if (c < '0' || c > '9') return false;
  }

  String a = s;
  if (!seenDot) {
    outMilli = (int64_t)a.toInt() * 1000;
    return true;
  }

  String intPart = a.substring(0, dotPos);
  String decPart = a.substring(dotPos+1);
  while (decPart.length() < 3) decPart += "0";
  if (decPart.length() > 3) decPart = decPart.substring(0,3);

  int64_t ip = (intPart.length() ? intPart.toInt() : 0);
  int64_t dp = (decPart.length() ? decPart.toInt() : 0);
  outMilli = ip*1000 + (ip>=0 ? dp : -dp);
  return true;
}

String milliToString(int64_t m) {
  bool neg = m < 0;
  if (neg) m = -m;
  int64_t ip = m / 1000;
  int64_t dp = m % 1000;
  char buf[32];
  snprintf(buf, sizeof(buf), "%s%lld.%03lld", neg?"-":"", (long long)ip, (long long)dp);
  return String(buf);
}

// -------------------- TIME --------------------
bool timeValid = false;

bool syncTimeNTP(unsigned long timeoutMs = 6000) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) {
    delay(200);
  }
  if (WiFi.status() != WL_CONNECTED) return false;

  configTzTime(TZ_INFO, "pool.ntp.org", "time.nist.gov");
  // attendi che l'ora si assesti
  struct tm tmNow;
  for (int i=0;i<20;i++){
    if (getLocalTime(&tmNow, 500)) return true;
    delay(100);
  }
  return false;
}

time_t nowEpochOrZero() {
  time_t t = time(nullptr);
  // se non è sincronizzato, spesso torna 0 o valori vecchi.
  if (t < 1700000000) return 0; // soglia ragionevole (fine 2023)
  return t;
}

String formatDateTime(time_t t) {
  struct tm tmNow;
  localtime_r(&t, &tmNow);
  char buf[32];
  strftime(buf, sizeof(buf), "%d/%m %H:%M", &tmNow);
  return String(buf);
}

String formatDateTimeLong(time_t t) {
  struct tm tmNow;
  localtime_r(&t, &tmNow);
  char buf[32];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", &tmNow);
  return String(buf);
}

// -------------------- READ LAST LINE --------------------
bool readLastLogLine(String& outLine) {
  File f = LittleFS.open(LOG_PATH, "r");
  if (!f) return false;
  String last;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length()) last = line;
  }
  f.close();
  if (!last.length()) return false;
  outLine = last;
  return true;
}

// Legge ultima data/ora salvata (per precompilare inserimento manuale)
time_t lastSavedEpochOrZero() {
  String line;
  if (!readLastLogLine(line)) return 0;
  // epoch;TYPE;value;price
  int p = line.indexOf(';');
  if (p < 0) return 0;
  return (time_t) line.substring(0,p).toInt();
}

bool readLastValues(int64_t &gasMilli, int64_t &acqMilli) {
  gasMilli = -1;
  acqMilli = -1;
  File f = LittleFS.open(LOG_PATH, "r");
  if (!f) return false;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (!line.length()) continue;

    // epoch;TYPE;value;price
    int p1 = line.indexOf(';');
    int p2 = line.indexOf(';', p1+1);
    int p3 = line.indexOf(';', p2+1);
    if (p1<0||p2<0||p3<0) continue;

    String type = line.substring(p1+1, p2);
    String valS = line.substring(p2+1, p3);
    int64_t val = valS.toInt();

    if (type == "GAS") gasMilli = val;
    if (type == "ACQ") acqMilli = val;
  }
  f.close();
  return true;
}

// -------------------- DELETE LAST ENTRY (sempre possibile) --------------------
bool deleteLastEntry() {
  // se non esiste, niente da fare
  if (!LittleFS.exists(LOG_PATH)) return false;

  // 1) leggi e memorizza l'ultima riga non vuota
  File f = LittleFS.open(LOG_PATH, "r");
  if (!f) return false;

  String lastNonEmpty = "";
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length()) lastNonEmpty = line;
  }
  f.close();

  if (!lastNonEmpty.length()) return false; // file vuoto

  // 2) riscrivi tutto tranne l'ultima occorrenza di quella riga
  // (nota: se due righe identiche consecutive è raro ma possibile;
  //  in quel caso eliminiamo l'ultima uguale, che è corretto.)
  const char* TMP_PATH = "/log.tmp";

  f = LittleFS.open(LOG_PATH, "r");
  if (!f) return false;

  File w = LittleFS.open(TMP_PATH, "w");
  if (!w) { f.close(); return false; }

  // leggiamo tutte le righe in una lista "minima": teniamo una riga "in sospeso"
  // e la scriviamo solo quando sappiamo che non è l'ultima da eliminare.
  String pending = "";
  bool havePending = false;

  // Per eliminare l'ULTIMA riga non vuota, dobbiamo evitare di scrivere proprio l'ultima.
  // Strategia: scrivo sempre la riga precedente (pending), e tengo in pending quella corrente.
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (!line.length()) continue;

    if (havePending) {
      w.println(pending);
    }
    pending = line;
    havePending = true;
  }
  f.close();

  // A questo punto, pending è l'ultima riga non vuota del file (quella da eliminare),
  // quindi NON la scriviamo.
  w.close();

  // 3) sostituisci il file originale
  LittleFS.remove(LOG_PATH);
  if (!LittleFS.rename(TMP_PATH, LOG_PATH)) {
    // se rename fallisce, prova a ripristinare (best effort)
    return false;
  }

  return true;
}


// -------------------- UI helpers --------------------
void lcd2(const String& a, const String& b) {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print(a);
  lcd.setCursor(0,1); lcd.print(b);
}

char waitKey() {
  char k = 0;
  while (!k) {
    k = keypad.getKey();
    delay(20);
  }
  return k;
}

// editor numerico con decimali: 0-9 e '.', cancella con C, conferma con D, annulla con A (o C tenuto non gestito qui)
bool editNumber(const String& title, String& out, bool allowEmpty=false) {
  out = "";
  lcd2(title, "D=OK  C=DEL A=ESC");

  while (true) {
    lcd.setCursor(0,1);
    String shown = out;
    if (shown.length() > 20) shown = shown.substring(shown.length()-20);
    lcd.print("                    ");
    lcd.setCursor(0,1);
    lcd.print(shown);

    char k = keypad.getKey();
    if (!k) { delay(20); continue; }

    if (k >= '0' && k <= '9') {
      out += k;
    } else if (k == '*') {
      if (out.indexOf('.') < 0) out += '.';
    } else if (k == 'C') {
      if (out.length()) out.remove(out.length()-1);
    } else if (k == 'A') {
      return false; // annulla
    } else if (k == 'D') {
      if (!allowEmpty && out.length()==0) continue;
      return true;
    }
  }
}

// Inserimento data/ora manuale precompilata: dd/mm/yyyy hh:mm
// Campi modificabili con # (campo successivo), C (indietro), D (OK), A (ESC)
bool editDateTime(time_t seed, time_t& outEpoch) {
  struct tm tmSeed;
  if (seed > 0) localtime_r(&seed, &tmSeed);
  else {
    // default: 01/01/2025 00:00
    tmSeed = {};
    tmSeed.tm_mday = 1;
    tmSeed.tm_mon  = 0;
    tmSeed.tm_year = 125;
    tmSeed.tm_hour = 0;
    tmSeed.tm_min  = 0;
  }

  int d = tmSeed.tm_mday;
  int m = tmSeed.tm_mon + 1;
  int y = tmSeed.tm_year + 1900;
  int hh = tmSeed.tm_hour;
  int mm = tmSeed.tm_min;

  int field = 0; // 0 d,1 m,2 y,3 hh,4 mm
  String buf = "";

  auto redraw = [&]() {
    char l1[21], l2[21];
    snprintf(l1, sizeof(l1), "Data %02d/%02d/%04d", d,m,y);
    snprintf(l2, sizeof(l2), "Ora  %02d:%02d  #> D=OK", hh,mm);
    lcd2(String(l1), String(l2));

    // cursore “simbolico” sul campo
    int col = 0, row = 0;
    if (field==0){ row=0; col=5; }
    if (field==1){ row=0; col=8; }
    if (field==2){ row=0; col=11; }
    if (field==3){ row=1; col=5; }
    if (field==4){ row=1; col=8; }
    lcd.setCursor(col,row);
  };

  redraw();

  while (true) {
    char k = keypad.getKey();
    if (!k) { delay(20); continue; }

    if (k == 'A') return false; // annulla
    if (k == '#') { field = (field+1)%5; buf=""; redraw(); continue; }
    if (k == 'C') {
      if (buf.length()) buf.remove(buf.length()-1);
      else { field = (field+4)%5; } // torna al precedente
      redraw();
      continue;
    }
    if (k == 'D') {
      // valida
      if (y < 2000 || y > 2099) continue;
      if (m < 1 || m > 12) continue;
      if (d < 1 || d > 31) continue;
      if (hh < 0 || hh > 23) continue;
      if (mm < 0 || mm > 59) continue;

      struct tm t = {};
      t.tm_mday = d;
      t.tm_mon = m-1;
      t.tm_year = y-1900;
      t.tm_hour = hh;
      t.tm_min = mm;
      t.tm_isdst = -1;
      time_t epoch = mktime(&t);
      if (epoch <= 0) continue;
      outEpoch = epoch;
      return true;
    }

    // numeri per campo
    if (k >= '0' && k <= '9') {
      buf += k;
      int v = buf.toInt();
      if (field==0 && buf.length()<=2) d = v;
      if (field==1 && buf.length()<=2) m = v;
      if (field==2 && buf.length()<=4) y = v;
      if (field==3 && buf.length()<=2) hh = v;
      if (field==4 && buf.length()<=2) mm = v;
      redraw();
    }
  }
}

// -------------------- APP LOGIC --------------------
int64_t priceGasMilli = 0; // €/unità * 1000
int64_t priceAcqMilli = 0;

bool appendReading(const String& type, int64_t valueMilli, int64_t priceMilli, time_t whenEpoch) {
  File f = LittleFS.open(LOG_PATH, "a");
  if (!f) return false;
  // epoch;TYPE;value;price
  f.printf("%ld;%s;%lld;%lld\n",
           (long)whenEpoch, type.c_str(),
           (long long)valueMilli, (long long)priceMilli);
  f.close();
  return true;
}

// Menu minimale:
// A = menu
// 1: nuova GAS
// 2: nuova ACQ
// 3: prezzi
// 4: elimina ultimo
// C = esci
void showHome() {
  int64_t g=-1,a=-1;
  readLastValues(g,a);

  String l1 = "GAS ";
  l1 += (g>=0 ? milliToString(g) : String("--"));
  String l2 = "ACQ ";
  l2 += (a>=0 ? milliToString(a) : String("--"));

  time_t n = nowEpochOrZero();
  if (n == 0) {
    time_t last = lastSavedEpochOrZero();
    if (last>0) l2 += " " + formatDateTime(last);
    else l2 += " NO-TIME";
  } else {
    l2 += " " + formatDateTime(n);
  }

  if (l1.length()>20) l1 = l1.substring(0,20);
  if (l2.length()>20) l2 = l2.substring(0,20);
  lcd2(l1, l2);
}

bool ensureTimeOrAsk(time_t& outEpoch) {
  time_t n = nowEpochOrZero();
  if (n != 0) { outEpoch = n; return true; }

  // no internet/time -> chiedi data/ora precompilata con ultima lettura
  time_t seed = lastSavedEpochOrZero();
  lcd2("No Internet / Ora", "Inserisci data/ora");
  delay(900);
  return editDateTime(seed, outEpoch);
}

void doNewReading(const String& type) {
  // 1) inserisci valore
  String s;
  if (!editNumber("Nuova lettura " + type, s)) {
    lcd2("Operazione", "Annullata");
    delay(700);
    return;
  }

  int64_t v;
  if (!parseToMilli(s, v)) {
    lcd2("Valore non valido", "Riprova");
    delay(900);
    return;
  }

  // 2) data/ora (auto se NTP ok, altrimenti richiesta)
  time_t ts;
  if (!ensureTimeOrAsk(ts)) {
    lcd2("Operazione", "Annullata");
    delay(700);
    return;
  }

  // 3) conferma finale
  lcd2("Salvare lettura?", "D=OK   C=NO");
  while (true) {
    char k = waitKey();
    if (k=='C' || k=='A') {
      lcd2("Operazione", "Annullata");
      delay(700);
      return;
    }
    if (k=='D') break;
  }

  int64_t p = (type=="GAS") ? priceGasMilli : priceAcqMilli;

  if (!appendReading(type, v, p, ts)) {
    lcd2("Errore salvataggio", "LittleFS?");
    delay(1000);
    return;
  }

  lcd2("Salvato!", formatDateTimeLong(ts));
  delay(900);
}

void doPrices() {
  // prezzo gas
  String s;
  if (editNumber("Prezzo GAS (€/u)", s, true)) {
    if (s.length()==0) priceGasMilli = 0;
    else {
      int64_t v;
      if (parseToMilli(s, v)) priceGasMilli = v;
    }
  } else {
    // annulla, torna menu
    return;
  }

  // prezzo acqua
  if (editNumber("Prezzo ACQ (€/u)", s, true)) {
    if (s.length()==0) priceAcqMilli = 0;
    else {
      int64_t v;
      if (parseToMilli(s, v)) priceAcqMilli = v;
    }
  }
  lcd2("Prezzi salvati", "GAS/ACQ aggiornati");
  delay(900);
}

void doDeleteLast() {
  lcd2("Eliminare ULTIMA?", "D=SI   C=NO");
  while (true) {
    char k = waitKey();
    if (k=='C' || k=='A') return;
    if (k=='D') break;
  }

  if (deleteLastEntry()) {
    lcd2("Ultima lettura", "ELIMINATA");
  } else {
    lcd2("Nessuna lettura", "da eliminare");
  }
  delay(900);
}

void showMenu() {
  lcd2("1 GAS 2 ACQ", "3 Prezzi 4 Del");
  while (true) {
    char k = waitKey();
    if (k=='C' || k=='A') return;

    if (k=='1') { doNewReading("GAS"); return; }
    if (k=='2') { doNewReading("ACQ"); return; }
    if (k=='3') { doPrices(); return; }
    if (k=='4') { doDeleteLast(); return; }
  }
}

// -------------------- SETUP / LOOP --------------------
void setup() {
  Serial.begin(115200);

  lcd.begin(20, 2);
  lcd2("Avvio...", "Init FS");

  if (!LittleFS.begin(true)) {
    lcd2("LittleFS FAIL", "Riavvia");
    while (true) delay(1000);
  }

  lcd2("WiFi/NTP...", "attendere");
  bool ok = syncTimeNTP();
  timeValid = ok;

  if (ok) lcd2("Ora sincronizzata", formatDateTimeLong(nowEpochOrZero()));
  else    lcd2("Niente Internet", "Ora manuale in lettura");
  delay(1200);

  showHome();
}

void loop() {
  // refresh home ogni tanto (solo ora)
  static unsigned long lastRefresh = 0;
  if (millis() - lastRefresh > 2000) {
    showHome();
    lastRefresh = millis();
  }

  char k = keypad.getKey();
  if (k == 'A') {
    showMenu();
    showHome();
  }

  delay(20);
}
