/*
 * This program receives weather information via radio signal.
 * The received data includes temperature, rain, wind, wind direction,
 * pressure, humidity, and dew point. These are displayed on a 4-character,
 * 7-segment display. The program also manages two servomotors to indicate
 * pressure and humidity. Note: negative values are not represented on the display.
 * 
 * Libraries used:
 * - SevSeg for controlling the 7-segment display.
 * - ServoTimer2 for controlling the servomotors.
 * - LkMultivibrator and LkRadioStructure for handling radio signal.
 * 
 * The program includes pin configuration, display and servomotor management,
 * reception and processing of weather data, and data conversion for visual representation.
 */

// bug: negative values are not represented

#include "SevSeg.h"
SevSeg sevseg; //Instantiate a seven segment controller object

#define MAX_NUMBER_STRINGS 14
#define MAX_STRING_SIZE 6
char testStrings[MAX_NUMBER_STRINGS][MAX_STRING_SIZE];
int testStringsPos = 1;

#include <ServoTimer2.h>  // the servo library

// define the pins for the servos
#define pin_pressure  2 // 2 16
#define pin_humidity 4 //4 17

#define ZERO_DEGREE 750
#define NINETY_DEGREE 1500
#define MAX_DEGREE 2250

ServoTimer2 servo_pressure;    // 170 degree max
ServoTimer2 servo_humidity;    // 110 degree max

const int pin_pushbutton = A5; 
const int tx_pin = 18; // unused
const int receive_pin = 11;
const int speed = 2000;


#include <LkMultivibrator.h> 
LkMultivibrator nextRow(300,ASTABLE);
LkMultivibrator noRadioSignal(900000,MONOSTABLE); // after 15 mins puts "----"
LkMultivibrator starting(1000,MONOSTABLE);

#include <LkRadioStructure.h>

struct myStructA{
  uint8_t identifier;
  uint8_t temperature;
  uint8_t rain;
  uint8_t wind;
  uint8_t windDirection;
  uint8_t pressure;
  uint8_t humidity;
  uint8_t dewPoint;
};

LkRadioStructure<myStructA> myStructA_o;

#define BAROMETRIC_DATA_B 0b10000100

void setup() {
  pinMode(pin_pushbutton,INPUT_PULLUP);
  
  byte numDigits = 4;
  byte digitPins[] = {13, 14, 15, 16}; // 
  byte segmentPins[] = {3, 5, 6, 7, 8, 9, 10, 12};
  bool resistorsOnSegments = false; // 'false' means resistors are on digit pins
  byte hardwareConfig = COMMON_CATHODE; // See README.md for options
  bool updateWithDelays = false; // Default. Recommended
  bool leadingZeros = false; // Use 'true' if you'd like to keep the leading zeros

  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments, updateWithDelays, leadingZeros);
  sevseg.setBrightness(1); // value 1-100:  refresh frequency

  resetDisplay();

  servo_pressure.attach(pin_pressure);     // attach a pin to the servos and they will start pulsing
  servo_humidity.attach(pin_humidity); 

  servo_pressure.write(NINETY_DEGREE);  
  servo_humidity.write(NINETY_DEGREE);

  myStructA_o.globalSetup(speed, tx_pin, receive_pin); // only one setup for both

  starting.start();
  sevseg.setNumber(8888,0);
  
}

void loop() {
  static boolean firstStart = true;
  sevseg.refreshDisplay(); // Must run repeatedly

  // after a few moments it starts working
  // and eliminates the 8888 initially
  // present on the display

  if(starting.expired()){
    firstStart = false;
    resetDisplay();
    sevseg.setChars(testStrings[testStringsPos]);
  }
  
  if (!firstStart){

    // EVERYTHING THAT COMES AFTER THAT
    // IS EXECUTED AFTER THE FIRST BOOT

    if(nextRow.expired()){
      if (digitalRead(pin_pushbutton)==LOW){
        testStringsPos++;
      }
    }
    
    if (testStringsPos >= MAX_NUMBER_STRINGS) testStringsPos = 0; 
      if (myStructA_o.have_structure()){
        
        // I received a radio signal appropriate to me
        // so I process it
      
        myStructA v;
        v = myStructA_o.getStructure();
        if (v.identifier == BAROMETRIC_DATA_B){
          
          // I reset the radio signal timer
          noRadioSignal.start();
          
          char temp[5];

          // ========== temperature

          // if the temperature is negative is 
          // sent a value > of 127
          // so it needs to be corrected appropriately
          int temperat = 0;
          if (v.temperature > 127){
            temperat = v.temperature - 256;
          } else {
            temperat = v.temperature;
          }

          // converts an integer value to an array      
          itoa(temperat,temp,10);

          // the last value is the terminator of string
          // that is, the character 0
          temp[4]=0;
          if (temperat >=0){
            if (temperat < 10){
              // 1 cifra
              temp[1]='*';
              temp[2]='C';
              temp[3]=' ';
            } else {
              // 2 digits (I obviously do not consider 3 digits!!!)
              temp[2]='*';
              temp[3]='C';
            }
          } else {
            if (temperat < -9){
              // 3 digits
              temp[3]='*';
            } else {
              // 2 digits
              temp[2]='*';
              temp[3]='C';
            }
          }
          
          // copies the array of characters into the string
          strcpy(testStrings[1], temp);
          
          // DEW POINT
          itoa(v.dewPoint,temp,10);
          temp[4]=0;
          strcpy(testStrings[3], temp);

          // RAIN
          itoa(v.rain,temp,10);
          temp[4]=0;
          strcpy(testStrings[5], temp);

          // WIND (SPEED)
          itoa(v.wind,temp,10);
          temp[4]=0;
          strcpy(testStrings[7], temp);

          // WIND (DIRECTION)
          direzionewind(v.windDirection);            

          // PRESSURE
          int pressure_value = v.pressure + 900; // 900 is added to the value (subtracted at the start to save space)        
          servo_pressure.write(convertValueToServoStep(pressure_value,ZERO_DEGREE,MAX_DEGREE,990,1025)); 
          
          itoa(pressure_value,temp,10);
          temp[4]=0;
          strcpy(testStrings[13], temp);

          // ========== humidity
          int humidity_value = v.humidity;          
          servo_humidity.write(convertValueToServoStep(humidity_value,ZERO_DEGREE,MAX_DEGREE,20,93));
          
          itoa(humidity_value,temp,10);
          temp[4]=0;
          strcpy(testStrings[11], temp);
        }
      } 

    // scrive il contenuto della stringa
    // sul display
    sevseg.setChars(testStrings[testStringsPos]);
    
    // se non viene ricevuto alcun segnale radio 
    // entro un determinato tempo azzera il display
    if(noRadioSignal.expired()){
      resetDisplay();
      noRadioSignal.start();
    }
  }
}

// ===================================
// converte il numero in una direzione
// ===================================
void direzionewind(uint8_t valore){
  switch (valore){
        case 0:
          strcpy(testStrings[9], "Nord");
          break;
        case 1:
          strcpy(testStrings[9], "NNE");
          break;
        case 2:
          strcpy(testStrings[9], "NEst");
          break;
        case 3:
          strcpy(testStrings[9], "ENE");
          break;
        case 4:
          strcpy(testStrings[9], "Est");
          break;
        case 5:
          strcpy(testStrings[9], "ESE");
          break;
        case 6:
          strcpy(testStrings[9], "SEst");
          break;
        case 7:
          strcpy(testStrings[9], "SSE");
          break;
        case 8:
          strcpy(testStrings[9], "Sud");
          break;
        case 9:
          strcpy(testStrings[9], "SSO");
          break;
        case 10:
          strcpy(testStrings[9], "SO");
          break;
        case 11:
          strcpy(testStrings[9], "OSO");
          break;
        case 12:
          strcpy(testStrings[9], "Oves");
          break;
        case 13:
          strcpy(testStrings[9], "ONO");
          break;
        case 14:
          strcpy(testStrings[9], "NO");
          break;
        case 15:
          strcpy(testStrings[9], "NNO");
          break;        
      }
}

// =========================================
// imposta le scritte di base e i trattini
// che indicano che non è stato ricevuto niente
// =========================================
void resetDisplay(){
  strcpy(testStrings[0], "temp"); // temperature
  strcpy(testStrings[1], "----");
  strcpy(testStrings[2], "rugi"); // temperature di rugiada
  strcpy(testStrings[3], "----");
  strcpy(testStrings[4], "piog"); // rain
  strcpy(testStrings[5], "----");
  strcpy(testStrings[6], "vent"); // wind
  strcpy(testStrings[7], "----");
  strcpy(testStrings[8], "dire"); // direzione del wind
  strcpy(testStrings[9], "----");
  strcpy(testStrings[10], "umid"); // direzione del wind
  strcpy(testStrings[11], "----");
  strcpy(testStrings[12], "pres"); // direzione del wind
  strcpy(testStrings[13], "----");  
}

int convertValueToServoStep(int value, int min_servo, int max_servo, int min_value, int max_value){
  int diffValue = max_value - min_value;
  int diffDuty = max_servo - min_servo;
  int stepSize = diffDuty/diffValue;
  int equivalent = (value - min_value)*stepSize + min_servo;
  return equivalent;
}
