//
// Send the contents of two counters via VirtualWire 
// (embedded in LkRadioStructure) simulating LkENELmodule
//

const uint8_t transmit_pin = 12;
const int speed = 2000;

#include <LkRadioStructure.h>
struct txData1{
  uint8_t sender; // value 0x01 (sender)
  uint16_t countActiveWh;
  uint16_t countReactiveWh;
};

LkRadioStructure<txData1> txData1_o;

#include <LkMultivibrator.h>
LkMultivibrator every8secs(random(1000,1100),ASTABLE);
uint16_t pulsesPowerActive = 65530;
uint16_t pulsesPowerReactive = 12345;

void setup() {
  // radio setup just one for all instances of LkRadioStructure
  txData1_o.globalSetup(speed, transmit_pin, -1);
  Serial.begin(115200);
  delay(500);
  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,LOW);
}

void loop() {
  // ogni 8 secondi
  if(every8secs.expired()){

    if(random(0,10)>4){
      // a volte manda a volte no
      // simulando una cattiva ricezione del segnale
      txData1 sendvalue;
      sendvalue.sender = 1;

      
      sendvalue.countActiveWh = pulsesPowerActive;
      sendvalue.countReactiveWh = pulsesPowerReactive;

      digitalWrite(LED_BUILTIN,HIGH);
      Serial.println(pulsesPowerActive);
      Serial.println(pulsesPowerReactive);
      Serial.println("");
      txData1_o.sendStructure(sendvalue);
      digitalWrite(LED_BUILTIN,LOW);
  
//    } else {
//      txData6 sendvalue;
//      sendvalue.mittente = 6;
//      //sendvalue.extra1 = 13;
//      //sendvalue.extra2 = 33;
//      sendvalue.timestamp = 1650656811;
//  
//      Serial.println(sendvalue.timestamp);
//      txData6_o.sendStructure(sendvalue);
//      
   }


      pulsesPowerReactive += random(0,4);
      pulsesPowerActive += random(0,10); // 

    // simulator

    //pulsesPowerActive=10001;
    //pulsesPowerReactive=31001;
    
    
    // if (pulsesPowerReactive < 0){
    //   pulsesPowerReactive = random(0,4);
    // }
    // if (pulsesPowerActive < 0){
    //   pulsesPowerActive = random(0,8);
    // }

  }
}
  int getNewValue(){
    static int retvalue = 1;
    static bool flag = false;

    if (flag) {
      // retvalue is big
      retvalue -= 1;
      if (retvalue < 2){
        flag = false;
      } else {
        flag = true;
      }
    } else {
      // retvalue is small
      retvalue += 1;
      if (retvalue > 7){
        flag = true;
      } else {
        flag = false;
      }
    }
    return retvalue;
  }
