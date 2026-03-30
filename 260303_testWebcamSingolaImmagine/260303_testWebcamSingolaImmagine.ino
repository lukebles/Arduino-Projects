#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
/*
programmare come XIAO_ESP32S3
PSRAM OPI PSRAM
Sketch uses 1003327 bytes (12%) of program storage space. Maximum is 8257536 bytes.
Global variables use 66756 bytes (20%) of dynamic memory, leaving 260924 bytes for local variables. Maximum is 327680 bytes.
esptool v5.1.0
Serial port /dev/cu.usbmodem1101:
Connecting...
Connected to ESP32-S3 on /dev/cu.usbmodem1101:
Chip type:          ESP32-S3 (QFN56) (revision v0.2)
Features:           Wi-Fi, BT 5 (LE), Dual Core + LP Core, 240MHz, Embedded PSRAM 8MB (AP_3v3)
Crystal frequency:  40MHz
USB mode:           USB-Serial/JTAG
MAC:                80:b5:4e:f0:84:14

*/
// Serve per la conversione RAW -> BMP
#include "img_converters.h"

// ===========================
// Select camera model in board_config.h
// (tu lo hai già nel progetto)
// ===========================
#include "board_config.h"

// -------- WiFi (metti i tuoi dati) --------
static const char* WIFI_SSID = "teoles2";
static const char* WIFI_PASS = "Privato-9";

// -------- Web server --------
WebServer server(80);

// Pagina HTML minimale: un pulsante + immagine
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="it">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>XIAO ESP32S3 - BMP Capture</title>
  <style>
    body { font-family: sans-serif; margin: 16px; }
    button { font-size: 18px; padding: 10px 16px; }
    img { display:block; margin-top: 16px; max-width: 100%; height:auto; border:1px solid #ccc; }
    .small { color:#666; font-size: 12px; margin-top: 8px; }
  </style>
</head>
<body>
  <button id="btn">Cattura immagine (BMP)</button>
  <img id="shot" alt="immagine" />
  

<script>
  const btn  = document.getElementById('btn');
  const shot = document.getElementById('shot');

  btn.addEventListener('click', async () => {
    btn.disabled = true;
    try {
      // Cache-buster per evitare che il browser riusi la stessa immagine
      shot.src = '/capture.bmp?t=' + Date.now();
    } finally {
      // riabilita dopo un attimo (tempo di download)
      setTimeout(() => btn.disabled = false, 8000);
    }
  });
</script>
</body>
</html>
)HTML";

static void handle_root() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

// Cattura frame RAW e lo converte in BMP
static void handle_capture_bmp() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Errore: esp_camera_fb_get() ha restituito NULL");
    return;
  }

  // Converti in BMP (alloca un buffer con malloc)
  uint8_t *bmp_buf = nullptr;
  size_t   bmp_len = 0;

  bool ok = fmt2bmp(
    fb->buf,
    fb->len,
    fb->width,
    fb->height,
    fb->format,
    &bmp_buf,
    &bmp_len
  );

  // Rilascia subito il frame buffer alla camera
  esp_camera_fb_return(fb);

  if (!ok || !bmp_buf || bmp_len == 0) {
    server.send(500, "text/plain", "Errore: conversione BMP fallita");
    if (bmp_buf) free(bmp_buf);
    return;
  }

  // Risposta HTTP con BMP
  server.setContentLength(bmp_len);
  server.send(200, "image/bmp", "");
  WiFiClient client = server.client();
  client.write(bmp_buf, bmp_len);

  free(bmp_buf);
}

// Setup camera: 800x600, RAW RGB565
static bool setup_camera_svga_bmp() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;

  // --- Pin definiti dal tuo board_config.h (XIAO ESP32S3 Sense) ---
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

  // IMPORTANTISSIMO: non JPEG, ma RAW
  config.pixel_format = PIXFORMAT_RGB565;

  // 800x600
  config.frame_size   = FRAMESIZE_QVGA; //FRAMESIZE_SVGA;

  // Questi due contano poco in RGB565, ma lasciamoli sensati:
  config.jpeg_quality = 12;
  config.fb_count     = 1;

  // Con PSRAM conviene usare frame buffer in PSRAM
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("esp_camera_init fallita: 0x%x\n", err);
    return false;
  }

  // Opzionale: qualche tuning sensore
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_hmirror(s, 1);   // 0 = immagine normale, 1 = specchiata orizzontalmente
    // s->set_vflip(s, 0);     // 0 = normale, 1 = capovolta verticalmente

    // Se l’immagine risulta troppo scura/chiara puoi aggiustare qui:
    // s->set_brightness(s, 0);
    // s->set_contrast(s, 0);
    // s->set_saturation(s, 0);
  }

  return true;
}

static void connect_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connessione WiFi");
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (millis() - t0 > 20000) break;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connesso. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi NON connesso (controlla SSID/PASS o segnale).");
  }
}

void setup() {
  Serial.begin(115200);
  delay(400);

  // Camera
  if (!setup_camera_svga_bmp()) {
    Serial.println("Camera non inizializzata. Stop.");
    while (true) delay(1000);
  }

  // WiFi
  connect_wifi();

  // Server routes
  server.on("/", handle_root);
  server.on("/capture.bmp", HTTP_GET, handle_capture_bmp);
  server.begin();
  Serial.println("HTTP server avviato.");
}

void loop() {
  server.handleClient();
}