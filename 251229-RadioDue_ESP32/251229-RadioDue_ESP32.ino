/*
https://www.az-delivery.de/en/blogs/azdelivery-blog-fur-arduino-und-raspberry-pi/internet-radio-mit-dem-esp32?srsltid=AfmBOorHI3Pv-zo16o8dZNUuAVCtgTOFN4Lpzng262CLCM3_YS784Wek
Le due cose “chiave” che dice (e che impattano direttamente sul tuo caso “minimalista, una sola stazione”) sono:

Non usare ESP32 core 2.0.0 o superiore per quel progetto, perché “ha problemi legati alla ESP8266Audio”. 
L’uscita audio “analogica” sfrutta il DAC interno dell’ESP32: l’audio esce su GPIO25 e GPIO26. 

Quindi: per replicare l’esperienza “funzionante” di quel progetto, la combo più coerente è:

ESP32 core 1.0.6 (o comunque < 2.0.0) come raccomanda AZ-Delivery 
ESP8266Audio 1.9.5 (era disponibile prima/attorno a marzo 2022; 1.9.7 è giugno 2022)
*/
#include <WiFi.h>

// ESP8266Audio (in uso versione 2.4.1)
#include "AudioFileSourceICYStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

// configurazione wifi
#include "config.h"

#define LED_PIN 2

static const char *STREAM_URL = "http://icestreaming.rai.it/2.mp3";
 
static const int BUF_SZ   = 80 * 1024;
static const int CODEC_SZ = 29192;

static void *preBuf   = nullptr;
static void *preCodec = nullptr;

static AudioGeneratorMP3        *decoder = nullptr;
static AudioFileSourceICYStream *file    = nullptr;
static AudioFileSourceBuffer    *buff    = nullptr;
static AudioOutputI2S           *out     = nullptr;

// contatore campioni (diagnostica)
static volatile uint32_t g_samples = 0;


// ===== DEBUG =====
#define DEBUG 0
#if DEBUG
  #define prt(x)   Serial.print(x)
  #define prtn(x)  Serial.println(x)
  #define prtf(...) Serial.printf(__VA_ARGS__)
#else
  #define prt(x)
  #define prtn(x)
  #define prtf(...)
#endif

// callback chiamata quando l’audio viene effettivamente “spinto” verso l’output
static void audioStatusCB(void *cbData, int code, const char *string) {
  (void)cbData;
  prtf("AUDIO STATUS code=%d msg=%s\n", code, string ? string : "");
}

static void stopAudio() {
  if (decoder) { decoder->stop(); delete decoder; decoder = nullptr; }
  if (buff)    { buff->close();   delete buff;   buff = nullptr; }
  if (file)    { file->close();   delete file;   file = nullptr; }
}

static bool startAudio() {
  stopAudio();

  prt("Start stream: ");
  prtn(STREAM_URL);

  file = new AudioFileSourceICYStream(STREAM_URL);
  buff = new AudioFileSourceBuffer(file, preBuf, BUF_SZ);
  decoder = new AudioGeneratorMP3(preCodec, CODEC_SZ);

  decoder->RegisterStatusCB(audioStatusCB, nullptr);

  bool ok = decoder->begin(buff, out);
  prtf("Decoder begin: %s (heap=%u)\n", ok ? "OK" : "FAIL", ESP.getFreeHeap());
  return ok;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  preBuf   = malloc(BUF_SZ);
  preCodec = malloc(CODEC_SZ);
  if (!preBuf || !preCodec) {
    prtn("FATAL: malloc failed");
    while (true) delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  prt("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    prt(".");
  }
  prt("\nWiFi OK. IP: ");
  prtn(WiFi.localIP());
  digitalWrite(LED_PIN, HIGH);

  // DAC interno
  out = new AudioOutputI2S(0, 1);

  // ESCLUDI “audio troppo basso”:
  out->SetGain(0.8);

  // Se disponibile nella tua versione, PROVA a forzare mono:
  // out->SetOutputModeMono(true);

  if (!startAudio()) {
    delay(1500);
    ESP.restart();
  }
}

void loop() {
  static uint32_t lastDbg = 0;

  if (decoder && decoder->isRunning()) {
    if (!decoder->loop()) {
      prtn("decoder->loop() false -> restart stream");
      startAudio();
    }
  } else {
    prtn("decoder not running -> restart stream");
    startAudio();
  }

  // DBG 1 volta al secondo: fill e pos
  if (millis() - lastDbg > 1000) {
    lastDbg = millis();
    uint32_t fill = buff ? buff->getFillLevel() : 0;
    uint32_t pos  = buff ? buff->getPos() : 0;
    prtf("DBG running=%d fill=%u pos=%u heap=%u\n",
                  (decoder && decoder->isRunning()) ? 1 : 0,
                  fill, pos, ESP.getFreeHeap());
  }

  delay(1);
}
