#include <avr/wdt.h>
#include <avr/sleep.h>
#include <util/atomic.h>
#include <LkRadioStructure_RH.h>

const int TX_PIN = 12;
const int PTT_PIN = 13;
const bool PTT_INV = false;
const int SPEED = 2000;

#define ID_ENERGYSEND 11

struct __attribute__((packed)) EnergyData {
    byte sender;
    uint16_t activeCount;
    uint16_t reactiveCount;
};

LkRadioStructure<EnergyData> radioEnergy;

volatile boolean ext_int_0 = false;
volatile boolean ext_int_1 = false;
volatile boolean wdt_int   = false;

volatile uint16_t activePulses   = 65530;
volatile uint16_t reactivePulses = 64000;

// --- nuovi handler per interrupt esterni (niente ISR dirette) ---
void onInt0() {
  ext_int_0 = true;
  detachInterrupt(digitalPinToInterrupt(2)); // evita rimbalzi finché non torniamo a dormire
}
void onInt1() {
  ext_int_1 = true;
  detachInterrupt(digitalPinToInterrupt(3));
}

void setup() {
  for (int i = 0; i < 20; i++) pinMode(i, INPUT_PULLUP);

  radioEnergy.globalSetup(SPEED, TX_PIN, -1, PTT_PIN, PTT_INV); // solo TX

  // Watchdog ~8s
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    wdt_reset();
    MCUSR &= ~_BV(WDRF);
    WDTCSR |= _BV(WDCE) | _BV(WDE);
    WDTCSR = _BV(WDIE) | _BV(WDP3) | _BV(WDP0); // ~8s, solo interrupt
  }
}

void loop() {
  gotoSleep();

  // al risveglio
  if (ext_int_0) { ext_int_0 = false; activePulses++; }
  if (ext_int_1) { ext_int_1 = false; reactivePulses++; }

  if (wdt_int) {
    wdt_int = false;
    EnergyData energyMsg;
    energyMsg.sender        = ID_ENERGYSEND;
    energyMsg.activeCount   = activePulses;
    energyMsg.reactiveCount = reactivePulses;
    radioEnergy.sendStructure(energyMsg);
  }
}

void gotoSleep(void) {
  // (ri)attacca gli interrupt esterni prima di dormire
  attachInterrupt(digitalPinToInterrupt(2), onInt0, FALLING);
  attachInterrupt(digitalPinToInterrupt(3), onInt1, FALLING);

  // pulisci eventuali pending sugli INT0/INT1
  EIFR = bit(INTF0) | bit(INTF1);

  uint8_t adcsra = ADCSRA;   // salva stato ADC e spegnilo
  ADCSRA = 0;

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  noInterrupts();
  sleep_enable();
  sleep_bod_disable();       // risparmia ~20–25 µA
  interrupts();
  sleep_cpu();               // --- dorme qui ---

  sleep_disable();           // risveglio qui (da INT0/INT1 o WDT)
  ADCSRA = adcsra;           // ripristina ADC
}

// Watchdog interrupt
ISR(WDT_vect) {
  wdt_int = true;
}
