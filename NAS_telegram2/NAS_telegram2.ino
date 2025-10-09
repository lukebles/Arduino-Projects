#include <Arduino.h>
#include <ESP32Servo.h>
#include "LdrBlinkDetector.h"

// -------- SERVO --------
Servo servo;
const int SERVO_PIN  = 18;
const int PREMI_POS  = 156;  // premi
const int RIPOSO_POS = 40;   // rilascia

// -------- LDR DETECTOR --------
LdrBlinkDetector ldr;  // pin=34, soglie 1000/900, flicker 1–4 Hz di default

// Azione in corso (guidata da comando)
enum PendingAction : uint8_t { PA_NONE, PA_WAIT_POWER_ON, PA_WAIT_POWER_OFF };
PendingAction pending = PA_NONE;
bool announcedTransition = false;  // per stampare la transizione una sola volta

// -------- MOSSE SERVO --------
static void accendi() {
  for (int a = RIPOSO_POS; a <= PREMI_POS; a += 2) {
    servo.write(a);
    delay(10);
  }

  delay(1000);  // tieni premuto 1s

  for (int a = PREMI_POS; a >= RIPOSO_POS; a -= 2) {
    servo.write(a);
    delay(10);
  }
}

static void spegni() {
  for (int a = RIPOSO_POS; a <= PREMI_POS; a += 2) {
    servo.write(a);
    delay(10);
  }

  delay(6000);  // tieni premuto 6s (se serve press lunga)

  for (int a = PREMI_POS; a >= RIPOSO_POS; a -= 2) {
    servo.write(a);
    delay(10);
  }
}

// -------- CALLBACK STATO --------
static void onState(LdrBlinkDetector::LightState s, int /*raw*/) {
  // Emesso solo quando lo stato cambia
  const char* name =
    (s == LdrBlinkDetector::OFF)       ? "SPENTA" :
    (s == LdrBlinkDetector::STEADY_ON) ? "ACCESA FISSA" :
                                         "LAMPEGGIANTE";

  Serial.printf("[STATO] %s\n", name);
}

static void onLdrChange2(LdrBlinkDetector::LightState /*oldS*/,
                         LdrBlinkDetector::LightState newS,
                         int /*raw*/) {
  // Transizione stampata SOLO se richiesta da un comando in corso
  if (newS == LdrBlinkDetector::BLINKING && !announcedTransition && pending != PA_NONE) {
    if (pending == PA_WAIT_POWER_ON)
      Serial.println("[AZIONE] transizione verso accensione");
    if (pending == PA_WAIT_POWER_OFF)
      Serial.println("[AZIONE] transizione verso spegnimento");
    announcedTransition = true;
  }

  // Conclusione azioni pendenti
  if (pending == PA_WAIT_POWER_ON && newS == LdrBlinkDetector::STEADY_ON) {
    Serial.println("[AZIONE] macchina accesa");
    pending = PA_NONE;
    announcedTransition = false;
  }

  if (pending == PA_WAIT_POWER_OFF && newS == LdrBlinkDetector::OFF) {
    Serial.println("[AZIONE] macchina spenta");
    pending = PA_NONE;
    announcedTransition = false;
  }
}

// -------- COMANDI SERIALI --------
static void comando_ACCENDI() {
  Serial.println("[COMANDO] ACCENDI_APPARECCHIO");
  auto s = ldr.getState();

  if (s == LdrBlinkDetector::STEADY_ON) {
    Serial.println("[AZIONE] macchina già accesa");
    return;
  }

  if (pending == PA_NONE) {
    pending = PA_WAIT_POWER_ON;
    announcedTransition = false;
    accendi();
  }
}

static void comando_SPEGNI() {
  Serial.println("[COMANDO] SPEGNI_APPARECCHIO");
  auto s = ldr.getState();

  if (s == LdrBlinkDetector::OFF) {
    Serial.println("[AZIONE] macchina già spenta");
    return;
  }

  if (pending == PA_NONE) {
    pending = PA_WAIT_POWER_OFF;
    announcedTransition = false;
    spegni();
  }
}

static void comando_STATO() {
  // stampa solo lo stato corrente
  const char* name =
    (ldr.getState() == LdrBlinkDetector::OFF)       ? "SPENTA" :
    (ldr.getState() == LdrBlinkDetector::STEADY_ON) ? "ACCESA FISSA" :
                                                      "LAMPEGGIANTE";

  Serial.printf("[STATO] %s\n", name);
}

// -------- SETUP/LOOP --------
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Avvio");

  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2400);
  servo.write(RIPOSO_POS);

  ldr.begin();
  ldr.setOnChange(onState);
  ldr.setOnChange2(onLdrChange2);

  // Stampa lo stato iniziale una sola volta
  // comando_STATO();

  Serial.println("Pronto.");
}

void loop() {
  // fa girare il detector (necessario per generare gli eventi)
  ldr.update();

  // comandi via seriale: ACCENDI / SPEGNI / STATO
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if      (cmd == "ACCENDI") comando_ACCENDI();
    else if (cmd == "SPEGNI")  comando_SPEGNI();
    else if (cmd == "STATO")   comando_STATO();
    // nessun altro output
  }
}
