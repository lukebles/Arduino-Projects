/*
This class allows to visualize a number on a led bar of 9 elements
This visualization is suitable to display either a number from 0 to 512 in binary format
through:

setBar(0...512, false) 

or to indicate a value from 0 to 8 through the led-bar which lengthens or shortens 
depending on the value entered

setBar(0...8, true)
*/

class LKLedBar {
  uint8_t p8;
  uint8_t p7;
  uint8_t p6;
  uint8_t p5;
  uint8_t p4;
  uint8_t p3;
  uint8_t p2;
  uint8_t p1;
  uint8_t p0;
  uint8_t ledPins[9];

private:

  void setBar(int barValue){
    int mask = 1;
    bool highlow;
    for (int i = 0; i < 9; ++i){ 
      int result = barValue & mask;
      if (result > 0){
        highlow = HIGH;
      } else {
        highlow = LOW;
      }
      digitalWrite(ledPins[i],highlow);

      mask = mask << 1;
    }
  }

  int binToBar(byte value){
    switch (value) {
      case 0:
      return 0b0000000000000000;
      break;
      case 1:
      return 0b0000000000000001;
      break;
      case 2:
      return 0b0000000000000011;
      break;
      case 3:
      return 0b0000000000000111;
      break;
      case 4:
      return 0b0000000000001111;
      break;
      case 5:
      return 0b0000000000011111;
      break;
      case 6:
      return 0b0000000000111111;
      break;
      case 7:
      return 0b0000000001111111;
      break;
      case 8:
      return 0b0000000011111111;
      break;
      case 9:
      return 0b0000000111111111;
      break;
      case 10:
      return 0b0000001111111111;
      break;
      case 11:
      return 0b0000011111111111;
      break;
      case 12:
      return 0b0000111111111111;
      break;
      case 13:
      return 0b0001111111111111;
      break;
      case 14:
      return 0b0010000000000000;
      break;
      case 15:
      return 0b0100000000000000;
      break;
      default:
      break;
    }
  }

public:
  //
  // the Arduino IDE pins for the 9 leds must be inserted
  //
  LKLedBar(uint8_t led0idePin, uint8_t led1idePin, uint8_t led2idePin,
           uint8_t led3idePin, uint8_t led4idePin, uint8_t led5idePin, 
           uint8_t led6idePin, uint8_t led7idePin, uint8_t led8idePin) :
    p0(led0idePin),
    p1(led1idePin),
    p2(led2idePin),
    p3(led3idePin),
    p4(led4idePin),
    p5(led5idePin),
    p6(led6idePin),
    p7(led7idePin),
    p8(led8idePin)
  {
  }

  void setup() {  
    ledPins[8]=p8;
    ledPins[7]=p7;
    ledPins[6]=p6;
    ledPins[5]=p5;
    ledPins[4]=p4;
    ledPins[3]=p3;
    ledPins[2]=p2;
    ledPins[1]=p1;
    ledPins[0]=p0;
    for (int d = 0; d < 9; ++d){ 
      pinMode(ledPins[d], OUTPUT);
    }
  }

  void loop() {
    // ununsed
  }

  
  // ===============
  // setBar() method
  // ===============
  void setBar(int barValue, bool decimal){
    if (decimal){
      setBar(binToBar(barValue));
    } else {
      setBar(barValue);
    }
  }
    
};
