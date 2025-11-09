/*
  ClimaNet ultra-low-power
  - Wake ogni ~8 s (LowPower.powerDown SLEEP_8S)
  - Misura + TX ogni 32 s (dopo 4 wake): ruota T -> P -> H -> ...
  - BME280 alimentato da D8: HIGH=ON, INPUT=OFF
*/

#include <LkRadioStructure_RH.h>
#include <LowPower.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define RADIO_SPEED 2025 // 2040 max ricevuto

// ---------- Pinout ----------
#define TRANSMIT_PIN   12
#define TX_POWER_PIN        13
#define BME_VCC_PIN     8   // <-- collega VCC del modulo BME280 qui

// ---------- BME ----------
Adafruit_BME280 bme;   // I2C (addr 0x76/0x77)

// ---------- Payload ----------
struct __attribute__((packed)) packet_temperatura { byte sender; float temperatura; };
struct __attribute__((packed)) packet_pressione   { byte sender; float pressione_hPa; };
struct __attribute__((packed)) packet_umidita     { byte sender; float umidita_perc; };

LkRadioStructure<packet_temperatura> radioT;
LkRadioStructure<packet_pressione>   radioP;
LkRadioStructure<packet_umidita>     radioH;

packet_temperatura dsT;
packet_pressione   dsP;
packet_umidita     dsH;

#define DEBUG 0
#if DEBUG
  #define prt(x)  Serial.print(x)
  #define prtn(x) Serial.println(x)
#else
  #define prt(x)
  #define prtn(x)
#endif

// ---- Sequenziamento: misura+TX ogni 4 wake (~32 s) ----
static uint8_t wakeSlots = 0;   // 0..3
static uint8_t phase     = 0;   // 0=T, 1=P, 2=H

// ---- Helpers radio ----
static inline void radioSendNow(void (*sendFn)()) {
  digitalWrite(TX_POWER_PIN, HIGH);      // alimenta TX
  delay(8);                         // stabilizzazione
  sendFn();
  digitalWrite(TX_POWER_PIN, LOW);       // spegni TX
}

static packet_temperatura lastT;
static packet_pressione   lastP;
static packet_umidita     lastH;

static void sendT_(){ radioT.sendStructure(lastT); }
static void sendP_(){ radioP.sendStructure(lastP); }
static void sendH_(){ radioH.sendStructure(lastH); }

// ---- I2C: rilascia il bus quando il BME è OFF per evitare backfeeding ----
static inline void i2cReleaseBus() {
#if defined(TWCR)
  TWCR = 0;               // disabilita TWI su AVR
#endif
  pinMode(SDA, INPUT);    // hi-Z
  pinMode(SCL, INPUT);    // hi-Z
}

// ---- Power control BME direttamente da GPIO ----
static inline void bmePowerOff() {
  pinMode(BME_VCC_PIN, INPUT);      // pin in alta impedenza = BME isolato
  i2cReleaseBus();
}

static inline bool bmePowerOnAndInit() {
  pinMode(BME_VCC_PIN, OUTPUT);
  digitalWrite(BME_VCC_PIN, HIGH);  // alimenta il modulo
  delay(10);                        // 5–20 ms a seconda del breakout/LDO

  Wire.begin();

  // Prova 0x76, poi 0x77
  if (!bme.begin(0x76, &Wire)) {
    if (!bme.begin(0x77, &Wire)) {
      prtn("BME280 non trovato dopo power-on");
      return false;
    }
  }

  // Misura singola veloce e a basso consumo
  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X1, // temp
                  Adafruit_BME280::SAMPLING_X1, // press
                  Adafruit_BME280::SAMPLING_X1, // hum
                  Adafruit_BME280::FILTER_OFF);
  return true;
}

// ---- Setup pin a basso consumo ----
static inline void preparePins() {
  for (uint8_t p = 0; p <= A5; p++) {
    if (p == TRANSMIT_PIN || p == TX_POWER_PIN || p == SDA || p == SCL || p == BME_VCC_PIN) continue;
    pinMode(p, INPUT_PULLUP);
  }
  pinMode(TX_POWER_PIN, OUTPUT);  digitalWrite(TX_POWER_PIN, LOW);  // TX sempre spento salvo invio
  bmePowerOff();                                       // BME spento all'avvio
}

void setup() {
#if DEBUG
  Serial.begin(115200); delay(200);
  prtn("\nClimaNet low-power: wake 8s, misura+TX ogni 32s (4 wake)");
#endif

  preparePins();

  // Radio solo TX (una init basta per tutte le strutture)
  // Radio TX: usa i pin di default interni di RH_ASK (nessun PTT gestito dalla libreria)
  LkRadioStructure<packet_temperatura>::globalSetup(RADIO_SPEED);

  //radioT.globalSetup(2000, TRANSMIT_PIN, -1);

  // Sender IDs
  dsT.sender = 106;
  dsH.sender = 107;
  dsP.sender = 108;

  wakeSlots = 0;
  phase = 0; // si parte con Temperatura
}

void loop() {
  // Conta i risvegli (~8 s ciascuno)
  wakeSlots = (wakeSlots + 1) & 0x03;   // mod 4 veloce (0..3)

  if (wakeSlots == 0) {
    // Ogni 4 wake (~32 s) esegui UNA misura e trasmetti
    if (bmePowerOnAndInit()) {
      if (phase == 0) {
        bme.takeForcedMeasurement();
        lastT = dsT;
        lastT.temperatura = bme.readTemperature();
        radioSendNow(sendT_);
        prt("TX T="); prtn(lastT.temperatura);
      } else if (phase == 1) {
        bme.takeForcedMeasurement();
        lastP = dsP;
        lastP.pressione_hPa = bme.readPressure() / 100.0f;
        radioSendNow(sendP_);
        prt("TX P="); prtn(lastP.pressione_hPa);
      } else { // phase == 2
        bme.takeForcedMeasurement();
        lastH = dsH;
        lastH.umidita_perc = bme.readHumidity();
        radioSendNow(sendH_);
        prt("TX H="); prtn(lastH.umidita_perc);
      }
    }
    bmePowerOff();                 // spegni sempre il BME prima di dormire
    phase = (phase + 1) % 3;       // ruota T -> P -> H -> ...
  }

  // Deep sleep ~8 s con consumi minimi (ADC OFF, BOD OFF)
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
}
