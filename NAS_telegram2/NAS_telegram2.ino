#include <Arduino.h>
#include <ESP32Servo.h>
#include "LdrBlinkDetector.h"

// ================== SERVO ==================
Servo servo;
const int SERVO_PIN   = 18;
const int PREMI_POS   = 170;   // angolo "premi"
const int RIPOSO_POS  = 40;    // angolo "rilascia"

// ================== BOTTONI ==================
// Bottone a massa con pullup interno (premuto = LOW)
const int BTN_ON_PIN  = 15;    // ⚠️ strapping pin: ok se non tenuto basso all'avvio
const int BTN_OFF_PIN = 4;     // pin "sicuro"
bool btnOnPrev  = HIGH;
bool btnOffPrev = HIGH;
uint32_t btnDebounceMs = 30;
uint32_t btnOnLast  = 0;
uint32_t btnOffLast = 0;

// ================== LDR DETECTOR ==================
LdrBlinkDetector ldr;  // usa i default: pin=34, soglie 1000/900, flicker 1–4 Hz

// Stato stabile precedente (OFF o STEADY_ON) per direzione transizioni
LdrBlinkDetector::LightState lastStable = LdrBlinkDetector::OFF;

// Azioni pendenti dopo comandi
enum PendingAction : uint8_t { PA_NONE, PA_WAIT_POWER_ON, PA_WAIT_POWER_OFF };
PendingAction pending = PA_NONE;
bool announcedTransition = false;  // per stampare "transizione verso ..." una volta sola

// ------------------ Utility I/O ------------------
// bool justPressed(int pin, bool &prev, uint32_t &lastTs) {
//   bool now = digitalRead(pin);
//   uint32_t t = millis();
//   if (now != prev) { lastTs = t; prev = now; }
//   if ((t - lastTs) < btnDebounceMs) return false;      // debounce
//   return (now == LOW); // con pullup: premuto=LOW
// }

// ------------------ SERVO MOSSE ------------------
void accendi() {
  // premi e rilascia
  for (int a = RIPOSO_POS; a <= PREMI_POS; a += 2) { servo.write(a); delay(10); }
  delay(1000); // tieni premuto 1 s
  for (int a = PREMI_POS; a >= RIPOSO_POS; a -= 2) { servo.write(a); delay(10); }
}

void spegni() {
  for (int a = RIPOSO_POS; a <= PREMI_POS; a += 2) { servo.write(a); delay(10); }
  delay(6000); // tieni premuto 6 s (se il tuo device richiede press lunga)
  for (int a = PREMI_POS; a >= RIPOSO_POS; a -= 2) { servo.write(a); delay(10); }
}

// ------------------ CALLBACK STATO ------------------
void onState(LdrBlinkDetector::LightState s, int raw){
  const char* name = (s==LdrBlinkDetector::OFF) ? "SPENTA" :
                     (s==LdrBlinkDetector::STEADY_ON) ? "ACCESA FISSA" :
                                                        "LAMPEGGIANTE";
  Serial.printf("[STATO] %s  raw=%d\n", name, raw);
}

// Ricevi VECCHIO e NUOVO stato: qui gestiamo transizioni e messaggi “una sola volta”
void onLdrChange2(LdrBlinkDetector::LightState oldS,
                  LdrBlinkDetector::LightState newS,
                  int raw)
{
  // Aggiorna "stabile" quando raggiungiamo OFF o ACCESA FISSA
  if (newS == LdrBlinkDetector::OFF || newS == LdrBlinkDetector::STEADY_ON) {
    lastStable = newS;
  }

  // Quando entriamo in LAMPEGGIANTE, deduci direzione dalla ultima stabilità
  if (newS == LdrBlinkDetector::BLINKING && oldS != LdrBlinkDetector::BLINKING) {

    // Se abbiamo un'azione pendente, annuncia una sola volta
    if (!announcedTransition) {
      if (pending == PA_WAIT_POWER_ON)  Serial.println("[AZIONE] transizione verso accensione");
      if (pending == PA_WAIT_POWER_OFF) Serial.println("[AZIONE] transizione verso spegnimento");
      announcedTransition = true;
    }
  }

  // Chiusura delle azioni pendenti
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

// ------------------ COMANDI DI ALTO LIVELLO ------------------
void comando_ACCENDI() {
  auto s = ldr.getState();

  // if (s == LdrBlinkDetector::STEADY_ON) {
  //   Serial.println("Apparecchio acceso");
  //   return;
  // }
  // if (s == LdrBlinkDetector::BLINKING) {
  //   // Direzione in base all’ultima stabilità
  //   if (lastStable == LdrBlinkDetector::STEADY_ON) Serial.println("transizione verso spegnimento");
  //   else                                           Serial.println("transizione verso accensione");
  //   return;
  // }

  // SPENTO: prova ad accendere
  Serial.println("[COMANDO] ACCENDI_APPARECCHIO");
  pending = PA_WAIT_POWER_ON;
  announcedTransition = false;
  accendi();
}

void comando_SPEGNI() {
  auto s = ldr.getState();

  // if (s == LdrBlinkDetector::OFF) {
  //   Serial.println("Apparecchio spento");
  //   return;
  // }
  if (s == LdrBlinkDetector::BLINKING) {
    if (lastStable == LdrBlinkDetector::STEADY_ON) Serial.println("transizione verso spegnimento");
    else                                           Serial.println("transizione verso accensione");
    return;
  }

  // ACCESO FISSO: prova a spegnere
  Serial.println("[COMANDO] SPEGNI_APPARECCHIO");
  pending = PA_WAIT_POWER_OFF;
  announcedTransition = false;
  spegni();
}

void comando_STATO() {
  auto s = ldr.getState();
  if (s == LdrBlinkDetector::STEADY_ON) {
    Serial.println("STATO_APPARECCHIO: acceso");
  } else if (s == LdrBlinkDetector::OFF) {
    Serial.println("STATO_APPARECCHIO: spento");
  } else {
    // lampeggiante
    if (lastStable == LdrBlinkDetector::STEADY_ON)
      Serial.println("STATO_APPARECCHIO: verso spegnimento (lampeggiante)");
    else
      Serial.println("STATO_APPARECCHIO: verso accensione (lampeggiante)");
  }
}

// ================== SETUP/LOOP ==================
void setup(){
  Serial.begin(115200);

  // Servo
  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2400);
  servo.write(RIPOSO_POS);

  // Bottoni
  pinMode(BTN_ON_PIN,  INPUT_PULLUP);
  pinMode(BTN_OFF_PIN, INPUT_PULLUP);

  // LDR
  ldr.begin();
  ldr.setOnChange(onState);
  ldr.setOnChange2(onLdrChange2);
  //Serial.println("LDR pronta (GPIO34). Soglie: ON>=1000, OFF<=900. Flicker 1–4 Hz.");
}

void loop(){
  // Mantieni vivo il detector (non blocca)
  ldr.update();

  // Bottone ACCENDI
  //if (justPressed(BTN_ON_PIN, btnOnPrev, btnOnLast)) {
  //  comando_ACCENDI();
  //}

  // Bottone SPEGNI
  //if (justPressed(BTN_OFF_PIN, btnOffPrev, btnOffLast)) {
  //  comando_SPEGNI();
  //}

  // Comandi via seriale (opzionali): ACCENDI / SPEGNI / STATO
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n'); cmd.trim();
    cmd.toUpperCase();
    if (cmd == "ACCENDI")      comando_ACCENDI();
    else if (cmd == "SPEGNI")  comando_SPEGNI();
    else if (cmd == "STATO")   comando_STATO();
  }

  // (facoltativo) log periodico
  // static uint32_t t = 0;
  // if (millis() - t >= 500) {
  //   t = millis();
  //   Serial.printf("raw=%d  stato=%s  (on=%d, blink=%d)\n",
  //                 ldr.raw(), ldr.stateName(), ldr.isOn(), ldr.isBlinking());
  // }
}
