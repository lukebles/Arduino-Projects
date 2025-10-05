#include <ESP32Servo.h>
#include <Arduino.h>
Servo servo;
const int SERVO_PIN = 18;
const int PREMI_POS = 170;
const int RIPOSO_POS = 40;

const int LDR_PIN = 34;  // ADC1, ok con Wi-Fi
const int ADC_BITS = 12;             // 0..4095
const adc_attenuation_t ATT = ADC_11db; // ~0..3.3V
// ===== SOGLIE =====
const int TH_ON  = 1000;  // luce considerata ACCESA quando supera 1000
const int TH_OFF = 900;   // isteresi: torna SPENTA sotto 900

void setup(){
  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2400); // min/max µs
  servo.write(RIPOSO_POS);


  /*
  più luce ⇒ valore più alto
  3.3V ── LDR ──●── Rfisso (27 kΩ) ── GND
                │
               GPIO34
  */
  Serial.begin(115200);
  pinMode(LDR_PIN, INPUT);
  analogReadResolution(12);                    // 0..4095
  analogSetPinAttenuation(LDR_PIN, ADC_11db);  // ~0..3.3V
}

void loop(){
  int raw = analogRead(LDR_PIN);  // valore grezzo
  Serial.println(raw);
  delay(200);
}

void accendi(){
  for(int a=RIPOSO_POS;a<=PREMI_POS;a+=2){ servo.write(a); delay(10); }
  sleep(1000);
  for(int a=PREMI_POS;a>=RIPOSO_POS;a-=2){ servo.write(a); delay(10); }  
}

void spegni(){
  for(int a=RIPOSO_POS;a<=PREMI_POS;a+=2){ servo.write(a); delay(10); }
  sleep(5000);
  for(int a=PREMI_POS;a>=RIPOSO_POS;a-=2){ servo.write(a); delay(10); }  
}