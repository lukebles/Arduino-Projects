#include <ESP32Servo.h>
#include <Arduino.h>
Servo servo;
const int SERVO_PIN = 18;
const int PREMI_POS = 170;
const int RIPOSO_POS = 40;

#include "LdrBlinkDetector.h"

LdrBlinkDetector::Config cfg; // valori di default: pin=34, TH_ON=1000, TH_OFF=900
LdrBlinkDetector ldr(cfg);

void onState(LdrBlinkDetector::LightState s, int raw){
  const char* name = (s==LdrBlinkDetector::OFF) ? "SPENTA" :
                     (s==LdrBlinkDetector::STEADY_ON)      ? "ACCESA FISSA" :
                                                             "LAMPEGGIANTE";
  Serial.print("[STATO] "); Serial.print(name);
  Serial.print("  raw=");   Serial.println(raw);
}

// Callback 2: riceve vecchio e nuovo stato (utile per transizioni)
void onLdrChange2(LdrBlinkDetector::LightState oldS,
                  LdrBlinkDetector::LightState newS,
                  int raw)
{
  if (oldS == LdrBlinkDetector::BLINKING && newS == LdrBlinkDetector::STEADY_ON) {
    Serial.printf("TRANSIZIONE: LAMPEGGIANTE → FISSA (raw=%d)\n", raw);
  }
  if (oldS == LdrBlinkDetector::BLINKING && newS == LdrBlinkDetector::OFF) {
    Serial.printf("TRANSIZIONE: LAMPEGGIANTE → SPENTA (raw=%d)\n", raw);
  }
}


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
  ldr.setOnChange(onState); // callback (opzionale)
  ldr.setOnChange2(onLdrChange2);
  ldr.begin();
  Serial.println("LDR pronta (GPIO34). Soglie: ON>=1000, OFF<=900. Flicker 1–4 Hz.");
}

void loop(){
  ldr.update(); // chiamare spesso (non blocca)

  static uint32_t t = 0;
  if (millis() - t >= 500) {
    t = millis();
    Serial.printf("raw=%d  stato=%s  (code=%d, on=%d, blink=%d)\n",
                  ldr.raw(),
                  ldr.stateName(),
                  ldr.stateCode(),
                  ldr.isOn(),
                  ldr.isBlinking());
  }
}

void accendi(){
  for(int a=RIPOSO_POS;a<=PREMI_POS;a+=2){ servo.write(a); delay(10); }
  sleep(1000);
  for(int a=PREMI_POS;a>=RIPOSO_POS;a-=2){ servo.write(a); delay(10); }  
}

void spegni(){
  for(int a=RIPOSO_POS;a<=PREMI_POS;a+=2){ servo.write(a); delay(10); }
  sleep(6000);
  for(int a=PREMI_POS;a>=RIPOSO_POS;a-=2){ servo.write(a); delay(10); }  
}