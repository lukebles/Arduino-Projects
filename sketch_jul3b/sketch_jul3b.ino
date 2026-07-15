#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <esp_mac.h>

// =======================================================
// DEBUG
// =======================================================

// Metti 1 solo quando vuoi vedere le stampe seriali.
// Per basso consumo lascia 0.
#define DEBUG_SERIAL 0

#if DEBUG_SERIAL
  #define DBG_BEGIN() Serial.begin(115200)
  #define DBG_PRINT(x) Serial.print(x)
  #define DBG_PRINTLN(x) Serial.println(x)
  #define DBG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DBG_BEGIN()
  #define DBG_PRINT(x)
  #define DBG_PRINTLN(x)
  #define DBG_PRINTF(...)
#endif

// =======================================================
// CONFIGURAZIONE
// =======================================================

// MAC STA della CENTRALE
uint8_t macCentrale[] = {0x94, 0xE6, 0x86, 0x2D, 0xA7, 0x50};

#define NODE_TX1 1
#define TYPE_TWO_UINT16 1

#define ESPNOW_CHANNEL 6
#define SLEEP_SECONDS 10

// Timeout breve: resta sveglio solo il tempo della trasmissione.
// Se hai pacchetti persi, prova 150 o 200.
#define SEND_TIMEOUT_MS 100

// =======================================================
// PACCHETTO
// =======================================================

struct __attribute__((packed)) PacketTwoUint16 {
  uint8_t nodeId;
  uint8_t payloadType;
  uint32_t seq;
  uint16_t value1;
  uint16_t value2;
};

RTC_DATA_ATTR uint32_t packetCounter = 0;

volatile bool sendDone = false;
volatile bool sendOk = false;

// =======================================================
// CALLBACK INVIO
// =======================================================

void onDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  sendOk = (status == ESP_NOW_SEND_SUCCESS);
  sendDone = true;
}

// =======================================================
// DEEP SLEEP
// =======================================================

void vaiInDeepSleep() {
#if DEBUG_SERIAL
  DBG_PRINT("Deep sleep per secondi: ");
  DBG_PRINTLN(SLEEP_SECONDS);
  Serial.flush();
#endif

  esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_SECONDS * 1000000ULL);
  esp_deep_sleep_start();
}

// =======================================================
// SETUP
// =======================================================

void setup() {
  DBG_BEGIN();

#if DEBUG_SERIAL
  delay(300);
  DBG_PRINTLN();
  DBG_PRINTLN("TX1 ESP-NOW low power");
#endif

  // WiFi in modalità station.
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  // Imposta il canale 6.
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  // Inizializza ESP-NOW.
  if (esp_now_init() != ESP_OK) {
    vaiInDeepSleep();
  }

  esp_now_register_send_cb(onDataSent);

  // Aggiunge la centrale come peer.
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, macCentrale, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    vaiInDeepSleep();
  }

  // Prepara pacchetto.
  PacketTwoUint16 p;
  p.nodeId = NODE_TX1;
  p.payloadType = TYPE_TWO_UINT16;
  p.seq = packetCounter++;
  p.value1 = esp_random() % 65536;
  p.value2 = esp_random() % 65536;

  sendDone = false;
  sendOk = false;

  // Invio.
  esp_err_t result = esp_now_send(macCentrale, (uint8_t *)&p, sizeof(p));

  if (result == ESP_OK) {
    unsigned long startWait = millis();

    while (!sendDone && millis() - startWait < SEND_TIMEOUT_MS) {
      delay(1);
    }
  }

#if DEBUG_SERIAL
  DBG_PRINT("SEQ: ");
  DBG_PRINTLN(p.seq);

  DBG_PRINT("Invio: ");
  if (result != ESP_OK) {
    DBG_PRINTLN("ERRORE esp_now_send");
  } else if (!sendDone) {
    DBG_PRINTLN("TIMEOUT");
  } else if (sendOk) {
    DBG_PRINTLN("OK");
  } else {
    DBG_PRINTLN("FAIL");
  }
#endif

  // Subito in deep sleep.
  vaiInDeepSleep();
}

void loop() {
}