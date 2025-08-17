#include <SoftwareSerial.h>

// ---------------- Config ----------------
#define MIDI_RX_PIN 8      // RX dal tuo MIDI-IN (uscita opto)
#define MIDI_TX_PIN 9      // TX verso MIDI-OUT (DIN pin 5 tramite 220Ω)
SoftwareSerial midi(MIDI_RX_PIN, MIDI_TX_PIN); // RX, TX

// Offsets nel dump EX-800 (vedi tuo MainForm.cs)
const uint16_t VOICES_START = 0x0244;  // inizio nibble LSB voce 0
const uint8_t  VOICE_LEN    = 21;
const uint16_t NEED_NIBBLES = VOICE_LEN * 2;

// Stato parser SysEx
bool inSysex = false;
uint32_t idx = 0;
bool wantNibbles = false;
uint16_t nibblesRead = 0;
uint8_t curLSB = 0;
bool nextIsLSB = true;

uint8_t voice11[VOICE_LEN];
bool voiceReady = false;

// Retry richiesta dump
bool requestSent = false;
unsigned long lastRequestMs = 0;
const unsigned long REQUEST_INTERVAL_MS = 4000;  // ritenta ogni 4 s finché non arriva nulla

// ---------------- Strutture di output decodificato ----------------
struct Env { uint8_t attack, decay, brk, sustain, slope, release; };
struct DCO_t { uint8_t octave, waveform, level, harmonics; Env env; };
struct VCF_t {
  uint8_t cutoff, resonance, kbdTrack, polarity, egIntensity, trigger; Env env;
};
struct MG_t { uint8_t freq, delay, dco, vcf; };
struct VoiceDecoded {
  DCO_t dco1, dco2;
  uint8_t dcoMode, dcoInterval, dcoDetune;
  VCF_t vcf;
  MG_t mg;
  uint8_t noise, chorus;
};
VoiceDecoded dec;

// --- helper per etichette “da pannello” ---
const __FlashStringHelper* dispWave(uint8_t w)   { return (w==0)?F("Square (1)"):F("Saw (2)"); }
const __FlashStringHelper* dispOct(uint8_t o)    {
  switch(o){ case 0:return F("Low (1)"); case 1:return F("Mid (2)"); default:return F("High (3)"); }
}
const __FlashStringHelper* dispMode(uint8_t m)   { return (m==0)?F("Whole (1)"):F("Double (2)"); }
const __FlashStringHelper* dispTrig(uint8_t t)   { return (t==0)?F("Single (1)"):F("Multi (2)"); }
const __FlashStringHelper* dispPol(uint8_t p)    { return (p==0)?F("Down (1)"):F("Up (2)"); }
const __FlashStringHelper* dispKbd(uint8_t k)    {
  switch(k){ case 0:return F("Off (0)"); case 1:return F("Half (1)"); default:return F("Full (2)"); }
}
const __FlashStringHelper* dispChorus(uint8_t c) { return (c!=0)?F("On (1)"):F("Off (0)"); }

// stampa bitmask armoniche: Sixteenth=1 (16'), Eighth=2 (8'), Fourth=4 (4'), Second=8 (2')
void printHarmonics(uint8_t h){
  if(h==0){ Serial.print(F("None (0)")); return; }
  bool first=true;
  if(h & 1 ){ if(!first) Serial.print(F(", ")); Serial.print(F("16'")); first=false; }
  if(h & 2 ){ if(!first) Serial.print(F(", ")); Serial.print(F("8'"));  first=false; }
  if(h & 4 ){ if(!first) Serial.print(F(", ")); Serial.print(F("4'"));  first=false; }
  if(h & 8 ){ if(!first) Serial.print(F(", ")); Serial.print(F("2'"));  first=false; }
}

// --- stampa envelope in ordine A/D/B/S/Sl/R (come nel tuo editor) ---
void printEnv(const Env& e){
  Serial.print(e.attack);  Serial.print('/');
  Serial.print(e.decay);   Serial.print('/');
  Serial.print(e.brk);     Serial.print('/');
  Serial.print(e.sustain); Serial.print('/');
  Serial.print(e.slope);   Serial.print('/');
  Serial.print(e.release);
}

void printVoice(const VoiceDecoded &o) {
  Serial.println(F("=== PROGRAM 11 (Voice 11) ==="));

  Serial.print(F("DCO Mode: ")); Serial.println(dispMode(o.dcoMode));
  Serial.print(F("Detune: "));   Serial.println(o.dcoDetune);   // 0..3
  Serial.print(F("Interval: ")); Serial.println(o.dcoInterval); // 0..12
  Serial.print(F("Noise: "));    Serial.println(o.noise);       // 0..15
  Serial.print(F("Chorus: "));   Serial.println(dispChorus(o.chorus));

  // --- DCO1 ---
  Serial.println(F("\nDCO1"));
  Serial.print(F("  Octave: "));   Serial.println(dispOct(o.dco1.octave));
  Serial.print(F("  Waveform: ")); Serial.println(dispWave(o.dco1.waveform));
  Serial.print(F("  Level: "));    Serial.println(o.dco1.level); // 0..31
  Serial.print(F("  Harmonics: ")); printHarmonics(o.dco1.harmonics); Serial.println();
  Serial.print(F("  ENV A/D/B/S/Sl/R: ")); printEnv(o.dco1.env); Serial.println();

  // --- DCO2 ---
  Serial.println(F("\nDCO2"));
  Serial.print(F("  Octave: "));   Serial.println(dispOct(o.dco2.octave));
  Serial.print(F("  Waveform: ")); Serial.println(dispWave(o.dco2.waveform));
  Serial.print(F("  Level: "));    Serial.println(o.dco2.level); // 0..31
  Serial.print(F("  Harmonics: ")); printHarmonics(o.dco2.harmonics); Serial.println();
  Serial.print(F("  ENV A/D/B/S/Sl/R: ")); printEnv(o.dco2.env); Serial.println();

  // --- VCF ---
  Serial.println(F("\nVCF"));
  Serial.print(F("  Cutoff: "));    Serial.println(o.vcf.cutoff);        // 0..99 sul pannello
  Serial.print(F("  Resonance: ")); Serial.println(o.vcf.resonance);     // 0..15
  Serial.print(F("  Keyboard Track: ")); Serial.println(dispKbd(o.vcf.kbdTrack));
  Serial.print(F("  Polarity: "));      Serial.println(dispPol(o.vcf.polarity));
  Serial.print(F("  EG Intensity: "));  Serial.println(o.vcf.egIntensity); // 0..15
  Serial.print(F("  Trigger: "));       Serial.println(dispTrig(o.vcf.trigger));
  Serial.print(F("  ENV A/D/B/S/Sl/R: ")); printEnv(o.vcf.env); Serial.println();

  // --- MG ---
  Serial.println(F("\nMG"));
  Serial.print(F("  Frequency: ")); Serial.println(o.mg.freq);   // 0..15
  Serial.print(F("  Delay: "));     Serial.println(o.mg.delay);  // 0..15
  Serial.print(F("  -> DCO: "));    Serial.println(o.mg.dco);    // 0..15
  Serial.print(F("  -> VCF: "));    Serial.println(o.mg.vcf);    // 0..15

  Serial.println();
}

// ---------------- Funzioni ----------------
void resetParser() {
  inSysex = false;
  idx = 0;
  wantNibbles = false;
  nibblesRead = 0;
  nextIsLSB = true;
  voiceReady = false;
}

void beginSysex() {
  inSysex = true;
  idx = 0;              // posizione logica nel buffer (0 == 0xF0)
  wantNibbles = false;
  nibblesRead = 0;
  nextIsLSB = true;
}

inline void feedNibble(uint8_t nib) {
  nib &= 0x0F;
  if (nextIsLSB) { curLSB = nib; nextIsLSB = false; }
  else {
    uint16_t byteIndex = nibblesRead / 2;
    voice11[byteIndex] = (curLSB) | (nib << 4);
    nextIsLSB = true;
  }
  nibblesRead++;
  if (nibblesRead >= NEED_NIBBLES) {
    voiceReady = true;
    wantNibbles = false;
  }
}

void decodeVoice(const uint8_t v[VOICE_LEN], VoiceDecoded &o) {
  // byte 01
  o.dco1.octave   = (v[0] >> 0) & 0x03;
  o.dco2.octave   = (v[0] >> 2) & 0x03;
  o.dco1.waveform = (v[0] >> 4) & 0x03;  // 0=Square,1=Saw
  o.dco2.waveform = (v[0] >> 6) & 0x03;

  // byte 02
  o.dco1.harmonics = (v[1] >> 0) & 0x0F;
  o.dco2.harmonics = (v[1] >> 4) & 0x0F;

  // byte 03
  o.dcoDetune = (v[2] >> 0) & 0x03;
  o.dcoMode   = (v[2] >> 6) & 0x01;                 // 0=Whole,1=Double
  o.chorus    = ((v[2] >> 7) == 0) ? 1 : 0;         // 1=On, 0=Off

  // byte 04
  o.dcoInterval = (v[3] >> 0) & 0x0F;
  o.noise       = (v[3] >> 4) & 0x0F;

  // byte 05
  o.vcf.egIntensity = (v[4] >> 0) & 0x0F;
  o.vcf.polarity    = ((v[4] >> 4) & 0x01) ? 1 : 0;
  o.dco1.level      = ((v[4] >> 5) & 0x07) << 2;

  // byte 06
  o.dco1.level     |= (v[5] & 0x03);
  o.dco2.level      = (v[5] >> 2) & 0x1F;

  // byte 07
  o.vcf.cutoff  = v[6] & 0x7F;
  o.vcf.trigger = (v[6] >> 7) & 0x01;

  // byte 08
  o.mg.freq  = (v[7] >> 0) & 0x0F;
  o.mg.delay = (v[7] >> 4) & 0x0F;

  // byte 09
  o.mg.dco = (v[8] >> 0) & 0x0F;
  o.mg.vcf = (v[8] >> 4) & 0x0F;

  // DCO1 ENV
  o.dco1.env.attack  = (v[9]  >> 0) & 0x1F;
  o.dco1.env.decay   = (v[9]  >> 5) & 0x07;
  o.dco1.env.decay  |= (v[10] & 0x03) << 3;
  o.dco1.env.brk     = (v[10] >> 2) & 0x1F;
  o.dco1.env.slope   = (v[10] >> 7) & 0x01;
  o.dco1.env.slope  |= (v[11] & 0x0F) << 1;
  o.dco1.env.sustain = (v[11] >> 4) & 0x0F;
  o.dco1.env.sustain|= (v[12] & 0x01) << 4;
  o.dco1.env.release = (v[12] >> 1) & 0x1F;

  // DCO2 ENV
  o.dco2.env.attack  = (v[12] >> 6) & 0x03;
  o.dco2.env.attack |= (v[13] & 0x07) << 2;
  o.dco2.env.decay   = (v[13] >> 3) & 0x1F;
  o.dco2.env.brk     = (v[14] >> 0) & 0x1F;
  o.dco2.env.slope   = (v[14] >> 5) & 0x07;
  o.dco2.env.slope  |= (v[15] & 0x03) << 3;
  o.dco2.env.sustain = (v[15] >> 2) & 0x1F;
  o.dco2.env.release = (v[15] >> 7) & 0x01;
  o.dco2.env.release|= (v[16] & 0x0F) << 1;

  // VCF ENV
  o.vcf.env.attack  = (v[16] >> 4) & 0x0F;
  o.vcf.env.attack |= (v[17] & 0x01) << 4;
  o.vcf.env.decay   = (v[17] >> 1) & 0x1F;
  o.vcf.env.brk     = (v[17] >> 6) & 0x03;
  o.vcf.env.brk    |= (v[18] & 0x07) << 2;
  o.vcf.env.slope   = (v[18] >> 3) & 0x1F;
  o.vcf.env.sustain = (v[19] >> 0) & 0x1F;
  o.vcf.env.release = (v[19] >> 5) & 0x07;
  o.vcf.env.release|= (v[20] & 0x03) << 3;

  o.vcf.resonance   = (v[20] >> 2) & 0x0F;
  o.vcf.kbdTrack    = (v[20] >> 6) & 0x03;
}

// ---- invia la bulk dump request all'EX-800 ----
void sendDumpRequest() {
  const uint8_t req[] = { 0xF0, 0x42, 0x21, 0x01, 0x10, 0xF7 }; // Korg EX-800 bulk dump request
  midi.write(req, sizeof(req));
  midi.flush();
  lastRequestMs = millis();
  requestSent = true;
  Serial.println(F(">> SysEx dump request inviato all'EX-800"));
}

// ---------------- Setup / Loop ----------------
void setup() {
  Serial.begin(115200);      // debug USB
  midi.begin(31250);         // porta MIDI
  delay(100);                // piccolo respiro per sicurezza

  Serial.println(F("Avvio: invio richiesta BULK DUMP all'EX-800..."));
  resetParser();
  sendDumpRequest();
}

void loop() {
  // Se non abbiamo ancora ottenuto la voce, ritenta la richiesta a intervalli
  if (!voiceReady && requestSent && (millis() - lastRequestMs >= REQUEST_INTERVAL_MS)) {
    Serial.println(F("Nessuna risposta: ritento richiesta dump..."));
    // ripulisco lo stato del parser per sicurezza
    resetParser();
    sendDumpRequest();
  }

  while (midi.available()) {
    uint8_t b = midi.read();

    if (!inSysex) {
      if (b == 0xF0) {
        beginSysex();
      }
      continue;
    }

    if (b == 0xF7) { // End of SysEx
      inSysex = false;
      if (voiceReady) {
        decodeVoice(voice11, dec);
        printVoice(dec);
        // Se vuoi fermarti qui, puoi non ritentare più.
        // Se vuoi ricevere di nuovo in futuro, lascia lo stato così com'è.
      }
      continue;
    }

    // dentro SysEx: avanzamento indice virtuale
    idx++;

    // quando raggiungiamo l'offset della voce 0, iniziamo a catturare 42 nibble
    if (!wantNibbles && (idx == VOICES_START)) {
      wantNibbles = true;
      nibblesRead = 0;
      nextIsLSB = true;
    }

    if (wantNibbles && !voiceReady) {
      feedNibble(b & 0x0F);
    }
  }
}
