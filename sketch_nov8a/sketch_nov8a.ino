// -------------------------------------------------------------
// Test per LdrBlinkDetector su ESP32
// - Serial Monitor a 115200 bps
// - LED su GPIO 2: spento=OFF, acceso fisso=STEADY_ON, lampeggio=BLINKING
// - Comandi seriali per tuning: vedi fondo file
// -------------------------------------------------------------

#if !defined(ARDUINO_ARCH_ESP32)
  #error "Seleziona una scheda ESP32 in Strumenti → Scheda"
#endif

#include <Arduino.h>
#include "esp32-hal-adc.h"
#include "LdrBlinkDetector.h"   // metti il tuo header accanto allo sketch

// ====== OPZIONI HARDWARE ======
static const int LED_PIN = 2;     // LED integrato su molte dev board ESP32

// ====== CONFIG INIZIALE RAGIONEVOLE ======
// Nota: thOn/thOff dipendono dal tuo partitore. Con ADC a 12 bit: 0..4095.
LdrBlinkDetector::Config makeDefaultCfg() {
  LdrBlinkDetector::Config c;
  c.pin                 = 34;        // ADC
  c.adcBits             = 12;        // 0..4095
  c.attenuation         = ADC_11db;  // ~0..3.1V
  c.sampleMs            = 5;         // frequenza di campionamento
  // Soglie di esempio: parti così e poi regola da seriale con "t on off"
  c.thOn                = 30;      // accende (dipende dal tuo partitore)
  c.thOff               = 0;      // spegne (isteresi)
  // Lampi riconosciuti tra 80 ms e 1000 ms (periodi min/max di accensione/spegnimento)
  c.minFlashPeriodMs    = 80;
  c.maxFlashPeriodMs    = 1000;
  // Finestra per dire "accensa fissa" o "spenta fissa"
  c.steadyOnMs          = 2000;
  c.steadyOffMs         = 2000;
  c.flashScoreTarget    = 2;         // quanti lampi coerenti servono
  return c;
}

LdrBlinkDetector det(makeDefaultCfg());

// ====== STATO BLINK LED PER MODALITÀ "LAMPEGGIANTE" ======
uint32_t lastLedToggle = 0;
bool ledOn = false;

// ====== LOG PERIODICO ======
uint32_t lastLogMs = 0;
const uint32_t LOG_EVERY_MS = 250;  // stampa telemetria ogni 250 ms

// ====== CALLBACK ======
void onChange1(LdrBlinkDetector::LightState newS, int raw) {
  Serial.printf("[onChange] → %s (raw=%d)\r\n", det.stateName(), raw);
}
void onChange2(LdrBlinkDetector::LightState oldS, LdrBlinkDetector::LightState newS, int raw) {
  Serial.printf("[onChange2] %d→%d (%s) raw=%d\r\n",
                (int)oldS, (int)newS, det.stateName(), raw);
}

// ====== UTILS ======
void printConfig() {
  const auto& c = det.config();
  Serial.println(F("\r\n=== CONFIG CORRENTE ==="));
  Serial.printf("Pin ADC            : %d\r\n",  c.pin);
  Serial.printf("ADC bits           : %u\r\n",  c.adcBits);
  Serial.printf("Attenuation        : %d\r\n",  (int)c.attenuation);
  Serial.printf("thOn / thOff       : %d / %d\r\n", c.thOn, c.thOff);
  Serial.printf("sampleMs           : %lu\r\n", (unsigned long)c.sampleMs);
  Serial.printf("flash min/max (ms) : %lu / %lu\r\n",
                (unsigned long)c.minFlashPeriodMs, (unsigned long)c.maxFlashPeriodMs);
  Serial.printf("steady On/Off (ms) : %lu / %lu\r\n",
                (unsigned long)c.steadyOnMs, (unsigned long)c.steadyOffMs);
  Serial.printf("flashScoreTarget   : %u\r\n", c.flashScoreTarget);
  Serial.println(F("========================\r\n"));
}

void printHelp() {
  Serial.println(F("\r\nComandi:"));
  Serial.println(F("  h                : help"));
  Serial.println(F("  p                : stampa config corrente"));
  Serial.println(F("  r <min> <max>    : set flash range ms (es: r 80 1000)"));
  Serial.println(F("  t <on> <off>     : set soglie (es: t 1800 1200)"));
  Serial.println(F("  w <onMs> <offMs> : set finestre steady ms (es: w 2000 2000)"));
  Serial.println(F("  s <ms>           : set sampleMs (es: s 5)"));
  Serial.println(F("  g <target>       : set flashScoreTarget (es: g 2)"));
  Serial.println(F("Suggerimento: osserva 'raw' e regola t on/off per avere isteresi stabile.\r\n"));
}

// Lettura linea semplice da Serial
bool readLine(String& out) {
  static String buf;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      out = buf;
      buf = "";
      return true;
    }
    buf += c;
  }
  return false;
}

void handleCmd(const String& line) {
  if (line.length() == 0) return;
  char cmd = tolower(line.charAt(0));
  if (cmd == 'h') {
    printHelp();
  } else if (cmd == 'p') {
    printConfig();
  } else if (cmd == 'r') {
    long a, b;
    if (sscanf(line.c_str()+1, "%ld %ld", &a, &b) == 2) {
      det.setFlashRange((uint32_t)a, (uint32_t)b);
      Serial.printf("OK: flash range = %ld..%ld ms\r\n", a, b);
    } else Serial.println(F("Uso: r <minMs> <maxMs>"));
  } else if (cmd == 't') {
    long on, off;
    if (sscanf(line.c_str()+1, "%ld %ld", &on, &off) == 2) {
      det.setThresholds((int)on, (int)off);
      Serial.printf("OK: soglie thOn=%ld thOff=%ld\r\n", on, off);
    } else Serial.println(F("Uso: t <thOn> <thOff>"));
  } else if (cmd == 'w') {
    long onMs, offMs;
    if (sscanf(line.c_str()+1, "%ld %ld", &onMs, &offMs) == 2) {
      det.setSteadyWindows((uint32_t)onMs, (uint32_t)offMs);
      Serial.printf("OK: steady on/off = %ld/%ld ms\r\n", onMs, offMs);
    } else Serial.println(F("Uso: w <onMs> <offMs>"));
  } else if (cmd == 's') {
    long ms;
    if (sscanf(line.c_str()+1, "%ld", &ms) == 1 && ms > 0) {
      det.setSampleMs((uint32_t)ms);
      Serial.printf("OK: sampleMs = %ld\r\n", ms);
    } else Serial.println(F("Uso: s <ms>"));
  } else if (cmd == 'g') {
    long tgt;
    if (sscanf(line.c_str()+1, "%ld", &tgt) == 1 && tgt >= 0 && tgt <= 255) {
      // piccolo hack: accediamo direttamente al config via (const_cast) se servisse,
      // ma qui usiamo semplicemente un trucco: ricreiamo la config attuale.
      auto cfg = det.config();
      cfg.flashScoreTarget = (uint8_t)tgt;
      // Ricreiamo l'oggetto per applicare subito (opzionale). In alternativa aggiungi un setter nella classe.
      LdrBlinkDetector tmp(cfg);
      // Trasferiamo i callback e sostituiamo
      tmp.setOnChange(onChange1);
      tmp.setOnChange2(onChange2);
      det = tmp;         // richiede operatore= generato dal compilatore (OK qui)
      det.begin();
      Serial.printf("OK: flashScoreTarget = %ld\r\n", tgt);
    } else Serial.println(F("Uso: g <0..255>"));
  } else {
    Serial.println(F("Comando sconosciuto. Premi 'h' per la guida."));
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("\r\n=== LdrBlinkDetector - Test ==="));
  printHelp();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  det.setOnChange(onChange1);
  det.setOnChange2(onChange2);
  det.begin();

  printConfig();
}

void loop() {
  // 1) Aggiorna il detector spesso (non blocca)
  det.update();

  // 2) LED di stato
  auto st = det.getState();
  if (st == LdrBlinkDetector::OFF) {
    digitalWrite(LED_PIN, LOW);
  } else if (st == LdrBlinkDetector::STEADY_ON) {
    digitalWrite(LED_PIN, HIGH);
  } else { // BLINKING
    uint32_t now = millis();
    // lampeggio visivo a ~4 Hz per feedback (separato dalla frequenza letta)
    if (now - lastLedToggle >= 125) {
      lastLedToggle = now;
      ledOn = !ledOn;
      digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
    }
  }

  // 3) Log periodico: raw, level, stato
  uint32_t now = millis();
  if (now - lastLogMs >= LOG_EVERY_MS) {
    lastLogMs = now;
    Serial.printf("raw=%4d  lvl=%d  state=%d(%s)\r\n",
                  det.raw(), det.level() ? 1 : 0, det.stateCode(), det.stateName());
  }

  // 4) Comandi seriali
  String line;
  if (readLine(line)) handleCmd(line);
}
