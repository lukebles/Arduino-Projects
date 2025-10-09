#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ESP32Servo.h>
#include "LdrBlinkDetector.h"
#include "config.h"  // WIFI_SSID, WIFI_PASSWORD, TELEGRAM_BOT_TOKEN, TELEGRAM_CHAT_ID

// =================== Dichiarazioni anticipate ===================
static void eseguiComandoTesto(const String& cmdIn);

// =================== TELEGRAM ===================
WiFiClientSecure tgClient;
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, tgClient);

// Chat di destinazione: se vuota useremo l’ultima chat che ci scrive
String g_chatId = String(TELEGRAM_CHAT_ID);

// Stato/diagnostica Telegram (versione base: niente last_http_code, niente bot.user)
static bool telegram_ready = false;

// Polling Telegram
unsigned long lastTelegramPoll = 0;
const unsigned long TELEGRAM_POLL_MS = 500;   // leggero ma reattivo

static void tgSetTimeouts() {
  tgClient.setInsecure();   // semplice e robusto in embedded
  tgClient.setTimeout(15000);
}

static void sendTelegram(const String& msg) {
  if (!telegram_ready) return;
  if (g_chatId.isEmpty())   return;
  bool ok = bot.sendMessage(g_chatId, msg, "");
  if (!ok) {
    Serial.println("[TG] sendMessage FALLITA");
  }
}

static void telegramSelfTest() {
  // Nella tua versione: getMe() -> bool
  bool ok = bot.getMe();
  if (!ok) {
    Serial.println("[TG] getMe() FALLITA (controlla token/rete)");
    telegram_ready = false;
    return;
  }
  Serial.println("[TG] Bot pronto.");
  telegram_ready = true;

  if (String(TELEGRAM_CHAT_ID).length() > 0) {
    g_chatId = TELEGRAM_CHAT_ID;
    bool sent = bot.sendMessage(g_chatId, "Online ✅", "");
    Serial.println(sent ? "[TG] Annuncio chat fissa OK" : "[TG] Annuncio chat fissa ERRORE");
  } else {
    Serial.println("[TG] Nessuna TELEGRAM_CHAT_ID. Invia /start al bot: aggancio la prima chat.");
  }
}

// =================== WIFI (eventi + backoff) ===================
static volatile bool wifi_online = false;
static unsigned long nextWifiAttempt = 0;
static unsigned long wifiBackoffMs  = 1000;      // parte da 1s
static const unsigned long WIFI_BACKOFF_MAX = 60000; // fino a 60s

static void wifiResetBackoff() { wifiBackoffMs = 1000; }

static void wifiScheduleRetry(unsigned long now) {
  nextWifiAttempt = now + wifiBackoffMs;
  wifiBackoffMs = min(WIFI_BACKOFF_MAX, wifiBackoffMs * 2);
}

static void wifiTryConnect() {
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      wifi_online = true;
      wifiResetBackoff();
      Serial.print("WiFi OK. IP: ");
      Serial.println(WiFi.localIP());
      // Inizializza/ritesta Telegram ad ogni aggancio IP
      tgSetTimeouts();
      telegramSelfTest();
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      wifi_online = false;
      Serial.println("WiFi disconnesso. Pianifico retry…");
      wifiScheduleRetry(millis());
      break;

    default:
      break;
  }
}

static void wifiMaintain() {
  unsigned long now = millis();
  if (!wifi_online && now >= nextWifiAttempt) {
    Serial.println("WiFi: tentativo di riconnessione…");
    wifiTryConnect();
    wifiScheduleRetry(now); // se non arriva GOT_IP, ritenterà al prossimo slot
  }
}

// =================== SERVO ===================
Servo servo;
const int SERVO_PIN  = 18;
const int PREMI_POS  = 156;  // premi
const int RIPOSO_POS = 40;   // rilascia

// =================== LDR DETECTOR ===================
LdrBlinkDetector ldr;  // pin=34, soglie 1000/900, flicker 1–4 Hz default

// Azione in corso (guidata da comando)
enum PendingAction : uint8_t { PA_NONE, PA_WAIT_POWER_ON, PA_WAIT_POWER_OFF };
PendingAction pending = PA_NONE;
bool announcedTransition = false; // per stampare la transizione una sola volta

// =================== Log duale (Serial + Telegram) ===================
#include <stdarg.h>

static void logln(const String& s) {
  Serial.println(s);
  sendTelegram(s);
}

static void logfln(const char* fmt, ...) {
  char buf[192];
  va_list ap; va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  String s(buf);
  Serial.println(s);
  sendTelegram(s);
}

// =================== MOSSE SERVO ===================
static void accendi() {
  for (int a = RIPOSO_POS; a <= PREMI_POS; a += 2) { servo.write(a); delay(10); }
  delay(1000); // tieni premuto 1s
  for (int a = PREMI_POS; a >= RIPOSO_POS; a -= 2) { servo.write(a); delay(10); }
}
static void spegni() {
  for (int a = RIPOSO_POS; a <= PREMI_POS; a += 2) { servo.write(a); delay(10); }
  delay(6000); // tieni premuto 6s (se serve press lunga)
  for (int a = PREMI_POS; a >= RIPOSO_POS; a -= 2) { servo.write(a); delay(10); }
}

// =================== CALLBACK STATO LDR ===================
static void onState(LdrBlinkDetector::LightState s, int /*raw*/) {
  const char* name =
    (s==LdrBlinkDetector::OFF)       ? "SPENTA" :
    (s==LdrBlinkDetector::STEADY_ON) ? "ACCESA FISSA" :
                                       "LAMPEGGIANTE";
  logfln("[STATO] %s", name);
}

static void onLdrChange2(LdrBlinkDetector::LightState /*oldS*/,
                         LdrBlinkDetector::LightState newS,
                         int /*raw*/) {
  // Transizione stampata SOLO se richiesta da un comando in corso
  if (newS == LdrBlinkDetector::BLINKING && !announcedTransition && pending != PA_NONE) {
    if (pending == PA_WAIT_POWER_ON)  logln("[AZIONE] transizione verso accensione");
    if (pending == PA_WAIT_POWER_OFF) logln("[AZIONE] transizione verso spegnimento");
    announcedTransition = true;
  }

  // Conclusione azioni pendenti
  if (pending == PA_WAIT_POWER_ON  && newS == LdrBlinkDetector::STEADY_ON) {
    logln("[AZIONE] macchina accesa");
    pending = PA_NONE;
    announcedTransition = false;
  }
  if (pending == PA_WAIT_POWER_OFF && newS == LdrBlinkDetector::OFF) {
    logln("[AZIONE] macchina spenta");
    pending = PA_NONE;
    announcedTransition = false;
  }
}

// =================== COMANDI ===================
static void comando_ACCENDI() {
  logln("[COMANDO] ACCENDI_APPARECCHIO");
  auto s = ldr.getState();

  if (s == LdrBlinkDetector::STEADY_ON) {
    logln("[AZIONE] macchina già accesa");
    return;
  }
  if (pending == PA_NONE) {
    pending = PA_WAIT_POWER_ON;
    announcedTransition = false;
    accendi();
  }
}

static void comando_SPEGNI() {
  logln("[COMANDO] SPEGNI_APPARECCHIO");
  auto s = ldr.getState();

  if (s == LdrBlinkDetector::OFF) {
    logln("[AZIONE] macchina già spenta");
    return;
  }
  if (pending == PA_NONE) {
    pending = PA_WAIT_POWER_OFF;
    announcedTransition = false;
    spegni();
  }
}

static void comando_STATO() {
  const char* name =
    (ldr.getState()==LdrBlinkDetector::OFF)       ? "SPENTA" :
    (ldr.getState()==LdrBlinkDetector::STEADY_ON) ? "ACCESA FISSA" :
                                                    "LAMPEGGIANTE";
  logfln("[STATO] %s", name);
}

static void eseguiComandoTesto(const String& cmdIn) {
  String up = cmdIn; up.trim(); up.toUpperCase();
  if      (up == "ACCENDI") comando_ACCENDI();
  else if (up == "SPEGNI")  comando_SPEGNI();
  else if (up == "STATO")   comando_STATO();
  // nessun altro output
}

// =================== TELEGRAM HANDLER ===================
static void handleTelegram() {
  if (!wifi_online || !telegram_ready) return;

  if (millis() - lastTelegramPoll < TELEGRAM_POLL_MS) return;
  lastTelegramPoll = millis();

  int numNew = bot.getUpdates(bot.last_message_received + 1);

  if (numNew <= 0) {
    // nessun messaggio nuovo (o errore silenzioso nella tua versione)
    return;
  }

  for (int i = 0; i < numNew; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text    = bot.messages[i].text;

    if (String(TELEGRAM_CHAT_ID).length() == 0 && !chat_id.isEmpty()) {
      g_chatId = chat_id; // memorizza la prima chat che ci parla
      Serial.printf("[TG] Chat agganciata: %s\n", g_chatId.c_str());
      bot.sendMessage(g_chatId, "Agganciato ✅ Invia ACCENDI / SPEGNI / STATO", "");
    }

    eseguiComandoTesto(text);
  }
}

// =================== SETUP / LOOP ===================
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Avvio");

  // Eventi Wi-Fi + primo tentativo
  WiFi.onEvent(onWiFiEvent);
  wifiResetBackoff();
  nextWifiAttempt = 0;     // prova subito
  wifiTryConnect();

  // Servo
  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2400);
  servo.write(RIPOSO_POS);

  // LDR
  ldr.begin();
  ldr.setOnChange(onState);
  ldr.setOnChange2(onLdrChange2);

  Serial.println("Pronto.");
  // Annuncio Telegram viene inviato in telegramSelfTest() quando il Wi-Fi prende l'IP
}

void loop() {
  // Mantieni Wi-Fi vivo (retry automatici)
  wifiMaintain();

  // Detector (necessario per generare gli eventi)
  ldr.update();

  // Comandi via seriale: ACCENDI / SPEGNI / STATO
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    eseguiComandoTesto(cmd);
  }

  // Comandi via Telegram (solo se online)
  handleTelegram();
}
