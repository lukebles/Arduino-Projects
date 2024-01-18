#include <LkBlinker.h>
LkBlinker blinker(13,false,10);

void setup() {
  // put your setup code here, to run once:
  blinker.enable(); 
}

void loop() {
  // put your main code here, to run repeatedly:
  blinker.loop();
}