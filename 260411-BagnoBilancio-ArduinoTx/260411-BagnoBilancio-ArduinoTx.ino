#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <LkRadioStructure_RH.h>

const int TX_PIN  = 12;
const int PTT_PIN = 13;
const bool PTT_INV = false;
const int SPEED = 2000;

// Reed su D4 = PD4 = PCINT20
#define REED_PIN 4

// =====================================================
// DEVICE ID fisso impostato a compile-time
// valori ammessi: 0..7
// =====================================================
#define DEVICE_ID 3 // bagno water id = 3

// Con INPUT_PULLUP:
// - contatto aperto => pin HIGH
// - contatto chiuso verso GND => pin LOW
#define DOOR_OPEN_LEVEL HIGH

struct __attribute__((packed)) DoorCmd {
  byte cmd;
};

LkRadioStructure<DoorCmd> radioDoor;

volatile bool reed_int = false;

// ultimo stato stabile noto/inviato
bool lastDoorState = false;
bool lastDoorStateValid = false;

// =====================================================
// Costruisce il byte comando:
// bit 7..5 = device ID (3 bit)
// bit 0    = stato porta (1 aperta, 0 chiusa)
// bit 4..1 = 0
// formato: xxx0000y
// =====================================================
byte makeDoorCommand(bool doorOpen) {
  return ((DEVICE_ID & 0x07) << 5) | (doorOpen ? 0x01 : 0x00);
}

// =====================================================
// Trasmette lo stato porta
// =====================================================
void sendDoorState(bool doorOpen) {
  DoorCmd msg;
  msg.cmd = makeDoorCommand(doorOpen);
  radioDoor.sendStructure(msg);
}

// =====================================================
// Aspetta finché trova 10 letture consecutive uguali
// distanziate di 100 ms
// =====================================================
bool readStableDoorState(bool &isValid) {
  bool lastState = (digitalRead(REED_PIN) == DOOR_OPEN_LEVEL);
  byte stableCount = 0;

  unsigned long t0 = millis();

  while (stableCount < 10) {
    delay(100);

    bool currentState = (digitalRead(REED_PIN) == DOOR_OPEN_LEVEL);

    if (currentState == lastState) {
      stableCount++;
    } else {
      stableCount = 0;
      lastState = currentState;
    }

    if (millis() - t0 >= 5000) {
      isValid = false;
      return lastState;
    }
  }

  isValid = true;
  return lastState;
}

// =====================================================
// Sleep profondo: si sveglia su Pin Change Interrupt
// =====================================================
void gotoSleep() {
  uint8_t oldADCSRA = ADCSRA;
  ADCSRA = 0;  // spegne ADC per ridurre il consumo

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  noInterrupts();

  reed_int = false;              // dimentica eventuali eventi vecchi
  PCIFR |= (1 << PCIF2);         // cancella flag interrupt pending su PORTD

  sleep_enable();
  sleep_bod_disable();

  interrupts();
  sleep_cpu();

  sleep_disable();
  ADCSRA = oldADCSRA;
}

void setup() {
  // Imposta tutti i pin come INPUT_PULLUP per evitare floating
  for (int i = 0; i < 20; i++) {
    pinMode(i, INPUT_PULLUP);
  }

  // Reed
  pinMode(REED_PIN, INPUT_PULLUP);

  // Radio TX
  pinMode(TX_PIN, OUTPUT);
  pinMode(PTT_PIN, OUTPUT);

  radioDoor.globalSetup(SPEED, TX_PIN, -1, PTT_PIN, PTT_INV);

  // Abilita Pin Change Interrupt su D4 = PD4 = PCINT20
  // gruppo PORTD => PCIE2
  PCICR  |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT20);

  // Stato iniziale
  bool isValid = false;
  bool initialState = readStableDoorState(isValid);
  if (isValid) {
    lastDoorState = initialState;
    lastDoorStateValid = true;
    sendDoorState(initialState);
  }
}

void loop() {
  gotoSleep();

  if (reed_int) {
    reed_int = false;

    bool isValid = false;
    bool currentState = readStableDoorState(isValid);

    if (!isValid) return;

    byte sendCount = 0;
    bool stateToSend = currentState;

    while (sendCount < 3) {
      // Trasmette lo stato attuale
      sendDoorState(stateToSend);
      sendCount++;

      // Piccola pausa tra una trasmissione e l'altra
      delay(50);

      // Rilegge lo stato stabile
      bool newValid = false;
      bool newState = readStableDoorState(newValid);

      if (newValid) {
        // Se lo stato è cambiato, riparte da zero
        if (newState != stateToSend) {
          stateToSend = newState;
          sendCount = 0;
        }
      }
    }

    lastDoorState = stateToSend;
    lastDoorStateValid = true;
  }
}

// =====================================================
// ISR Pin Change Interrupt per PORTD
// =====================================================
ISR(PCINT2_vect) {
  reed_int = true;
}