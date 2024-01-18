// Part of the code (concerning the watchdog timer and external interrupt)
// is by Jack Christensen 19Nov2013 (https://forum.arduino.cc/index.php?topic=199576.0)
// CC BY-SA, see http://creativecommons.org/licenses/by-sa/3.0/
/*
  ================================================
  Every about 8 seconds, it reads a light value and a temperature value and transmits them via radio
	================================================

  This circuit is battery-powered, so it cares attention to consumption.
  Each ATMEGA328 pin is an output except for the three necessary inputs.
  The circuit is always in the sleep state
  
  Moreover, the built-in watchdog wakes up the microcontroller
  every 8 seconds allowing a periodic sending of the counters through the radio
  module at 433 MHz

*/

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
#define sensorLight     A0
#define sensorTemp      A1
const int ptt_pin       = 13;
const int ptt_inverted = false;
const int speed = 2000;

// sender and recipient
#define MYSELF 4     // 

struct txData4{
  uint8_t sender;
  int light;
  int temperature;
};

LkRadioStructure<txData4> txData4_o;

// ==========================
// 'volatile' variables
// ==========================

volatile boolean interrupt_0 = false;             // external interrupt flag
volatile boolean interrupt_1 = false;             // external interrupt flag
volatile boolean interrupt_watchdog = false;      // watchdog timer interrupt flag

// ====================
// setup
// ====================
void setup() {
  // all pins resetted to OUTPUT with LOW state
  for (int i = 0; i < 20; i++){
    pinMode(i, INPUT_PULLUP);
  }

  txData4_o.globalSetup(speed, transmit_pin, receive_pin, ptt_pin, ptt_inverted);

  // Watchdog & external interrupt
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    wdt_reset();
    MCUSR &= ~_BV(WDRF);                            //clear WDRF
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
  }

  if (interrupt_1) {
    interrupt_1 = false;
  }

  // If it was woken up by the watchdog, it transmits the values
  if (interrupt_watchdog) {
    interrupt_watchdog = false;
    // message composition
    txData4 sendvalue;
    sendvalue.sender = MYSELF;

    analogReference( DEFAULT );
    int light = analogRead(sensorLight);
    analogReference( INTERNAL );
    int temperature = analogRead(sensorTemp);
      
    sendvalue.light = light;
    sendvalue.temperature = temperature;

    txData4_o.sendStructure(sendvalue);
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
