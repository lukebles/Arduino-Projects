#include <Arduino.h>
#include "BluetoothA2DPSource.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>   // ✅ serve per HTTPS
#include "config.h"             // #define WIFI_SSID ...
#include <esp_bt.h>
extern "C" {
  #include "esp_coexist.h"
}
#include <mp3dec.h>   // dalla libreria "arduino-libhelix"

BluetoothA2DPSource a2dp;

// --- stato ---
static volatile bool g_a2dp_connected = false;

static void wifi_off() {
  if (WiFi.getMode() != WIFI_OFF) {
    WiFi.disconnect(true, true);   // drop e pulizia
    WiFi.mode(WIFI_OFF);           // spegni radio Wi-Fi
    delay(100);
    Serial.println("[WiFi] OFF");
  }
}

static void wifi_on() {
  if (WiFi.getMode() == WIFI_OFF) {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("[WiFi] ON → mi collego a \"%s\"...\n", WIFI_SSID);
  }
}

// ====== RING PCM SEMPLICE (frame = 2 canali int16 interleaved) ======
struct RingPCM {
  volatile size_t r = 0, w = 0, cap = 0;
  Frame* buf = nullptr; // Frame = {int16_t ch1, ch2} (già definito dalla tua lib A2DP)

  void init(size_t capacity_frames) {
    cap = capacity_frames;
    buf = (Frame*)malloc(sizeof(Frame)*cap);
    r = w = 0;
  }
  size_t available_to_read() const {
    size_t wr = w, rd = r;
    return (wr >= rd) ? (wr - rd) : (cap - (rd - wr));
  }
  size_t available_to_write() const {
    return cap - 1 - available_to_read(); // -1 per distinguere pieno/vuoto
  }
  // push da buffer interleaved int16_t [L,R,L,R,...] per N frames
  size_t push(const int16_t* interleavedLR, size_t nframes) {
    size_t can = available_to_write();
    if (nframes > can) nframes = can;
    for (size_t i=0;i<nframes;i++) {
      buf[w] = { interleavedLR[2*i+0], interleavedLR[2*i+1] };
      w++; if (w>=cap) w=0;
    }
    return nframes;
  }
  // pop in Frame* (come richiede il callback A2DP)
  size_t pop(Frame* out, size_t nframes) {
    size_t can = available_to_read();
    if (nframes > can) nframes = can;
    for (size_t i=0;i<nframes;i++) {
      out[i] = buf[r];
      r++; if (r>=cap) r=0;
    }
    return nframes;
  }
} ringpcm;

// === Callback audio A2DP: tono LA 440 Hz ===
int32_t get_data(Frame* frame, int32_t frame_count) {
  static float phase = 0.0f;
  const float freq = 440.0f;   // Hz
  const float sr   = 44100.0f; // sample rate
  const float amp  = 0.2f;     // ampiezza

  for (int i = 0; i < frame_count; i++) {
    float s = amp * sinf(2.0f * (float)M_PI * freq * phase / sr);
    int16_t sample = (int16_t)(s * 32767.0f);
    frame[i].channel1 = sample;
    frame[i].channel2 = sample;
    phase += 1.0f;
    if (phase >= sr) phase -= sr;
  }
  return frame_count;
}

// Sostituisci il tuo target:
const char* SINK_TARGET = "00:0C:8A:F0:44:5E"; // <-- MAC reale della tua Bose

// Coesistenza dinamica
void on_conn_change(esp_a2d_connection_state_t state, void*) {
  const char* txt =
    (state == ESP_A2D_CONNECTION_STATE_CONNECTED)    ? "CONNESSO" :
    (state == ESP_A2D_CONNECTION_STATE_CONNECTING)   ? "IN CONNESSIONE" :
    (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) ? "DISCONNESSO" :
    (state == ESP_A2D_CONNECTION_STATE_DISCONNECTING)? "IN DISCONNESSIONE" : "ALTRO";
  Serial.printf("[A2DP] Stato connessione: %s\n", txt);

  g_a2dp_connected = (state == ESP_A2D_CONNECTION_STATE_CONNECTED);

#ifdef ESP_COEX_PREFER_BT
  if (state == ESP_A2D_CONNECTION_STATE_CONNECTING) {
    esp_coex_preference_set(ESP_COEX_PREFER_BT);   // massima priorità a BT
    wifi_off();                                     // niente Wi-Fi finché non aggancia
  } else if (g_a2dp_connected) {
    esp_coex_preference_set(ESP_COEX_PREFER_BALANCE);
    wifi_on();                                      // ora possiamo accendere il Wi-Fi
  } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    esp_coex_preference_set(ESP_COEX_PREFER_BT);
    wifi_off();                                     // prepara un nuovo tentativo “pulito”
  }
#endif
}



// === Wi-Fi helper ===
static void connectWiFi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  if (!WiFi.isConnected()) {
    Serial.printf("[WiFi] Mi collego a \"%s\" ...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
      delay(250);
      Serial.print(".");
    }
    Serial.println();
  }

  if (WiFi.isConnected()) {
    Serial.printf("[WiFi] Connesso. IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[WiFi] NON connesso ora (ritenterò in seguito).");
  }
}

// === Task che scarica e scarta il flusso Radio 2 ===
// Usa HTTP "grezzo" con WiFiClient per minimizzare overhead.
// === Task che scarica e SCARTA il flusso di RAI Radio 2 ===
// === Task che scarica e SCARTA il flusso (segue redirect HTTP/HTTPS) ===
// === Task che scarica e SCARTA il flusso (segue redirect HTTP/HTTPS) ===


// === Task: scarica MP3 (segue redirect), decodifica con Helix, scrive nel ring ===
void stream_to_a2dp_task(void* pv) {
  (void)pv;

  String currentUrl = "http://icestreaming.rai.it/2.mp3";

  static const size_t NET_BUF_SZ = 2048;      // lettura rete
  static const size_t MP3_IN_SZ  = 4096;      // buffer input decoder
  static const size_t PCM_TMP_FR = 1152;      // max frame samples per canale
  static const size_t PCM_TMP_SZ = PCM_TMP_FR * 2; // stereo interleaved

  uint8_t*  netbuf  = (uint8_t*)malloc(NET_BUF_SZ);
  uint8_t*  mp3in   = (uint8_t*)malloc(MP3_IN_SZ);
  int16_t*  pcmtmp  = (int16_t*)malloc(sizeof(int16_t)*PCM_TMP_SZ);

  HMP3Decoder dec = MP3InitDecoder();

  auto trim = [](String& s){ s.trim(); };

  auto parseUrl = [](const String& url, String& scheme, String& host, uint16_t& port, String& path) {
    scheme = ""; host = ""; path = "/";
    port = 0;
    int posScheme = url.indexOf("://");
    if (posScheme < 0) return false;
    scheme = url.substring(0, posScheme);
    String rest = url.substring(posScheme + 3);
    int slash = rest.indexOf('/');
    String hostport = (slash >= 0) ? rest.substring(0, slash) : rest;
    path = (slash >= 0) ? rest.substring(slash) : "/";
    int colon = hostport.indexOf(':');
    if (colon >= 0) {
      host = hostport.substring(0, colon);
      port = (uint16_t) hostport.substring(colon + 1).toInt();
    } else {
      host = hostport;
      port = (scheme == "https") ? 443 : 80;
    }
    return (scheme.length() && host.length() && port > 0);
  };

  auto readHeadersBlock = [](Client& c, String& headers, uint32_t timeout_ms)->bool {
    headers = "";
    uint32_t t0 = millis();
    const size_t LIMIT = 8192;
    while (millis() - t0 < timeout_ms) {
      while (c.available()) {
        char ch = (char)c.read();
        headers += ch;
        if (headers.length() > LIMIT) return true;
        int L = headers.length();
        if ((L >= 4 && headers.endsWith("\r\n\r\n")) ||
            (L >= 2 && headers.endsWith("\n\n"))) return true;
      }
      if (!c.connected()) return headers.length() > 0;
      vTaskDelay(pdMS_TO_TICKS(5));
    }
    return headers.length() > 0;
  };

  auto getHeaderValue = [](const String& headers, const char* key)->String {
    String low = headers; low.toLowerCase();
    String k = String(key); k.toLowerCase();
    int pos = low.indexOf(k + ":");
    if (pos < 0) return "";
    int lineStart = headers.lastIndexOf('\n', pos);
    if (lineStart < 0) lineStart = 0; else lineStart++;
    int lineEnd = headers.indexOf('\n', pos);
    if (lineEnd < 0) lineEnd = headers.length();
    String line = headers.substring(lineStart, lineEnd);
    int colon = line.indexOf(':');
    if (colon >= 0) { String v = line.substring(colon + 1); v.trim(); return v; }
    return "";
  };

  for (;;) {
    if (!WiFi.isConnected()) {
      connectWiFi();
      vTaskDelay(pdMS_TO_TICKS(800));
      continue;
    }

    String scheme, host, path;
    uint16_t port = 0;
    int redirectHops = 0;

    // Buffer di input per Helix (riempi/consuma)
    int inFill = 0; // quanti byte validi in mp3in[]

    while (true) {
      if (!parseUrl(currentUrl, scheme, host, port, path)) {
        Serial.printf("[MP3] URL non valido: %s\n", currentUrl.c_str());
        vTaskDelay(pdMS_TO_TICKS(2000));
        break;
      }

      Serial.printf("[MP3] Connessione a %s:%u (%s)\n", host.c_str(), port, scheme.c_str());

      std::unique_ptr<Client> cli;
      if (scheme == "https") {
        auto *sc = new WiFiClientSecure();
        sc->setInsecure();
        cli.reset(sc);
      } else {
        cli.reset(new WiFiClient());
      }

      cli->setTimeout(5000);
      if (!cli->connect(host.c_str(), port)) {
        Serial.println("[MP3] Connessione fallita. Riprovo tra 2s.");
        vTaskDelay(pdMS_TO_TICKS(2000));
        break;
      }

      // GET
      String req;
      req.reserve(256);
      req += "GET " + path + " HTTP/1.1\r\n";
      req += "Host: " + host + "\r\n";
      req += "User-Agent: ESP32-A2DP-MP3/1.0\r\n";
      req += "Icy-MetaData: 0\r\n";
      req += "Connection: close\r\n";
      req += "\r\n";
      cli->print(req);

      // Header
      String headers;
      if (!readHeadersBlock(*cli, headers, 6000)) {
        Serial.println("[MP3] Header non ricevuti. Riprovo.");
        cli->stop(); vTaskDelay(pdMS_TO_TICKS(1200)); break;
      }

      int eol = headers.indexOf('\n');
      String status = (eol >= 0) ? headers.substring(0, eol) : headers; status.trim();
      Serial.printf("[MP3] Status: %s\n", status.c_str());

      bool ok200 =
        status.startsWith("HTTP/1.1 200") ||
        status.startsWith("HTTP/1.0 200") ||
        status.startsWith("ICY 200");
      bool isRedirect =
        status.startsWith("HTTP/1.1 301") || status.startsWith("HTTP/1.0 301") ||
        status.startsWith("HTTP/1.1 302") || status.startsWith("HTTP/1.0 302") ||
        status.startsWith("HTTP/1.1 307") || status.startsWith("HTTP/1.0 307") ||
        status.startsWith("HTTP/1.1 308") || status.startsWith("HTTP/1.0 308");

      if (isRedirect) {
        String loc = getHeaderValue(headers, "Location");
        cli->stop();
        if (loc.isEmpty()) { Serial.println("[MP3] Redirect senza Location."); vTaskDelay(pdMS_TO_TICKS(1500)); break; }
        if (++redirectHops > 5) { Serial.println("[MP3] Troppi redirect."); vTaskDelay(pdMS_TO_TICKS(1500)); break; }
        if (loc.indexOf("://") < 0) {
          loc = scheme + "://" + host +
                ((port == 80 && scheme=="http") || (port==443 && scheme=="https") ? "" : (String(":") + port))
                + loc;
        }
        Serial.printf("[MP3] Redirect -> %s\n", loc.c_str());
        currentUrl = loc;
        continue; // riparte con nuova URL
      }

      if (!ok200) {
        cli->stop(); Serial.println("[MP3] Status non OK."); vTaskDelay(pdMS_TO_TICKS(1500)); break;
      }

      // --- STREAM/DECODE LOOP ---
      Serial.println("[MP3] Streaming/decoding in corso...");
      inFill = 0;

      MP3FrameInfo fi;
      uint32_t last_ms = millis();

      while (cli->connected()) {
        // Riempi buffer input
        if (inFill < MP3_IN_SZ/2 && cli->available()) {
          int n = cli->read(netbuf, NET_BUF_SZ);
          if (n > 0) {
            int can = MP3_IN_SZ - inFill;
            if (n > can) n = can;
            memcpy(mp3in + inFill, netbuf, n);
            inFill += n;
            last_ms = millis();
          }
        }

        // Timeout rete?
        if (!cli->available() && (millis()-last_ms > 15000)) {
          Serial.println("[MP3] Timeout dati. Riconnessione...");
          break;
        }

        // Se abbiamo abbastanza dati, trova sync e decodifica
        if (inFill > 0) {
          int offs = MP3FindSyncWord(mp3in, inFill);
          if (offs >= 0) {
            // Shift per allineare
            if (offs > 0) { memmove(mp3in, mp3in + offs, inFill - offs); inFill -= offs; }

            int bytesLeft = inFill;
            unsigned char* readPtr = mp3in;

            int err = MP3Decode(dec, &readPtr, &bytesLeft, pcmtmp, 0);
            if (err == 0) {
              MP3GetLastFrameInfo(dec, &fi);

              // Controllo sample rate / canali
              if (fi.samprate != 44100) {
                // (Semplifico: molte radio sono 44100; se diverso, potresti resamplare)
                // Qui riproduciamo a 44100; se 48000, potresti scartare o fare downsample grezzo.
                // Per ora procedo uguale (verrà “pitched” se non 44100).
              }
              int nCh = fi.nChans; // 1 o 2
              int nOut = fi.outputSamps; // campioni totali (mono o stereo interleaved)
              int nFrames = nOut / nCh;

              // Se mono, duplica sui 2 canali
              static int16_t stereo[PCM_TMP_SZ];
              int16_t* outLR;
              if (nCh == 1) {
                if (nFrames > (int)PCM_TMP_FR) nFrames = PCM_TMP_FR;
                for (int i=0;i<nFrames;i++) { stereo[2*i] = pcmtmp[i]; stereo[2*i+1] = pcmtmp[i]; }
                outLR = stereo;
              } else {
                outLR = pcmtmp; // già interleaved L,R
              }

              // Scrivi nel ring
              size_t pushed = ringpcm.push(outLR, nFrames);
              // Se il ring è pieno, i frame in eccesso vengono persi (ok per streaming)

              // Avanza input consumato dal decoder
              int used = inFill - bytesLeft;
              if (used > 0 && used <= inFill) {
                memmove(mp3in, mp3in + used, inFill - used);
                inFill -= used;
              } else {
                inFill = 0; // recovery
              }
            } else {
              // frame non valido -> scarta 1 byte e riprova
              memmove(mp3in, mp3in + 1, inFill - 1);
              inFill -= 1;
            }
          } else {
            // nessun sync trovato: scarta tutto tranne un pezzetto
            if (inFill > 1024) {
              memmove(mp3in, mp3in + (inFill - 1024), 1024);
              inFill = 1024;
            }
          }
        }

        taskYIELD();
      }

      cli->stop();
      Serial.println("[MP3] Connessione terminata. Riprovo tra 2s.");
      vTaskDelay(pdMS_TO_TICKS(2000));
      break; // esci dal while url
    } // while(true) della catena URL
  } // for(;;)

  MP3FreeDecoder(dec);
  free(netbuf); free(mp3in); free(pcmtmp);
  vTaskDelete(NULL);
}





void setup() {
  Serial.begin(115200);
  delay(200);

#if !defined(CONFIG_IDF_TARGET_ESP32)
  Serial.println("Serve ESP32 (BT Classic).");
  for(;;) delay(1000);
#endif

  esp_bt_controller_mem_release(ESP_BT_MODE_BLE); // solo BT Classic

#ifdef ESP_COEX_PREFER_BT
  esp_coex_preference_set(ESP_COEX_PREFER_BT);    // priorità BT all'inizio
#endif

  wifi_off(); // 🔴 Wi-Fi spento prima di tentare il pairing/aggancio

  a2dp.set_local_name("ESP32 Audio");
  a2dp.set_on_connection_state_changed(on_conn_change);
  a2dp.set_auto_reconnect(true);

  Serial.printf("[A2DP] Mi connetto a %s (MAC)...\n", SINK_TARGET);
  a2dp.start(SINK_TARGET, get_data);              // passa il MAC, non il nome

  // Avvia subito il task streaming: resterà “in attesa” finché g_a2dp_connected==false
  xTaskCreatePinnedToCore(stream_to_a2dp_task, "radio2_to_bt", 8192, nullptr, 1, nullptr, APP_CPU_NUM);
}


void loop() {
  // Teniamo d'occhio il Wi-Fi e, se cade, riproviamo.
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    if (!WiFi.isConnected()) {
      Serial.println("[WiFi] Disconnesso, ritento...");
      connectWiFi();
    }
  }
  delay(200);
}
