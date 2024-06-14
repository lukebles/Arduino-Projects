// Part of the code (concerning the watchdog timer and external interrupt)
// is by Jack Christensen 19Nov2013 (https://forum.arduino.cc/index.php?topic=199576.0)
// CC BY-SA, see http://creativecommons.org/licenses/by-sa/3.0/
/*

Il circuito hardware descritto in questo programma consente la misurazione 
e la trasmissione via radio dei valori di energia attiva 
e reattiva leggendo i lampeggi delle due LED sul contatore elettrico.
Questo circuito è alimentato a batteria, prestando particolare attenzione 
a minimizzare il consumo energetico. 

Il circuito include i seguenti componenti:

2 fotoresistori
1 modulo radio TX a 433 MHz
1 ATMEGA328 con bootloader Arduino

Per maggiori dettagli sulla programmazione di un ATMEGA328P, vedi: 
Caricare sketch su un ATmega su una breadboard
https://www.arduino.cc/en/Tutorial/BuiltInExamples/ArduinoToBreadboard

=====================================================

The hardware circuit described in this program enables the measurement 
and radio transmission of the values of active 
and reactive by reading the flashes of the two LEDs on the electric meter.
This circuit is battery-powered, paying special attention 
to minimize power consumption. 

The circuit includes the following components:

2 photoresistors
1 433 MHz TX radio module
1 ATMEGA328 with Arduino bootloader.

For more details on programming an ATMEGA328P, see: 
Loading sketches to an ATmega on a breadboard
https://www.arduino.cc/en/Tutorial/BuiltInExamples/ArduinoToBreadboard
  
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

// 5/6/2024: piccole modifiche
// 25 Maggio 2024: miglioramento della leggibilità del codice e rimozione ridondanze
// 12 Apr 2024: lkENELmodule.ino rinominato in EnergiSend.ino 

// ==========================
// libraries
// ==========================
#include <avr/wdt.h>      // per gli interrupt (watchdog interno ed esterno)
#include <avr/sleep.h>    // per gli interrupt (watchdog interno ed esterno)
#include <util/atomic.h>  // per gli interrupt (watchdog interno ed esterno)

#include <LkRadioStructure.h>

const int TX_PIN = 12;
const int PTT_PIN = 13;
const bool PTT_INV = false;
const int SPEED = 2000;

#define ID_ENERGYSEND 1 // CAMBIARE A SECONDA DEL DISPOSITIVO

struct __attribute__((packed)) EnergyData {
    byte sender; // 1 byte
    uint16_t activeCount; // 2 bytes
    uint16_t reactiveCount; // 2 bytes
};

LkRadioStructure<EnergyData> radioEnergy;

volatile boolean ext_int_0 = false;             // flag per interrupt esterno
volatile boolean ext_int_1 = false;             // flag per interrupt esterno
volatile boolean wdt_int = false;               // flag per interrupt watchdog

volatile uint16_t activePulses = 65530;      // numero iniziale di impulsi per Potenza Attiva
volatile uint16_t reactivePulses = 64000;    // numero iniziale di impulsi per Potenza Reattiva

void setup() {
  for (int i = 0; i < 20; i++) {
    pinMode(i, INPUT_PULLUP);
  }

  radioEnergy.globalSetup(SPEED, TX_PIN, -1, PTT_PIN, PTT_INV); // solo trasmissione

  // Configurazione Watchdog e interrupt esterno
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    wdt_reset();
    MCUSR &= ~_BV(WDRF);                            // cancella il bit WDRF nel registro MCUSR
    WDTCSR |= _BV(WDCE) | _BV(WDE);                 // abilita il cambiamento di WDTCSR
    WDTCSR = _BV(WDIE) | _BV(WDP3) | _BV(WDP0);     // ~8 sec
  }
}

void loop(void) {
  gotoSleep();

  // Al risveglio
  // Controlla se il risveglio è dovuto a un interrupt esterno (LED lampeggia)

  if (ext_int_0) {
    ext_int_0 = false;
    activePulses++;
  }

  if (ext_int_1) {
    ext_int_1 = false;
    reactivePulses++;
  }

  // Se il risveglio è dovuto al watchdog, trasmette i valori
  if (wdt_int) {
    wdt_int = false;
    // Composizione del messaggio
    EnergyData energyMsg;
    energyMsg.sender = ID_ENERGYSEND;
    energyMsg.activeCount = activePulses;
    energyMsg.reactiveCount = reactivePulses;
    radioEnergy.sendStructure(energyMsg);
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
  ext_int_0 = true;
}

// Interrupt esterno 1 risveglia il MCU
ISR(INT1_vect) {
  EIMSK = 0;                     // disabilita interrupt esterni (serve solo uno per risvegliare)
  ext_int_1 = true;
}

// Gestisce il Watchdog Time-out Interrupt
ISR(WDT_vect) {
  wdt_int = true;
}

