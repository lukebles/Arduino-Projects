#include <LkRadioStructure_RH.h>
#include <LowPower.h>          // RocketScream LowPower (AVR)

#define LED_PIN            5
#define TRANSMIT_PIN      12
#define PTT_PIN           13
#define LUCE_CALDAIA_PIN   3     // INPUT_PULLUP → OFF = LOW

// ---------- Payload ----------
struct __attribute__((packed)) packet_contatori {
  byte     sender;            // 1 byte
  uint16_t luceCaldaiaOn;     // 2 bytes (contatore)
  uint16_t a0_raw;            // 2 bytes (ADC grezzo 0..1023)
};

LkRadioStructure<packet_contatori> radio_contatori;
packet_contatori ds_contatori;

#define DEBUG 0
#if DEBUG
  #define prt(x)  Serial.print(x)
  #define prtn(x) Serial.println(x)
#else
  #define prt(x)
  #define prtn(x)
#endif

static uint8_t wakeSlots = 0;   // invio ogni 8 risvegli ≈ 64 s

// Riduci rumore e consumi: disattiva buffer digitale su A0
static inline void preparePins() {
  for (uint8_t p = 0; p <= A5; p++) {
    if (p == LED_PIN || p == TRANSMIT_PIN || p == PTT_PIN || p == LUCE_CALDAIA_PIN) continue;
    pinMode(p, INPUT_PULLUP);
  }
  pinMode(LED_PIN, OUTPUT);    digitalWrite(LED_PIN, LOW);
  pinMode(PTT_PIN, OUTPUT);    digitalWrite(PTT_PIN, LOW);
  pinMode(LUCE_CALDAIA_PIN, INPUT_PULLUP);
  // Disabilita buffer digitale su A0 per meno rumore
  DIDR0 |= _BV(ADC0D);
}

static inline void blinkBrief() {
#if !DEBUG
  digitalWrite(LED_PIN, HIGH); delay(8); digitalWrite(LED_PIN, LOW);
#else
  digitalWrite(LED_PIN, HIGH); delay(40); digitalWrite(LED_PIN, LOW);
#endif
}

static inline void radioSendNow(const packet_contatori &pkt) {
  digitalWrite(PTT_PIN, HIGH);   // accendi TX
  delay(8);                      // stabilizzazione modulo (adatta se serve)
  radio_contatori.sendStructure(pkt);
  digitalWrite(PTT_PIN, LOW);    // spegni TX
}

void setup() {
#if DEBUG
  Serial.begin(115200);
  delay(200);
  prtn("\nUNO: wake 8s, TX ~64s, A0 raw (INTERNAL 1.1V)");
#endif

  preparePins();

  // --- AUMENTA SENSIBILITÀ ADC ---
  // Usa la referenza interna 1.1V (A0 <= 1.1V!)
  analogReference(DEFAULT);
  //delay(5);
  (void)analogRead(A0);   // prima lettura "di scarto" dopo cambio referenza

  radio_contatori.globalSetup(2000, TRANSMIT_PIN, -1); // solo TX

  ds_contatori.sender        = 99;
  ds_contatori.luceCaldaiaOn = 0;
  ds_contatori.a0_raw        = 0;
}

void loop() {
  // --- WAKE ogni ~8 s ---
  bool isOff = (digitalRead(LUCE_CALDAIA_PIN) == LOW); // INPUT_PULLUP: OFF=LOW
  if (isOff && ds_contatori.luceCaldaiaOn < 0xFFFF) {
    ds_contatori.luceCaldaiaOn++;
  }

  wakeSlots++;
  if (wakeSlots >= 8) {         // ogni ~64 s
    wakeSlots = 0;

    // Lettura A0 GREZZA (0..1023) con referenza 1.1V
    (void)analogRead(A0);               // throw-away per stabilizzare dopo sleep
    ds_contatori.a0_raw = analogRead(A0);

    radioSendNow(ds_contatori);
    blinkBrief();

#if DEBUG
    prt("TX cnt="); prt(ds_contatori.luceCaldaiaOn);
    prt(" A0raw="); prtn(ds_contatori.a0_raw);
#endif
  }

  // --- SLEEP profondo per ~8 s ---
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
}
