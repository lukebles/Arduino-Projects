#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include "img_converters.h"

// ===========================
// Select camera model in board_config.h
// (pin e config XIAO ESP32S3 Sense)
// ===========================
#include "board_config.h"

// -------- WiFi --------
static const char* WIFI_SSID = "teoles";
static const char* WIFI_PASS = "Nanoun-9";

// -------- Web server (pagina + capture manuale) --------
WebServer server(80);

// -------- Upload su Debian --------
// Server Debian: 192.168.1.40
// Python receiver in ascolto su 8090 (non 8088 perché occupata da docker)
static const char*   UPLOAD_HOST = "192.168.1.40";
static const uint16_t UPLOAD_PORT = 8090;
static const char*   UPLOAD_PATH = "/cam.bmp";  // server salva su /srv/webcam/cam.bmp (sovrascrive)

static const uint32_t UPLOAD_EVERY_MS = 5UL * 60UL * 1000UL; // 5 minuti
static uint32_t lastUploadMs = 0;

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
  <div class="small">Risoluzione richiesta: 800×600 (SVGA)</div>

<script>
  const btn  = document.getElementById('btn');
  const shot = document.getElementById('shot');

  btn.addEventListener('click', async () => {
    btn.disabled = true;
    try {
      // Cache-buster per evitare che il browser riusi la stessa immagine
      shot.src = '/capture.bmp?t=' + Date.now();
    } finally {
      setTimeout(() => btn.disabled = false, 800);
    }
  });
</script>
</body>
</html>
)HTML";

static void handle_root() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

// Cattura frame RAW e lo converte in BMP (risposta HTTP)
static void handle_capture_bmp() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Errore: esp_camera_fb_get() ha restituito NULL");
    return;
  }

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

  esp_camera_fb_return(fb);

  if (!ok || !bmp_buf || bmp_len == 0) {
    server.send(500, "text/plain", "Errore: conversione BMP fallita");
    if (bmp_buf) free(bmp_buf);
    return;
  }

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

  // RAW (non JPEG)
  config.pixel_format = PIXFORMAT_RGB565;

  // 800x600
  config.frame_size   = FRAMESIZE_SVGA;

  // In RGB565 contano poco, ma lasciali così
  config.jpeg_quality = 12;
  config.fb_count     = 1;

  // Con PSRAM: frame buffer in PSRAM
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("esp_camera_init fallita: 0x%x\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    // eventuale tuning:
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

// Upload BMP al server Debian via HTTP PUT (sovrascrive sempre lo stesso file)
static bool upload_bmp_to_debian() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Upload: esp_camera_fb_get() NULL");
    return false;
  }

  uint8_t *bmp_buf = nullptr;
  size_t   bmp_len = 0;

  bool ok = fmt2bmp(
    fb->buf, fb->len,
    fb->width, fb->height,
    fb->format,
    &bmp_buf, &bmp_len
  );

  esp_camera_fb_return(fb);

  if (!ok || !bmp_buf || bmp_len == 0) {
    Serial.println("Upload: conversione BMP fallita");
    if (bmp_buf) free(bmp_buf);
    return false;
  }

  WiFiClient client;
  Serial.printf("Upload: connessione a %s:%u ...\n", UPLOAD_HOST, UPLOAD_PORT);

  if (!client.connect(UPLOAD_HOST, UPLOAD_PORT)) {
    Serial.println("Upload: connessione fallita");
    free(bmp_buf);
    return false;
  }

  // Richiesta HTTP PUT
  client.print(String("PUT ") + UPLOAD_PATH + " HTTP/1.1\r\n");
  client.print(String("Host: ") + UPLOAD_HOST + "\r\n");
  client.print("Content-Type: image/bmp\r\n");
  client.print(String("Content-Length: ") + bmp_len + "\r\n");
  client.print("Connection: close\r\n\r\n");

  // Body
  size_t sent = client.write(bmp_buf, bmp_len);
  free(bmp_buf);

  if (sent != bmp_len) {
    Serial.printf("Upload: inviati %u/%u byte\n", (unsigned)sent, (unsigned)bmp_len);
    client.stop();
    return false;
  }

  // Legge la prima riga della risposta (debug)
  uint32_t t0 = millis();
  while (client.connected() && !client.available() && (millis() - t0 < 5000)) {
    delay(10);
  }

  String line = client.readStringUntil('\n');
  Serial.print("Upload: risposta: ");
  Serial.println(line);

  client.stop();

  return line.indexOf("200") >= 0;
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

  // Avvia subito un upload (opzionale):
  lastUploadMs = millis() - UPLOAD_EVERY_MS;
}

void loop() {
  server.handleClient();

  if (WiFi.status() == WL_CONNECTED) {
    uint32_t now = millis();
    if (now - lastUploadMs >= UPLOAD_EVERY_MS) {
      lastUploadMs = now;
      Serial.println("Scatto programmato: upload BMP...");
      bool ok = upload_bmp_to_debian();
      Serial.println(ok ? "Upload OK" : "Upload FALLITO");
    }
  }
}