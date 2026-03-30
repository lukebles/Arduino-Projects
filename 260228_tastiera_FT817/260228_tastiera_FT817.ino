#include <Keypad.h>

// ---------------------
// KEYPAD 4x4
// ---------------------
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// Cambia questi pin come da tuo cablaggio
byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ---------------------
// FT-817 CAT
// ---------------------
enum CatMode : uint8_t {
  MODE_LSB = 0x00,
  MODE_USB = 0x01,
  MODE_CW  = 0x02,
  MODE_CWR = 0x03,
  MODE_AM  = 0x04,
  MODE_FM  = 0x08,
  MODE_DIG = 0x0A,
  MODE_PKT = 0x0C
};
CatMode currentMode = MODE_USB;

CatMode modes[] = {
  MODE_LSB,
  MODE_USB,
  MODE_CW,
  MODE_CWR,
  MODE_AM,
  MODE_FM,
  MODE_DIG,
  MODE_PKT
};

const int NUM_MODES = sizeof(modes) / sizeof(modes[0]);
int currentModeIndex = 1; // USB come default

// Buffer frequenza in MHz (testo)
String freqBuf;

// LED (feedback minimo senza Serial Monitor)
const int LED_PIN = 13;

void blinkOk(int n=1) {
  for (int i=0;i<n;i++){
    digitalWrite(LED_PIN, HIGH); delay(80);
    digitalWrite(LED_PIN, LOW);  delay(120);
  }
}
void blinkErr(int n=2) {
  for (int i=0;i<n;i++){
    digitalWrite(LED_PIN, HIGH); delay(250);
    digitalWrite(LED_PIN, LOW);  delay(180);
  }
}

// invio CAT 5 byte (con piccoli delay tra byte)
void catSend5(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t opcode) {
  Serial.write(b1); delay(5);
  Serial.write(b2); delay(5);
  Serial.write(b3); delay(5);
  Serial.write(b4); delay(5);
  Serial.write(opcode); delay(5);
}

// Set mode: opcode 0x07, mode in byte1
void ft817SetMode(CatMode m) {
  catSend5((uint8_t)m, 0x00, 0x00, 0x00, 0x07);
}

// Parse stringa MHz -> Hz
bool parseMHzToHz(const String& s, uint32_t &hzOut) {
  String t = s;
  t.trim();
  if (t.length() == 0) return false;

  double mhz = t.toFloat();
  if (mhz <= 0.0) return false;

  double hz = mhz * 1000000.0;
  if (hz < 10.0) return false;
  if (hz > 999999999.0) return false;

  hzOut = (uint32_t)(hz + 0.5);
  return true;
}

// Set freq: opcode 0x01
// freq viene codificata come (Hz/10) in 8 cifre BCD -> 4 byte
bool ft817SetFrequencyHz(uint32_t hz) {
  uint32_t f10 = hz / 10;  // Hz/10

  char buf[9];
  snprintf(buf, sizeof(buf), "%08lu", (unsigned long)f10);

  auto bcdByte = [](char d1, char d2) -> uint8_t {
    uint8_t hi = (uint8_t)(d1 - '0');
    uint8_t lo = (uint8_t)(d2 - '0');
    return (uint8_t)((hi << 4) | lo);
  };

  uint8_t b1 = bcdByte(buf[0], buf[1]);
  uint8_t b2 = bcdByte(buf[2], buf[3]);
  uint8_t b3 = bcdByte(buf[4], buf[5]);
  uint8_t b4 = bcdByte(buf[6], buf[7]);

  catSend5(b1, b2, b3, b4, 0x01);
  return true;
}

void cycleModeAndSend() {
  currentModeIndex++;
  if (currentModeIndex >= NUM_MODES)
    currentModeIndex = 0;

  currentMode = modes[currentModeIndex];

  ft817SetMode(currentMode);
  delay(250);   // pausa sicurezza

  blinkOk(1);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // FT-817 richiede 8N2 (attenzione: niente Serial Monitor mentre è collegato!)
  Serial.begin(4800, SERIAL_8N2);

  blinkOk(2); // “pronto”
}

void loop() {
  char k = keypad.getKey();
  if (!k) return;

  if (k >= '0' && k <= '9') {
    freqBuf += k;
    blinkOk(1);
    return;
  }

  switch (k) {
    case '*': // punto decimale
      if (freqBuf.indexOf('.') == -1) {
        if (freqBuf.length() == 0) freqBuf = "0";
        freqBuf += '.';
        blinkOk(1);
      } else {
        blinkErr(1);
      }
      break;

    case '#': { // conferma frequenza
      uint32_t hz;
      if (!parseMHzToHz(freqBuf, hz)) {
        freqBuf = "";
        blinkErr(2);
        break;
      }
      // invia modo corrente e poi freq
      ft817SetMode(currentMode);
      ft817SetFrequencyHz(hz);
      freqBuf = "";
      blinkOk(3);
    } break;

    case 'A': // cambia modo
      cycleModeAndSend();
      break;

    case 'B': // backspace
      if (freqBuf.length() > 0) {
        freqBuf.remove(freqBuf.length() - 1);
        blinkOk(1);
      } else {
        blinkErr(1);
      }
      break;

    case 'C': // clear
      freqBuf = "";
      blinkOk(2);
      break;

    case 'D':
      // libero: se vuoi ci mettiamo memorie, step, ecc.
      blinkErr(1);
      break;
  }
}