#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <map>

// === TARGET SERVICE ===
static BLEUUID TARGET_SVC_UUID("7D74F4BD-C74A-4431-862C-CCE884371592");

// === Stato globale ===
static BLEScan*             g_scan      = nullptr;
static BLEAdvertisedDevice* g_targetAdv = nullptr;
static BLEClient*           g_client    = nullptr;
static bool                 g_doConnect = false;

// Cache per polling
static std::map<std::string, std::string> g_lastRead;

// ---------- Utils ----------
static bool isPrintableString(const std::string& s) {
  if (s.empty()) return false;
  for (unsigned char c : s) { if (c == 0) return false; if (c < 32 && c!='\n' && c!='\r' && c!='\t') return false; }
  return true;
}
static void printHex(const uint8_t* data, size_t len) {
  Serial.print("0x");
  for (size_t i=0;i<len;++i){ uint8_t b=data[i]; if(b<16) Serial.print('0'); Serial.print(b,HEX); if(i+1<len) Serial.print(' '); }
}
static void printHex(const std::string& s){ printHex((const uint8_t*)s.data(), s.size()); }
static void logValueLine(const char* prefix, const std::string& v) {
  Serial.print(prefix); printHex(v); Serial.println();
  if (isPrintableString(v)) { Serial.print("  ASCII: "); Serial.write((const uint8_t*)v.data(), v.size()); Serial.println(); }
}
static String propsToString(BLERemoteCharacteristic* c) {
  String s; if (c->canRead()) s+="READ "; if (c->canWrite()) s+="WRITE "; if (c->canWriteNoResponse()) s+="WRITE_NR "; if (c->canNotify()) s+="NOTIFY "; if (c->canIndicate()) s+="INDICATE ";
  return s.length()?s:"none";
}

// ---------- Notify ----------
static void notifyCallback(BLERemoteCharacteristic* chr, uint8_t* data, size_t len, bool isNotify) {
  Serial.printf("\n[UPDATE] %s (%s) handle=%u len=%u\n",
    chr->getUUID().toString().c_str(), isNotify?"notify":"indicate", chr->getHandle(), (unsigned)len);
  std::string v((char*)data, (char*)data + len);
  logValueLine("  DATA: ", v);
}

// ---------- Forza CCCD 0x2902 ----------
static void tryEnableCCCD(BLERemoteCharacteristic* c) {
  BLERemoteDescriptor* cccd = c->getDescriptor(BLEUUID((uint16_t)0x2902));
  if (!cccd) { Serial.println("  [CCCD] 0x2902 non presente"); return; }
  uint8_t notifOn[] = {0x01, 0x00};
  cccd->writeValue(notifOn, sizeof(notifOn), true);
  Serial.println("  [CCCD] Notify ON");
  uint8_t indOn[] = {0x02, 0x00};
  cccd->writeValue(indOn, sizeof(indOn), true);
  Serial.println("  [CCCD] Indicate ON");
}

// ---------- Init-writes “gentili” ----------
static void tryInitWrites(BLERemoteCharacteristic* c) {
  if (!(c->canWrite() || c->canWriteNoResponse())) return;
  Serial.printf("  INIT su %s: 0x01, 0x00\n", c->getUUID().toString().c_str());
  uint8_t b1=0x01; c->writeValue(&b1,1,true); delay(80);
  uint8_t b0=0x00; c->writeValue(&b0,1,true); delay(80);
}

static void prepareService(BLEClient* client) {
  BLERemoteService* svc = client->getService(TARGET_SVC_UUID);
  if (!svc) { Serial.println("[X] Servizio target NON trovato sul GATT."); return; }

  Serial.printf("[OK] Servizio: %s\n", svc->getUUID().toString().c_str());
  auto* chMap = svc->getCharacteristics();
  if (!chMap || chMap->empty()) { Serial.println("Nessuna caratteristica."); return; }

  // --- helper: attiva CCCD solo Notify (0x0100) ---
  auto enableNotifyOnly = [](BLERemoteCharacteristic* c) {
    BLERemoteDescriptor* cccd = c->getDescriptor(BLEUUID((uint16_t)0x2902));
    if (!cccd) { Serial.println("  [CCCD] 0x2902 non presente"); return; }
    uint8_t notifOn[] = {0x01, 0x00};
    cccd->writeValue(notifOn, sizeof(notifOn), true);
    Serial.println("  [CCCD] Notify ON");
  };

  // --- helper: leggi 0x2901/0x2904 se presenti ---
  auto readUsefulDescriptors = [](BLERemoteCharacteristic* c) {
    auto* dmap = c->getDescriptors();
    if (!dmap || dmap->empty()) return;

    for (auto& kd : *dmap) {
      BLERemoteDescriptor* d = kd.second;
      if (!d) continue;

      // prova a recuperare come 16-bit, se non lo è non fa nulla di male
      uint16_t du = d->getUUID().getNative()->uuid.uuid16;
      if (du == 0x2901 || du == 0x2904) {
        String dvA = d->readValue();
        std::string dv(dvA.c_str(), dvA.length());
        Serial.printf("  Desc %s (handle=%u): ", d->getUUID().toString().c_str(), d->getHandle());
        printHex(dv); Serial.println();

        if (du == 0x2901 && isPrintableString(dv)) {
          Serial.print("    User Description: ");
          Serial.write((const uint8_t*)dv.data(), dv.size()); Serial.println();
        }
        if (du == 0x2904 && dv.size() >= 7) {
          uint8_t  fmt = (uint8_t)dv[0];
          int8_t   exp = (int8_t)dv[1];
          uint16_t unit = (uint8_t)dv[2] | ((uint16_t)(uint8_t)dv[3] << 8);
          Serial.printf("    2904: fmt=0x%02X exp=%d unit=0x%04X\n", fmt, exp, unit);
        }
      }
    }
  };

  // --- helper: snapshot READ + caching opzionale ---
  auto snapshotRead = [&](BLERemoteCharacteristic* c) {
    if (!c->canRead()) { Serial.println("  READ: (non leggibile)"); return; }
    String vA = c->readValue();
    std::string v(vA.c_str(), vA.length());
    logValueLine("  READ: ", v);

    // se usi una mappa cache esterna g_lastRead:
    String kA = c->getUUID().toString(); std::string k(kA.c_str(), kA.length());
    g_lastRead[k] = v;
  };

  // --- helper: init-writes “gentili” sulla caratteristica c ---
  auto gentleInitWrites = [](BLERemoteCharacteristic* c) {
    if (!(c->canWrite() || c->canWriteNoResponse())) return;
    Serial.printf("  INIT su %s: 0x01, 0x00\n", c->getUUID().toString().c_str());
    uint8_t b1=0x01; c->writeValue(&b1,1,true); delay(100);
    uint8_t b0=0x00; c->writeValue(&b0,1,true); delay(100);
  };

  // --- helper: scrivi su una UUID e poi peek su alcune output ---
  auto writeAndPeek = [&](const char* uuidStr, const uint8_t* buf, size_t n){
    BLERemoteCharacteristic* cW = svc->getCharacteristic(BLEUUID(uuidStr));
    if (!cW || !(cW->canWrite() || cW->canWriteNoResponse())) return;
    Serial.printf("  WRITE %s : ", uuidStr);
    printHex(std::string((const char*)buf, (const char*)buf+n)); Serial.println();
    cW->writeValue((uint8_t*)buf, n, true);
    delay(150);

    // probe su canali output più probabili
    const char* outs[] = {
      "a391c6f1-20bb-495a-abbf-2017098fbc61",
      "adc3023d-bfd2-43fd-86f6-7ae05a619092",
      "f3f594f9-e210-48f3-85e2-4b0cf235a9d3"
    };
    for (auto u : outs) {
      BLERemoteCharacteristic* oc = svc->getCharacteristic(BLEUUID(u));
      if (oc && oc->canRead()) {
        String vA = oc->readValue();
        std::string v(vA.c_str(), vA.length());
        Serial.printf("    PEEK %s -> ", u); printHex(v); Serial.println();
        if (isPrintableString(v)) { Serial.print("      ASCII: "); Serial.write((const uint8_t*)v.data(), v.size()); Serial.println(); }
      }
    }
  };

  // === ciclo principale sulle caratteristiche ===
  for (auto& kv : *chMap) {
    BLERemoteCharacteristic* c = kv.second;
    if (!c) continue;

    Serial.printf("\nCaratteristica %s (handle=%u)\n", c->getUUID().toString().c_str(), c->getHandle());
    // proprietà
    {
      String s;
      if (c->canRead())            s += "READ ";
      if (c->canWrite())           s += "WRITE ";
      if (c->canWriteNoResponse()) s += "WRITE_NR ";
      if (c->canNotify())          s += "NOTIFY ";
      if (c->canIndicate())        s += "INDICATE ";
      Serial.printf("  Prop: %s\n", s.length()? s.c_str() : "none");
    }

    // descrittori utili
    readUsefulDescriptors(c);

    // snapshot lettura
    snapshotRead(c);

    // subscribe (Notify only)
    if (c->canNotify() || c->canIndicate()) {
      c->registerForNotify(notifyCallback);
      Serial.println("  SUB: richiesto");
      enableNotifyOnly(c);      // ← scrive 0x0100 su 0x2902
    } else {
      Serial.println("  SUB: non supportato");
    }

    // init scritture leggere
    gentleInitWrites(c);
  }

  // === tentativi mirati su canali “comando” noti (se esistono) ===
  {
    const uint8_t p1[] = {0x01};
    const uint8_t p0[] = {0x00};
    const uint8_t p2[] = {0x02};

    // 9c12... e a28b... nel tuo dump sono WRITE/READ: proviamo
    writeAndPeek("9c12a3db-9ce8-4865-a217-d394b3bc9311", p1, sizeof(p1));
    writeAndPeek("9c12a3db-9ce8-4865-a217-d394b3bc9311", p0, sizeof(p0));
    writeAndPeek("a28b6be1-2fa4-42f8-aeb2-b15a1dbd837a", p1, sizeof(p1));
    writeAndPeek("a28b6be1-2fa4-42f8-aeb2-b15a1dbd837a", p2, sizeof(p2));
  }

  Serial.println("\n-- In ascolto; muovi pulsanti/app e osserva UPDATE/PEEK --");
}


// ---------- Polling READ (change-detect) ----------
static void pollReadsOnce(BLEClient* client) {
  BLERemoteService* svc = client->getService(TARGET_SVC_UUID);
  if (!svc) return;
  auto* chMap = svc->getCharacteristics();
  if (!chMap) return;

  for (auto& kv : *chMap) {
    BLERemoteCharacteristic* c = kv.second;
    if (!c || !c->canRead()) continue;

    String vA = c->readValue();
    std::string v(vA.c_str(), vA.length());
    String kA = c->getUUID().toString(); std::string k(kA.c_str(), kA.length());

    auto it = g_lastRead.find(k);
    if (it == g_lastRead.end() || it->second != v) {
      Serial.printf("\n[CHANGE] %s (handle=%u)\n", k.c_str(), c->getHandle());
      logValueLine("  NEW: ", v);
      g_lastRead[k] = v;
    }
  }
}

// ---------- Scanner: collega quando vede il service in ADV ----------
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    if (!advertisedDevice.haveServiceUUID()) return;
    BLEUUID u = advertisedDevice.getServiceUUID();
    if (!u.equals(TARGET_SVC_UUID)) return;

    Serial.printf("Trovato target in ADV: %s (RSSI %d)\n",
                  advertisedDevice.getAddress().toString().c_str(), advertisedDevice.getRSSI());
    if (g_scan) g_scan->stop();
    if (g_targetAdv) delete g_targetAdv;
    g_targetAdv = new BLEAdvertisedDevice(advertisedDevice);
    g_doConnect = true;
  }
};

// ---------- Setup / Loop ----------
void setup() {
  Serial.begin(115200);
  Serial.println("BLE Client (Bluedroid) — auto-pair al bisogno");

  BLEDevice::init("ESP32-BLE-Client");
  // (opzionale) BLEDevice::setMTU(247);

  g_scan = BLEDevice::getScan();
  g_scan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  g_scan->setActiveScan(true);
  g_scan->setInterval(100);
  g_scan->setWindow(99);
}

void loop() {
  if (g_doConnect && g_targetAdv) {
    g_doConnect = false;

    if (!g_client) g_client = BLEDevice::createClient();
    Serial.printf("Connessione a %s ...\n", g_targetAdv->getAddress().toString().c_str());
    if (!g_client->connect(g_targetAdv)) {
      Serial.println("[X] Connessione fallita. Riprendo lo scan...");
      delete g_targetAdv; g_targetAdv = nullptr;
      g_scan->start(8, false);
      return;
    }

    Serial.println("[OK] Connesso. Preparazione servizio...");
    prepareService(g_client);
  }

  if (!g_client || !g_client->isConnected()) {
    g_scan->start(8, false);   // blocca per 8s
    g_scan->clearResults();
    delay(300);
  } else {
    static uint32_t lastPoll = 0;
    uint32_t now = millis();
    if (now - lastPoll > 1000) { pollReadsOnce(g_client); lastPoll = now; }
    delay(50);
  }
}
