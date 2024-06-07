#include <LkRadioStructure.h>

// ==========================
// Pin
// ==========================

const int TX_PIN = 12;
const int PTT_PIN = 13;
const bool PTT_INV = false;
const int RADIO_BAUD = 2000;

// Identificatore del dispositivo
#define SENDER_ENERGY 1

struct __attribute__((packed)) EnergyData {
    byte sender; // 1 byte
    uint16_t activeCount; // 2 bytes
    uint16_t reactiveCount; // 2 bytes
};


LkRadioStructure<EnergyData> radioEnergy; // Radio configurazione per energia

// ==========================
// Variabili
// ==========================

uint16_t activePulses = 65530; // Conteggi iniziali di energia attiva
uint16_t reactivePulses = 64000; // Conteggi iniziali di energia reattiva

// ====================
// Setup
// ====================
void setup() {
  radioEnergy.globalSetup(RADIO_BAUD, TX_PIN, -1); // Solo trasmissione
}

// ====================
// Loop
// ====================
void loop() {
  // Simula da 0 a 12 conteggi per il contatore di energia attiva
  uint8_t simulatedActive = random(0, 13);
  activePulses += simulatedActive;

  // Simula da 0 a 3 conteggi per il contatore di energia reattiva
  uint8_t simulatedReactive = random(0, 4);
  reactivePulses += simulatedReactive;

  // Composizione del messaggio di energia
  EnergyData energyMsg;
  energyMsg.sender = SENDER_ENERGY;
  energyMsg.activeCount = activePulses;
  energyMsg.reactiveCount = reactivePulses;
  radioEnergy.sendStructure(energyMsg);

  delay(1000);

}
