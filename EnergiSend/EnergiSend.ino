// Part of the code (concerning the watchdog timer and external interrupt)
// is by Jack Christensen 19Nov2013 (https://forum.arduino.cc/index.php?topic=199576.0)
// CC BY-SA, see http://creativecommons.org/licenses/by-sa/3.0/
/*

  The hardware circuit of this program enables the measurement of active and 
  reactive power by reading the flashes of the two LEDs on the energy meter.

  This circuit is battery-powered, paying particular attention to power 
  consumption. Each pin of the ATMEGA328 is configured as an output, except for 
  the three required inputs. The circuit is in a constant 'sleep' state, activated
  with each ENEL meter flash. In addition, the built-in watchdog wakes up the 
  microcontroller every 8 seconds, allowing periodic sending of meter data via the
  433 MHz radio module.

  The circuit consists of the following:

  2 photoresistors
  2 33K/56K resistors
  1 433 MHz TX radio module
  1 ATMEGA328 with Arduino bootloader.

  The microcontroller is programmed by connecting it to an Arduino board (after 
  removing the microcontroller from the board).

  see
  https://www.arduino.cc/en/Tutorial/BuiltInExamples/ArduinoToBreadboard
  Uploading sketches to an ATmega on a breadboard.




  12 Apr 2024 lkENELmodule.ino rinominato in EnergiSend.ino 

*/

//
//                                                 |-------------|
// |||--/\/\/\----------------> pin 2 IDE (INT0) ->|             |
// GND  VDR A                                      |             |
//                                                 |  ATMEGA328P |
// GND  VDR B                                      |  w/         |
// |||--/\/\/\----------------> pin 3 IDE (INT1) ->|  Arduino    |
//                                                 |  boot       |
//                                                 |  loader     |
//      |--------------|                           |             |
// GND  |           VCC|----------< pinPTT -------<|             |
// |||--| radio module |                           |             |
//      |           TX |----------< pinTx --------<|             |
//      |--------------|                           |             |
//                                                 |-------------|


// 25 Maggio 2024
// miglioramento della leggibilità del codice e rimozione ridindanze

// ==========================
// libraries
// ==========================
#include <avr/wdt.h>      // per gli interrupt (watchdog interno ed esterno)
#include <avr/sleep.h>    // per gli interrupt (watchdog interno ed esterno)
#include <util/atomic.h>  // per gli interrupt (watchdog interno ed esterno)

#include <LkRadioStructure.h>

// ==========================
// Pin
// ==========================

const int transmit_pin = 12;
const int ptt_pin = 13;
const bool ptt_inverted = false;
const int speed = 2000;

// Sender e destinatario
#define MYSELF 1

struct txData1 {
  uint8_t sender;
  uint16_t countActiveWh;
  uint16_t countReactiveWh;
};

LkRadioStructure<txData1> txData1_o;

// ==========================
// Variabili 'volatile'
// ==========================

volatile boolean interrupt_0 = false;             // flag per interrupt esterno
volatile boolean interrupt_1 = false;             // flag per interrupt esterno
volatile boolean interrupt_watchdog = false;      // flag per interrupt watchdog
volatile uint16_t pulsesPowerActive = 65530;      // numero iniziale di impulsi per Potenza Attiva
volatile uint16_t pulsesPowerReactive = 64000;    // numero iniziale di impulsi per Potenza Reattiva

// ====================
// Setup
// ====================
void setup() {
  // Imposta tutti i pin come INPUT_PULLUP
  for (int i = 0; i < 20; i++) {
    pinMode(i, INPUT_PULLUP);
  }

  txData1_o.globalSetup(speed, transmit_pin, -1, ptt_pin, ptt_inverted); // solo trasmissione

  // Configurazione Watchdog e interrupt esterno
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    wdt_reset();
    MCUSR &= ~_BV(WDRF);                            // cancella il bit WDRF nel registro MCUSR
    WDTCSR |= _BV(WDCE) | _BV(WDE);                 // abilita il cambiamento di WDTCSR
    WDTCSR = _BV(WDIE) | _BV(WDP3) | _BV(WDP0);     // ~8 sec
  }
}

// ====================
// Loop
// ====================
void loop(void) {
  // Mette ATMEGA328P in sleep
  gotoSleep();

  // Al risveglio
  // Controlla se il risveglio è dovuto a un interrupt esterno (LED lampeggia)

  if (interrupt_0) {
    interrupt_0 = false;
    pulsesPowerActive++;
  }

  if (interrupt_1) {
    interrupt_1 = false;
    pulsesPowerReactive++;
  }

  // Se il risveglio è dovuto al watchdog, trasmette i valori
  if (interrupt_watchdog) {
    interrupt_watchdog = false;
    // Composizione del messaggio
    txData1 sendvalue;
    sendvalue.sender = MYSELF;
    sendvalue.countActiveWh = pulsesPowerActive;
    sendvalue.countReactiveWh = pulsesPowerReactive;

    txData1_o.sendStructure(sendvalue);
  }
}

void gotoSleep(void) {
  uint8_t adcsra = ADCSRA;       // salva il registro ADC Control and Status Register A
  ADCSRA = 0;                    // disabilita l'ADC

  EICRA = B00001010;             // configura INT0/INT1 per attivarsi sul fronte di discesa
  EIFR =  B00000011;             // assicura che il flag di interrupt sia pulito
  EIMSK = B00000011;             // abilita INT0/INT1

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  ATOMIC_BLOCK(ATOMIC_FORCEON) {
    sleep_enable();
    sleep_bod_disable();         // disabilita il brown-out detection (risparmia 20-25µA)
  }
  sleep_cpu();                   // mette il processore in sleep
  sleep_disable();               // risveglio qui
  ADCSRA = adcsra;               // ripristina ADCSRA
}

// Interrupt esterno 0 risveglia il MCU
ISR(INT0_vect) {
  EIMSK = 0;                     // disabilita interrupt esterni (serve solo uno per risvegliare)
  interrupt_0 = true;
}

// Interrupt esterno 1 risveglia il MCU
ISR(INT1_vect) {
  EIMSK = 0;                     // disabilita interrupt esterni (serve solo uno per risvegliare)
  interrupt_1 = true;
}

// Gestisce il Watchdog Time-out Interrupt
ISR(WDT_vect) {
  interrupt_watchdog = true;
}

