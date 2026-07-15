#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <driver/gpio.h>
#include <math.h>

#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>

// ======================================================
// CONFIGURAZIONE
// ======================================================

// Sostituisci con il MAC della centrale
// Esempio MAC centrale 24:6F:28:AA:BB:CC
// diventa {0x24, 0x6F, 0x28, 0xAA, 0xBB, 0xCC}
uint8_t macCentrale[] = {0x04, 0x83, 0x08, 0x59, 0x3b, 0xa0};

// Deve essere uguale al canale impostato sulla centrale
#define ESPNOW_CHANNEL 13 // era 1

#define NODE_TX1 1

#define TYPE_ENVIRONMENT 10

#define SENSOR_UNKNOWN 0
#define SENSOR_BME280  1
#define SENSOR_BMP280  2

// Deep sleep: 5 minuti
#define SLEEP_SECONDS 300

// GPIO che alimenta il sensore.
// Scegli un GPIO normale, evitando pin di boot/strapping.
#define SENSOR_POWER_PIN  4

// Pin I2C.
// Cambiali in base alla tua scheda ESP32S3.
#define I2C_SDA           5
#define I2C_SCL           6

// Indirizzi I2C più comuni per BME/BMP280
#define SENSOR_ADDR_1 0x76
#define SENSOR_ADDR_2 0x77

// Se vuoi tenere il pin alimentazione sensore forzato LOW anche in deep sleep.
// Utile per evitare che il pin diventi flottante.
#define USA_GPIO_HOLD_IN_DEEP_SLEEP 1

// ======================================================
// FORMATO PACCHETTO
// ======================================================
//
// temp_x100  = temperatura °C x 100
// hum_x100   = umidità % x 100, oppure -1 se BMP280
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
// OGGETTI E VARIABILI
// ======================================================

Adafruit_BME280 bme;
Adafruit_BMP280 bmp;

RTC_DATA_ATTR uint32_t packetCounter = 0;

volatile bool sendDone = false;
volatile bool sendOk = false;

// ======================================================
// CALLBACK INVIO ESP-NOW
// Firma corretta per Arduino-ESP32 3.3.x
// ======================================================

void onDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  sendOk = (status == ESP_NOW_SEND_SUCCESS);
  sendDone = true;
}

// ======================================================
// GESTIONE ALIMENTAZIONE SENSORE
// ======================================================

void accendiSensore() {
#if USA_GPIO_HOLD_IN_DEEP_SLEEP
  gpio_deep_sleep_hold_dis();
  gpio_hold_dis((gpio_num_t)SENSOR_POWER_PIN);
#endif

  pinMode(SENSOR_POWER_PIN, OUTPUT);
  digitalWrite(SENSOR_POWER_PIN, LOW);
  delay(20);

  digitalWrite(SENSOR_POWER_PIN, HIGH);

  // Tempo per stabilizzare alimentazione sensore e breakout
  delay(150);

  Wire.begin(I2C_SDA, I2C_SCL);
  delay(30);
}

void spegniSensore() {
  Wire.end();

  // Evita alimentazione di ritorno attraverso SDA/SCL.
  pinMode(I2C_SDA, INPUT);
  pinMode(I2C_SCL, INPUT);

  digitalWrite(SENSOR_POWER_PIN, LOW);
  pinMode(SENSOR_POWER_PIN, OUTPUT);

#if USA_GPIO_HOLD_IN_DEEP_SLEEP
  gpio_hold_en((gpio_num_t)SENSOR_POWER_PIN);
  gpio_deep_sleep_hold_en();
#endif
}

// ======================================================
// DEEP SLEEP
// ======================================================

void vaiInDeepSleep() {
  Serial.print("Deep sleep per secondi: ");
  Serial.println(SLEEP_SECONDS);
  Serial.flush();

  esp_now_deinit();

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop();

  esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_SECONDS * 1000000ULL);
  esp_deep_sleep_start();
}

// ======================================================
// RILEVAZIONE E LETTURA SENSORE
// ======================================================

uint8_t rilevaSensore() {
  if (bme.begin(SENSOR_ADDR_1, &Wire)) {
    return SENSOR_BME280;
  }

  if (bme.begin(SENSOR_ADDR_2, &Wire)) {
    return SENSOR_BME280;
  }

  if (bmp.begin(SENSOR_ADDR_1)) {
    return SENSOR_BMP280;
  }

  if (bmp.begin(SENSOR_ADDR_2)) {
    return SENSOR_BMP280;
  }

  return SENSOR_UNKNOWN;
}

bool leggiSensore(uint8_t sensorType, float &temperatura, float &umidita, float &pressione_hPa) {
  if (sensorType == SENSOR_BME280) {
    // Modalità a basso consumo: misura singola forzata, oversampling minimo.
    bme.setSampling(
      Adafruit_BME280::MODE_FORCED,
      Adafruit_BME280::SAMPLING_X1,
      Adafruit_BME280::SAMPLING_X1,
      Adafruit_BME280::SAMPLING_X1,
      Adafruit_BME280::FILTER_OFF
    );

    if (!bme.takeForcedMeasurement()) {
      return false;
    }

    temperatura = bme.readTemperature();
    umidita = bme.readHumidity();
    pressione_hPa = bme.readPressure() / 100.0;

    return true;
  }

  if (sensorType == SENSOR_BMP280) {
    // Il BMP280 non misura umidità.
    temperatura = bmp.readTemperature();
    umidita = NAN;
    pressione_hPa = bmp.readPressure() / 100.0;

    return true;
  }

  return false;
}

// ======================================================
// ESP-NOW
// ======================================================

bool inizializzaEspNow() {
#if USA_GPIO_HOLD_IN_DEEP_SLEEP
  // Il deep sleep hold può essere rimasto attivo dal ciclo precedente.
  // Lo disattiviamo, altrimenti alcuni GPIO potrebbero non cambiare stato.
  gpio_deep_sleep_hold_dis();
#endif

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  delay(100);

  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  Serial.print("MAC TX1: ");
  Serial.println(WiFi.macAddress());

  Serial.print("Canale ESP-NOW: ");
  Serial.println(ESPNOW_CHANNEL);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Errore init ESP-NOW");
    return false;
  }

  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, macCentrale, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Errore aggiunta peer centrale");
    return false;
  }

  return true;
}

bool inviaPacchetto(const PacketEnvironment &p) {
  sendDone = false;
  sendOk = false;

  esp_err_t result = esp_now_send(macCentrale, (uint8_t *)&p, sizeof(p));

  if (result != ESP_OK) {
    Serial.print("Errore esp_now_send: ");
    Serial.println(result);
    return false;
  }

  unsigned long startWait = millis();

  while (!sendDone && millis() - startWait < 1000) {
    delay(10);
  }

  Serial.print("Esito invio: ");
  Serial.println(sendOk ? "OK" : "ERRORE / TIMEOUT");

  return sendOk;
}

// ======================================================
// SETUP
// ======================================================

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("TX1 LOW POWER - BME/BMP280 alimentato da GPIO + ESP-NOW");

  // Frequenza CPU ridotta: non indispensabile, ma aiuta un po' nei consumi
  // durante il breve periodo in cui il micro è sveglio.
  setCpuFrequencyMhz(80);

  // 1. Accendo il sensore tramite GPIO
  accendiSensore();

  // 2. Rilevo se è BME280 o BMP280
  uint8_t sensorType = rilevaSensore();

  if (sensorType == SENSOR_UNKNOWN) {
    Serial.println("Sensore BME280/BMP280 non trovato");

    spegniSensore();
    vaiInDeepSleep();
  }

  Serial.print("Sensore rilevato: ");

  if (sensorType == SENSOR_BME280) {
    Serial.println("BME280");
  } else if (sensorType == SENSOR_BMP280) {
    Serial.println("BMP280");
  }

  // 3. Leggo il sensore
  float temperatura = NAN;
  float umidita = NAN;
  float pressione_hPa = NAN;

  if (!leggiSensore(sensorType, temperatura, umidita, pressione_hPa)) {
    Serial.println("Errore lettura sensore");

    spegniSensore();
    vaiInDeepSleep();
  }

  // 4. Preparo il pacchetto
  PacketEnvironment p;

  p.nodeId = NODE_TX1;
  p.payloadType = TYPE_ENVIRONMENT;
  p.seq = packetCounter++;

  p.temp_x100 = (int16_t)round(temperatura * 100.0);

  if (sensorType == SENSOR_BME280) {
    p.hum_x100 = (int16_t)round(umidita * 100.0);
  } else {
    p.hum_x100 = -1;
  }

  p.press_x100 = (uint32_t)round(pressione_hPa * 100.0);
  p.sensorType = sensorType;

  Serial.println();
  Serial.print("SEQ: ");
  Serial.println(p.seq);

  Serial.print("Temperatura: ");
  Serial.print(temperatura, 2);
  Serial.println(" °C");

  if (sensorType == SENSOR_BME280) {
    Serial.print("Umidità: ");
    Serial.print(umidita, 2);
    Serial.println(" %");
  } else {
    Serial.println("Umidità: non disponibile su BMP280");
  }

  Serial.print("Pressione: ");
  Serial.print(pressione_hPa, 2);
  Serial.println(" hPa");

  // 5. Spengo subito il sensore prima della trasmissione
  spegniSensore();

  // 6. Inizializzo ESP-NOW
  if (!inizializzaEspNow()) {
    vaiInDeepSleep();
  }

  // 7. Trasmetto
  inviaPacchetto(p);

  delay(100);

  // 8. Torno in deep sleep
  vaiInDeepSleep();
}

// ======================================================
// LOOP
// ======================================================

void loop() {
  // Non usato.
  // Il dispositivo si sveglia, esegue setup(), trasmette e torna in deep sleep.
}