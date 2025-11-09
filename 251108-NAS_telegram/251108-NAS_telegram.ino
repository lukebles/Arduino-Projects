#include <Arduino.h>
#include <ESP32Servo.h>
#include "LdrBlinkDetector.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "config.h" // TELEGRAM_BOT_TOKEN, TELEGRAM_CHAT_ID, WIFI_SSID, WIFI_PASSWORD

///// //////////////////////////// BUG /////////////
// NON RILEVA IL LAMPEGGIO DEL LED
// O ACCESO O SPENTO
///////////////////////////////////////////////////


#define DEBUG 1
#define LED_PIN 2

#define ACCESO LOW
#define SPENTO HIGH

#if DEBUG
  #define prt(x)  Serial.print(x)
  #define prtn(x) Serial.println(x)
#else
  #define prt(x)
  #define prtn(x)
#endif

// ------------ TELEGRAM --------------
String lastChatId; // ultimo chat_id che ti ha scritto (per risposte dirette)
WiFiClientSecure client;
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, client);

// === THROTTLE MESSAGGI TELEGRAM ===
static const unsigned long MSG_MIN_INTERVAL_MS = 10000; // 10 s
static unsigned long lastMsgMs = 0;
static String queuedChatId, queuedText;

// Invia subito, ignorando il throttle (usala per messaggi critici di avvio)
static void sendMsgImmediate(const String& chatId, const String& text) {
  bot.sendMessage(chatId, text, "");
  lastMsgMs = millis(); // opzionale: fa partire il timer del throttle
}


// invia subito se >=10s dall’ultimo invio; altrimenti accoda (l’ultimo messaggio vince)
static void sendMsgThrottled(const String& chatId, const String& text) {
  unsigned long now = millis();
  if (now - lastMsgMs >= MSG_MIN_INTERVAL_MS) {
    bot.sendMessage(chatId, text, "");
    lastMsgMs = now;
  } else {
    queuedChatId = chatId;
    queuedText   = text;
  }
}

// da chiamare spesso nel loop per svuotare la coda quando scadono i 10s
static void flushMsgQueueIfDue() {
  if (queuedText.length() == 0) return;
  unsigned long now = millis();
  if (now - lastMsgMs >= MSG_MIN_INTERVAL_MS) {
    bot.sendMessage(queuedChatId, queuedText, "");
    lastMsgMs = now;
    queuedChatId = "";
    queuedText   = "";
  }
}

// === TELEGRAM HEALTH (no chiamate extra, tracciamo l’ultimo poll OK) ===
static unsigned long tgLastOkMs = 0;                 // timestamp ultimo poll OK
static const unsigned long TG_STALE_MS = 30000;      // 30 s
static inline bool isTelegramStale() { return (millis() - tgLastOkMs) > TG_STALE_MS; }

// Sostituisce la tua getUpdates “nuda”: traccia l’ultimo OK senza ulteriori side-effects
static int pollTelegramAndTrack() {
  int r = bot.getUpdates(bot.last_message_received + 1);
  if (r >= 0) tgLastOkMs = millis(); // anche r==0 è OK
  return r;
}

// ====== GLOBALI PER RICONNESSIONE ======
static unsigned long _connLastWindowStart = 0;        // inizio ultima finestra (ms)
static unsigned long _connLastAttempt     = 0;        // ultimo tentativo (ms)
static const unsigned long _CONN_WINDOW_MS  = 30000;  // 30 s di tentativi
static const unsigned long _CONN_PERIOD_MS  = 60000;  // ogni minuto
static const unsigned long _CONN_ATTEMPT_MS = 1000;   // 1 tentativo/s

// Tentativo singolo non bloccante (prima Wi-Fi, poi Telegram TLS se “stale”)
static void attemptReconnectOnce() {
  if (WiFi.status() != WL_CONNECTED) {
    prtn("[NET] WiFi giù: tento reconnect");
    WiFi.disconnect(true, true);
    delay(5);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    return;
  }
  if (isTelegramStale()) {
    prtn("[NET] Telegram stale: reset TLS client");
    client.stop();
    delay(5);
    client.setInsecure(); // ricrea contesto TLS
    // niente getUpdates qui: ci pensa il poll normale
  }
}

// Scheduler: apre una finestra di 30s ogni 60s, 1 tentativo/s, solo se serve
static void connectionMaintenanceTick() {
  unsigned long now = millis();
  bool wifiOk = (WiFi.status() == WL_CONNECTED);
  bool needWindow = (!wifiOk) || (wifiOk && isTelegramStale());

  bool windowActive = (now - _connLastWindowStart) < _CONN_WINDOW_MS;

  if (needWindow && !windowActive && (now - _connLastWindowStart) >= _CONN_PERIOD_MS) {
    _connLastWindowStart = now;
    _connLastAttempt     = 0;    // tentativo immediato
    windowActive         = true;
    prtn("[NET] Avvio finestra riconnessione (30s).");
  }

  if (windowActive && (now - _connLastAttempt) >= _CONN_ATTEMPT_MS) {
    _connLastAttempt = now;
    attemptReconnectOnce();
  }
}

// -------- SERVO --------
Servo servo;
const int SERVO_PIN  = 18;
const int PREMI_POS  = 156;  // premi
const int RIPOSO_POS = 40;   // rilascia

static void servoAccendi() {
  for (int a = RIPOSO_POS; a <= PREMI_POS; a += 2) { servo.write(a); delay(10); }
  delay(1000); // tieni premuto 1 s (accensione)
  for (int a = PREMI_POS; a >= RIPOSO_POS; a -= 2) { servo.write(a); delay(10); }
}
static void servoSpegni() {
  for (int a = RIPOSO_POS; a <= PREMI_POS; a += 2) { servo.write(a); delay(10); }
  delay(5000); // tieni premuto 5 s (spegnimento)
  for (int a = PREMI_POS; a >= RIPOSO_POS; a -= 2) { servo.write(a); delay(10); }
}

// -------- LDR DETECTOR --------
LdrBlinkDetector::Config makeDefaultCfg() {
  LdrBlinkDetector::Config c;
  c.pin                 = 34;        // ADC
  c.adcBits             = 12;        // 0..4095
  c.attenuation         = ADC_11db;  // ~0..3.1V
  c.sampleMs            = 5;
  c.thOn                = 1;        // regola poi da seriale se serve
  c.thOff               = 0;
  c.minFlashPeriodMs    = 80;
  c.maxFlashPeriodMs    = 1000;
  c.steadyOnMs          = 2000;
  c.steadyOffMs         = 2000;
  c.flashScoreTarget    = 2;
  return c;
}
LdrBlinkDetector ldr(makeDefaultCfg());

// -------- STATO ULTIMO COMANDO --------
enum class LastCmd : uint8_t { NONE, ACCENDI, SPEGNI };
LastCmd lastCmd = LastCmd::NONE;

// -------- GUARDIA DI ANNUNCIO STATO --------
static bool firstStateKnown = false;
static LdrBlinkDetector::LightState lastAnnouncedState;

// -------- COMPOSIZIONE TESTO STATO --------
static String composeStatoText() {
  using LS = LdrBlinkDetector::LightState;
  LS s = ldr.getState();

  if (s == LS::BLINKING) {
    if (lastCmd == LastCmd::ACCENDI) return "NAS: in accensione";
    if (lastCmd == LastCmd::SPEGNI)  return "NAS: in spegnimento";
    return "NAS: stato lampeggiante";
  }
  if (s == LS::OFF)       return "NAS spento";
  if (s == LS::STEADY_ON) return "NAS acceso";
  return "NAS: stato sconosciuto";
}

// Forzato (comando "stato")
static void sendStatoForced(const String& chatId) {
  sendMsgThrottled(chatId, composeStatoText());
}

// Solo su vero cambio di stato (callback detector)
static void sendStatoIfStateChanged(const String& chatId,
                                    LdrBlinkDetector::LightState newS) {
  if (!firstStateKnown || newS != lastAnnouncedState) {
    lastAnnouncedState = newS;
    firstStateKnown = true;
    sendMsgThrottled(chatId, composeStatoText());
  }
}

// -------- CALLBACK CAMBIO STATO LDR --------
static void onLdrChange2(LdrBlinkDetector::LightState oldS,
                         LdrBlinkDetector::LightState newS,
                         int /*raw*/) {
  using LS = LdrBlinkDetector::LightState;

  // Nessuna notifica se non è cambiato nulla
  if (newS == oldS) return;

  // Entrata nel lampeggio: invia una sola volta
  if (newS == LS::BLINKING && oldS != LS::BLINKING) {
    // Componi messaggio coerente con l’ultimo comando
    if      (lastCmd == LastCmd::ACCENDI) sendMsgThrottled(String(TELEGRAM_CHAT_ID), "NAS: in accensione");
    else if (lastCmd == LastCmd::SPEGNI)  sendMsgThrottled(String(TELEGRAM_CHAT_ID), "NAS: in spegnimento");
    else                                  sendMsgThrottled(String(TELEGRAM_CHAT_ID), "NAS: stato lampeggiante");
    return;
  }

  // Uscita dal lampeggio o cambio tra stati stabili: invia stato corrente una volta
  if (newS == LS::OFF) {
    sendMsgThrottled(String(TELEGRAM_CHAT_ID), "NAS spento");
  } else if (newS == LS::STEADY_ON) {
    sendMsgThrottled(String(TELEGRAM_CHAT_ID), "NAS acceso");
  }
}


// -------- COMANDI --------
static void comando_ACCENDI(const String& chatId) {
  using LS = LdrBlinkDetector::LightState;
  lastCmd = LastCmd::ACCENDI;
  LS s = ldr.getState();

  if (s == LS::OFF) {
    // luce spenta fissa -> esegui accensione (1s)
    servoAccendi();
  } else if (s == LS::BLINKING) {
    // luce lampeggiante -> attendi
    sendMsgThrottled(chatId, "NAS: comando ricevuto, attendi");
  } else { // LS::STEADY_ON
    // luce accesa fissa -> già acceso
    sendMsgThrottled(chatId, "NAS già acceso");
  }
}

static void comando_SPEGNI(const String& chatId) {
  using LS = LdrBlinkDetector::LightState;
  lastCmd = LastCmd::SPEGNI;
  LS s = ldr.getState();

  if (s == LS::STEADY_ON) {
    // luce accesa fissa -> esegui spegnimento (5s)
    servoSpegni();
  } else if (s == LS::BLINKING) {
    // luce lampeggiante -> attendi
    sendMsgThrottled(chatId, "NAS: comando ricevuto, attendi");
  } else { // LS::OFF
    // luce spenta fissa -> già spento
    sendMsgThrottled(chatId, "NAS già spento");
  }
}

static void comando_STATO(const String& chatId) {
  sendStatoForced(chatId);
}

// -------- HANDLE MESSAGGI TELEGRAM --------
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    lastChatId = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    text.trim();

    if (text == "accendi") {
      comando_ACCENDI(lastChatId);
    } else if (text == "spegni") {
      comando_SPEGNI(lastChatId);
    } else if (text == "stato") {
      comando_STATO(lastChatId);
    } else {
      sendMsgThrottled(lastChatId, "comandi: accendi spegni stato");
    }
  }
}

// -------- SETUP / LOOP --------
void setup() {
  Serial.begin(115200);
  delay(300);
  prtn("Avvio");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, SPENTO);

  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2400);
  servo.write(RIPOSO_POS);

  ldr.begin();
  ldr.setOnChange2(onLdrChange2);

  // Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setAutoReconnect(true);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, SPENTO);  delay(30);
    digitalWrite(LED_PIN, ACCESO); delay(1000);
    prt(".");
  }
  digitalWrite(LED_PIN, SPENTO);
  prtn("\nWiFi connesso: " + WiFi.localIP().toString());

  client.setInsecure(); // TLS senza CA (semplice)


  sendMsgImmediate(String(TELEGRAM_CHAT_ID),
                 "Bot pronto (comandi: stato, accendi, spegni).");
  prtn("Pronto.");

  // inizializza “salute” Telegram: considera il primo poll come OK appena parte il loop
  tgLastOkMs = millis();
}

void loop() {
  static unsigned long lastCheckTime = 0;

  // Aggiorna il detector (genera eventuali callback)
  ldr.update();

  // Comandi via seriale (opzionali)
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n'); cmd.trim(); cmd.toUpperCase();
    if      (cmd == "ACCENDI") comando_ACCENDI(String(TELEGRAM_CHAT_ID));
    else if (cmd == "SPEGNI")  comando_SPEGNI(String(TELEGRAM_CHAT_ID));
    else if (cmd == "STATO")   comando_STATO(String(TELEGRAM_CHAT_ID));
  }

  // Poll Telegram ogni 1 s (con tracking OK)
  if (millis() - lastCheckTime > 1000) {
    int numNewMessages = pollTelegramAndTrack();
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = pollTelegramAndTrack();
    }
    lastCheckTime = millis();
  }

  // Svuota coda messaggi se sono passati 10 s dall’ultimo invio
  flushMsgQueueIfDue();

  // Manutenzione connessione (finestra 30 s ogni 60 s, 1 tentativo/s)
  connectionMaintenanceTick();
}

