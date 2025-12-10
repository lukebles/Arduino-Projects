#include <Arduino.h>
#include <LiquidCrystal.h>
#include <math.h>
#include "LkRadioStructure_RH.h"

// ================== PIN LCD (HD44780 16x2, modalità 4-bit) ==================

// Configurazione richiesta
const int LCD_RS = 4;
const int LCD_EN = 16;
const int LCD_D4 = 17;
const int LCD_D5 = 5;
const int LCD_D6 = 18;
const int LCD_D7 = 19;

// Oggetto LCD
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// ================== RADIO (LkRadioStructure_RH + RadioHead RH_ASK) ==================

// Tipo fittizio: non usiamo la parte "structure", ci interessa solo il buffer grezzo
struct DummyPayload {
  uint8_t dummy;
};

using RxRadio = LkRadioStructure<DummyPayload>;
RxRadio radio;

// ================== POSIZIONI FISSE SUL DISPLAY ==================

// Riga 0
const uint8_t COL_ELETT = 0;   // 5 char
const uint8_t COL_TEMP  = 6;   // 4 char
const uint8_t COL_UMID  = 11;  // 4 char

// Riga 1
// - Caldaia2 a sinistra (5 char)
// - Caldaia1 a destra (4 char)
const uint8_t COL_CALD_LEFT  = 0;   // 5 char (Caldaia2)
const uint8_t COL_PRESS      = 6;   // 4 char
const uint8_t COL_CALD_RIGHT = 11;  // 4 char (Caldaia1)

// ================== GESTIONE CAMPI NON BLOCCANTE + TIMEOUT ==================

const unsigned long STAR_DURATION_MS = 500;
const unsigned long ONE_HOUR_MS      = 3600000UL;

// Stato di ciascun campo sul display
struct FieldState {
  uint8_t col;
  uint8_t row;
  uint8_t width;
  String  pendingValue;     // valore da scrivere dopo la fase "asterisco"
  bool    starActive;       // true = stiamo mostrando l'asterisco
  unsigned long starStartMs;

  bool    hasValue;         // true = almeno un valore è stato mostrato
  unsigned long lastUpdateMs; // istante in cui è stato scritto l'ultimo valore
  bool    dashShown;        // true = abbiamo mostrato "-" per timeout
};

// Campi: Elett, Temp, Umid, Caldaia1, Caldaia2, Pressione
FieldState fieldElett   { COL_ELETT,      0, 5, "", false, 0, false, 0, false };
FieldState fieldTemp    { COL_TEMP,       0, 4, "", false, 0, false, 0, false };
FieldState fieldUmid    { COL_UMID,       0, 4, "", false, 0, false, 0, false };
FieldState fieldCald1   { COL_CALD_RIGHT, 1, 4, "", false, 0, false, 0, false }; // Caldaia1 (destra, 4 char)
FieldState fieldCald2   { COL_CALD_LEFT,  1, 5, "", false, 0, false, 0, false }; // Caldaia2 (sinistra, 5 char)
FieldState fieldPress   { COL_PRESS,      1, 4, "", false, 0, false, 0, false };

// Valori globali (solo per debug/logica, opzionali)
uint16_t g_elettAttiva = 0;
uint16_t g_caldaia1    = 0;
uint16_t g_caldaia2    = 0;
float    g_temp        = NAN;
float    g_umid        = NAN;
float    g_press       = NAN;

// Flag per sapere se è il primo pacchetto ricevuto
bool g_firstReception = true;

// ----------------- Utility LCD -----------------

// Stampa un campo a larghezza fissa, opzionalmente allineato a destra
void lcdPrintField(uint8_t col, uint8_t row, const String &value, uint8_t width, bool rightAlign = true) {
  String s = value;

  // Se troppo lunga, tieni solo gli ultimi 'width' caratteri
  if (s.length() > width) {
    s = s.substring(s.length() - width);
  }

  // Padding fino a width
  while (s.length() < width) {
    if (rightAlign) s = " " + s;
    else            s += " ";
  }

  lcd.setCursor(col, row);
  lcd.print(s);
}

// Ritorna stringa con asterisco "centrato" nel campo
String starForWidth(uint8_t width) {
  if (width == 5) return "  *  ";
  if (width == 4) return " *  ";
  // fallback generico
  String s = "*";
  while (s.length() < width) {
    s = " " + s;
  }
  return s;
}

// Ritorna stringa con trattino "centrato" nel campo (per timeout)
String dashForWidth(uint8_t width) {
  if (width == 5) return "  -  ";
  if (width == 4) return " -  ";
  // fallback generico
  String s = "-";
  while (s.length() < width) {
    s = " " + s;
  }
  return s;
}

// Avvia l’aggiornamento non bloccante di un campo:
// - mostra subito l'asterisco
// - dopo STAR_DURATION_MS verrà scritto il valore pending
void startFieldUpdate(FieldState &f, const String &val) {
  f.pendingValue = val;
  f.starActive   = true;
  f.starStartMs  = millis();

  // nuovo valore in arrivo → non siamo più in stato di dash
  f.dashShown = false;

  // Mostra immediatamente l'asterisco (non blocca)
  String star = starForWidth(f.width);
  lcd.setCursor(f.col, f.row);
  lcd.print(star);
}

// Da chiamare periodicamente nel loop per completare gli aggiornamenti dei campi
void updateField(FieldState &f) {
  if (f.starActive) {
    unsigned long now = millis();
    if (now - f.starStartMs >= STAR_DURATION_MS) {
      // tempo scaduto: scrivi il valore definitivo
      lcdPrintField(f.col, f.row, f.pendingValue, f.width, true);
      f.starActive    = false;
      f.hasValue      = true;
      f.lastUpdateMs  = now;
      f.dashShown     = false;
    }
  }
}

// Aggiorna il timeout di un singolo campo
void updateTimeoutField(FieldState &f, unsigned long now) {
  if (!f.hasValue) return;         // mai visto un valore
  if (f.starActive) return;        // ancora in fase "asterisco"
  if (f.dashShown) return;         // dash già mostrato

  if (now - f.lastUpdateMs > ONE_HOUR_MS) {
    // mostra trattino al posto del valore
    String dash = dashForWidth(f.width);
    lcd.setCursor(f.col, f.row);
    lcd.print(dash);
    f.dashShown = true;
  }
}

// Aggiorna tutti i campi (asterisco → valore)
void updateAllFields() {
  updateField(fieldElett);
  updateField(fieldTemp);
  updateField(fieldUmid);
  updateField(fieldCald1);
  updateField(fieldCald2);
  updateField(fieldPress);
}

// Aggiorna timeouts (trattino dopo 1h di silenzio)
void updateAllTimeouts() {
  unsigned long now = millis();
  updateTimeoutField(fieldElett, now);
  updateTimeoutField(fieldTemp,  now);
  updateTimeoutField(fieldUmid,  now);
  updateTimeoutField(fieldCald1, now);
  updateTimeoutField(fieldCald2, now);
  updateTimeoutField(fieldPress, now);
}

// ================== ELABORAZIONE PACCHETTO ==================

void elaboraDatiBytes(const uint8_t* data, uint8_t len) {
  // Alla prima ricezione, pulisci il display e inizializza i campi vuoti
  if (g_firstReception) {
    g_firstReception = false;
    lcd.clear();
    lcdPrintField(fieldElett.col, fieldElett.row, "", fieldElett.width);
    lcdPrintField(fieldTemp.col,  fieldTemp.row,  "", fieldTemp.width);
    lcdPrintField(fieldUmid.col,  fieldUmid.row,  "", fieldUmid.width);
    lcdPrintField(fieldCald1.col, fieldCald1.row, "", fieldCald1.width);
    lcdPrintField(fieldPress.col, fieldPress.row, "", fieldPress.width);
    lcdPrintField(fieldCald2.col, fieldCald2.row, "", fieldCald2.width);
  }

  // Ci aspettiamo almeno 5 byte: 4 di payload + 1 di dispositivo
  if (len < 5) {
    Serial.print(F("⚠️ Pacchetto troppo corto, len="));
    Serial.println(len);
    return;
  }

  // Prendiamo gli ULTIMI 5 byte (nel caso in cui nel pacchetto ci fossero header aggiuntivi)
  const uint8_t* payloadBytes = data + (len - 5);

  uint8_t b0 = payloadBytes[0];
  uint8_t b1 = payloadBytes[1];
  uint8_t b2 = payloadBytes[2];
  uint8_t b3 = payloadBytes[3];
  uint8_t dispositivo = payloadBytes[4];

  // Debug in HEX dei 5 byte usati
  Serial.print(F("RX bytes HEX (ultimi 5): "));
  for (uint8_t i = 0; i < 5; i++) {
    if (payloadBytes[i] < 0x10) Serial.print('0');
    Serial.print(payloadBytes[i], HEX);
    Serial.print(' ');
  }
  Serial.print(F("  ID=0x"));
  if (dispositivo < 0x10) Serial.print('0');
  Serial.println(dispositivo, HEX);

  // ---------------- Dispositivi 0x0B (elettricità) e 0x63 (caldaia) ----------------
  if (dispositivo == 0x0B || dispositivo == 0x63) {
    uint16_t valore1 = ((uint16_t)b0 << 8) | b1;
    uint16_t valore2 = ((uint16_t)b2 << 8) | b3;

    if (dispositivo == 0x0B) {
      // Elettricità attiva / reattiva (ma visualizziamo solo l'attiva)
      g_elettAttiva = valore1;

      Serial.print(F("⚡ Elett Attiva: "));
      Serial.print(g_elettAttiva);
      Serial.print(F("  Reatt: "));
      Serial.println(valore2);

      // 1ª riga, col 0–4 (5 char) → ElettAttiva
      startFieldUpdate(fieldElett, String(g_elettAttiva));
    }
    else { // dispositivo == 0x63 → Caldaia1 / Caldaia2
      g_caldaia1 = valore1;
      g_caldaia2 = valore2;

      Serial.print(F("🔥 Caldaia1: "));
      Serial.print(g_caldaia1);
      Serial.print(F("  Caldaia2: "));
      Serial.println(g_caldaia2);

      // Inversione:
      // - Caldaia2 a sinistra (5 char, fieldCald2)
      // - Caldaia1 a destra (4 char, fieldCald1)
      startFieldUpdate(fieldCald2, String(g_caldaia2));
      startFieldUpdate(fieldCald1, String(g_caldaia1));
    }
  }

  // ---------------- Dispositivo 0x6A: Temperatura (float) ----------------
  else if (dispositivo == 0x6A) {
    uint8_t raw[4]      = { b0, b1, b2, b3 };
    uint8_t reversed[4] = { raw[3], raw[2], raw[1], raw[0] };

    float valore_float;
    memcpy(&valore_float, reversed, sizeof(float));

    g_temp = valore_float;

    Serial.print(F("🌡️ Temp: "));
    Serial.println(g_temp, 2);

    // 4 caratteri, es. "12.4"
    String s = String(g_temp, 1);  // 1 decimale
    if (s.length() > 4) s = s.substring(0, 4);

    // 1ª riga, col 6–9 (4 char)
    startFieldUpdate(fieldTemp, s);
  }

  // ---------------- Dispositivo 0x6B: Umidità (float → int arrotondato + %) ----------------
  else if (dispositivo == 0x6B) {
    uint8_t raw[4]      = { b0, b1, b2, b3 };
    uint8_t reversed[4] = { raw[3], raw[2], raw[1], raw[0] };

    float valore_float;
    memcpy(&valore_float, reversed, sizeof(float));

    g_umid = valore_float;

    // Arrotonda a intero (max 3 cifre visuali)
    int32_t u = (int32_t)roundf(g_umid);

    Serial.print(F("💧 Umid (int): "));
    Serial.println(u);

    // Costruisci stringa con simbolo % e lascia che il campo da 4 char la allinei
    // Esempi:
    //   u=5   -> "5%"   -> diventa "  5%"
    //   u=85  -> "85%"  -> " 85%"
    //   u=100 -> "100%" -> "100%"
    String s = String(u) + "%";
    if (s.length() > 4) {
      s = s.substring(s.length() - 4);
    }

    // 1ª riga, col 11–14 (4 char)
    startFieldUpdate(fieldUmid, s);
  }

  // ---------------- Dispositivo 0x6C: Pressione (float) ----------------
  else if (dispositivo == 0x6C) {
    uint8_t raw[4]      = { b0, b1, b2, b3 };
    uint8_t reversed[4] = { raw[3], raw[2], raw[1], raw[0] };

    float valore_float;
    memcpy(&valore_float, reversed, sizeof(float));

    g_press = valore_float;

    // Arrotonda al più vicino intero
    int32_t p = (int32_t)roundf(g_press);

    Serial.print(F("🌬️ Press: "));
    Serial.println(p);

    // 2ª riga, col 6–9 (4 char)
    startFieldUpdate(fieldPress, String(p));
  }

  // ---------------- Dispositivo sconosciuto ----------------
  else {
    Serial.print(F("ℹ️ Dispositivo sconosciuto ID=0x"));
    if (dispositivo < 0x10) Serial.print('0');
    Serial.println(dispositivo, HEX);
    // Sul display non facciamo nulla per i dispositivi sconosciuti
  }
}

// ================== SETUP & LOOP ==================

void setup() {
  Serial.begin(115200);
  while (!Serial) {;}
  delay(2000);

  // Inizializza LCD
  lcd.begin(16, 2);
  lcd.clear();

  // Messaggio iniziale (verrà cancellato alla prima ricezione radio)
  lcd.setCursor(0, 0);
  lcd.print("ESP32 RadioHead");
  lcd.setCursor(0, 1);
  lcd.print("In ascolto...");

  // Info su piattaforma RadioHead
  Serial.println();
  Serial.println(F("=== ESP32 + LkRadioStructure_RH + LCD 16x2 ==="));
  Serial.print(F("RH_PLATFORM = "));
  Serial.println(RH_PLATFORM);

  // Inizializza RadioHead tramite LkRadioStructure_RH
  // speed = 2000 bps, TX pin = 22 (non usato), RX = 21 (modulo RF), PTT = 23 (non usato)
  RxRadio::globalSetup(
      2000,   // speed
      22,     // transmit_pin (non usato)
      21,     // receive_pin (DOUT modulo RF)
      23,     // ptt_pin (non usato)
      false   // ptt_inverted
  );

  Serial.println(F("In ascolto sui pacchetti..."));
}

void loop() {
  if (radio.haveRawMessage()) {
    uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
    uint8_t len = 0;
    radio.getRawBuffer(buf, len);

    Serial.print(F("Pacchetto completo, len="));
    Serial.println(len);

    elaboraDatiBytes(buf, len);

    Serial.println(F("--------------------------------"));
  }

  // Aggiorna logica "non bloccante" dei campi (asterisco → valore)
  updateAllFields();

  // Aggiorna i timeout (mostra "-" dopo 1h di silenzio per ciascun campo)
  updateAllTimeouts();

  // Piccolo respiro per ESP32
  delay(1);
}
