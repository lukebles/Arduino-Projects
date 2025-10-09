
#include <Arduino.h>
#include <ESP32Servo.h>
#include "LdrBlinkDetector.h"
#include <WiFi.h> // Libreria per ESP32
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "config.h" // Contiene TELEGRAM_BOT_TOKEN, lastChatId, WIFI_SSID, WIFI_PASSWORD
/*
const int SERVO2_PIN = 25; filo viola
// I/O
const int IO1_PIN = 27;   // generico I/O filo marrone
const int IO2_PIN = 33;   // generico I/O (ADC1, ok anche come digitale)

*/
#define DEBUG 1 // 0 off, 1 on
#define LED_PIN 2    

#if DEBUG
#define prt(x) Serial.print(x)
#define prtn(x) Serial.println(x)
#else
#define prt(x)
#define prtn(x)
#endif

// ------------ TELEGRAM --------------
String lastChatId; // globale

WiFiClientSecure client;
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, client);

// -------- SERVO --------
Servo servo;
const int SERVO_PIN  = 18;
const int PREMI_POS  = 156;  // premi
const int RIPOSO_POS = 40;   // rilascia

// -------- LDR DETECTOR --------
LdrBlinkDetector ldr;  // pin=34, soglie 1000/900, flicker 1–4 Hz di default

// Azione in corso (guidata da comando)
enum PendingAction : uint8_t { PA_NONE, PA_WAIT_POWER_ON, PA_WAIT_POWER_OFF };
PendingAction pending = PA_NONE;
bool announcedTransition = false; // per stampare la transizione una sola volta

// -------- MOSSE SERVO --------
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

// -------- CALLBACK STATO --------
// static void onState(LdrBlinkDetector::LightState s, int /*raw*/) {
//   const char* name =
//     (s==LdrBlinkDetector::BLINKING)       ? "LAMPEGGIANTE" :
//     (s==LdrBlinkDetector::STEADY_ON) ? "ACCESA FISSA" :
//                                        "SPENTA";
//   Serial.printf("[STATO] %s\n", name);

//   // Se lo stato è stabile, resetta eventuali pending rimasti appesi
//   if (s == LdrBlinkDetector::OFF || s == LdrBlinkDetector::STEADY_ON) {
//     pending = PA_NONE;
//     announcedTransition = false;
//   }
// }


static void onLdrChange2(LdrBlinkDetector::LightState /*oldS*/,
                         LdrBlinkDetector::LightState newS,
                         int /*raw*/) {
  // Transizione stampata SOLO se richiesta da un comando in corso
  if (newS == LdrBlinkDetector::BLINKING && !announcedTransition && pending != PA_NONE) {
    if (pending == PA_WAIT_POWER_ON)  bot.sendMessage(lastChatId, "NAS in accensione", ""); 
    if (pending == PA_WAIT_POWER_OFF) bot.sendMessage(lastChatId, "NAS in spegnimento", ""); 
    announcedTransition = true;
  }

  // Conclusione azioni pendenti
  if (pending == PA_WAIT_POWER_ON  && newS == LdrBlinkDetector::STEADY_ON) {
    bot.sendMessage(lastChatId, "NAS acceso", ""); 
    pending = PA_NONE;
    announcedTransition = false;
  }
  if (pending == PA_WAIT_POWER_OFF && newS == LdrBlinkDetector::OFF) {
    bot.sendMessage(lastChatId, "NAS spento", ""); 
    pending = PA_NONE;
    announcedTransition = false;
  }
}

// -------- COMANDI SERIALI --------
static void comando_ACCENDI() {
  prtn("[COMANDO] ACCENDI_APPARECCHIO");
  auto s = ldr.getState();

  prtn(pending);

  if (s == LdrBlinkDetector::STEADY_ON) {
    prtn("[AZIONE] macchina già accesa");
    bot.sendMessage(lastChatId, "NAS già acceso", "");
    return;
  }

  if (pending == PA_NONE) {
    pending = PA_WAIT_POWER_ON;
    announcedTransition = false;
    accendi();
  }
}

static void comando_SPEGNI() {
  prtn("[COMANDO] SPEGNI_APPARECCHIO");
  auto s = ldr.getState();

  prtn(pending);

  if (s == LdrBlinkDetector::OFF) {
    prtn("[AZIONE] macchina già spenta");
    bot.sendMessage(lastChatId, "NAS già spento", "");
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

  char buf[64];
  snprintf(buf, sizeof(buf), "[STATO] %s", name);

  bot.sendMessage(lastChatId, buf, ""); 
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    lastChatId = String(bot.messages[i].chat_id);   // <-- usa questo
    String text    = bot.messages[i].text;

    if (text == "accendi") {
      comando_ACCENDI();
      bot.sendMessage(lastChatId, "OK, accendo.", "");
    } else if (text == "spegni") {
      comando_SPEGNI();
      bot.sendMessage(lastChatId, "OK, spengo.", "");
    } else if (text == "stato") {
      comando_STATO(); // dentro comando_STATO usa chat_id (vedi sotto)
    } else {
      bot.sendMessage(lastChatId, "comandi: accendi spegni stato", "");
    }
  }
}


// -------- SETUP/LOOP --------
void setup() {
  Serial.begin(115200);
  delay(2000);
  prtn("Avvio");

  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2400);
  servo.write(RIPOSO_POS);

  ldr.begin();
  //ldr.setOnChange(onState);
  ldr.setOnChange2(onLdrChange2);

  // Stampa lo stato iniziale una sola volta
  //comando_STATO();

    // Connessione Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setAutoReconnect(true);

  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, LOW);
    delay(30);
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    prt(".");
  }

  digitalWrite(LED_PIN, LOW);

  prtn("\nWiFi connesso: " + WiFi.localIP().toString());

  //client.setCACert(telegram_cert);

  client.setInsecure(); // Ignora certificati SSL

  if (bot.sendMessage(TELEGRAM_CHAT_ID, "Bot printo a ricevere comandi (stato accendi spegni) per il NAS.", "")) {
    prtn("Messaggio Telegram inviato!");
  } else {
    prtn("Errore nell'invio Telegram");
  }

  prtn("Pronto.");
}

void loop() {
  static unsigned long lastCheckTime = 0;
  // fa girare il detector (necessario per generare gli eventi)
  ldr.update();

  // comandi via seriale: ACCENDI / SPEGNI / STATO
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n'); cmd.trim(); cmd.toUpperCase();
    if      (cmd == "ACCENDI") comando_ACCENDI();
    else if (cmd == "SPEGNI")  comando_SPEGNI();
    else if (cmd == "STATO")   comando_STATO();
    // nessun altro output
  }

  // chiede ogni secondo se ci son omessaggi nuovi
  if (millis() - lastCheckTime > 1000) {
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastCheckTime = millis();
  }
}