#include <WiFi.h>
#include <LiquidCrystal.h>
#include "config.h" // Contiene TELEGRAM_BOT_TOKEN, TELEGRAM_CHAT_ID, WIFI_SSID, WIFI_PASSWORD

// LiquidCrystal(rs, en, d4, d5, d6, d7)
LiquidCrystal lcd(13, 12, 14, 27, 26, 25);

#include <time.h>
#include "esp_sntp.h"

// ---------- CONFIG ----------
#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 2

// CET-1CEST con cambio legale automatico (ultima dom di marzo/ottobre)
const char* TZ_EUROPE_ROME = "CET-1CEST,M3.5.0/2,M10.5.0/3";

// Sync ogni 5 minuti
const uint32_t NTP_SYNC_INTERVAL_MS = 5UL * 60UL * 1000UL;

// ---------- STATI ----------
bool internetOK = false;
unsigned long lastNTPTry = 0;
unsigned long lastLine1Update = 0;     // aggiornamento riga 1 (1 Hz)
unsigned long lastScrollUpdate = 0;    // aggiornamento scorrimento riga 2
const uint32_t LINE1_INTERVAL_MS = 1000;
const uint32_t SCROLL_INTERVAL_MS = 600;

// Variabili per lo scrolling della riga 2
String line2Current = "";      // frase completa corrente (senza taglio)
String line2Padded = "";       // frase + spazi per pausa
int line2ScrollIdx = 0;        // indice di scorrimento nella stringa "ripetuta"
bool line2NeedsRebuild = true; // forza la ricostruzione del buffer di scroll

// ---------- UTIL ----------

// --- Nomi speciali per alcuni orari ---
// Usa minuti arrotondati a 5 (m5 ∈ {0,5,...,55}) come nel resto del codice.
struct SpecialName { uint8_t h; uint8_t m5; const char* phrase; };

static const SpecialName kSpecialNames[] = {
  {12, 30, "La mezza"},      // 12:30
  // Aggiungi qui altri nomi particolari se vuoi:
  // {0,  0,  "Mezzanotte"},
  // {12, 0,  "Mezzogiorno"},
};

static String specialNameForTime(int hh, int m5) {
  for (size_t i = 0; i < (sizeof(kSpecialNames)/sizeof(kSpecialNames[0])); ++i) {
    if (kSpecialNames[i].h == hh && kSpecialNames[i].m5 == m5) {
      return String(kSpecialNames[i].phrase);
    }
  }
  return String(); // vuota = nessun nome speciale per quell'orario
}


static inline int roundToNearest5(int m) {
  int r = ((m + 2) / 5) * 5;
  if (r == 60) r = 55;
  return r;
}

String capitalize(const String& s) {
  if (s.length() == 0) return s;
  String out = s;
  out.setCharAt(0, (char)toupper(out.charAt(0)));
  return out;
}

String numToItalianHourWord(int h) {
  static const char* ore[] = {
    "Zero","Uno","Due","Tre","Quattro","Cinque","Sei","Sette","Otto","Nove",
    "Dieci","Undici","Dodici","Tredici","Quattordici","Quindici","Sedici","Diciassette",
    "Diciotto","Diciannove","Venti","Ventuno","Ventidue","Ventitre"
  };
  if (h < 0 || h > 23) return "Errore";
  return ore[h];
}

String numToItalianMinute5Word(int m5) {
  switch (m5) {
    case 0:  return "Zero";
    case 5:  return "Cinque";
    case 10: return "Dieci";
    case 15: return "Quindici";
    case 20: return "Venti";
    case 25: return "Venticinque";
    case 30: return "Trenta";
    case 35: return "Trentacinque";
    case 40: return "Quaranta";
    case 45: return "Quarantacinque";
    case 50: return "Cinquanta";
    case 55: return "Cinquantacinque";
  }
  return "Errore";
}

// *** RIMOSSO il taglio a 16 caratteri: servono tutti i caratteri per lo scrolling ***
String line2TextFromTime(int hh, int mm) {
  if (!internetOK) return "No Internet";
  int m5 = roundToNearest5(mm);

  // NEW: se esiste un nome speciale per questo orario, usalo
  String special = specialNameForTime(hh, m5);
  if (special.length() > 0) return special;

  // fallback: frase standard "Ore e Minuti"
  String s = capitalize(numToItalianHourWord(hh)) + " e " + capitalize(numToItalianMinute5Word(m5));
  return s;
}


void onTimeSync(struct timeval* tv) { internetOK = true; }

void connectWiFiNonBlocking(unsigned long timeout_ms = 8000) {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeout_ms) {
    delay(100);
  }
  internetOK = (WiFi.status() == WL_CONNECTED);
}

void setupTime() {
  sntp_set_time_sync_notification_cb(onTimeSync);
  sntp_set_sync_interval(NTP_SYNC_INTERVAL_MS);
  // Imposta direttamente timezone + server NTP
  configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3",
               "pool.ntp.org", "time.google.com");
}


bool trySyncNow() {
  if (WiFi.status() != WL_CONNECTED) { internetOK = false; return false; }
  // “Pungola” una nuova sync rispettando la timezone
  configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3",
               "pool.ntp.org", "time.google.com");
  struct tm t;
  if (getLocalTime(&t, 2000)) { internetOK = true; return true; }
  internetOK = false; return false;
}

// ---------- SETUP ----------
void setup() {
  lcd.begin(LCD_COLS, LCD_ROWS);
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Orologio Valk");
  lcd.setCursor(0, 1); lcd.print("in dialetto...");

  connectWiFiNonBlocking();
  setupTime();
  lastNTPTry = millis() - NTP_SYNC_INTERVAL_MS; // prova subito al boot
  delay(800);
  lcd.clear();
}

// Funzione di utilità per aggiornare/bufferizzare la riga 2 per lo scroll
void rebuildLine2BufferIfNeeded(const String& newText) {
  if (line2NeedsRebuild || newText != line2Current) {
    line2Current = newText;
    line2ScrollIdx = 0;
    line2NeedsRebuild = false;
    // Aggiungo una pausa di 3 spazi tra una “corsa” e la successiva
    line2Padded = line2Current + "   ";
  }
}

// Disegna la finestra di 16 caratteri della riga 2, con scorrimento se necessario
void drawLine2() {
  String src = line2Padded;
  if (src.length() <= 16) {
    // Nessuno scroll: pad a 16 per “pulire” residui
    while (src.length() < 16) src += ' ';
    lcd.setCursor(0, 1);
    lcd.print(src);
    return;
  }

  // Per lo scroll continuo, duplico la stringa così posso fare wrapping semplice
  String doubled = src + src;

  // Prendo la “finestra” di 16 char a partire da line2ScrollIdx
  String window = doubled.substring(line2ScrollIdx, line2ScrollIdx + 16);
  lcd.setCursor(0, 1);
  lcd.print(window);

  // Avanza indice (mod la lunghezza originale "src")
  line2ScrollIdx = (line2ScrollIdx + 1) % src.length();
}

// ---------- LOOP ----------
void loop() {
  unsigned long nowMs = millis();

  // Prova NTP ogni 5 minuti (o subito al boot)
  if (nowMs - lastNTPTry >= NTP_SYNC_INTERVAL_MS) {
    lastNTPTry = nowMs;
    connectWiFiNonBlocking();
    trySyncNow();
  }

  // --- Riga 1: aggiorna una volta al secondo ---
  if (nowMs - lastLine1Update >= LINE1_INTERVAL_MS) {
    lastLine1Update = nowMs;

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 10)) {
      lcd.setCursor(0, 0); lcd.print("LUN 00/00 00:00");

      // Se non abbiamo l'ora, mostra “No Internet” fisso e resetta lo scroll
      line2NeedsRebuild = true;
      rebuildLine2BufferIfNeeded("No Internet");
      // Aggiorna subito anche la riga 2 (senza scorrimento)
      drawLine2();
    } else {
      // Riga 1: "dd/mm/yyyy HH:MM" (16 char esatti)
      char line1[17];
      //strftime(line1, sizeof(line1), "%d/%m/%Y %H:%M", &timeinfo);
      strftime(line1, sizeof(line1), "%a %d/%m %H:%M", &timeinfo);

      // Traduzione abbreviazioni inglesi -> italiane
      if      (strncmp(line1, "Mon", 3) == 0) memcpy(line1, "LUN", 3);
      else if (strncmp(line1, "Tue", 3) == 0) memcpy(line1, "MAR", 3);
      else if (strncmp(line1, "Wed", 3) == 0) memcpy(line1, "MER", 3);
      else if (strncmp(line1, "Thu", 3) == 0) memcpy(line1, "GIO", 3);
      else if (strncmp(line1, "Fri", 3) == 0) memcpy(line1, "VEN", 3);
      else if (strncmp(line1, "Sat", 3) == 0) memcpy(line1, "SAB", 3);
      else if (strncmp(line1, "Sun", 3) == 0) memcpy(line1, "DOM", 3);


      lcd.setCursor(0, 0);
      lcd.print(line1);

      // *** Lampeggio dei due punti tra HH e MM ***
      // La stringa ha il ':' alla colonna 13 (0-based)
      lcd.setCursor(12, 0);
      lcd.print((timeinfo.tm_sec % 2 == 0) ? ":" : " ");

      // Prepara/aggiorna buffer per lo scrolling della riga 2
      String l2 = line2TextFromTime(timeinfo.tm_hour, timeinfo.tm_min);
      rebuildLine2BufferIfNeeded(l2);
    }
  }

  // --- Riga 2: scorrimento indipendente (ogni ~300 ms) ---
  if (nowMs - lastScrollUpdate >= SCROLL_INTERVAL_MS) {
    lastScrollUpdate = nowMs;
    drawLine2();
  }
}
