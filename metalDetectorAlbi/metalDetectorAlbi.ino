const int pin_oscillator = 2; // pin with interrupt available 
const int pin_beeper = 15;
const int pin_led_red = 12;
const int pin_led_green = 13;

long frequency_counter = 0;

bool green_status_passed_by = false;

int nr_of_bubbles = 0;

int launching_status = 0;
long min_freq_measure = 999999;
long max_freq_measure = 0;
int min_max_frequency_loop_counter = 0;
long last_frequency_detected = 0;
bool have_new_frequency_detected = false;

#include <LkMultivibrator.h>

LkMultivibrator m(200,ASTABLE);

void setup() {
  //Serial.begin(38400);

  pinMode(pin_beeper,OUTPUT);
  digitalWrite(pin_beeper,LOW);
 
  pinMode(pin_led_red,OUTPUT);
  digitalWrite(pin_led_red,HIGH);
  
  delay(100);
  
  pinMode(pin_led_green,OUTPUT); 
  digitalWrite(pin_led_green,HIGH);
  
  delay(100);   
  
  digitalWrite(pin_led_red,LOW);
  digitalWrite(pin_led_green,LOW);

  pinMode(pin_oscillator,INPUT);
  attachInterrupt(digitalPinToInterrupt(pin_oscillator),interruptReceived , RISING);
}

void loop() {
  //
  if (m.expired()){
    astable_expired(); 
  }

  if (haveNewFrequency()){
    
    long the_frequency = getNewFrequency();

    if (launching_status == 0) {
      // start the first count
      launching_status = 1;
      blinking_green_led();
    } else {
      if (launching_status == 1) {
        // the first frequency detections I need 
        // to detect the maximum and minimum 
        // of the detected frequency to make 
        // the 'zero' of the system
        if (the_frequency < min_freq_measure){
          min_freq_measure = the_frequency;
        }
        if (the_frequency > max_freq_measure){
          max_freq_measure = the_frequency;
        }
        min_max_frequency_loop_counter++;
        if (min_max_frequency_loop_counter > 20){
          launching_status = 2;
          digitalWrite(pin_beeper,HIGH);
          delay(100); 
          digitalWrite(pin_beeper,LOW);
        }
        blinking_green_led();
      } else {
          // I already have the minimum and maximum frequency values
          if ((the_frequency >= max_freq_measure )||(the_frequency <= min_freq_measure)){
            // changed frequency significantly: need only report 
            // if this problem persists for more than once, 
            // because the presence of bubbles can distort 
            // the reading therefore I read the value 
            // for 10 consecutive times before giving the alarm
            nr_of_bubbles += 1;
            if (nr_of_bubbles > 10){
              nr_of_bubbles = 0;
              digitalWrite(pin_led_green,LOW);
              digitalWrite(pin_led_red,HIGH);
              beep();
            }
          } else {
            // no change in frequency
            digitalWrite(pin_led_red,LOW);
            digitalWrite(pin_led_green,HIGH);
            green_status_passed_by = true;
            // instantly resets any false change detection
            nr_of_bubbles = 0;
          }

      }
    } 
  }
}

void astable_expired(){
  last_frequency_detected = frequency_counter;
  frequency_counter = 0;
  have_new_frequency_detected = true;
}

bool haveNewFrequency(){
  return have_new_frequency_detected;
}

long getNewFrequency(){
  have_new_frequency_detected = false;
  return last_frequency_detected;
}

void interruptReceived(){
  frequency_counter +=1;
}

void blinking_green_led(){
  digitalWrite(pin_led_green,!digitalRead(pin_led_green));
}

void beep(){
  bool stato = false;
  if (green_status_passed_by){
    green_status_passed_by = false;
    for (int i = 0; i <= 11; i++) {
      stato = !stato;
      digitalWrite(pin_beeper,stato);
      delay(100);
    } 
  }
}
