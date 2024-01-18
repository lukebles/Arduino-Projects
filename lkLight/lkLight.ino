//
// lkLight.ino
//
// The code is designed for a radio communication system using a microcontroller to
// receive commands and control a set of LEDs and a relay. Upon startup, the 
// system sets up various pins as inputs or outputs and reads a saved state from 
// the EEPROM to determine the initial state of the relay and the blue LED. During 
// the main loop, the system waits to receive a data structure via radio. If it 
// receives a structure intended for a specific recipient, it executes actions 
// based on the received command: turning on or off the relay and the blue LED, 
// reading the state from memory, or inverting the current state. After each 
// action, the new state is saved in the EEPROM and retransmitted. The program also
// includes commented-out code for reading the state of a button and altering the 
// relay's state accordingly.
// 
// 

const int transmit_en_pin = 3 ;
const int transmit_pin = 12;
const int receive_pin = 11;
const int pin_ptt_inverted = false;
const int ledGreen = 19;
const int ledRed = 18;
const int ledBlue = 17;
const int rele = 8;
const int pushbutton = 9;
const int speed = 2000;

#include <LkRadioStructure.h>
#include <EEPROM.h>

struct myStruct {
  uint8_t recipient;
  uint8_t command;
};

LkRadioStructure<myStruct> myStruct_o;

int status = LOW;

void setup() {

  // outputs
  pinMode(rele, OUTPUT);
  pinMode(transmit_en_pin, OUTPUT);
  pinMode(transmit_pin, OUTPUT);
  pinMode(ledGreen, OUTPUT);
  pinMode(ledRed, OUTPUT);
  pinMode(ledBlue, OUTPUT);
  pinMode(pushbutton, INPUT);
  // off all outputs
  //digitalWrite(rele,LOW);
  digitalWrite(ledGreen, LOW);
  digitalWrite(ledRed, LOW);
  digitalWrite(ledBlue, LOW);


  Serial.begin(115200);
  status = EEPROM.read(100);
  digitalWrite(rele, !status);
  digitalWrite(ledBlue, status);

  myStruct_o.globalSetup(speed, transmit_pin, receive_pin, transmit_en_pin, pin_ptt_inverted);

  delay(1000);
  //  Serial.println("START");
}

void loop() {
  if (myStruct_o.have_structure()) {
    myStruct v;
    v = myStruct_o.getStructure();
    Serial.println(v.recipient);
    Serial.println(v.command);

    if (v.recipient == 135) {
      if (v.command == 1) {
        // turn on
        status = HIGH;
        digitalWrite(rele, !status);
        digitalWrite(ledBlue, status);
        EEPROM.write(100, status);
        delay(2000);  // waits 2 secondi for receiver AGC
        myStruct_o.sendStructure(v);
      }
      if (v.command == 0) {
        // turn off
        status = LOW;
        digitalWrite(rele, !status);
        digitalWrite(ledBlue, status);
        EEPROM.write(100, status);
        delay(2000);  // waits 2 secondi for receiver AGC
        myStruct_o.sendStructure(v);
      }
      if (v.command == 2) {
        // get the status
        status = EEPROM.read(100);
        if (status == HIGH) {
          v.command = 1;
        } else {
          v.command = 0;
        }
        delay(2000);  // waits 2 secondi for receiver AGC
        myStruct_o.sendStructure(v);
      }
      if (v.command == 3) {
        // invert
        status = EEPROM.read(100);
        status = !status;
        digitalWrite(rele, !status);  // inversion
        digitalWrite(ledBlue, status);
        EEPROM.write(100, status);
        delay(2000);  // waits 2 secondi for receiver AGC
        myStruct_o.sendStructure(v);
      }
    }
  }

  //  if (digitalRead(pushbutton)==LOW){
  //    status = EEPROM.read(100);
  //    status = !status;
  //    digitalWrite(rele,!status);
  //    EEPROM.write(100, status);
  //    delay(1000);
  //  }
}
