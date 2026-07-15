#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ======================================================
// CONFIGURAZIONE
// ======================================================

#define ESPNOW_CHANNEL 13

#define NODE_TX1 1

#define TYPE_ENVIRONMENT 10

#define SENSOR_UNKNOWN 0
#define SENSOR_BME280  1
#define SENSOR_BMP280  2

// ======================================================
// FORMATO PACCHETTO AMBIENTALE
// ======================================================
//
// temp_x100  = temperatura °C x 100
// hum_x100   = umidità % x 100, oppure -1 se non disponibile
// press_x100 = pressione hPa x 100
//

struct __attribute__((packed)) PacketEnvironment {
  uint8_t nodeId;
  uint8_t payloadType;
  uint32_t seq;
  int16_t temp_x100;
  int16_t hum_x100;
  uint32_t press_x100;
  uint8_t sensorType;
};

// ======================================================
// UTILITY
// ======================================================

void stampaMacBytes(const uint8_t *mac) {
  char macStr[18];

  snprintf(
    macStr,
    sizeof(macStr),
    "%02X:%02X:%02X:%02X:%02X:%02X",
    mac[0], mac[1], mac[2],
    mac[3], mac[4], mac[5]
  );

  Serial.print(macStr);
}

void stampaMacCentrale() {
  Serial.print("MAC centrale WiFi.macAddress(): ");
  Serial.println(WiFi.macAddress());

  uint8_t macSta[6];

  esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, macSta);

  if (err == ESP_OK) {
    Serial.print("MAC centrale esp_wifi_get_mac(): ");
    stampaMacBytes(macSta);
    Serial.println();
  } else {
    Serial.print("Errore esp_wifi_get_mac(): ");
    Serial.println(err);
  }
}

const char *nomeSensore(uint8_t sensorType) {
  switch (sensorType) {
    case SENSOR_BME280:
      return "BME280";
    case SENSOR_BMP280:
      return "BMP280";
    default:
      return "Sconosciuto";
  }
}

// ======================================================
// CALLBACK RICEZIONE ESP-NOW
// ======================================================

void onDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  Serial.println();
  Serial.println("======================================");

  Serial.print("Ricevuto da MAC: ");
  stampaMacBytes(recv_info->src_addr);
  Serial.println();

  Serial.print("Lunghezza pacchetto: ");
  Serial.println(len);

  if (len != (int)sizeof(PacketEnvironment)) {
    Serial.println("Pacchetto ignorato: lunghezza non compatibile");
    return;
  }

  PacketEnvironment p;
  memcpy(&p, incomingData, sizeof(p));

  if (p.nodeId != NODE_TX1 || p.payloadType != TYPE_ENVIRONMENT) {
    Serial.println("Pacchetto ignorato: nodo/tipo non compatibile");
    return;
  }

  float temperatura = p.temp_x100 / 100.0;
  float pressione = p.press_x100 / 100.0;

  Serial.print("Nodo: TX");
  Serial.println(p.nodeId);

  Serial.print("Sensore: ");
  Serial.println(nomeSensore(p.sensorType));

  Serial.print("Sequenza: ");
  Serial.println(p.seq);

  Serial.print("Temperatura: ");
  Serial.print(temperatura, 2);
  Serial.println(" °C");

  if (p.hum_x100 >= 0) {
    float umidita = p.hum_x100 / 100.0;

    Serial.print("Umidità: ");
    Serial.print(umidita, 2);
    Serial.println(" %");
  } else {
    Serial.println("Umidità: non disponibile");
  }

  Serial.print("Pressione: ");
  Serial.print(pressione, 2);
  Serial.println(" hPa");
}

// ======================================================
// SETUP
// ======================================================

void setup() {
  Serial.begin(9600);
  delay(5000);

  Serial.println();
  Serial.println("CENTRALE ESP-NOW - Ricezione BME/BMP280");

  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  delay(200);

  WiFi.mode(WIFI_STA);
  delay(300);

  esp_wifi_start();
  delay(300);

  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  stampaMacCentrale();

  Serial.print("Canale ESP-NOW: ");
  Serial.println(ESPNOW_CHANNEL);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Errore inizializzazione ESP-NOW");
    while (true) {
      delay(1000);
    }
  }

  esp_now_register_recv_cb(onDataRecv);

  Serial.println("Centrale pronta");
}

// ======================================================
// LOOP
// ======================================================

void loop() {
  delay(1000);
}