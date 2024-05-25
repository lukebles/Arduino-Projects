#include <LkRadioStructure.h>

// ==========================
// Pin
// ==========================

const int TRANSMIT_PIN = 12;
const int PTT_PIN = 13;
const bool PTT_INVERTED = false;
const int RADIO_SPEED = 2000;

// Identificatore del dispositivo
#define DEVICE_ID 1

struct EnergyData {
  uint8_t senderID;
  uint16_t activeEnergyCount;
  uint16_t reactiveEnergyCount;
};

LkRadioStructure<EnergyData> radioModule;

// ==========================
// Variabili
// ==========================

uint16_t activeEnergyPulses = 65530;     // Conteggi iniziali di energia attiva
uint16_t reactiveEnergyPulses = 64000;   // Conteggi iniziali di energia reattiva

// ====================
// Setup
// ====================
void setup() {
  // Imposta tutti i pin come INPUT_PULLUP
  for (int i = 0; i < 20; i++) {
    pinMode(i, INPUT_PULLUP);
  }

  radioModule.globalSetup(RADIO_SPEED, TRANSMIT_PIN, -1, PTT_PIN, PTT_INVERTED); // Solo trasmissione

  // Seed per il generatore di numeri casuali
  randomSeed(analogRead(0));
}

// ====================
// Loop
// ====================
void loop(void) {
  // Simula da 0 a 12 conteggi per il contatore di energia attiva
  uint8_t simulatedActiveCounts = random(0, 13);
  activeEnergyPulses += simulatedActiveCounts;

  // Simula da 0 a 3 conteggi per il contatore di energia reattiva
  uint8_t simulatedReactiveCounts = random(0, 4);
  reactiveEnergyPulses += simulatedReactiveCounts;

  // Composizione del messaggio
  EnergyData energyDataToSend;
  energyDataToSend.senderID = DEVICE_ID;
  energyDataToSend.activeEnergyCount = activeEnergyPulses;
  energyDataToSend.reactiveEnergyCount = reactiveEnergyPulses;

  radioModule.sendStructure(energyDataToSend);

  // Attende 8 secondi
  delay(8000);
}
