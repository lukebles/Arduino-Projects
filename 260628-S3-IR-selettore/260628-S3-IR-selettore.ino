#include <WiFi.h>
#include <WebServer.h>
#include <IRremote.hpp>
#include "esp_camera.h"
#include "config.h"

// =====================================================
// PIN IR
// =====================================================
// Cambia questo pin in base a dove colleghi il LED IR.
// Su ESP32 DevKit1 usavi probabilmente GPIO26.
// Su ESP32-S3 con camera integrata scegli un pin libero.
#define IR_SEND_PIN 4

WebServer server(80);

// =====================================================
// PIN CAMERA - Seeed XIAO ESP32S3 Sense / OV2640
// =====================================================
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15

#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

// =====================================================
// COMANDI IR
// =====================================================
struct ComandoIR {
  const char* nome;
  uint16_t address;
  uint16_t command;
  uint32_t rawData;
};

ComandoIR comandiTV[] = {
  { "TASTO_0", 0xDF, 0x9A, 0x659A00DF },
  { "TASTO_1", 0xDF, 0x8E, 0x718E00DF },
  { "TASTO_2", 0xDF, 0x86, 0x798600DF },
  { "TASTO_3", 0xDF, 0x8F, 0x708F00DF },
  { "TASTO_4", 0xDF, 0x92, 0x6D9200DF },
  { "TASTO_5", 0xDF, 0x87, 0x788700DF },
  { "TASTO_6", 0xDF, 0x93, 0x6C9300DF },
  { "TASTO_7", 0xDF, 0x96, 0x699600DF },
  { "TASTO_8", 0xDF, 0x82, 0x7D8200DF },
  { "TASTO_9", 0xDF, 0x97, 0x689700DF },

  { "RADIO_TV",      0xDF, 0xCC, 0x33CC00DF },
  { "PROGRAMMI_SU",  0xDF, 0xDE, 0x21DE00DF },
  { "PROGRAMMI_GIU", 0xDF, 0xD6, 0x29D600DF }
};

// Indici comodi
#define IDX_TVRADIO 10
#define IDX_PRUP    11
#define IDX_PRDN    12

// =====================================================
// STATO PER AGGIORNARE LA FOTO
// =====================================================
volatile unsigned long sequenzaComando = 0;
String ultimoCanale = "";

// =====================================================
// HTML
// =====================================================
const char paginaHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">

  <title>Telecomando TV</title>

  <style>
    body {
      margin: 0;
      padding: 15px;
      background-color: #111111;
      color: white;
      font-family: Arial, Helvetica, sans-serif;
      text-align: center;
      -webkit-user-select: none;
      user-select: none;
    }

    h1 {
      font-size: 26px;
      margin: 10px 0 14px 0;
    }

    #display {
      width: 240px;
      height: 60px;
      margin: 0 auto 14px auto;
      background-color: #222222;
      border: 2px solid #555555;
      border-radius: 10px;
      font-size: 36px;
      line-height: 60px;
      letter-spacing: 6px;
      color: #00ff88;
      overflow: hidden;
    }

    /*
      FOTO RUOTATA 90° ORARIA.

      La foto originale è orizzontale.
      Qui la mostriamo ruotata senza modificare il JPEG lato ESP32.
    */
    #fotoBox {
      width: 220px;
      height: 300px;
      margin: 0 auto 18px auto;
      padding: 8px;
      background-color: #1b1b1b;
      border: 2px solid #444444;
      border-radius: 10px;
      overflow: hidden;
      position: relative;
    }

    #fotoDisplay {
      width: 300px;
      height: 220px;
      display: block;
      background-color: #000000;
      border-radius: 6px;

      transform: rotate(90deg);
      transform-origin: center center;

      position: absolute;
      left: -32px;
      top: 40px;
    }

    #fotoNota {
      margin-top: 6px;
      font-size: 12px;
      color: #aaaaaa;
    }

    table {
      margin: 0 auto;
      border-collapse: separate;
      border-spacing: 10px;
    }

    button {
      width: 70px;
      height: 65px;
      font-size: 26px;
      border: 0;
      border-radius: 12px;
      background-color: #333333;
      color: white;
      -webkit-appearance: none;
      appearance: none;
    }

    button:active {
      background-color: #777777;
    }

    .invio {
      width: 150px;
      background-color: #0066cc;
      font-size: 24px;
    }

    .funzione {
      width: 70px;
      height: 65px;
      font-size: 18px;
      background-color: #cc5500;
    }

    .prog {
      background-color: #444444;
      font-size: 24px;
    }

    .nota {
      margin-top: 18px;
      font-size: 14px;
      color: #aaaaaa;
    }
  </style>
</head>

<body>

  <h1>Telecomando TV</h1>

  <div id="display"></div>

  <div id="fotoBox">
    <img id="fotoDisplay" src="/foto.jpg?start=1" alt="Display canale">
  </div>

  <table>
    <tr>
      <td><button onclick="premiNumero('1')">1</button></td>
      <td><button onclick="premiNumero('2')">2</button></td>
      <td><button onclick="premiNumero('3')">3</button></td>
    </tr>

    <tr>
      <td><button onclick="premiNumero('4')">4</button></td>
      <td><button onclick="premiNumero('5')">5</button></td>
      <td><button onclick="premiNumero('6')">6</button></td>
    </tr>

    <tr>
      <td><button onclick="premiNumero('7')">7</button></td>
      <td><button onclick="premiNumero('8')">8</button></td>
      <td><button onclick="premiNumero('9')">9</button></td>
    </tr>

    <tr>
      <td><button onclick="premiNumero('0')">0</button></td>
      <td colspan="2"><button class="invio" onclick="inviaCanale()">INVIO</button></td>
    </tr>

    <tr>
      <td><button class="funzione" onclick="tvRadio()">TV<br>RADIO</button></td>
      <td><button class="funzione prog" onclick="programmiSu()">P+</button></td>
      <td><button class="funzione prog" onclick="programmiGiu()">P-</button></td>
    </tr>
  </table>

  <div class="nota">
    Massimo 4 cifre. Dopo 3 secondi senza INVIO si cancella.
  </div>

  <script type="text/javascript">
    var digit = "";
    var timerCancella = null;
    var ultimaSequenza = -1;
    var timerFoto = null;

    function aggiornaDisplay() {
      document.getElementById("display").innerHTML = digit;
    }

    function aggiornaFoto() {
      var img = document.getElementById("fotoDisplay");

      /*
        Parametro casuale/tempo per evitare cache.
        Importante su browser vecchi e iPad vecchi.
      */
      img.src = "/foto.jpg?t=" + new Date().getTime();
    }

    function aggiornaFotoFraUnSecondo() {
      if (timerFoto !== null) {
        clearTimeout(timerFoto);
      }

      /*
        Aspetta che il dispositivo comandato abbia aggiornato
        il display LED del canale.
      */
      timerFoto = setTimeout(function() {
        aggiornaFoto();
        timerFoto = null;
      }, 1500);
    }

    function resetTimer() {
      if (timerCancella !== null) {
        clearTimeout(timerCancella);
      }

      timerCancella = setTimeout(function() {
        digit = "";
        aggiornaDisplay();
      }, 3000);
    }

    function premiNumero(n) {
      if (digit.length >= 4) {
        return;
      }

      digit = digit + n;
      aggiornaDisplay();
      resetTimer();
    }

    function richiestaHttp(url) {
      var xhr = new XMLHttpRequest();

      xhr.open("GET", url, true);

      xhr.onreadystatechange = function() {
        if (xhr.readyState == 4) {
          // Comando inviato. La foto viene aggiornata tramite /stato.
        }
      };

      xhr.send(null);
    }

    function inviaCanale() {
      if (digit.length == 0) {
        return;
      }

      richiestaHttp("/canale?num=" + encodeURIComponent(digit));

      digit = "";
      aggiornaDisplay();

      if (timerCancella !== null) {
        clearTimeout(timerCancella);
        timerCancella = null;
      }
    }

    function tvRadio() {
      richiestaHttp("/tvradio");
    }

    function programmiSu() {
      richiestaHttp("/programmi_su");
    }

    function programmiGiu() {
      richiestaHttp("/programmi_giu");
    }

    function controllaStato() {
      var xhr = new XMLHttpRequest();

      xhr.open("GET", "/stato?t=" + new Date().getTime(), true);

      xhr.onreadystatechange = function() {
        if (xhr.readyState == 4 && xhr.status == 200) {
          var testo = xhr.responseText;
          var parti = testo.split("|");
          var seq = parseInt(parti[0], 10);

          if (ultimaSequenza < 0) {
            ultimaSequenza = seq;
            return;
          }

          if (seq != ultimaSequenza) {
            ultimaSequenza = seq;
            aggiornaFotoFraUnSecondo();
          }
        }
      };

      xhr.send(null);
    }

    setInterval(controllaStato, 500);
  </script>

</body>
</html>
)rawliteral";

// =====================================================
// WIFI
// =====================================================
const char* statoWiFi(wl_status_t s) {
  switch (s) {
    case WL_IDLE_STATUS:     return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL:   return "WL_NO_SSID_AVAIL - rete non trovata";
    case WL_SCAN_COMPLETED:  return "WL_SCAN_COMPLETED";
    case WL_CONNECTED:       return "WL_CONNECTED";
    case WL_CONNECT_FAILED:  return "WL_CONNECT_FAILED - connessione fallita";
    case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED:    return "WL_DISCONNECTED - disconnesso";
    default:                 return "STATO SCONOSCIUTO";
  }
}

bool collegaWiFi() {
  Serial.println();
  Serial.println("Connessione al router WiFi...");
  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setSleep(false);

  WiFi.disconnect(true, true);
  delay(1000);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long inizio = millis();
  const unsigned long timeout = 30000;

  while (WiFi.status() != WL_CONNECTED && millis() - inizio < timeout) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  wl_status_t stato = WiFi.status();

  if (stato == WL_CONNECTED) {
    Serial.println("WiFi connesso.");
    Serial.print("Indirizzo IP: ");
    Serial.println(WiFi.localIP());

    Serial.print("RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

    return true;
  }

  Serial.print("Errore WiFi: ");
  Serial.print((int)stato);
  Serial.print(" - ");
  Serial.println(statoWiFi(stato));

  return false;
}

// =====================================================
// CAMERA
// =====================================================
bool avviaCamera() {
  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  /*
    Per leggere un display a 4 caratteri basta VGA.
    Se vuoi più dettaglio prova FRAMESIZE_SVGA.
    Se è lento o instabile prova FRAMESIZE_QVGA.
  */
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 10;

  /*
    Importante per ridurre immagini vecchie in buffer.
  */
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    Serial.print("Errore inizializzazione camera: 0x");
    Serial.println(err, HEX);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();

  if (s) {
    /*
      Queste NON ruotano di 90°.
      Servono solo per capovolgere/specchiare.
      Se l'immagine è sottosopra cambia questi valori.
    */
    s->set_hmirror(s, 0);
    s->set_vflip(s, 1);

    /*
      Regolazioni generiche utili per display LED.
      Puoi modificarle dopo aver visto l'immagine.
    */
    s->set_brightness(s, 0);
    s->set_contrast(s, 1);
    s->set_saturation(s, -1);

    s->set_gain_ctrl(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_aec2(s, 1);
    s->set_ae_level(s, -1);
    s->set_gainceiling(s, GAINCEILING_4X);
  }

  Serial.println("Camera avviata.");
  return true;
}

void handleFoto() {
  camera_fb_t *fb = NULL;

  /*
    Scarta alcuni frame per evitare che venga inviata
    una foto vecchia rimasta nel buffer della camera.
  */
  for (int i = 0; i < 3; i++) {
    fb = esp_camera_fb_get();

    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
    }

    delay(80);
  }

  fb = esp_camera_fb_get();

  if (!fb) {
    server.send(500, "text/plain", "Errore acquisizione foto");
    return;
  }

  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");

  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);

  esp_camera_fb_return(fb);
}

// =====================================================
// IR
// =====================================================
void inviaTasto(int indice) {
  Serial.print("Invio IR: ");
  Serial.println(comandiTV[indice].nome);

  IrSender.sendNEC(
    comandiTV[indice].address,
    comandiTV[indice].command,
    0
  );
}

void segnaComandoRicevuto(String canale) {
  ultimoCanale = canale;
  sequenzaComando++;

  Serial.print("Sequenza comando: ");
  Serial.println(sequenzaComando);
}

bool canaleValido(String canale) {
  canale.trim();

  if (canale.length() == 0) {
    return false;
  }

  if (canale.length() > 4) {
    return false;
  }

  for (int i = 0; i < canale.length(); i++) {
    if (!isDigit(canale.charAt(i))) {
      return false;
    }
  }

  return true;
}

void inviaCanale(String canale) {
  canale.trim();

  if (!canaleValido(canale)) {
    return;
  }

  Serial.print("Canale richiesto: ");
  Serial.println(canale);

  for (int i = 0; i < canale.length(); i++) {
    char c = canale.charAt(i);

    if (c >= '0' && c <= '9') {
      int cifra = c - '0';
      inviaTasto(cifra);
      delay(120);
    }
  }

  segnaComandoRicevuto(canale);
}

// =====================================================
// WEB HANDLER
// =====================================================
void handleRoot() {
  server.send_P(200, "text/html", paginaHTML);
}

void handleCanale() {
  if (!server.hasArg("num")) {
    server.send(400, "text/plain", "Parametro num mancante");
    return;
  }

  String canale = server.arg("num");
  canale.trim();

  if (!canaleValido(canale)) {
    server.send(400, "text/plain", "Canale non valido");
    return;
  }

  inviaCanale(canale);

  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.send(200, "text/plain", "Canale inviato: " + canale);
}

void handleCH() {
  String canale = "";

  /*
    Supporta:
    /CH?0123
    /CH?num=0123
  */
  if (server.hasArg("num")) {
    canale = server.arg("num");
  } else if (server.args() > 0) {
    canale = server.argName(0);

    if (canale.length() == 0) {
      canale = server.arg(0);
    }
  }

  canale.trim();

  if (!canaleValido(canale)) {
    server.send(400, "text/plain", "Canale non valido");
    return;
  }

  inviaCanale(canale);

  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.send(200, "text/plain", "Canale inviato: " + canale);
}

void handleTvRadio() {
  inviaTasto(IDX_TVRADIO);
  segnaComandoRicevuto("TVRADIO");

  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.send(200, "text/plain", "Comando TV/RADIO inviato");
}

void handleProgrammiSu() {
  inviaTasto(IDX_PRUP);
  segnaComandoRicevuto("P+");

  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.send(200, "text/plain", "Comando PROGRAMMI_SU inviato");
}

void handleProgrammiGiu() {
  inviaTasto(IDX_PRDN);
  segnaComandoRicevuto("P-");

  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.send(200, "text/plain", "Comando PROGRAMMI_GIU inviato");
}

void handleStato() {
  String risposta = String(sequenzaComando) + "|" + ultimoCanale;

  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");

  server.send(200, "text/plain", risposta);
}

void handleNotFound() {
  server.send(404, "text/plain", "Pagina non trovata");
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("======================================");
  Serial.println("ESP32-S3 Telecomando IR + Camera");
  Serial.println("======================================");

  Serial.println("Avvio trasmettitore IR...");
  IrSender.begin(IR_SEND_PIN);

  if (!collegaWiFi()) {
    Serial.println("WiFi non connesso. Riavvio tra 5 secondi...");
    delay(5000);
    ESP.restart();
  }

  Serial.println("Avvio camera...");
  avviaCamera();

  server.on("/", handleRoot);

  server.on("/foto.jpg", handleFoto);
  server.on("/stato", handleStato);

  server.on("/canale", handleCanale);
  server.on("/CH", handleCH);

  server.on("/tvradio", handleTvRadio);
  server.on("/programmi_su", handleProgrammiSu);
  server.on("/programmi_giu", handleProgrammiGiu);

  /*
    URL diretti:
    http://IP/PRUP
    http://IP/PRDN
    http://IP/CH?0123
    http://IP/RADIO
    http://IP/TV
  */
  server.on("/PRUP", handleProgrammiSu);
  server.on("/PRDN", handleProgrammiGiu);
  server.on("/RADIO", handleTvRadio);
  server.on("/TV", handleTvRadio);

  server.onNotFound(handleNotFound);

  server.begin();

  Serial.println("Server web avviato.");
  Serial.print("Apri: http://");
  Serial.println(WiFi.localIP());
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  server.handleClient();
}