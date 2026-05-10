/*
  ESP32 bagno con Telegram + coda prenotazioni + deep sleep
  ---------------------------------------------------------

  Hardware:
  - ESP32 DevKit V1
  - Sensore reed su GPIO 33 (RTC GPIO)
  - Reed tra GPIO33 e GND
  - INPUT_PULLUP attiva
  - Porta chiusa  = reed chiuso  = pin LOW
  - Porta aperta  = pin HIGH

  Logica:
  - Se la porta passa da APERTA -> CHIUSA: bagno occupato
  - Se la porta passa da CHIUSA -> APERTA: bagno libero
    -> viene avvisato il primo utente in coda e rimosso dalla lista
  - Quando il bagno è occupato, l'ESP32 si sveglia periodicamente
    per controllare i messaggi Telegram
  - Quando il bagno è libero, dorme e si sveglia al cambio del reed

  Comandi bot:
  /start
  /prenota
  /annulla
  /stato
  /coda
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

#include "config.h"

// ======================== CONFIG BOT =========================
const char* BOT_NAME = "BagnoPrenotazioniBot";

// ====================== CONFIG HARDWARE ======================
static const gpio_num_t REED_PIN = GPIO_NUM_33;   // RTC GPIO per ext0
static const int DOOR_CLOSED_LEVEL = LOW;         // reed chiuso -> LOW

// ======================= CONFIG LOGICA =======================
const uint32_t WIFI_TIMEOUT_MS       = 15000;
const uint32_t TELEGRAM_POLL_MS      = 3000;      // finestra di polling ad ogni risveglio
const uint32_t BOT_MTBS              = 1000;      // intervallo tra getUpdates
const uint32_t OCCUPIED_WAKE_SEC     = 60;        // mentre occupato, wake periodico
const uint32_t SENSOR_SETTLE_MS      = 120;       // anti-rimbalzo
const uint8_t  SENSOR_SAMPLES        = 7;
const uint16_t SENSOR_SAMPLE_DELAYMS = 8;

const uint16_t MAX_OCCUPIED_MINUTES = 30; // se porta chiusa per mezz'ora: deepl sleep
uint16_t occupiedWakeCount = 0;


const int MAX_QUEUE = 30;

// ======================= OGGETTI GLOBALI =====================
WiFiClientSecure secured_client;
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, secured_client);
Preferences prefs;

// ======================== STRUTTURE DATI =====================
struct QueueEntry {
  String chatId;
  String name;
};

QueueEntry queueUsers[MAX_QUEUE];
int queueCount = 0;

bool doorClosed = false;
bool bathroomOccupied = false;
long lastUpdateId = 0;

// ===================== DICHIARAZIONI =========================
bool connectWiFi();
void disconnectWiFi();
bool readDoorClosedStable();

void loadState();
void saveState();

void loadQueue();
void saveQueue();

bool queueContains(const String& chatId, int* indexOut = nullptr);
bool enqueueUser(const String& chatId, const String& name);
bool removeUser(const String& chatId);
bool popFirstUser(QueueEntry& out);

String queuePositionText(const String& chatId);
String getStatusText();
String safeName(const String& s);

void notifyAdmin(const String& msg);
void notifyNextUserIfAny();

void processDoorTransition(bool oldClosed, bool newClosed);
void handleTelegram();
void handleSingleMessage(int i);

void prepareSleep(bool currentDoorClosed, bool occupied);

void updateOccupiedWakeCounter(bool currentDoorClosed);
bool allowOccupiedTimerWake();

// ============================ SETUP ==========================
void setup() {
  Serial.begin(9600);
  delay(2000);

  pinMode((int)REED_PIN, INPUT_PULLUP);

  // Mantiene la pull-up anche in deep sleep sui pin RTC
  rtc_gpio_init(REED_PIN);
  rtc_gpio_set_direction(REED_PIN, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(REED_PIN);
  rtc_gpio_pulldown_dis(REED_PIN);

  loadState();
  loadQueue();

  bool currentDoorClosed = readDoorClosedStable();
  updateOccupiedWakeCounter(currentDoorClosed);

  Serial.println();
  Serial.println("=== AVVIO ===");
  Serial.print("Wakeup cause: ");
  Serial.println((int)esp_sleep_get_wakeup_cause());

  Serial.print("Stato salvato doorClosed = ");
  Serial.println(doorClosed ? "CHIUSA" : "APERTA");

  Serial.print("Lettura attuale doorClosed = ");
  Serial.println(currentDoorClosed ? "CHIUSA" : "APERTA");

  Serial.print("occupiedWakeCount = ");
  Serial.println(occupiedWakeCount);

  // Gestione transizione porta
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

  // Stato occupato derivato dal sensore
  bathroomOccupied = doorClosed;

  bool needTelegramWindow = bathroomOccupied;

  if (needTelegramWindow) {
    if (connectWiFi()) {
      //configTime(0, 0, "pool.ntp.org", "time.nist.gov");
      secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

      uint32_t start = millis();
      uint32_t lastBotRun = 0;

      //while (millis() - start < TELEGRAM_POLL_MS) {
      //  if (millis() - lastBotRun >= BOT_MTBS) {
          handleTelegram();
      //    lastBotRun = millis();
      //  }
      //  delay(20);
      //}

      disconnectWiFi();
    } else {
      Serial.println("WiFi non connesso, salto polling Telegram.");
    }
  }

  saveState();
  prepareSleep(doorClosed, bathroomOccupied);
}

// ============================= LOOP ==========================
void loop() {
  // Mai usato: dopo deep sleep il chip riparte da setup()
}

// ======================== SENSORE PORTA ======================

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

void processDoorTransition(bool oldClosed, bool newClosed) {
  Serial.print("Transizione porta: ");
  Serial.print(oldClosed ? "CHIUSA" : "APERTA");
  Serial.print(" -> ");
  Serial.println(newClosed ? "CHIUSA" : "APERTA");

  // APERTA -> CHIUSA = bagno occupato
  if (!oldClosed && newClosed) {
    occupiedWakeCount = 0;
    bathroomOccupied = true;
    Serial.println("Bagno occupato.");

    if (strlen(ADMIN_CHAT_ID) > 0 && connectWiFi()) {
      configTime(0, 0, "pool.ntp.org", "time.nist.gov");
      secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
      notifyAdmin("🚪 Porta chiusa: bagno occupato.");
      disconnectWiFi();
    }
  }
  // CHIUSA -> APERTA = bagno libero
  else if (oldClosed && !newClosed) {
    occupiedWakeCount = 0;
    bathroomOccupied = false;
    Serial.println("Bagno libero.");

    if (connectWiFi()) {
      configTime(0, 0, "pool.ntp.org", "time.nist.gov");
      secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

      notifyNextUserIfAny();

      if (strlen(ADMIN_CHAT_ID) > 0) {
        notifyAdmin("✅ Porta aperta: bagno libero.");
      }

      disconnectWiFi();
    }
  }
}

// ========================= TELEGRAM ==========================

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

  Serial.println("Messaggio Telegram:");
  Serial.println("chat_id: " + chat_id);
  Serial.println("text: " + text);

  if (text == "/start") {
    String msg;
    msg += "Ciao " + from + "!\n";
    msg += "Sono " + String(BOT_NAME) + ".\n\n";
    msg += "Comandi disponibili:\n";
    msg += "/prenota - ti metti in coda\n";
    msg += "/annulla - esci dalla coda\n";
    msg += "/stato - stato bagno e lunghezza coda\n";
    msg += "/coda - tua posizione in coda\n\n";
    msg += "Stato attuale: ";
    msg += (bathroomOccupied ? "occupato" : "libero");

    bot.sendMessage(chat_id, msg, "");
    return;
  }

  if (text == "/stato") {
    bot.sendMessage(chat_id, getStatusText(), "");
    return;
  }

  if (text == "/coda") {
    bot.sendMessage(chat_id, queuePositionText(chat_id), "");
    return;
  }

  if (text == "/annulla") {
    if (removeUser(chat_id)) {
      saveQueue();
      bot.sendMessage(chat_id, "❌ Prenotazione annullata. Sei stato rimosso dalla coda.", "");
    } else {
      bot.sendMessage(chat_id, "Non eri in coda.", "");
    }
    return;
  }

  if (text == "/prenota") {
    if (!bathroomOccupied) {
      bot.sendMessage(chat_id, "✅ Il bagno al momento risulta libero. Non serve prenotarsi.", "");
      return;
    }

    int existingIdx = -1;
    if (queueContains(chat_id, &existingIdx)) {
      String msg = "Sei già in coda alla posizione #" + String(existingIdx + 1) + ".";
      bot.sendMessage(chat_id, msg, "");
      return;
    }

    if (enqueueUser(chat_id, from)) {
      saveQueue();
      String msg = "📝 Prenotazione registrata.\n";
      msg += "Sei in posizione #" + String(queueCount) + ".";
      bot.sendMessage(chat_id, msg, "");
    } else {
      bot.sendMessage(chat_id, "⚠️ Coda piena, impossibile prenotarti adesso.", "");
    }
    return;
  }

  bot.sendMessage(chat_id, "Comando non riconosciuto.\nUsa /start per vedere i comandi.", "");
}

// ===================== GESTIONE CODA =========================

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

bool removeUser(const String& chatId) {
  int idx = -1;
  if (!queueContains(chatId, &idx)) return false;

  for (int i = idx; i < queueCount - 1; i++) {
    queueUsers[i] = queueUsers[i + 1];
  }

  queueCount--;
  queueUsers[queueCount].chatId = "";
  queueUsers[queueCount].name   = "";
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

String queuePositionText(const String& chatId) {
  int idx = -1;
  if (!queueContains(chatId, &idx)) {
    return "Non sei in coda.";
  }

  String msg;
  msg += "Sei in coda alla posizione #" + String(idx + 1) + ".\n";
  msg += "Persone totali in coda: " + String(queueCount) + ".";
  return msg;
}

String getStatusText() {
  String msg;
  msg += "Stato bagno: ";
  msg += (bathroomOccupied ? "🚫 occupato" : "✅ libero");
  msg += "\nPersone in coda: ";
  msg += String(queueCount);
  return msg;
}

void notifyNextUserIfAny() {
  QueueEntry next;

  if (popFirstUser(next)) {
    saveQueue();

    String msg;
    msg += "🚽 Il bagno si è liberato.\n";
    msg += "Tocca a te.";

    bool ok = bot.sendMessage(next.chatId, msg, "");
    Serial.println(ok ? "Notifica inviata al primo in coda." : "Errore invio notifica.");

    if (strlen(ADMIN_CHAT_ID) > 0) {
      String adminMsg = "📣 Ho notificato il primo utente in coda";
      if (next.name.length() > 0) {
        adminMsg += " (" + next.name + ")";
      }
      adminMsg += ".";
      bot.sendMessage(ADMIN_CHAT_ID, adminMsg, "");
    }
  } else {
    Serial.println("Nessun utente in coda da notificare.");
  }
}

void notifyAdmin(const String& msg) {
  if (strlen(ADMIN_CHAT_ID) > 0) {
    bot.sendMessage(ADMIN_CHAT_ID, msg, "");
  }
}

// ======================== PERSISTENZA ========================

void loadState() {
  prefs.begin("bagno", true);
  doorClosed         = prefs.getBool("doorClosed", false);
  bathroomOccupied   = prefs.getBool("occupied", false);
  lastUpdateId       = prefs.getLong("lastUpd", 0);
  occupiedWakeCount  = prefs.getUShort("occWakeCnt", 0);
  prefs.end();
}

void saveState() {
  prefs.begin("bagno", false);
  prefs.putBool("doorClosed", doorClosed);
  prefs.putBool("occupied", bathroomOccupied);
  prefs.putLong("lastUpd", lastUpdateId);
  prefs.putUShort("occWakeCnt", occupiedWakeCount);
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

// =========================== WIFI ============================

bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

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
    return true;
  }

  Serial.println("WiFi NON connesso.");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  return false;
}

void disconnectWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(50);
}

// ======================== DEEP SLEEP =========================

void updateOccupiedWakeCounter(bool currentDoorClosed) {
  if (!currentDoorClosed) {
    occupiedWakeCount = 0;
    return;
  }

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

  // Incrementa solo se il risveglio è da timer e la porta è ancora chiusa
  if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    occupiedWakeCount++;
  }
}

bool allowOccupiedTimerWake() {
  uint32_t maxWakeCount = (MAX_OCCUPIED_MINUTES * 60UL) / OCCUPIED_WAKE_SEC;
  if (maxWakeCount == 0) maxWakeCount = 1;

  return occupiedWakeCount < maxWakeCount;
}

void prepareSleep(bool currentDoorClosed, bool occupied) {
  saveState();
  saveQueue();

  // wake al livello opposto a quello attuale
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
    if (allowOccupiedTimerWake()) {
      Serial.print("Abilito anche timer wake ogni ");
      Serial.print(OCCUPIED_WAKE_SEC);
      Serial.println(" secondi per controllare Telegram.");
      Serial.print("occupiedWakeCount = ");
      Serial.println(occupiedWakeCount);

      esp_sleep_enable_timer_wakeup((uint64_t)OCCUPIED_WAKE_SEC * 1000000ULL);
    } else {
      Serial.println("Timeout porta chiusa raggiunto: niente timer wake.");
      Serial.println("Resto in deep sleep fino al cambio stato del reed.");
    }
  }

  Serial.println("Entro in deep sleep.");
  Serial.flush();
  delay(100);
  esp_deep_sleep_start();
}

// =========================== UTILS ===========================

String safeName(const String& s) {
  String out = s;
  out.trim();
  out.replace("\n", " ");
  out.replace("\r", " ");
  return out;
}