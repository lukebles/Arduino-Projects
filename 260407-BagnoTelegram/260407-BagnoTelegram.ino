/*
  ESP32 bagno con Telegram + coda prenotazioni + deep sleep
  ---------------------------------------------------------

  Versione semplificata:
  - unico comando: "prenota"
  - nessun admin
  - tutti gli utenti sono uguali
  - i messaggi ricevuti mentre il bot era spento NON vengono eseguiti
  - quando il bagno si libera, viene notificato il primo in coda e rimosso

  Hardware:
  - ESP32 DevKit V1
  - sensore reed su GPIO 33 (RTC GPIO)
  - reed tra GPIO33 e GND
  - INPUT_PULLUP attiva
  - porta chiusa = reed chiuso = LOW
  - porta aperta = HIGH
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

#include "config.h"

// ======================== CONFIG =========================
const char* BOT_NAME = "BagnoPrenotazioniBot";

static const gpio_num_t REED_PIN = GPIO_NUM_33;
static const int DOOR_CLOSED_LEVEL = LOW;

const int LED_PIN = 2;   // LED onboard ESP32 DevKit V1 (spesso GPIO2)

const uint32_t WIFI_TIMEOUT_MS       = 15000;
const uint32_t TELEGRAM_POLL_MS      = 7000;
const uint32_t BOT_MTBS              = 1000;
const uint32_t OCCUPIED_WAKE_SEC     = 20;
const uint32_t SENSOR_SETTLE_MS      = 120;
const uint8_t  SENSOR_SAMPLES        = 7;
const uint16_t SENSOR_SAMPLE_DELAYMS = 8;

const int MAX_QUEUE = 30;

// ======================= OGGETTI =========================
WiFiClientSecure secured_client;
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, secured_client);
Preferences prefs;

// ======================= DATI ============================
struct QueueEntry {
  String chatId;
  String name;
};

QueueEntry queueUsers[MAX_QUEUE];
int queueCount = 0;

bool doorClosed = false;
bool bathroomOccupied = false;
long lastUpdateId = 0;

// =================== DICHIARAZIONI =======================
bool connectWiFi();
void disconnectWiFi();
bool readDoorClosedStable();

void loadState();
void saveState();

void loadQueue();
void saveQueue();

bool queueContains(const String& chatId, int* indexOut = nullptr);
bool enqueueUser(const String& chatId, const String& name);
bool popFirstUser(QueueEntry& out);

String safeName(const String& s);

void processDoorTransition(bool oldClosed, bool newClosed);
void skipOldTelegramMessages();
void handleTelegram();
void handleSingleMessage(int i);
void notifyNextUserIfAny();

void prepareSleep(bool currentDoorClosed, bool occupied);

// ========================= SETUP =========================
void setup() {
  Serial.begin(9600);
  delay(2000);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);   // acceso = non connesso

  pinMode((int)REED_PIN, INPUT_PULLUP);

  rtc_gpio_init(REED_PIN);
  rtc_gpio_set_direction(REED_PIN, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(REED_PIN);
  rtc_gpio_pulldown_dis(REED_PIN);

  loadState();
  loadQueue();

  bool currentDoorClosed = readDoorClosedStable();

  Serial.println();
  Serial.println("=== AVVIO ===");
  Serial.print("Wakeup cause: ");
  Serial.println((int)esp_sleep_get_wakeup_cause());

  Serial.print("Stato salvato doorClosed = ");
  Serial.println(doorClosed ? "CHIUSA" : "APERTA");

  Serial.print("Lettura attuale doorClosed = ");
  Serial.println(currentDoorClosed ? "CHIUSA" : "APERTA");

  if (currentDoorClosed != doorClosed) {
    delay(SENSOR_SETTLE_MS);
    bool confirmDoorClosed = readDoorClosedStable();

    if (confirmDoorClosed != doorClosed) {
      processDoorTransition(doorClosed, confirmDoorClosed);
      doorClosed = confirmDoorClosed;
    } else {
      doorClosed = confirmDoorClosed;
    }
  } else {
    doorClosed = currentDoorClosed;
  }

  bathroomOccupied = doorClosed;

  // Finché il bagno è occupato controlliamo Telegram
  if (bathroomOccupied) {
    if (connectWiFi()) {
      secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

      // Scarta tutti i messaggi arrivati mentre il bot era "spento"
      //skipOldTelegramMessages();

      uint32_t start = millis();
      uint32_t lastBotRun = 0;

      while (millis() - start < TELEGRAM_POLL_MS) {
        if (millis() - lastBotRun >= BOT_MTBS) {
          handleTelegram();
          lastBotRun = millis();
        }
        delay(20);
      }

      disconnectWiFi();
    } else {
      Serial.println("WiFi non connesso, salto polling Telegram.");
    }
  }

  saveState();
  prepareSleep(doorClosed, bathroomOccupied);
}

// ========================== LOOP =========================
void loop() {
  // Non usato: dopo deep sleep riparte da setup()
}

// ===================== LETTURA REED ======================

bool readDoorClosedStable() {
  int closedVotes = 0;

  for (uint8_t i = 0; i < SENSOR_SAMPLES; i++) {
    int v = digitalRead((int)REED_PIN);
    if (v == DOOR_CLOSED_LEVEL) {
      closedVotes++;
    }
    delay(SENSOR_SAMPLE_DELAYMS);
  }

  return (closedVotes > SENSOR_SAMPLES / 2);
}

// =================== TRANSIZIONE PORTA ===================

void processDoorTransition(bool oldClosed, bool newClosed) {
  Serial.print("Transizione porta: ");
  Serial.print(oldClosed ? "CHIUSA" : "APERTA");
  Serial.print(" -> ");
  Serial.println(newClosed ? "CHIUSA" : "APERTA");

  // APERTA -> CHIUSA = bagno occupato
  if (!oldClosed && newClosed) {
    bathroomOccupied = true;
    Serial.println("Bagno occupato.");
  }
  // CHIUSA -> APERTA = bagno libero
  else if (oldClosed && !newClosed) {
    bathroomOccupied = false;
    Serial.println("Bagno libero.");

    if (connectWiFi()) {
      secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
      notifyNextUserIfAny();
      disconnectWiFi();
    }
  }
}

// ======================= TELEGRAM ========================

// Scarta tutto quello che era già in coda sui server Telegram
void skipOldTelegramMessages() {
  Serial.println("Scarto eventuali messaggi vecchi...");

  int numNewMessages = bot.getUpdates(lastUpdateId + 1);

  while (numNewMessages) {
    for (int i = 0; i < numNewMessages; i++) {
      long upd = bot.messages[i].update_id;
      if (upd > lastUpdateId) {
        lastUpdateId = upd;
      }
    }
    numNewMessages = bot.getUpdates(lastUpdateId + 1);
  }

  saveState();

  Serial.print("Ultimo update ignorato: ");
  Serial.println(lastUpdateId);
}

void handleTelegram() {
  int numNewMessages = bot.getUpdates(lastUpdateId + 1);

  while (numNewMessages) {
    for (int i = 0; i < numNewMessages; i++) {
      handleSingleMessage(i);

      long upd = bot.messages[i].update_id;
      if (upd > lastUpdateId) {
        lastUpdateId = upd;
      }
    }

    saveState();
    numNewMessages = bot.getUpdates(lastUpdateId + 1);
  }
}

void handleSingleMessage(int i) {
  String chat_id = bot.messages[i].chat_id;
  String text    = bot.messages[i].text;
  String from    = safeName(bot.messages[i].from_name);

  if (from.length() == 0) {
    from = "utente";
  }

  text.trim();
  text.toLowerCase();

  Serial.println("Messaggio Telegram:");
  Serial.println("chat_id: " + chat_id);
  Serial.println("text: " + text);

  if (text == "prenota") {
    if (!bathroomOccupied) {
      bot.sendMessage(chat_id, "Il bagno è libero.", "");
      return;
    }

    int existingIdx = -1;
    if (queueContains(chat_id, &existingIdx)) {
      String msg = "Sei già in coda alla posizione " + String(existingIdx + 1) + ".";
      bot.sendMessage(chat_id, msg, "");
      return;
    }

    if (enqueueUser(chat_id, from)) {
      saveQueue();
      String msg = "Prenotazione registrata. Sei in posizione " + String(queueCount) + ".";
      bot.sendMessage(chat_id, msg, "");
    } else {
      bot.sendMessage(chat_id, "Coda piena.", "");
    }

    return;
  }

  if (text == "istruzioni") {
    bot.sendMessage(chat_id, "*prenota* se il bagno è occupato, ti avverte quando è il tuo turno.", "");
    bot.sendMessage(chat_id, "se non si ha risposta entro 30 secondi il bagno è libero", "");
  }
  // Qualsiasi altro messaggio viene ignorato
}

// ======================= CODA ============================

bool queueContains(const String& chatId, int* indexOut) {
  for (int i = 0; i < queueCount; i++) {
    if (queueUsers[i].chatId == chatId) {
      if (indexOut) *indexOut = i;
      return true;
    }
  }

  if (indexOut) *indexOut = -1;
  return false;
}

bool enqueueUser(const String& chatId, const String& name) {
  if (queueCount >= MAX_QUEUE) return false;

  queueUsers[queueCount].chatId = chatId;
  queueUsers[queueCount].name   = name;
  queueCount++;
  return true;
}

bool popFirstUser(QueueEntry& out) {
  if (queueCount <= 0) return false;

  out = queueUsers[0];

  for (int i = 0; i < queueCount - 1; i++) {
    queueUsers[i] = queueUsers[i + 1];
  }

  queueCount--;
  queueUsers[queueCount].chatId = "";
  queueUsers[queueCount].name   = "";
  return true;
}

void notifyNextUserIfAny() {
  QueueEntry next;

  if (popFirstUser(next)) {
    saveQueue();

    String msg = "Il bagno si è liberato. Tocca a te.";
    bool ok = bot.sendMessage(next.chatId, msg, "");

    Serial.println(ok ? "Notifica inviata al primo in coda." : "Errore invio notifica.");
  } else {
    Serial.println("Nessun utente in coda da notificare.");
  }
}

// ===================== PERSISTENZA ========================

void loadState() {
  prefs.begin("bagno", true);
  doorClosed       = prefs.getBool("doorClosed", false);
  bathroomOccupied = prefs.getBool("occupied", false);
  lastUpdateId     = prefs.getLong("lastUpd", 0);
  prefs.end();
}

void saveState() {
  prefs.begin("bagno", false);
  prefs.putBool("doorClosed", doorClosed);
  prefs.putBool("occupied", bathroomOccupied);
  prefs.putLong("lastUpd", lastUpdateId);
  prefs.end();
}

void loadQueue() {
  queueCount = 0;

  prefs.begin("bagno", true);
  String json = prefs.getString("queue", "[]");
  prefs.end();

  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.println("Errore caricamento coda JSON, la resetto.");
    return;
  }

  JsonArray arr = doc.as<JsonArray>();
  for (JsonVariant v : arr) {
    if (queueCount >= MAX_QUEUE) break;

    queueUsers[queueCount].chatId = String(v["chatId"] | "");
    queueUsers[queueCount].name   = String(v["name"] | "");

    if (queueUsers[queueCount].chatId.length() > 0) {
      queueCount++;
    }
  }
}

void saveQueue() {
  DynamicJsonDocument doc(4096);
  JsonArray arr = doc.to<JsonArray>();

  for (int i = 0; i < queueCount; i++) {
    JsonObject o = arr.createNestedObject();
    o["chatId"] = queueUsers[i].chatId;
    o["name"]   = queueUsers[i].name;
  }

  String json;
  serializeJson(doc, json);

  prefs.begin("bagno", false);
  prefs.putString("queue", json);
  prefs.end();
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

// ===================== DEEP SLEEP =========================

void prepareSleep(bool currentDoorClosed, bool occupied) {
  saveState();
  saveQueue();

  int wakeLevel = currentDoorClosed ? HIGH : LOW;

  Serial.println();
  Serial.println("=== PREPARAZIONE DEEP SLEEP ===");
  Serial.print("Porta attuale: ");
  Serial.println(currentDoorClosed ? "CHIUSA" : "APERTA");
  Serial.print("Bagno: ");
  Serial.println(occupied ? "OCCUPATO" : "LIBERO");
  Serial.print("Wake ext0 su livello: ");
  Serial.println(wakeLevel == HIGH ? "HIGH" : "LOW");

  esp_sleep_enable_ext0_wakeup(REED_PIN, wakeLevel);

  if (occupied) {
    Serial.print("Abilito anche timer wake ogni ");
    Serial.print(OCCUPIED_WAKE_SEC);
    Serial.println(" secondi per controllare Telegram.");
    esp_sleep_enable_timer_wakeup((uint64_t)OCCUPIED_WAKE_SEC * 1000000ULL);
  }

  Serial.println("Entro in deep sleep.");
  Serial.flush();
  delay(100);
  esp_deep_sleep_start();
}

// ======================== UTILS ===========================

String safeName(const String& s) {
  String out = s;
  out.trim();
  out.replace("\n", " ");
  out.replace("\r", " ");
  return out;
}