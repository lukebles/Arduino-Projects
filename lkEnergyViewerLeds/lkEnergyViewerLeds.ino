/*
 * Arduino Program for Monitoring Electrical Consumption
 * 
 * This sketch uses a radio module to receive data related to electrical consumption. 
 * The data are transmitted by an ENEL counting unit and displayed on a 9 LED bar.
 * 
 * The system consists of:
 * - A radio receiver connected to the 'receive_pin'.
 * - A LED bar connected to pins from 5 to 13.
 * - A multivibrator to manage the indication of no radio signal.
 * 
 * Operation:
 * - Upon receiving radio data, the LED bar displays the level of electrical consumption.
 * - If the radio signal is not received for an extended period, the LEDs will flash to indicate no signal.
 * 
 * Pins Used:
 * - LED Pin: 13
 * - Transmit Pin: 12
 * - Receive Pin: 3
 * - Speed: 2000 bps
 */

const uint8_t ID_SENDER = 0b10000010;
const uint8_t led_pin = 13;
const uint8_t transmit_pin = 12;
const uint8_t receive_pin = 3;
const int speed = 2000;

#include <LkRadioStructure.h>

struct rxData6watt{
  uint8_t sender;
  uint8_t value; // value 1 - 9 ("0" turn off all the 9 leds)
};

LkRadioStructure<rxData6watt> rxData6watt_o;

#include <LKLedBar.h>          // 9 led bar
#include <LkMultivibrator.h>      // multivibrator

//
//              |  /| 9 leds: 4,6,7,8,9,10,11,12,13|-------------|
// |||--/\/\/\--|<  |-----------------------------<|             |
//              |  \|                              |             |
//                                                 |  ATMEGA328P |
//                                                 |             |
//      |--------------|                           |             |
// GND  |              |                           |             |
// |||--| radio module |                           |             |
//      |           RX |-----> receive_pin ------->|             |
//      |--------------|                           |             |
//                                                 |-------------|


bool toggleLed;
bool noRadioSignal = false;

LkMultivibrator maxWaitingTime(90000,MONOSTABLE); // 9 periods
LkMultivibrator ledFlashing(300,ASTABLE);
LKLedBar ledBar(5,6,7,8,9,10,11,12,13);

void setup() {
  rxData6watt_o.globalSetup(speed, transmit_pin, receive_pin);
  
  ledBar.setup();
  maxWaitingTime.start();
  Serial.begin(38400);

  ledBar.setBar(1,true); // the first led is always ON
}

void loop() {

  if (rxData6watt_o.have_structure()){
    rxData6watt retvalue;
    retvalue = rxData6watt_o.getStructure();
    // I make sure that I am getting the correct data
    // (the sender is the ENEL counting unit)
    if(retvalue.sender == ID_SENDER){
      // "watchdog"          
      noRadioSignal = false;
      maxWaitingTime.start();

      ledBar.setBar(0,true);
      ledBar.setBar(retvalue.value,true);
    }
  }

  /////////////////////////////////
  // no radio signal: led flashing
  /////////////////////////////////
  if (noRadioSignal){
    if (ledFlashing.expired()){
      if (toggleLed){
        ledBar.setBar(0b0000000000101010,false);
      } else {
        ledBar.setBar(0b0000000001010101,false);
      }
      toggleLed = ! toggleLed;
    }
  }

  //////////////////////////////////////////
  // maximum spent waiting for radio signal
  /////////////////////////////////////////
  if (maxWaitingTime.expired()){
    noRadioSignal = true;
  }
}
