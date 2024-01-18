/*
                 |-----------------------
 4 5 6 7 8 13    |
 ================|  MODULO LCM
                 |
                 |------------------------
2     -----
---°     °-----> switch 1
            
3     -----
---°     °-----> switch 2

1
---|<|----------< led LCM backlight

9
---|<|----------< led 1

13
---|<|----------< led 2

A0   ----/----
-----|  /    |-----> trimmer 
     --/------

12   |------------
-----| modulo TX |-------<
     |------------

11   |------------
-----| modulo RX |-------<
     |------------
     

 * LCD RS pin to digital pin 13
 * LCD Enable pin to digital pin 8
 * LCD D4 pin to digital pin 7
 * LCD D5 pin to digital pin 6
 * LCD D6 pin to digital pin 5
 * LCD D7 pin to digital pin 4
 * LCD R/W pin to ground
 * LCD VSS pin to ground
 * LCD VCC pin to 5V
 * 10K resistor:
 * ends to +5V and ground
 * wiper to LCD VO pin (pin 3)

*/
const int pin_led1 = 13;
const int pin_led2 = 9;
const int pin_backlight = 1;
const int pin_pushb1 = 2;
const int pin_pushb2 = 3;
const int pin_trimmer = A0;
const int pin_rx = 11;
const int pin_tx = 12;

// include the library code:
#include <LiquidCrystal.h>

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 8, en = 10, d4 = 7, d5 = 6, d6 = 5, d7 = 4;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

void setup() {
  // set up the LCD's number of columns and rows:
  pinMode(pin_led1, OUTPUT);
  pinMode(pin_led2, OUTPUT);
  pinMode(pin_backlight, OUTPUT);
  pinMode(pin_tx, OUTPUT);
  pinMode(pin_rx,INPUT);
  digitalWrite(pin_led1,LOW);
  digitalWrite(pin_led2,LOW);
  digitalWrite(pin_tx,LOW);
  Serial.begin(115200);
  lcd.begin(20, 4);
  // Print a message to the LCD.
  lcd.print("hello, world!");
}

void loop() {
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  lcd.setCursor(0, 1);
  // print the number of seconds since reset:
  lcd.print(millis() / 1000);
   lcd.setCursor(1, 2);
  // print the number of seconds since reset:
  lcd.print("due");
   lcd.setCursor(0, 3);
  // print the number of seconds since reset:
  lcd.print("tre");
  digitalWrite(pin_led1,(digitalRead(pin_rx)));
}
