#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include "img_converters.h"
#include "board_config.h"
#include <ESP32Servo.h>

/*
  Programmare come:
  - XIAO_ESP32S3
  - PSRAM: OPI PSRAM

  ATTENZIONE HARDWARE:
  - Il servo NON va alimentato dal 3.3V della scheda
  - Usa 5V esterni per il servo
  - Metti in comune i GND: GND servo/alimentatore <-> GND XIAO
*/

// -------- WiFi --------
static const char* WIFI_SSID = "teoles2";
static const char* WIFI_PASS = "Privato-9";

// -------- Web server --------
WebServer server(80);

// =========================
// PIN
// =========================
static const int SERVO_PIN = 2;
static const int LED_PIN   = 3;

// =========================
// Parametri servo
// =========================
static const int SERVO_ANGLE_RILASCIO = 20;
static const int SERVO_ANGLE_PREMUTO  = 80;

static const int SERVO_TEMPO_PRESSIONE_MS = 1000;
static const int SERVO_TEMPO_RILASCIO_MS  = 1000;

Servo triggerServo;

// =========================
// Parametri camera / stabilizzazione
// =========================
static const framesize_t CAMERA_FRAME_SIZE = FRAMESIZE_SVGA;  // 800x600

// Numero di frame da buttare appena prima dello scatto reale
static const int PRECAPTURE_FLUSH_FRAMES = 6;

// Pausa tra i frame di assestamento
static const int PRECAPTURE_FRAME_DELAY_MS = 120;

// Pagina HTML minimale compatibile con browser vecchi
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="it">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>XIAO ESP32S3 - BMP Capture</title>
  <style>
    body {
      font-family: Arial, Helvetica, sans-serif;
      margin: 16px;
    }

    button {
      font-size: 18px;
      padding: 10px 16px;
    }

    .img-wrap {
      position: relative;
      display: inline-block;
      margin-top: 16px;
      max-width: 100%;
      line-height: 0;
    }

    img {
      display: block;
      max-width: 100%;
      height: auto;
      border: 1px solid #ccc;
    }

    .grid-line {
      position: absolute;
      background: red;
      z-index: 10;
      filter: alpha(opacity=80);
      opacity: 0.8;
      pointer-events: none;
    }

    .v1 {
      top: 0;
      bottom: 0;
      left: 33.3333%;
      width: 2px;
      margin-left: -1px;
    }

    .v2 {
      top: 0;
      bottom: 0;
      left: 66.6666%;
      width: 2px;
      margin-left: -1px;
    }

    .h1 {
      left: 0;
      right: 0;
      top: 33.3333%;
      height: 2px;
      margin-top: -1px;
    }

    .h2 {
      left: 0;
      right: 0;
      top: 66.6666%;
      height: 2px;
      margin-top: -1px;
    }
  </style>
</head>
<body>
  <button id="btn" type="button">Premi pulsante esterno + cattura immagine</button>

  <div class="img-wrap">
    <img id="shot" alt="immagine" src="">
    <div class="grid-line v1"></div>
    <div class="grid-line v2"></div>
    <div class="grid-line h1"></div>
    <div class="grid-line h2"></div>
  </div>

<script type="text/javascript">
(function () {
  var btn = document.getElementById('btn');
  var shot = document.getElementById('shot');

  function onClick() {
    btn.disabled = true;

    shot.src = '/capture.bmp?t=' + (new Date().getTime());

    window.setTimeout(function () {
      btn.disabled = false;
    }, 5000);
  }

  if (btn.addEventListener) {
    btn.addEventListener('click', onClick, false);
  } else if (btn.attachEvent) {
    btn.attachEvent('onclick', onClick);
  } else {
    btn.onclick = onClick;
  }
})();
</script>
</body>
</html>
)HTML";

// =========================
// Funzioni disegno RGB565
// =========================
static inline void setPixelRGB565(camera_fb_t *fb, int x, int y, uint16_t color) {
  if (!fb) return;
  if (x < 0 || y < 0 || x >= (int)fb->width || y >= (int)fb->height) return;

  uint16_t *pix = (uint16_t *)fb->buf;
  pix[y * fb->width + x] = color;
}

static void fillRectRGB565(camera_fb_t *fb, int x, int y, int w, int h, uint16_t color) {
  if (!fb) return;
  for (int yy = y; yy < y + h; yy++) {
    for (int xx = x; xx < x + w; xx++) {
      setPixelRGB565(fb, xx, yy, color);
    }
  }
}

// =========================
// Font 5x7
// =========================
static const uint8_t GLYPH_0[7] = {
  0b01110,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b01110
};

static const uint8_t GLYPH_1[7] = {
  0b00100,
  0b01100,
  0b00100,
  0b00100,
  0b00100,
  0b00100,
  0b01110
};

static const uint8_t GLYPH_2[7] = {
  0b01110,
  0b10001,
  0b00001,
  0b00010,
  0b00100,
  0b01000,
  0b11111
};

static const uint8_t GLYPH_3[7] = {
  0b11110,
  0b00001,
  0b00001,
  0b01110,
  0b00001,
  0b00001,
  0b11110
};

static const uint8_t GLYPH_4[7] = {
  0b00010,
  0b00110,
  0b01010,
  0b10010,
  0b11111,
  0b00010,
  0b00010
};

static const uint8_t GLYPH_5[7] = {
  0b11111,
  0b10000,
  0b10000,
  0b11110,
  0b00001,
  0b00001,
  0b11110
};

static const uint8_t GLYPH_6[7] = {
  0b01110,
  0b10000,
  0b10000,
  0b11110,
  0b10001,
  0b10001,
  0b01110
};

static const uint8_t GLYPH_7[7] = {
  0b11111,
  0b00001,
  0b00010,
  0b00100,
  0b01000,
  0b01000,
  0b01000
};

static const uint8_t GLYPH_8[7] = {
  0b01110,
  0b10001,
  0b10001,
  0b01110,
  0b10001,
  0b10001,
  0b01110
};

static const uint8_t GLYPH_9[7] = {
  0b01110,
  0b10001,
  0b10001,
  0b01111,
  0b00001,
  0b00001,
  0b01110
};

static const uint8_t GLYPH_SLASH[7] = {
  0b00001,
  0b00010,
  0b00010,
  0b00100,
  0b01000,
  0b01000,
  0b10000
};

static const uint8_t GLYPH_COLON[7] = {
  0b00000,
  0b00100,
  0b00100,
  0b00000,
  0b00100,
  0b00100,
  0b00000
};

static const uint8_t GLYPH_SPACE[7] = {
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

static const uint8_t* getGlyph5x7(char c) {
  switch (c) {
    case '0': return GLYPH_0;
    case '1': return GLYPH_1;
    case '2': return GLYPH_2;
    case '3': return GLYPH_3;
    case '4': return GLYPH_4;
    case '5': return GLYPH_5;
    case '6': return GLYPH_6;
    case '7': return GLYPH_7;
    case '8': return GLYPH_8;
    case '9': return GLYPH_9;
    case '/': return GLYPH_SLASH;
    case ':': return GLYPH_COLON;
    case ' ': return GLYPH_SPACE;
    default:  return GLYPH_SPACE;
  }
}

static void drawChar5x7(camera_fb_t *fb, int x, int y, char c, uint16_t color, uint16_t bg, bool useBg = true) {
  const uint8_t *glyph = getGlyph5x7(c);

  for (int row = 0; row < 7; row++) {
    uint8_t rowBits = glyph[row];
    for (int col = 0; col < 5; col++) {
      bool on = rowBits & (1 << (4 - col));
      if (on) {
        setPixelRGB565(fb, x + col, y + row, color);
      } else if (useBg) {
        setPixelRGB565(fb, x + col, y + row, bg);
      }
    }
  }

  if (useBg) {
    for (int row = 0; row < 7; row++) {
      setPixelRGB565(fb, x + 5, y + row, bg);
    }
  }
}

static void drawString5x7(camera_fb_t *fb, int x, int y, const String &txt, uint16_t color, uint16_t bg, bool useBg = true) {
  for (size_t i = 0; i < txt.length(); i++) {
    drawChar5x7(fb, x + (int)i * 6, y, txt[i], color, bg, useBg);
  }
}

// =========================
// Data / ora
// =========================
static void setup_time() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  Serial.print("Sincronizzo ora NTP");

  struct tm timeinfo;
  int tentativi = 0;

  while (!getLocalTime(&timeinfo) && tentativi < 20) {
    delay(500);
    Serial.print(".");
    tentativi++;
  }

  Serial.println();

  if (tentativi < 20) {
    Serial.println("Ora sincronizzata.");
  } else {
    Serial.println("Errore sincronizzazione ora.");
  }
}

static String getDateTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "00/00/0000 00:00:00";
  }

  char buf[32];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(buf);
}

static void drawDateTimeBottomRight(camera_fb_t *fb) {
  if (!fb) return;

  String txt = getDateTimeString();

  int textW = txt.length() * 6;
  int textH = 7;
  int pad   = 4;
  int boxW  = textW + pad * 2;
  int boxH  = textH + pad * 2;

  int x = fb->width  - boxW - 6;
  int y = fb->height - boxH - 6;

  uint16_t black = 0x0000;
  uint16_t white = 0xFFFF;

  fillRectRGB565(fb, x, y, boxW, boxH, black);
  drawString5x7(fb, x + pad, y + pad, txt, white, black, true);
}

// =========================
// Servo + LED
// =========================
static void doExternalButtonSequence() {
  digitalWrite(LED_PIN, HIGH);

  triggerServo.write(SERVO_ANGLE_PREMUTO);
  delay(SERVO_TEMPO_PRESSIONE_MS);

  triggerServo.write(SERVO_ANGLE_RILASCIO);
  delay(SERVO_TEMPO_RILASCIO_MS);
}

// =========================
// Camera helpers
// =========================
static void flushOldFrames(int count, int delayMs) {
  for (int i = 0; i < count; i++) {
    camera_fb_t *tmp = esp_camera_fb_get();
    if (tmp) {
      esp_camera_fb_return(tmp);
    }
    if (delayMs > 0) delay(delayMs);
  }
}

static camera_fb_t* captureFreshFrame() {
  // Prima svuota eventuali frame vecchi o rimasti in coda
  flushOldFrames(PRECAPTURE_FLUSH_FRAMES, PRECAPTURE_FRAME_DELAY_MS);

  // Poi cattura il frame "vero"
  camera_fb_t *fb = esp_camera_fb_get();
  return fb;
}

// =========================
// Web handlers
// =========================
static void handle_root() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

static void handle_capture_bmp() {
  doExternalButtonSequence();

  camera_fb_t *fb = captureFreshFrame();
  if (!fb) {
    digitalWrite(LED_PIN, LOW);
    server.send(500, "text/plain", "Errore: esp_camera_fb_get() ha restituito NULL");
    return;
  }

  drawDateTimeBottomRight(fb);

  uint8_t *bmp_buf = nullptr;
  size_t bmp_len = 0;

  bool ok = fmt2bmp(
    fb->buf,
    fb->len,
    fb->width,
    fb->height,
    fb->format,
    &bmp_buf,
    &bmp_len
  );

  esp_camera_fb_return(fb);

  if (!ok || !bmp_buf || bmp_len == 0) {
    digitalWrite(LED_PIN, LOW);
    server.send(500, "text/plain", "Errore: conversione BMP fallita");
    if (bmp_buf) free(bmp_buf);
    return;
  }

  server.setContentLength(bmp_len);
  server.send(200, "image/bmp", "");
  WiFiClient client = server.client();
  client.write(bmp_buf, bmp_len);

  free(bmp_buf);
  digitalWrite(LED_PIN, LOW);
}

// =========================
// Camera
// =========================
static bool setup_camera_bmp() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;

  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size   = CAMERA_FRAME_SIZE;

  config.jpeg_quality = 12;

  // 2 frame buffer aiutano a non restare "bloccati" su un frame vecchio
  config.fb_count     = 2;
  config.fb_location  = CAMERA_FB_IN_PSRAM;

  // prova a prendere sempre il frame più recente
  config.grab_mode    = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("esp_camera_init fallita: 0x%x\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_hmirror(s, 0);
    s->set_vflip(s, 1);

    // Controlli base immagine
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_sharpness(s, 0);

    // Bilanciamento bianco automatico
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);

    // Guadagno automatico
    s->set_gain_ctrl(s, 1);

    // Esposizione automatica
    s->set_exposure_ctrl(s, 1);

    // Algoritmo AEC migliorato
    s->set_aec2(s, 1);

    // Livello esposizione: se è ancora troppo chiara prova -2
    s->set_ae_level(s, -1);

    // Limita un po' il gain massimo per evitare immagini troppo "sparate"
    // Se la build non accetta questo enum, commenta questa riga
    s->set_gainceiling(s, GAINCEILING_4X);

    // Disattiva effetti particolari
    s->set_special_effect(s, 0);
    s->set_lenc(s, 1);
    s->set_dcw(s, 1);
    s->set_bpc(s, 1);
    s->set_wpc(s, 1);

    // Valori manuali lasciati in automatico, quindi NON usati:
    // s->set_agc_gain(s, ...);
    // s->set_aec_value(s, ...);
  }

  // Assestamento iniziale dopo init camera
  flushOldFrames(8, 120);

  return true;
}

// =========================
// Servo setup
// =========================
static void setup_servo() {
  triggerServo.setPeriodHertz(50);
  triggerServo.attach(SERVO_PIN, 500, 2400);
  triggerServo.write(SERVO_ANGLE_RILASCIO);
  delay(300);
}

// =========================
// WiFi
// =========================
static uint32_t lastWifiAttempt = 0;
static bool timeConfigured = false;

static void connect_wifi_start() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.println("Avvio connessione WiFi...");
}

static void loop_wifi() {
  wl_status_t st = WiFi.status();

  if (st == WL_CONNECTED) {
    static bool wasConnected = false;

    if (!wasConnected) {
      Serial.print("WiFi connesso. IP: ");
      Serial.println(WiFi.localIP());

      if (!timeConfigured) {
        setup_time();
        timeConfigured = true;
      }

      wasConnected = true;
    }
    return;
  }

  static bool wasConnected = true;
  if (wasConnected) {
    Serial.println("WiFi disconnesso.");
    wasConnected = false;
  }

  uint32_t now = millis();

  if (now - lastWifiAttempt >= 10000) {
    lastWifiAttempt = now;

    Serial.println("Ritento connessione WiFi...");
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
}

// =========================
// Setup / loop
// =========================
void setup() {
  Serial.begin(115200);
  delay(400);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  setup_servo();

  if (!setup_camera_bmp()) {
    Serial.println("Camera non inizializzata. Stop.");
    while (true) delay(1000);
  }

  connect_wifi_start();

  if (WiFi.status() == WL_CONNECTED) {
    setup_time();
  }

  server.on("/", handle_root);
  server.on("/capture.bmp", HTTP_GET, handle_capture_bmp);
  server.begin();

  Serial.println("HTTP server avviato.");
}

void loop() {
  loop_wifi();
  server.handleClient();
}