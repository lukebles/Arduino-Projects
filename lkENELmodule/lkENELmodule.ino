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

// ==========================
// libraries
// ==========================

#include <avr/wdt.h>      // for interrupts (internal watchdog and external)
#include <avr/sleep.h>    // for interrupts (internal watchdog and external)
#include <util/atomic.h>  // for interrupts (internal watchdog and external)

#include <LkRadioStructure.h>

// ==========================
// pins
// ==========================

const int transmit_pin  = 12;
const int receive_pin = 99; // inutilizzato
const int ptt_pin       = 13;
const int ptt_inverted = false;
const int speed = 2000;

// sender e destinatario
#define MYSELF 1     // 

struct txData1{
  uint8_t sender;
  uint16_t countActiveWh;
  uint16_t countReactiveWh;
};

LkRadioStructure<txData1> txData1_o;

// ==========================
// 'volatile' variables
// ==========================

volatile boolean interrupt_0 = false;             // external interrupt flag
volatile boolean interrupt_1 = false;             // external interrupt flag
volatile boolean interrupt_watchdog = false;      // watchdog timer interrupt flag
volatile uint16_t pulsesPowerActive = 65530;      // initial number of pulses for Active Power
volatile uint16_t pulsesPowerReactive = 64000;    // initial number of pulses for Reactive Power

// ====================
// setup
// ====================
void setup() {
  // all pins resetted to OUTPUT with LOW state
  for (int i = 0; i < 20; i++){
    pinMode(i, INPUT_PULLUP);
  }

  txData1_o.globalSetup(speed, transmit_pin, receive_pin, ptt_pin, ptt_inverted);

  // Watchdog & external interrupt
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    wdt_reset();
    MCUSR &= ~_BV(WDRF);                            //clear WDRF bit in register MCUSR
    WDTCSR |= _BV(WDCE) | _BV(WDE);                 //enable WDTCSR change
    WDTCSR =  _BV(WDIE) | _BV(WDP3) | _BV(WDP0);    //~8 sec
  }
  
}

// ====================
// loop
// ====================
void loop(void) {

  // sleeps the ATMEGA328P

  gotoSleep();

  // When it wakes up
  // Check if it was woken up
  // by the external interrupt (LED flashes)

  if (interrupt_0) {
    interrupt_0 = false;
    pulsesPowerActive++;
  }

  if (interrupt_1) {
    interrupt_1 = false;
    pulsesPowerReactive++;
  }

  // If it was woken up by the watchdog, it transmits the values
  if (interrupt_watchdog) {
    interrupt_watchdog = false;
    // message composition
    txData1 sendvalue;
    sendvalue.sender = MYSELF;
    sendvalue.countActiveWh = pulsesPowerActive;
    sendvalue.countReactiveWh = pulsesPowerReactive;

    txData1_o.sendStructure(sendvalue);
    
  }
}

void gotoSleep(void) {
  byte adcsra = ADCSRA;          // save the ADC Control and Status Register A
  ADCSRA = 0;                    // disable the ADC

  EICRA = B00001010;             // configure INT0/INT1 to trigger on falling edge
  EIFR =  B00000011;             // ensure interrupt flag cleared
  EIMSK = B00000011;             // enable INT0/INT1

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  ATOMIC_BLOCK(ATOMIC_FORCEON) {
    sleep_enable();
    sleep_bod_disable();         // disable brown-out detection (saves 20-25µA)
  }
  sleep_cpu();                   // go to sleep
  sleep_disable();               // wake up here
  ADCSRA = adcsra;               // restore ADCSRA
}

//external interrupt 0 wakes the MCU
ISR(INT0_vect) {
  EIMSK = 0;                     //disable external interrupts (only need one to wake up)
  interrupt_0 = true;
}

//external interrupt 1 wakes the MCU
ISR(INT1_vect) {
  EIMSK = 0;                     //disable external interrupts (only need one to wake up)
  interrupt_1 = true;
}

//handles the Watchdog Time-out Interrupt
ISR(WDT_vect) {
  interrupt_watchdog = true;
}
