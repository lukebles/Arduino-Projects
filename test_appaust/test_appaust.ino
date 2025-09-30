#include <NimBLEDevice.h>

/* --- Config --- */
static const char* TARGET_MAC = ""; // es. "AA:BB:CC:DD:EE:FF" oppure ""

/* --- UUID VCS --- */
static const NimBLEUUID UUID_SVC_VCS       ("00001844-0000-1000-8000-00805f9b34fb"); // Volume Control Service
static const NimBLEUUID UUID_CHR_VOL_STATE ("00002B7D-0000-1000-8000-00805f9b34fb"); // Volume State

/* --- Stato --- */
static NimBLEAdvertisedDevice*     g_found       = nullptr;
static NimBLEClient*               g_client      = nullptr;
static NimBLERemoteCharacteristic* g_chrVolState = nullptr;

/* --- Utils --- */
static void logHex(const char* label, const std::string& data) {
  Serial.printf("%s (%u byte): ", label, (unsigned)data.size());
  for (uint8_t c : data) Serial.printf("%02X ", c);
  Serial.println();
}

/* --- Callback NOTIFY/INDICATE --- */
static void onVolStateNotify(NimBLERemoteCharacteristic* chr, uint8_t* data, size_t len, bool isNotify) {
  Serial.printf("[Notify] Volume State len=%u (%s)\n", (unsigned)len, isNotify ? "notify" : "indicate");
  logHex("Volume State (raw)", std::string((char*)data, (char*)data + len));

  if (len >= 1) {
    uint8_t vol = data[0];                 // 0..255
    float pct = (vol / 255.0f) * 100.0f;
    Serial.printf("  Volume: %u (~%.1f%%)\n", vol, pct);
  }
  if (len >= 2) {
    bool mute = (data[1] != 0);
    Serial.printf("  Mute: %s\n", mute ? "ON" : "OFF");
  }
  if (len >= 3) {
    Serial.printf("  Change Counter: %u\n", data[2]);
  }
  if (len == 0) Serial.println("  (0 byte: notificherà solo ai cambi di volume)");
}

/* --- Scan callbacks --- */
class MyScanCallbacks : public NimBLEScanCallbacks {
public:
  void onResult(NimBLEAdvertisedDevice* adv) { handleAdv(adv); }
  void onResult(const NimBLEAdvertisedDevice* adv) { handleAdv(const_cast<NimBLEAdvertisedDevice*>(adv)); }
  void onScanStart() { Serial.println("Scan avviato."); }
  void onScanEnd(NimBLEScanResults) { Serial.println("Scan terminato."); }

private:
  void handleAdv(NimBLEAdvertisedDevice* adv) {
    Serial.printf("Trovato: %s  RSSI=%d\n", adv->getAddress().toString().c_str(), adv->getRSSI());

    if (adv->haveManufacturerData()) {
      logHex("ManufacturerData", adv->getManufacturerData());
    }

    bool macMatch = (strlen(TARGET_MAC) == 0) ||
                    (adv->getAddress().toString() == std::string(TARGET_MAC));
    bool hasVCS = adv->isAdvertisingService(UUID_SVC_VCS);

    if (macMatch && (hasVCS || strlen(TARGET_MAC) > 0)) {
      Serial.println(">> Selezionato per la connessione.");
      g_found = adv;
      NimBLEDevice::getScan()->stop();
    }
  }
};

/* --- Connessione e subscribe --- */
static bool connectAndSubscribe() {
  if (!g_found) return false;

  g_client = NimBLEDevice::createClient();
  Serial.printf("Connessione a %s ...\n", g_found->getAddress().toString().c_str());

  g_client->setConnectionParams(12, 24, 0, 200);
  g_client->setConnectTimeout(7);

  if (!g_client->connect(g_found)) {
    Serial.println("Connessione fallita.");
    return false;
  }
  Serial.println("Connesso.");

  NimBLERemoteService* svc = g_client->getService(UUID_SVC_VCS);
  if (!svc) {
    Serial.println("Servizio VCS non trovato dopo la connessione.");
    return false;
  }

  g_chrVolState = svc->getCharacteristic(UUID_CHR_VOL_STATE);
  if (!g_chrVolState) {
    Serial.println("Caratteristica Volume State (0x2B7D) non trovata.");
    return false;
  }

  Serial.printf("Props: read=%d write=%d notify=%d indicate=%d\n",
                g_chrVolState->canRead(),
                g_chrVolState->canWrite(),
                g_chrVolState->canNotify(),
                g_chrVolState->canIndicate());

  if (g_chrVolState->canRead()) {
    std::string v = g_chrVolState->readValue();
    if (v.empty()) {
      Serial.println("Volume State (read) = 0 byte. Attendo notify/indicate...");
    } else {
      logHex("Volume State (read)", v);
      onVolStateNotify(g_chrVolState, (uint8_t*)v.data(), v.size(), false);
    }
  } else {
    Serial.println("Volume State non leggibile. Attivo notify/indicate...");
  }

  bool ok = false;
  if (g_chrVolState->canNotify() || g_chrVolState->canIndicate()) {
    // subscribe(notifications, callback, indicate)
    ok = g_chrVolState->subscribe(true, onVolStateNotify, true);
  }
  Serial.printf("Sottoscrizione (notify+indicate): %s\n", ok ? "OK" : "FALLITA");

  return true;
}

/* --- Setup --- */
void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println("\n=== ESP32 VCS Volume Monitor ===");
  if (strlen(TARGET_MAC) > 0) Serial.printf("Filtro MAC: %s\n", TARGET_MAC);

  NimBLEDevice::init("ESP32-VCS-Monitor");

  // MTU si imposta qui (a livello device), non sul client
  NimBLEDevice::setMTU(100);

  // Security/bonding: spesso richiesti dagli apparecchi acustici
  NimBLEDevice::setSecurityAuth(/*bond=*/true, /*mitm=*/false, /*secure=*/true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  NimBLEScan* scan = NimBLEDevice::getScan();
  auto* cb = new MyScanCallbacks();
  scan->setScanCallbacks(cb);          // se serve: scan->setScanCallbacks(cb, false);
  scan->setInterval(45);
  scan->setWindow(30);
  scan->setActiveScan(true);
  scan->start(0, false);
}

/* --- Loop --- */
void loop() {
  static bool tried = false;
  if (g_found && !tried) {
    tried = true;
    if (!connectAndSubscribe()) {
      Serial.println("Riprovo lo scan fra 2s...");
      delay(2000);
      g_found = nullptr;
      tried = false;
      NimBLEDevice::getScan()->start(0, false);
    }
  }

  if (g_client && !g_client->isConnected()) {
    NimBLEDevice::deleteClient(g_client);
    g_client = nullptr;
    g_chrVolState = nullptr;
    g_found = nullptr;
    Serial.println("Disconnesso. Riparto lo scan.");
    NimBLEDevice::getScan()->start(0, false);
    tried = false;
  }

  // Poll facoltativo ogni 3s
  static uint32_t t0 = 0;
  if (g_client && g_client->isConnected() && g_chrVolState && millis() - t0 > 3000) {
    t0 = millis();
    if (g_chrVolState->canRead()) {
      std::string v = g_chrVolState->readValue();
      if (!v.empty()) {
        Serial.print("[Poll] ");
        logHex("Volume State (read)", v);
        onVolStateNotify(g_chrVolState, (uint8_t*)v.data(), v.size(), false);
      } else {
        Serial.println("[Poll] read=0 byte (ok, attendi notify/indicate)");
      }
    }
  }

  delay(20);
}
