#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

#include "config.h"

// ======================== CONFIG =========================
const int LED_PIN = 2;   // LED onboard ESP32 DevKit V1 (spesso GPIO2)

const uint32_t WIFI_TIMEOUT_MS = 15000;
const uint32_t SEND_EVERY_MS   = 1000;

// ======================= OGGETTI =========================
WiFiClientSecure secured_client;
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, secured_client);

// =================== DICHIARAZIONI =======================
bool connectWiFi();
void disconnectWiFi();

// ========================= SETUP =========================
void setup() {
  Serial.begin(115200);
  delay(2000);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);   // acceso = non connesso

  Serial.println();
  Serial.println("=== TEST TELEGRAM ===");

  if (connectWiFi()) {
    secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

    bool ok = bot.sendMessage(TELEGRAM_CHAT_ID, "ESP32 connesso: avvio test invio ogni secondo.", "");
    Serial.println(ok ? "Messaggio iniziale inviato." : "Errore invio messaggio iniziale.");
  } else {
    Serial.println("WiFi non connesso.");
  }
}

// ========================== LOOP =========================
void loop() {
  static uint32_t lastSend = 0;
  static uint32_t counter = 0;

  // Se il WiFi cade, prova a riconnettere
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnesso, provo a riconnettere...");
    digitalWrite(LED_PIN, HIGH);   // acceso = non connesso
    disconnectWiFi();

    if (connectWiFi()) {
      secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
      bot.sendMessage(TELEGRAM_CHAT_ID, "WiFi riconnesso. Riprendo il test.", "");
    } else {
      delay(1000);
      return;
    }
  }

  if (millis() - lastSend >= SEND_EVERY_MS) {
    lastSend = millis();
    counter++;

    String msg = "Messaggio di test #" + String(counter);
    bool ok = bot.sendMessage(TELEGRAM_CHAT_ID, msg, "");

    Serial.print("Invio: ");
    Serial.print(msg);
    Serial.println(ok ? " -> OK" : " -> ERRORE");
  }

  delay(20);
}

// ======================== WIFI ============================

bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  digitalWrite(LED_PIN, HIGH);   // acceso = non connesso / tentativo in corso

  Serial.print("Connessione WiFi");
  uint32_t start = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(300);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK, IP: ");
    Serial.println(WiFi.localIP());

    digitalWrite(LED_PIN, LOW);   // spento = connesso
    return true;
  }

  Serial.println("WiFi NON connesso.");
  digitalWrite(LED_PIN, HIGH);    // acceso = non connesso

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  return false;
}

void disconnectWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  digitalWrite(LED_PIN, HIGH);   // acceso = non connesso
  delay(50);
}