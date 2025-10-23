/*
  ESP32 + LCD 20x2 HD44780 (parallelo 4-bit) + Keypad 4x4
  Logger letture contatori ACQUA/GAS con storico (ultime 100) su NVS.

  Tasti:
   - Cifre: inserimento valore (seconda riga)
   - '#'   : backspace
   - 'A'   : salva come ACQUA
   - 'B'   : salva come GAS
   - 'C'/'D': scorri storico (prima riga: "dd/MM/yyyy valore TIPO")
  All’avvio: Wi-Fi + NTP (Europa/Roma), poi spegne il Wi-Fi.

  Librerie:
   - Keypad (Mark Stanley, A. Brevig)
   - LiquidCrystal (standard Arduino)
*/

#include <WiFi.h>
#include <Preferences.h>
#include <Keypad.h>
#include <LiquidCrystal.h>
#include <time.h>
#include "config.h"


// ——— Rilevazione pressione lunga su '*' ———
const unsigned int STAR_HOLD_MS = 800;  // durata pressione per cancellare (ms)
void keypadEvent(KeypadEvent k);        // forward-declare

// PROTOTIPO della funzione di cancellazione
void deleteCurrentBrowseRecord();

// ====== CONFIG WiFi/NTP ======

const char* NTP_SERVER = "pool.ntp.org";
const char* TZ_EUROPE_ROME = "CET-1CEST,M3.5.0/2,M10.5.0/3";

// ====== PIN LCD (parallelo 4-bit) ======
// RS, E, D4, D5, D6, D7
LiquidCrystal lcd(4, 16, 17, 5, 18, 19);
// Nota: collegare RW a GND (solo scrittura).

// ====== Keypad 4x4 ======
const byte ROWS = 4, COLS = 4;
char hexaKeys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
// Righe/Colonne (scegli pin liberi su ESP32)
byte rowPins[ROWS] = {26, 25, 33, 32}; // R1..R4
byte colPins[COLS] = {27, 14, 12, 13}; // C1..C4
Keypad keypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

// ====== Persistenza su NVS ======
Preferences prefs;
const char* NVS_NAMESPACE = "gasaqua";
const char* NVS_KEY_BLOB   = "entries";
const char* NVS_KEY_HEAD   = "head";
const char* NVS_KEY_COUNT  = "count";

enum EntryType : uint8_t { TYPE_ACQUA = 0, TYPE_GAS = 1 };

struct LogEntry {
  uint32_t epoch;   // secondi dal 1970
  int32_t  value;   // lettura
  uint8_t  type;    // 0=ACQUA, 1=GAS
  uint8_t  _pad[3]; // allineamento
};

const int LOG_CAPACITY = 100;
LogEntry logBuf[LOG_CAPACITY];
int logHead = 0;           // prossima scrittura
int logCount = 0;          // quanti validi (<=100)

// ====== Stato UI ======
String inputBuffer;     // cifre in inserimento
int    browseOffset = 0;
bool   inBrowse = false;

// ====== LCD helper ======
void lcdClearLine(uint8_t row) {
  lcd.setCursor(0, row);
  for (int i=0; i<20; ++i) lcd.print(' ');
  lcd.setCursor(0, row);
}
void lcdPrintCentered(uint8_t row, const String& s) {
  int pad = max(0, (20 - (int)s.length())/2);
  lcdClearLine(row);
  for (int i=0; i<pad; ++i) lcd.print(' ');
  lcd.print(s.substring(0,20));
}

// ====== Persistenza ======
void loadLog() {
  prefs.begin(NVS_NAMESPACE, false);
  logHead  = prefs.getInt(NVS_KEY_HEAD, 0);
  logCount = prefs.getInt(NVS_KEY_COUNT, 0);
  if (logHead < 0 || logHead >= LOG_CAPACITY) logHead = 0;
  if (logCount < 0 || logCount > LOG_CAPACITY) logCount = 0;

  size_t need = sizeof(logBuf);
  size_t got  = prefs.getBytes(NVS_KEY_BLOB, logBuf, need);
  if (got != need) memset(logBuf, 0, sizeof(logBuf));
  prefs.end();
}
void saveLog() {
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putInt(NVS_KEY_HEAD,  logHead);
  prefs.putInt(NVS_KEY_COUNT, logCount);
  prefs.putBytes(NVS_KEY_BLOB, logBuf, sizeof(logBuf));
  prefs.end();
}
void addEntry(int32_t value, EntryType type) {
  LogEntry e;
  e.epoch = (uint32_t)time(nullptr);
  e.value = value;
  e.type  = (uint8_t)type;
  logBuf[logHead] = e;
  logHead = (logHead + 1) % LOG_CAPACITY;
  if (logCount < LOG_CAPACITY) logCount++;
  saveLog();
}
LogEntry* getEntryByOffset(int offset) { // 0=ultima, 1=precedente...
  if (logCount == 0 || offset < 0 || offset >= logCount) return nullptr;
  int newestIndex = (logHead - 1 + LOG_CAPACITY) % LOG_CAPACITY;
  int idx = (newestIndex - offset + LOG_CAPACITY) % LOG_CAPACITY;
  return &logBuf[idx];
}

// ====== WiFi & NTP ======
bool syncTimeViaNTP() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-t0 < 15000) delay(200);
  if (WiFi.status() != WL_CONNECTED) return false;

  setenv("TZ", TZ_EUROPE_ROME, 1);
  tzset();
  configTzTime(TZ_EUROPE_ROME, NTP_SERVER);

  for (int i=0; i<30; ++i) {
    time_t now = time(nullptr);
    if (now > 1700000000) break; // ~2023
    delay(200);
  }

  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  return true;
}

// ====== Formattazione riga storico ======
String formatEntry(const LogEntry& e) {
  // Conversione corretta: copia l'epoch in un time_t locale
  time_t t = (time_t)e.epoch;   // e.epoch è uint32_t; time_t su ESP32 è 64 bit
  struct tm lt;
  localtime_r(&t, &lt);           // usa l'indirizzo di t (64 bit), NON &e.epoch

  char datebuf[11];
  // in alternativa: strftime(datebuf, sizeof(datebuf), "%d/%m/%Y", &lt);
  snprintf(datebuf, sizeof(datebuf), "%02d/%02d/%04d",
           lt.tm_mday, lt.tm_mon + 1, lt.tm_year + 1900);

  String tipo = (e.type == TYPE_GAS) ? "GAS" : "ACQUA";
  String val  = String(e.value);

  String s = String(datebuf) + " " + val + " " + tipo;
  if (s.length() > 20) {
    int maxValLen = max(1, 20 - (int)String(datebuf).length() - 1 - (int)tipo.length());
    val = val.substring(0, maxValLen);
    s = String(datebuf) + " " + val + " " + tipo;
    if (s.length() > 20) s = s.substring(0, 20);
  }
  return s;
}

void keypadEvent(KeypadEvent k) {
  KeyState st = keypad.getState();
  if (st == HOLD) {
    if (k == '*' && inBrowse && logCount > 0) {
      deleteCurrentBrowseRecord();
    }
  }
  // Gli altri stati (PRESSED/RELEASED) li gestiamo già nel loop principale.
}

void deleteCurrentBrowseRecord() {
  int n = logCount;
  if (n == 0) return;
  if (browseOffset < 0 || browseOffset >= n) return;

  // Copia lo storico in ordine cronologico (dal più vecchio al più recente)
  LogEntry tmp[LOG_CAPACITY];
  for (int i = 0; i < n; ++i) {
    // getEntryByOffset(0) = più recente ⇒ per avere "oldest→newest" uso (n-1-i)
    LogEntry* e = getEntryByOffset(n - 1 - i);
    tmp[i] = *e;
  }

  // L'indice da cancellare in tmp è quello corrispondente al record visualizzato:
  // browseOffset conta da "più recente" verso il passato → mappa a:
  int delIdx = n - 1 - browseOffset;

  // Elimina spostando a sinistra
  for (int j = delIdx; j < n - 1; ++j) tmp[j] = tmp[j + 1];
  n -= 1;

  // Ricostruisci il buffer circolare preservando l'ordine
  logHead = 0;
  logCount = 0;
  for (int i = 0; i < n; ++i) {
    logBuf[logHead] = tmp[i];
    logHead = (logHead + 1) % LOG_CAPACITY;
    logCount++;
  }
  saveLog();

  // Aggiorna la vista
  if (logCount == 0) {
    inBrowse = false;
    browseOffset = 0;
    lcdClearLine(0); lcd.print("0 dati");
    lcdClearLine(1); // pulizia
    return;
  }

  // Se abbiamo cancellato l'ultima (la più vecchia), l'offset va rientrato
  if (browseOffset >= logCount) browseOffset = logCount - 1;

  // Mostra il nuovo record corrente
  LogEntry* e = getEntryByOffset(browseOffset);
  lcdClearLine(0);
  lcd.print(formatEntry(*e));

  // Feedback rapido sulla seconda riga
  lcdClearLine(1);
  lcd.print("Eliminato");
  delay(400);
  showInputLine(); // torna a mostrare l'input (o lascia "Eliminato" se preferisci)
}


// ====== UI ======
void showIdleHeader() {
  lcdClearLine(0);
  if (logCount == 0) lcd.print("0 dati");
  else               lcd.print("Storico: C/D");
}
void showInputLine() {
  lcdClearLine(1);
  if (inputBuffer.length()) {
    String s = inputBuffer;
    if (s.length() > 20) s = s.substring(s.length()-20);
    lcd.print(s);
  }
}
void enterBrowseIfPossible() {
  if (logCount == 0) {
    inBrowse = false; browseOffset = 0;
    lcdClearLine(0); lcd.print("0 dati");
    return;
  }
  inBrowse = true; browseOffset = 0;
  LogEntry* e = getEntryByOffset(browseOffset);
  lcdClearLine(0); lcd.print(formatEntry(*e));
}
void browseNext() { // verso più vecchi
  if (!inBrowse || logCount == 0) return;
  if (browseOffset < logCount-1) browseOffset++;
  LogEntry* e = getEntryByOffset(browseOffset);
  lcdClearLine(0); lcd.print(formatEntry(*e));
}
void browsePrev() { // verso più recenti
  if (!inBrowse || logCount == 0) return;
  if (browseOffset > 0) browseOffset--;
  LogEntry* e = getEntryByOffset(browseOffset);
  lcdClearLine(0); lcd.print(formatEntry(*e));
}
void resetToInputMode() {
  inBrowse = false; browseOffset = 0;
  showIdleHeader(); showInputLine();
}

// ====== Setup/Loop ======
void setup() {
  lcd.begin(20, 2);
  lcd.clear();
  lcdPrintCentered(0, "Avvio...");

  // ✨ nuovo: listener e hold-time per la tastiera
  keypad.addEventListener(keypadEvent);
  keypad.setHoldTime(STAR_HOLD_MS);

  loadLog();

  lcdPrintCentered(0, "Connessione WiFi");
  if (syncTimeViaNTP()) lcdPrintCentered(0, "Ora sincronizzata");
  else                  lcdPrintCentered(0, "WiFi/NTP non ok");
  delay(800);

  lcd.clear();
  showIdleHeader();
  showInputLine();
}


void loop() {
  char key = keypad.getKey();
  if (!key) return;

  if (key >= '0' && key <= '9') {
    if (inBrowse) resetToInputMode();
    if (inputBuffer.length() < 12) {
      inputBuffer += key;
      showInputLine();
    }
    return;
  }
  if (key == '#') { // backspace
    if (inBrowse) { resetToInputMode(); return; }
    if (inputBuffer.length()) {
      inputBuffer.remove(inputBuffer.length()-1);
      showInputLine();
    }
    return;
  }
  if (key == 'A' || key == 'B') {
    if (inBrowse) { resetToInputMode(); return; }
    if (!inputBuffer.length()) return;
    int32_t val = inputBuffer.toInt();
    addEntry(val, (key=='A') ? TYPE_ACQUA : TYPE_GAS);
    LogEntry* e = getEntryByOffset(0);
    lcdClearLine(0); lcd.print(formatEntry(*e));
    inputBuffer = "";
    showInputLine();
    return;
  }
  if (key == 'C') { if (!inBrowse) enterBrowseIfPossible(); else browseNext();  return; }
  if (key == 'D') { if (!inBrowse) enterBrowseIfPossible(); else browsePrev();  return; }
  // altri tasti ignorati
}
