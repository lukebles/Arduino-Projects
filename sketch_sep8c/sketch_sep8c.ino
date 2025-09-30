#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// === UUID target ===
static BLEUUID TARGET_SVC_UUID("7D74F4BD-C74A-4431-862C-CCE884371592");

// === Scan/connessione stato ===
static BLEScan*             g_scan      = nullptr;
static BLEAdvertisedDevice* g_targetAdv = nullptr;
static BLEClient*           g_client    = nullptr;
static bool                 g_doConnect = false;

// ========== Utility minimali ==========
static bool isPrintableString(const std::string& s) {
  if (s.empty()) return false;
  for (unsigned char c : s) {
    if (c == 0) return false;
    if (c < 32 && c != '\n' && c != '\r' && c != '\t') return false;
  }
  return true;
}
static void printHex(const uint8_t* data, size_t len) {
  Serial.print("0x");
  for (size_t i=0;i<len;++i){ uint8_t b=data[i]; if(b<16) Serial.print('0'); Serial.print(b,HEX); if(i+1<len) Serial.print(' '); }
}
static void printHex(const std::string& s){ printHex((const uint8_t*)s.data(), s.size()); }
static uint16_t rd_le_u16(const std::string& v, size_t off) {
  if (off + 2 > v.size()) return 0;
  return (uint16_t)((uint8_t)v[off] | ((uint16_t)(uint8_t)v[off+1] << 8));
}
static uint32_t rd_le_u32(const std::string& v, size_t off) {
  if (off + 4 > v.size()) return 0;
  return (uint32_t)((uint8_t)v[off] |
                   ((uint32_t)(uint8_t)v[off+1] << 8) |
                   ((uint32_t)(uint8_t)v[off+2] << 16) |
                   ((uint32_t)(uint8_t)v[off+3] << 24));
}
static void decode2904(const std::string& val) {
  if (val.size() < 7) { Serial.println("  [2904] formato corto"); return; }
  uint8_t  format  = (uint8_t)val[0];
  int8_t   exponent= (int8_t)val[1];
  uint16_t unit    = rd_le_u16(val, 2);
  uint8_t  ns      = (uint8_t)val[4];
  uint16_t desc    = rd_le_u16(val, 5);
  Serial.printf("  [2904] format=0x%02X exponent=%d unit=0x%04X ns=%u desc=0x%04X\n",
                format, exponent, unit, ns, desc);
}
static String propsToString(BLERemoteCharacteristic* c) {
  String s;
  if (c->canRead())            s += "READ ";
  if (c->canWrite())           s += "WRITE ";
  if (c->canWriteNoResponse()) s += "WRITE_NR ";
  if (c->canNotify())          s += "NOTIFY ";
  if (c->canIndicate())        s += "INDICATE ";
  return s.length() ? s : "none";
}

// ========== Notify callback ==========
static void notifyCallback(BLERemoteCharacteristic* chr, uint8_t* data, size_t len, bool isNotify) {
  Serial.printf("\n[NOTIFY] %s (%s) len=%u\n",
    chr->getUUID().toString().c_str(),
    isNotify ? "notify" : "indicate", (unsigned)len);
  std::string v((char*)data, (char*)data + len);
  Serial.print("  HEX: "); printHex(v); Serial.println();
  if (isPrintableString(v)) {
    Serial.print("  TXT: "); Serial.write((const uint8_t*)v.data(), v.size()); Serial.println();
  }
  if (len >= 1) Serial.printf("  u8 = %u\n", (unsigned)(uint8_t)v[0]);
  if (len >= 2) Serial.printf("  u16= %u\n", (unsigned)rd_le_u16(v, 0));
  if (len >= 4) Serial.printf("  u32= %lu\n", (unsigned long)rd_le_u32(v, 0));
}

// ========== Esplora SOLO il servizio target ==========
static void exploreTargetService(BLEClient* client) {
  BLERemoteService* svc = client->getService(TARGET_SVC_UUID);
  if (!svc) { Serial.println("Servizio target non trovato sul GATT."); return; }

  Serial.printf("Servizio: %s\n", svc->getUUID().toString().c_str());

  auto* chMap = svc->getCharacteristics();
  if (!chMap || chMap->empty()) { Serial.println("Nessuna caratteristica nel servizio."); return; }

  for (auto& kv : *chMap) {
    BLERemoteCharacteristic* c = kv.second;
    if (!c) continue;

    Serial.printf("\nCaratteristica: %s\n", c->getUUID().toString().c_str());
    Serial.printf("  Handle: %u  Proprietà: %s\n", c->getHandle(), propsToString(c).c_str());

    // Descrittori 0x2901 / 0x2904 (se ci sono)
    auto* dmap = c->getDescriptors();
    if (dmap && !dmap->empty()) {
      for (auto& kd : *dmap) {
        BLERemoteDescriptor* d = kd.second;
        if (!d) continue;

        if (d->getUUID().equals(BLEUUID((uint16_t)0x2901)) ||
            d->getUUID().equals(BLEUUID((uint16_t)0x2904))) {
          // readValue() qui ritorna Arduino String -> converti a std::string
          String dvA = d->readValue();
          std::string dv = std::string(dvA.c_str(), dvA.length());

          if (!dv.empty()) {
            Serial.printf("  Desc %s: ", d->getUUID().toString().c_str());
            printHex(dv); Serial.println();
            if (d->getUUID().equals(BLEUUID((uint16_t)0x2901)) && isPrintableString(dv)) {
              Serial.print("    User Description: ");
              Serial.write((const uint8_t*)dv.data(), dv.size()); Serial.println();
            }
            if (d->getUUID().equals(BLEUUID((uint16_t)0x2904))) decode2904(dv);
          }
        }
      }
    }

    // Lettura valore (in questa lib spesso è String)
    if (c->canRead()) {
      String vA = c->readValue();
      std::string v = std::string(vA.c_str(), vA.length());

      Serial.printf("  Valore (%u B): ", (unsigned)v.size());
      printHex(v); Serial.println();
      if (isPrintableString(v)) {
        Serial.print("  Testo: "); Serial.write((const uint8_t*)v.data(), v.size()); Serial.println();
      }
    } else {
      Serial.println("  (non leggibile)");
    }

    // Notifiche/Indicate (registerForNotify ritorna void in questa versione)
    if (c->canNotify() || c->canIndicate()) {
      c->registerForNotify(notifyCallback);
      Serial.println("  Subscribe: richiesto");
    }
  }
}

// ========== Scanner: cerca SOLO il service target in ADV ==========
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    if (advertisedDevice.haveServiceUUID()) {
      BLEUUID u = advertisedDevice.getServiceUUID();
      if (u.equals(TARGET_SVC_UUID)) {
        Serial.printf("Trovato device con service target in ADV: %s\n",
                      advertisedDevice.getAddress().toString().c_str());
        g_scan->stop();
        if (g_targetAdv) delete g_targetAdv;
        g_targetAdv = new BLEAdvertisedDevice(advertisedDevice); // copia
        g_doConnect = true;
      }
    }
  }
};

// ========== Setup/Loop minimali ==========
void setup() {
  Serial.begin(115200);
  Serial.println("Scanner -> Connect -> Describe (servizio target)");

  BLEDevice::init("");
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
      Serial.println("Connessione fallita. Riprendo lo scan.");
      delete g_targetAdv; g_targetAdv = nullptr;
      g_scan->start(5, false);
      return;
    }

    Serial.println("Connesso. Esploro il servizio target...");
    exploreTargetService(g_client);
    Serial.println("\n-- Fine descrizione. Rimango connesso per notifiche --\n");
  }

  // Avvia una scansione quando non connesso e non in fase di connessione
  if (!g_client || !g_client->isConnected()) {
    g_scan->start(5, false);   // blocca per 5 s, poi ritorna
    g_scan->clearResults();
    delay(200);                // piccola pausa tra gli scan
  } else {
    delay(50);
  }
}
