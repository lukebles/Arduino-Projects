#include <SoftwareSerial.h>
#include <Arduino.h>

// ---------------- Config ----------------
#define MIDI_RX_PIN 8
#define MIDI_TX_PIN 9
SoftwareSerial midi(MIDI_RX_PIN, MIDI_TX_PIN); // RX, TX

// Offsets nel dump EX-800 (nibble offset della voce 0)
const uint16_t VOICES_START = 0x0244;
const uint8_t  VOICE_LEN    = 21;
const uint16_t NEED_NIBBLES = VOICE_LEN * 2;

// ---- Header per scrivere una singola voce (Program 11) ----
// Se non funziona, prova a cambiare FUNC_SINGLE_PROG_WRITE in 0x20.
const uint8_t KORG_ID                  = 0x42;
const uint8_t DEVICE_BYTE              = 0x21;   // come nel tuo dump
const uint8_t MODEL_BYTE               = 0x01;   // come nel tuo dump
const uint8_t FUNC_SINGLE_PROG_WRITE   = 0x11;   // <--- prova 0x20 se il synth non risponde
const uint8_t TARGET_PROGRAM_INDEX     = 0x00;   // Program 11 corrisponde alla voce 0 (indice 0)

// Stato parser SysEx (senza buffer giganteschi)
bool inSysex = false;
uint32_t idx = 0;           // indice "virtuale" nei nibble
bool wantNibbles = false;
uint16_t nibblesRead = 0;
uint8_t curLSB = 0;
bool nextIsLSB = true;

uint8_t voice11[VOICE_LEN]; // 21 byte della voce 11
bool voiceReady = false;

// Timer invio modifica
bool scheduledModify = false;
unsigned long modifyAtMs = 0;
bool alreadySentEdit = false;

// ---------------- Strutture ----------------
struct Env { uint8_t attack, decay, brk, sustain, slope, release; };
struct DCO_t { uint8_t octave, waveform, level, harmonics; Env env; };
struct VCF_t { uint8_t cutoff, resonance, kbdTrack, polarity, egIntensity, trigger; Env env; };
struct MG_t { uint8_t freq, delay, dco, vcf; };
struct VoiceDecoded {
  DCO_t dco1, dco2;
  uint8_t dcoMode, dcoInterval, dcoDetune;
  VCF_t vcf;
  MG_t mg;
  uint8_t noise, chorus;
};
VoiceDecoded dec;

// --- helper per stampa compatta (in PROGMEM) ---
const __FlashStringHelper* dispWave(uint8_t w){ return (w==0)?F("Square (1)"):F("Saw (2)"); }
const __FlashStringHelper* dispOct(uint8_t o){ switch(o){case 0:return F("Low (1)");case 1:return F("Mid (2)");default:return F("High (3)");} }
const __FlashStringHelper* dispMode(uint8_t m){ return (m==0)?F("Whole (1)"):F("Double (2)"); }
const __FlashStringHelper* dispTrig(uint8_t t){ return (t==0)?F("Single (1)"):F("Multi (2)"); }
const __FlashStringHelper* dispPol(uint8_t p){ return (p==0)?F("Down (1)"):F("Up (2)"); }
const __FlashStringHelper* dispKbd(uint8_t k){ switch(k){case 0:return F("Off (0)");case 1:return F("Half (1)");default:return F("Full (2)");} }
const __FlashStringHelper* dispChorus(uint8_t c){ return (c!=0)?F("On (1)"):F("Off (0)"); }

void printHarmonics(uint8_t h){
  if(h==0){ Serial.print(F("None (0)")); return; }
  bool first=true;
  if(h & 1 ){ if(!first) Serial.print(F(", ")); Serial.print(F("16'")); first=false; }
  if(h & 2 ){ if(!first) Serial.print(F(", ")); Serial.print(F("8'"));  first=false; }
  if(h & 4 ){ if(!first) Serial.print(F(", ")); Serial.print(F("4'"));  first=false; }
  if(h & 8 ){ if(!first) Serial.print(F(", ")); Serial.print(F("2'"));  first=false; }
}

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
  Serial.print(F("Detune: "));   Serial.println(o.dcoDetune);
  Serial.print(F("Interval: ")); Serial.println(o.dcoInterval);
  Serial.print(F("Noise: "));    Serial.println(o.noise);
  Serial.print(F("Chorus: "));   Serial.println(dispChorus(o.chorus));
  Serial.println(F("\nDCO1"));
  Serial.print(F("  Octave: "));   Serial.println(dispOct(o.dco1.octave));
  Serial.print(F("  Waveform: ")); Serial.println(dispWave(o.dco1.waveform));
  Serial.print(F("  Level: "));    Serial.println(o.dco1.level);
  Serial.print(F("  Harmonics: ")); printHarmonics(o.dco1.harmonics); Serial.println();
  Serial.print(F("  ENV A/D/B/S/Sl/R: ")); printEnv(o.dco1.env); Serial.println();
  Serial.println(F("\nDCO2"));
  Serial.print(F("  Octave: "));   Serial.println(dispOct(o.dco2.octave));
  Serial.print(F("  Waveform: ")); Serial.println(dispWave(o.dco2.waveform));
  Serial.print(F("  Level: "));    Serial.println(o.dco2.level);
  Serial.print(F("  Harmonics: ")); printHarmonics(o.dco2.harmonics); Serial.println();
  Serial.print(F("  ENV A/D/B/S/Sl/R: ")); printEnv(o.dco2.env); Serial.println();
  Serial.println(F("\nVCF"));
  Serial.print(F("  Cutoff: "));    Serial.println(o.vcf.cutoff);
  Serial.print(F("  Resonance: ")); Serial.println(o.vcf.resonance);
  Serial.print(F("  Keyboard Track: ")); Serial.println(dispKbd(o.vcf.kbdTrack));
  Serial.print(F("  Polarity: "));      Serial.println(dispPol(o.vcf.polarity));
  Serial.print(F("  EG Intensity: "));  Serial.println(o.vcf.egIntensity);
  Serial.print(F("  Trigger: "));       Serial.println(dispTrig(o.vcf.trigger));
  Serial.print(F("  ENV A/D/B/S/Sl/R: ")); printEnv(o.vcf.env); Serial.println();
  Serial.println(F("\nMG"));
  Serial.print(F("  Frequency: ")); Serial.println(o.mg.freq);
  Serial.print(F("  Delay: "));     Serial.println(o.mg.delay);
  Serial.print(F("  -> DCO: "));    Serial.println(o.mg.dco);
  Serial.print(F("  -> VCF: "));    Serial.println(o.mg.vcf);
  Serial.println();
}

// ---------------- Parser ----------------
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
  idx = 0;
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

// ---------------- Decode/Encode ----------------
void decodeVoice(const uint8_t v[VOICE_LEN], VoiceDecoded &o) {
  o.dco1.octave   = (v[0] >> 0) & 0x03;
  o.dco2.octave   = (v[0] >> 2) & 0x03;
  o.dco1.waveform = (v[0] >> 4) & 0x03;
  o.dco2.waveform = (v[0] >> 6) & 0x03;
  o.dco1.harmonics = (v[1] >> 0) & 0x0F;
  o.dco2.harmonics = (v[1] >> 4) & 0x0F;
  o.dcoDetune = (v[2] >> 0) & 0x03;
  o.dcoMode   = (v[2] >> 6) & 0x01;                 // 0=Whole,1=Double
  o.chorus    = ((v[2] >> 7) == 0) ? 1 : 0;         // 1=On, 0=Off
  o.dcoInterval = (v[3] >> 0) & 0x0F;
  o.noise       = (v[3] >> 4) & 0x0F;
  o.vcf.egIntensity = (v[4] >> 0) & 0x0F;
  o.vcf.polarity    = ((v[4] >> 4) & 0x01) ? 1 : 0;
  o.dco1.level      = ((v[4] >> 5) & 0x07) << 2;
  o.dco1.level     |= (v[5] & 0x03);
  o.dco2.level      = (v[5] >> 2) & 0x1F;
  o.vcf.cutoff  = v[6] & 0x7F;
  o.vcf.trigger = (v[6] >> 7) & 0x01;
  o.mg.freq  = (v[7] >> 0) & 0x0F;
  o.mg.delay = (v[7] >> 4) & 0x0F;
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

void encodeVoice(const VoiceDecoded &o, uint8_t v[VOICE_LEN]) {
  memset(v, 0, VOICE_LEN);
  v[0] |= (o.dco1.octave & 0x03) << 0;
  v[0] |= (o.dco2.octave & 0x03) << 2;
  v[0] |= (o.dco1.waveform & 0x03) << 4;
  v[0] |= (o.dco2.waveform & 0x03) << 6;
  v[1] |= (o.dco1.harmonics & 0x0F) << 0;
  v[1] |= (o.dco2.harmonics & 0x0F) << 4;
  v[2] |= (o.dcoDetune & 0x03) << 0;
  v[2] |= (o.dcoMode & 0x01) << 6;
  v[2] |= ((o.chorus ? 0 : 1) & 0x01) << 7; // 0=ON,1=OFF invertito
  v[3] |= (o.dcoInterval & 0x0F) << 0;
  v[3] |= (o.noise       & 0x0F) << 4;
  v[4] |= (o.vcf.egIntensity & 0x0F) << 0;
  v[4] |= (o.vcf.polarity    ? 1 : 0) << 4;
  v[4] |= ((o.dco1.level >> 2) & 0x07) << 5;
  v[5] |= (o.dco1.level & 0x03) << 0;
  v[5] |= (o.dco2.level & 0x1F) << 2;
  v[6] |= (o.vcf.cutoff & 0x7F) << 0;
  v[6] |= (o.vcf.trigger & 0x01) << 7;
  v[7] |= (o.mg.freq  & 0x0F) << 0;
  v[7] |= (o.mg.delay & 0x0F) << 4;
  v[8] |= (o.mg.dco & 0x0F) << 0;
  v[8] |= (o.mg.vcf & 0x0F) << 4;

  // DCO1 ENV
  v[9]  |= (o.dco1.env.attack & 0x1F) << 0;
  v[9]  |= (o.dco1.env.decay  & 0x07) << 5;
  v[10] |= ((o.dco1.env.decay >> 3) & 0x03) << 0;
  v[10] |= (o.dco1.env.brk    & 0x1F) << 2;
  v[10] |= (o.dco1.env.slope  & 0x01) << 7;
  v[11] |= ((o.dco1.env.slope >> 1) & 0x0F) << 0;
  v[11] |= (o.dco1.env.sustain & 0x0F) << 4;
  v[12] = (v[12] & 0xC0) | ((o.dco1.env.sustain >> 4) & 0x01) | ((o.dco1.env.release & 0x1F) << 1);

  // DCO2 ENV
  v[12] = (v[12] & 0x3F) | ((o.dco2.env.attack & 0x03) << 6);
  v[13] |= ((o.dco2.env.attack >> 2) & 0x07) << 0;
  v[13] |= (o.dco2.env.decay & 0x1F) << 3;
  v[14] |= (o.dco2.env.brk   & 0x1F) << 0;
  v[14] |= (o.dco2.env.slope & 0x07) << 5;
  v[15] |= ((o.dco2.env.slope >> 3) & 0x03) << 0;
  v[15] |= (o.dco2.env.sustain & 0x1F) << 2;
  v[15] |= (o.dco2.env.release & 0x01) << 7;
  v[16] |= ((o.dco2.env.release >> 1) & 0x0F) << 0;

  // VCF ENV
  v[16] |= (o.vcf.env.attack & 0x0F) << 4;
  v[17] |= ((o.vcf.env.attack >> 4) & 0x01) << 0;
  v[17] |= (o.vcf.env.decay  & 0x1F) << 1;
  v[17] |= (o.vcf.env.brk    & 0x03) << 6;
  v[18] |= ((o.vcf.env.brk >> 2) & 0x07) << 0;
  v[18] |= (o.vcf.env.slope  & 0x1F) << 3;
  v[19] |= (o.vcf.env.sustain & 0x1F) << 0;
  v[19] |= (o.vcf.env.release & 0x07) << 5;
  v[20] |= ((o.vcf.env.release >> 3) & 0x03) << 0;

  v[20] |= (o.vcf.resonance & 0x0F) << 2;
  v[20] |= (o.vcf.kbdTrack  & 0x03) << 6;
}

// ---------------- SysEx I/O ----------------
void sendDumpRequest() {
  const uint8_t req[] = { 0xF0, 0x42, 0x21, 0x01, 0x10, 0xF7 };
  midi.write(req, sizeof(req));
  midi.flush();
  Serial.println(F(">> Richiesta BULK DUMP inviata"));
}

// Invio “single program write” per il Program 11 con i 21 byte già codificati
void sendSingleProgram11(const uint8_t voice[VOICE_LEN]) {
  // Header: F0, KORG, device, model, func, programIndex
  midi.write(0xF0);
  midi.write(KORG_ID);
  midi.write(DEVICE_BYTE);
  midi.write(MODEL_BYTE);
  midi.write(FUNC_SINGLE_PROG_WRITE);
  midi.write(TARGET_PROGRAM_INDEX); // voce 0 => Program 11

  // Dati: 21 byte -> 42 nibble (ordine LSB poi MSB)
  for (uint8_t i = 0; i < VOICE_LEN; ++i) {
    uint8_t b = voice[i];
    midi.write(b & 0x0F);
    midi.write((b >> 4) & 0x0F);
  }
  midi.write(0xF7);
  midi.flush();
  Serial.println(F(">> Single Program WRITE inviato (Program 11 aggiornato)"));
}

// ---------------- Setup / Loop ----------------
void setup() {
  Serial.begin(115200);
  midi.begin(31250);
  delay(100);
  randomSeed(analogRead(A0));

  resetParser();
  sendDumpRequest();
}

void loop() {
  while (midi.available()) {
    uint8_t b = midi.read();

    if (!inSysex) {
      if (b == 0xF0) beginSysex();
      continue;
    }

    if (b == 0xF7) { // End SysEx
      inSysex = false;

      if (voiceReady) {
        decodeVoice(voice11, dec);
        printVoice(dec);

        if (!scheduledModify) {
          scheduledModify = true;
          modifyAtMs = millis() + 3000; // invia fra 3 s
          Serial.println(F("** Modifica casuale programmata tra 3 secondi..."));
        }
      }
      continue;
    }

    // Avanzamento "virtuale" (nibble index dal byte dopo F0)
    idx++;

    // Arrivati all'inizio dei 42 nibble della voce 0?
    if (!wantNibbles && (idx == VOICES_START)) {
      wantNibbles = true;
      nibblesRead = 0;
      nextIsLSB = true;
    }

    if (wantNibbles && !voiceReady) {
      uint8_t nib = b & 0x0F;
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
  }

  // Trascorsi 3 s: modifica random, ricompatta e invia “single program write”
  if (scheduledModify && !alreadySentEdit && millis() >= modifyAtMs && voiceReady) {
    Serial.println(F("** Eseguo modifica casuale e scrivo il Program 11..."));

    auto clamp = [](int v, int lo, int hi){ return (v<lo)?lo:((v>hi)?hi:v); };

    dec.chorus = dec.chorus ? 0 : 1;
    dec.vcf.cutoff = (random(0, 2) == 0) ? 0 : 99;
    dec.dco1.level = clamp((int)random(0, 32), 0, 31);
    dec.dco2.level = clamp((int)random(0, 32), 0, 31);
    dec.dcoDetune  = random(0, 4);
    dec.dcoInterval= random(0, 13);
    dec.mg.dco     = random(0, 16);
    dec.mg.vcf     = random(0, 16);
    dec.vcf.resonance   = random(0, 16);
    dec.vcf.egIntensity = random(0, 16);

    uint8_t newVoice[VOICE_LEN];
    encodeVoice(dec, newVoice);

    sendSingleProgram11(newVoice);
    alreadySentEdit = true;
    Serial.println(F("** Fatto."));
  }
}
