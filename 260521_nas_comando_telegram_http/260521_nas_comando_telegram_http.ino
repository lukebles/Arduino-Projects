#include <Arduino.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <UniversalTelegramBot.h>
#include "config.h"

#define DEBUG 1
#define LED_PIN 2

#define LED_ON  LOW
#define LED_OFF HIGH

// -------- RELE 220V --------
#define RELAY_PIN 23

// Cambia questi due valori se il tuo modulo relè è attivo LOW
#define RELAY_ON  HIGH
#define RELAY_OFF LOW

static bool rete220Accesa = false;

#if DEBUG
  #define prt(x)  Serial.print(x)
  #define prtn(x) Serial.println(x)
#else
  #define prt(x)
  #define prtn(x)
#endif

static void releAccendi() {
  digitalWrite(RELAY_PIN, RELAY_ON);
  rete220Accesa = true;
  prtn("Rele on");
}

static void releSpegni() {
  digitalWrite(RELAY_PIN, RELAY_OFF);
  rete220Accesa = false;
  prtn("Rele off");
}

static String stato220Text() {
  return rete220Accesa ? "rete 220 accesa" : "rete 220 spenta";
}

// ------------ TELEGRAM --------------
String lastChatId;
WiFiClientSecure client;
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, client);

// ------------ WEBSERVER --------------
WebServer server(80);

// === THROTTLE MESSAGGI TELEGRAM ===
static const unsigned long MSG_MIN_INTERVAL_MS = 10000;
static unsigned long lastMsgMs = 0;
static String queuedChatId, queuedText;

static void sendMsgImmediate(const String& chatId, const String& text) {
  bot.sendMessage(chatId, text, "");
  lastMsgMs = millis();
}

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

// === TELEGRAM HEALTH ===
static unsigned long tgLastOkMs = 0;
static const unsigned long TG_STALE_MS = 30000;

static inline bool isTelegramStale() {
  return (millis() - tgLastOkMs) > TG_STALE_MS;
}

static int pollTelegramAndTrack() {
  int r = bot.getUpdates(bot.last_message_received + 1);
  if (r >= 0) tgLastOkMs = millis();
  return r;
}

// ====== GLOBALI PER RICONNESSIONE ======
static unsigned long _connLastWindowStart = 0;
static unsigned long _connLastAttempt     = 0;
static const unsigned long _CONN_WINDOW_MS  = 30000;
static const unsigned long _CONN_PERIOD_MS  = 60000;
static const unsigned long _CONN_ATTEMPT_MS = 1000;

static void attemptReconnectOnce() {
  if (WiFi.status() != WL_CONNECTED) {
    prtn("[NET] WiFi giù: tento reconnect");
    WiFi.disconnect(false, true);
    delay(5);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    return;
  }

  if (isTelegramStale()) {
    prtn("[NET] Telegram stale: reset TLS client");
    client.stop();
    delay(5);
    client.setInsecure();
  }
}

static void connectionMaintenanceTick() {
  unsigned long now = millis();

  bool wifiOk = WiFi.status() == WL_CONNECTED;
  bool needWindow = (!wifiOk) || (wifiOk && isTelegramStale());

  bool windowActive = (now - _connLastWindowStart) < _CONN_WINDOW_MS;

  if (needWindow && !windowActive && (now - _connLastWindowStart) >= _CONN_PERIOD_MS) {
    _connLastWindowStart = now;
    _connLastAttempt     = 0;
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
const int PREMI_POS  = 156;
const int RIPOSO_POS = 40;

static void servoAccendi() {
  for (int a = RIPOSO_POS; a <= PREMI_POS; a += 2) {
    servo.write(a);
    delay(10);
  }

  delay(1000);

  for (int a = PREMI_POS; a >= RIPOSO_POS; a -= 2) {
    servo.write(a);
    delay(10);
  }
}

static void servoSpegni() {
  for (int a = RIPOSO_POS; a <= PREMI_POS; a += 2) {
    servo.write(a);
    delay(10);
  }

  delay(5000);

  for (int a = PREMI_POS; a >= RIPOSO_POS; a -= 2) {
    servo.write(a);
    delay(10);
  }
}

// -------- STATO NAS --------
enum class NasState : uint8_t {
  SPENTO,
  LAMPEGGIANTE,
  ACCESO
};

enum class Pending : uint8_t {
  NONE,
  TO_ACCESO,
  TO_SPENTO
};

static NasState nasState = NasState::SPENTO;
static Pending  pending  = Pending::NONE;
static unsigned long transitionEndMs = 0;

static const unsigned long T_ACCENDI_MS = 4UL * 60UL * 1000UL;
static const unsigned long T_SPEGNI_MS  = 3UL * 60UL * 1000UL;

static String statoTextForCommand() {
  switch (nasState) {
    case NasState::LAMPEGGIANTE:
      return "nas lampeggiante";

    case NasState::ACCESO:
      return "nas acceso";

    case NasState::SPENTO:
      return "nas spento";
  }

  return "nas spento";
}

// -------- SEQUENZE NAS / 220 --------
static const unsigned long T_ATTESA_220_PRIMA_SERVO_MS = 10UL * 1000UL;

// Se "accendi" deve prima alimentare la 220, il servo viene azionato
// 10 secondi dopo senza bloccare loop(), Telegram e web server.
static bool attesaServoAccensione = false;
static unsigned long servoAccensioneAtMs = 0;

// Se true, al termine dello spegnimento NAS deve essere tolta anche la 220.
static bool pending220Off = false;

// Serve a distinguere uno spegnimento richiesto con "spegni" da quello
// avviato internamente da "220off".
static bool notificaNasSpento = false;

static void onTransitionCompleted() {
  if (pending == Pending::TO_ACCESO) {
    nasState = NasState::ACCESO;
    pending  = Pending::NONE;
    transitionEndMs = 0;
    sendMsgImmediate(String(TELEGRAM_CHAT_ID), "nas acceso");

  } else if (pending == Pending::TO_SPENTO) {
    nasState = NasState::SPENTO;
    pending  = Pending::NONE;
    transitionEndMs = 0;

    // Se lo spegnimento era stato richiesto con il comando "spegni",
    // segnala il completamento del solo NAS.
    if (notificaNasSpento) {
      notificaNasSpento = false;
      sendMsgImmediate(String(TELEGRAM_CHAT_ID), "nas spento");
    }

    // Se era pendente un 220off, togli alimentazione solo ora,
    // cioe' quando il NAS e' considerato realmente spento.
    if (pending220Off) {
      pending220Off = false;
      releSpegni();
      sendMsgImmediate(
        String(TELEGRAM_CHAT_ID),
        "spegnimento completo: nas spento e rete 220 spenta"
      );
    }
  }
}

static void transitionTick() {
  unsigned long now = millis();

  // Seconda fase di "accendi": dopo 10 s dall'accensione della 220
  // viene premuto il pulsante del NAS tramite servo.
  if (attesaServoAccensione && (long)(now - servoAccensioneAtMs) >= 0) {
    attesaServoAccensione = false;
    servoAccendi();
    transitionEndMs = millis() + T_ACCENDI_MS;
  }

  if (nasState != NasState::LAMPEGGIANTE) return;
  if (pending == Pending::NONE) return;

  // Durante i 10 secondi iniziali di "accendi" il timer dei 4 minuti
  // non e' ancora partito.
  if (transitionEndMs == 0) return;

  now = millis();
  if ((long)(now - transitionEndMs) >= 0) {
    onTransitionCompleted();
  }
}

// -------- COMANDI NAS --------
static void comando_ACCENDI(const String& chatId) {
  // Conferma sempre immediata della ricezione del comando.
  sendMsgImmediate(chatId, "comando accendi ricevuto");

  if (nasState == NasState::LAMPEGGIANTE) {
    sendMsgImmediate(chatId, "comando accendi rifiutato: nas in transizione");
    return;
  }

  if (nasState == NasState::ACCESO) {
    sendMsgImmediate(chatId, "nas acceso");
    return;
  }

  // Da questo momento il NAS e' considerato in fase di accensione.
  // In questo modo un eventuale 220off viene rifiutato anche durante
  // i 10 secondi che precedono l'azionamento del servo.
  nasState = NasState::LAMPEGGIANTE;
  pending  = Pending::TO_ACCESO;
  transitionEndMs = 0;
  notificaNasSpento = false;

  if (!rete220Accesa) {
    // Prima alimenta NAS e altri apparecchi collegati al rele'.
    releAccendi();

    // Poi aspetta 10 s prima di premere il pulsante del NAS.
    attesaServoAccensione = true;
    servoAccensioneAtMs = millis() + T_ATTESA_220_PRIMA_SERVO_MS;
    return;
  }

  // Se la 220 era gia' presente, il NAS puo' essere acceso subito.
  servoAccendi();
  transitionEndMs = millis() + T_ACCENDI_MS;
}

static void comando_SPEGNI(const String& chatId) {
  // Conferma sempre immediata della ricezione del comando.
  sendMsgImmediate(chatId, "comando spegni ricevuto");

  if (nasState == NasState::LAMPEGGIANTE) {
    sendMsgImmediate(chatId, "comando spegni rifiutato: nas in transizione");
    return;
  }

  if (nasState == NasState::SPENTO) {
    sendMsgImmediate(chatId, "nas spento");
    return;
  }

  // "spegni" riguarda solo il NAS: non modifica mai il rele' 220 V.
  notificaNasSpento = true;
  servoSpegni();

  nasState = NasState::LAMPEGGIANTE;
  pending  = Pending::TO_SPENTO;
  transitionEndMs = millis() + T_SPEGNI_MS;
}

static void comando_STATO(const String& chatId) {
  sendMsgImmediate(chatId, "comando stato ricevuto");
  sendMsgImmediate(chatId, statoTextForCommand());
}

// -------- COMANDI 220 --------
static void comando_220ON(const String& chatId) {
  // Conferma sempre immediata della ricezione del comando.
  sendMsgImmediate(chatId, "comando 220on ricevuto");

  releAccendi();

  // Un nuovo 220on annulla l'eventuale richiesta pendente di togliere
  // alimentazione al termine dello spegnimento del NAS.
  pending220Off = false;

  sendMsgImmediate(chatId, "rete 220 accesa");
}

static void comando_220OFF(const String& chatId) {
  // Conferma sempre immediata della ricezione del comando.
  sendMsgImmediate(chatId, "comando 220off ricevuto");

  if (nasState == NasState::LAMPEGGIANTE) {
    if (pending == Pending::TO_ACCESO) {
      // Mai togliere la 220 mentre il NAS si sta accendendo.
      sendMsgImmediate(chatId, "comando 220off rifiutato: nas in fase di accensione");
      return;
    }

    if (pending == Pending::TO_SPENTO) {
      // Il NAS si sta gia' spegnendo: non premere di nuovo il servo.
      // Togli la 220 soltanto quando lo spegnimento sara' completato.
      pending220Off = true;
      return;
    }

    sendMsgImmediate(chatId, "comando 220off rifiutato: nas in transizione");
    return;
  }

  if (nasState == NasState::ACCESO) {
    // Prima spegni correttamente il NAS, poi verra' tolta la 220
    // da onTransitionCompleted().
    pending220Off = true;
    notificaNasSpento = false;

    servoSpegni();

    nasState = NasState::LAMPEGGIANTE;
    pending  = Pending::TO_SPENTO;
    transitionEndMs = millis() + T_SPEGNI_MS;
    return;
  }

  // NAS gia' spento: la 220 puo' essere tolta immediatamente.
  if (nasState == NasState::SPENTO) {
    pending220Off = false;
    releSpegni();
    sendMsgImmediate(
      chatId,
      "spegnimento completo: nas spento e rete 220 spenta"
    );
  }
}

static void comando_220STATO(const String& chatId) {
  sendMsgImmediate(chatId, "comando 220stato ricevuto");
  sendMsgImmediate(chatId, stato220Text());
}

// -------- WEBSERVER --------
static String htmlPage() {
  String ip = WiFi.localIP().toString();

  String html;
  html += "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Controllo NAS</title>";

  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:20px;background:#f4f4f4;}";
  html += ".box{max-width:480px;margin:auto;background:white;padding:20px;border-radius:12px;box-shadow:0 2px 8px #999;}";
  html += "h1{font-size:24px;}";
  html += "a{display:block;text-align:center;margin:10px 0;padding:14px;border-radius:8px;text-decoration:none;color:white;font-weight:bold;}";
  html += ".on{background:#16803c;}";
  html += ".off{background:#b00020;}";
  html += ".stato{background:#333;}";
  html += ".rete{background:#005bbb;}";
  html += ".info{font-size:14px;color:#555;margin-top:20px;}";
  html += "</style>";

  html += "</head><body><div class='box'>";
  html += "<h1>Controllo NAS</h1>";

  html += "<p><b>Stato NAS:</b> " + statoTextForCommand() + "</p>";
  html += "<p><b>Stato 220:</b> " + stato220Text() + "</p>";

  html += "<a class='on' href='/accendi'>ACCENDI NAS</a>";
  html += "<a class='off' href='/spegni'>SPEGNI NAS</a>";
  html += "<a class='stato' href='/stato'>STATO NAS</a>";

  html += "<hr>";

  html += "<a class='rete' href='/220on'>ACCENDI RETE 220</a>";
  html += "<a class='off' href='/220off'>SPEGNI RETE 220</a>";
  html += "<a class='stato' href='/220stato'>STATO RETE 220</a>";

  html += "<div class='info'>";
  html += "IP ESP32: " + ip + "<br>";
  html += "Comandi diretti:<br>";
  html += "/accendi<br>";
  html += "/spegni<br>";
  html += "/stato<br>";
  html += "/220on<br>";
  html += "/220off<br>";
  html += "/220stato";
  html += "</div>";

  html += "</div></body></html>";

  return html;
}

static void webSendHtml(const String& html) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html; charset=utf-8", html);
}

static void webSendText(const String& text) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain; charset=utf-8", text);
}

static void handleWebRoot() {
  webSendHtml(htmlPage());
}

static void handleWebAccendi() {
  comando_ACCENDI(String(TELEGRAM_CHAT_ID));
  webSendHtml(htmlPage());
}

static void handleWebSpegni() {
  comando_SPEGNI(String(TELEGRAM_CHAT_ID));
  webSendHtml(htmlPage());
}

static void handleWebStato() {
  String risposta;
  risposta += statoTextForCommand();
  risposta += "\n";
  risposta += stato220Text();

  webSendText(risposta);
}

static void handleWeb220On() {
  comando_220ON(String(TELEGRAM_CHAT_ID));
  webSendHtml(htmlPage());
}

static void handleWeb220Off() {
  comando_220OFF(String(TELEGRAM_CHAT_ID));
  webSendHtml(htmlPage());
}

static void handleWeb220Stato() {
  webSendText(stato220Text());
}

static void handleWebCmd() {
  if (!server.hasArg("c")) {
    webSendText("Parametro mancante: usa /cmd?c=accendi");
    return;
  }

  String c = server.arg("c");
  c.trim();
  c.toLowerCase();

  if (c == "accendi") {
    comando_ACCENDI(String(TELEGRAM_CHAT_ID));
    webSendText("comando accendi ricevuto");

  } else if (c == "spegni") {
    comando_SPEGNI(String(TELEGRAM_CHAT_ID));
    webSendText("comando spegni ricevuto");

  } else if (c == "stato") {
    String risposta;
    risposta += statoTextForCommand();
    risposta += "\n";
    risposta += stato220Text();
    webSendText(risposta);

  } else if (c == "220on") {
    comando_220ON(String(TELEGRAM_CHAT_ID));
    webSendText("comando 220on ricevuto");

  } else if (c == "220off") {
    comando_220OFF(String(TELEGRAM_CHAT_ID));
    webSendText("comando 220off ricevuto");

  } else if (c == "220stato") {
    webSendText(stato220Text());

  } else {
    webSendText("comando non riconosciuto");
  }
}

static void handleWebNotFound() {
  server.send(404, "text/plain; charset=utf-8", "Pagina non trovata");
}

static void setupWebServer() {
  server.on("/", HTTP_GET, handleWebRoot);

  server.on("/accendi", HTTP_GET, handleWebAccendi);
  server.on("/spegni", HTTP_GET, handleWebSpegni);
  server.on("/stato", HTTP_GET, handleWebStato);

  server.on("/220on", HTTP_GET, handleWeb220On);
  server.on("/220off", HTTP_GET, handleWeb220Off);
  server.on("/220stato", HTTP_GET, handleWeb220Stato);

  server.on("/cmd", HTTP_GET, handleWebCmd);

  server.onNotFound(handleWebNotFound);

  server.begin();
  prtn("Webserver avviato su porta 80");
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

    } else if (text == "220on") {
      comando_220ON(lastChatId);

    } else if (text == "220off") {
      comando_220OFF(lastChatId);

    } else if (text == "220stato") {
      comando_220STATO(lastChatId);

    } else {
      sendMsgThrottled(lastChatId, "comandi: accendi spegni stato 220on 220off 220stato");
    }
  }
}

// -------- SETUP --------
void setup() {
  Serial.begin(9600);
  delay(300);

  prtn("Avvio");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_OFF);

  pinMode(RELAY_PIN, OUTPUT);
  releSpegni();

  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2400);
  servo.write(RIPOSO_POS);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setAutoReconnect(true);

  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, LED_OFF);
    delay(30);

    digitalWrite(LED_PIN, LED_ON);
    delay(1000);

    prt(".");
  }

  digitalWrite(LED_PIN, LED_OFF);

  prtn("\nWiFi connesso: " + WiFi.localIP().toString());

  setupWebServer();

  client.setInsecure();

  sendMsgImmediate(
    String(TELEGRAM_CHAT_ID),
    "Bot pronto (comandi: stato, accendi, spegni, 220on, 220off, 220stato). Web: http://" + WiFi.localIP().toString()
  );

  tgLastOkMs = millis();
}

// -------- LOOP --------
void loop() {
  static unsigned long lastCheckTime = 0;

  server.handleClient();

  transitionTick();

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "ACCENDI") {
      comando_ACCENDI(String(TELEGRAM_CHAT_ID));

    } else if (cmd == "SPEGNI") {
      comando_SPEGNI(String(TELEGRAM_CHAT_ID));

    } else if (cmd == "STATO") {
      comando_STATO(String(TELEGRAM_CHAT_ID));

    } else if (cmd == "220ON") {
      comando_220ON(String(TELEGRAM_CHAT_ID));

    } else if (cmd == "220OFF") {
      comando_220OFF(String(TELEGRAM_CHAT_ID));

    } else if (cmd == "220STATO") {
      comando_220STATO(String(TELEGRAM_CHAT_ID));
    }
  }

  if (millis() - lastCheckTime > 1000) {
    int numNewMessages = pollTelegramAndTrack();

    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = pollTelegramAndTrack();
    }

    lastCheckTime = millis();
  }

  flushMsgQueueIfDue();
  connectionMaintenanceTick();
}