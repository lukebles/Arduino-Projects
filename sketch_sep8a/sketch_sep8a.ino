// Forward declarations PRIMA di tutto (l'IDE Arduino genera prototipi in cima)
struct AdvParsed;
struct Seen;

#include <NimBLEDevice.h>
#include <vector>
#include <string>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

// ---------- Config ----------
static const bool ACTIVE_SCAN          = true;
static const bool FILTER_DUPLICATES    = true;
static const bool ONLY_UNIQUE_FACTS    = true;
static const uint16_t SCAN_INTERVAL_MS = 45;
static const uint16_t SCAN_WINDOW_MS   = 30;
static const uint32_t SCAN_SLICE_SEC   = 3;

// ---------- Helpers ----------
const char* companyName(uint16_t cid) {
  switch (cid) {
    case 0x004C: return "Apple";
    case 0x0075: return "Samsung";
    case 0x00E0: return "Google";
    case 0x0006: return "Microsoft";
    case 0x0059: return "Nordic";
    default:     return nullptr;
  }
}

const char* classifyAddr(const std::string& mac) {
  if (mac.size() < 2) return "Unknown";
  char hi[3] = { mac[0], mac[1], 0 };
  int b0 = strtol(hi, nullptr, 16);
  int msb2 = (b0 >> 6) & 0x3;
  if (msb2 == 0b11) return "Random Static";
  if (msb2 == 0b01) return "RPA";
  if (msb2 == 0b00) return "NRPA";
  return "Public/Unknown";
}

// ---------- Modelli dati ----------
struct AdvParsed {
  std::vector<std::string> serviceUUIDs; // 128-bit lowercase
  std::vector<uint16_t>    companyIDs;   // Manufacturer IDs
};

struct Seen {
  std::string mac;
  std::vector<std::string> uuids;
  std::vector<uint16_t>     cids;
  std::vector<std::string>  names;
};

static std::vector<Seen> g_seen;

static bool vecHas(const std::vector<std::string>& v, const std::string& x) {
  for (auto& e : v) if (e == x) return true;
  return false;
}
static bool vecHas16(const std::vector<uint16_t>& v, uint16_t x) {
  for (auto e : v) if (e == x) return true;
  return false;
}
static Seen* findSeen(const std::string& mac) {
  for (auto& s : g_seen) if (s.mac == mac) return &s;
  return nullptr;
}

// ---------- UUID helpers ----------
static const char* BASE_SUFFIX = "-0000-1000-8000-00805f9b34fb";

static std::string uuid16to128(uint16_t u16) {
  char buf[37];
  snprintf(buf, sizeof(buf), "0000%04x-0000-1000-8000-00805f9b34fb", u16);
  for (char* p = buf; *p; ++p) *p = tolower(*p);
  return std::string(buf);
}

static std::string humanize16(uint16_t u16) {
  switch(u16) {
    case 0x180A: return "0x180A Device Information";
    case 0x180F: return "0x180F Battery";
    case 0x1844: return "0x1844 Volume Control";
    case 0x1846: return "0x1846 CSIS";
    case 0x1848: return "0x1848 Media Control";
    case 0x1854: return "0x1854 Hearing Access";
    case 0x1855: return "0x1855 TMAS";
    case 0xFEF3: return "0xFEF3 Google (member)";
    case 0xFCF1: return "0xFCF1 Google (member)";
    case 0xFD5A: return "0xFD5A Samsung (member)";
    case 0xFEAA: return "0xFEAA Eddystone";
    default: {
      char tmp[32];
      if ((u16 >= 0xFC00 && u16 <= 0xFCFF) || (u16 >= 0xFD00 && u16 <= 0xFEFF))
        snprintf(tmp,sizeof(tmp),"0x%04X",u16);
      else
        snprintf(tmp,sizeof(tmp),"0x%04X (non assegnato SIG)",u16);
      return std::string(tmp);
    }
  }
}

// ---------- Parser AD structures ----------
static void parsePayload(const uint8_t* data, size_t len, AdvParsed& out) {
  size_t i=0;
  while (i < len) {
    uint8_t L = data[i++];
    if (L == 0 || i+L > len) break;
    uint8_t type = data[i++];
    uint8_t dlen = L-1;
    const uint8_t* p = data + i;
    i += dlen;

    switch (type) {
      case 0x02: case 0x03: // 16-bit Service UUIDs
        for (uint8_t k=0; k+1 < dlen; k += 2) {
          uint16_t u16 = p[k] | (p[k+1] << 8);
          out.serviceUUIDs.push_back(uuid16to128(u16));
        }
        break;

      case 0x06: case 0x07: // 128-bit Service UUIDs
        for (uint8_t k=0; k+15 < dlen; k += 16) {
          const uint8_t* b = p + k;
          char buf[37];
          snprintf(buf, sizeof(buf),
            "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            b[15],b[14],b[13],b[12], b[11],b[10], b[9],b[8],
            b[7],b[6], b[5],b[4], b[3],b[2],b[1],b[0]);
          for (char* q = buf; *q; ++q) *q = tolower(*q);
          out.serviceUUIDs.push_back(std::string(buf));
        }
        break;

      case 0x16: { // Service Data - 16-bit
        if (dlen >= 2) {
          uint16_t u16 = p[0] | (p[1] << 8);
          out.serviceUUIDs.push_back(uuid16to128(u16));
        }
        break;
      }

      case 0x21: { // Service Data - 128-bit
        if (dlen >= 16) {
          const uint8_t* b = p;
          char buf[37];
          snprintf(buf, sizeof(buf),
            "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            b[15],b[14],b[13],b[12], b[11],b[10], b[9],b[8],
            b[7],b[6], b[5],b[4], b[3],b[2],b[1],b[0]);
          for (char* q = buf; *q; ++q) *q = tolower(*q);
          out.serviceUUIDs.push_back(std::string(buf));
        }
        break;
      }

      case 0xFF: { // Manufacturer Specific
        if (dlen >= 2) {
          uint16_t cid = p[0] | (p[1] << 8);
          out.companyIDs.push_back(cid);
        }
        break;
      }

      default: break;
    }
  }
}

// ---------- Stampa ----------
static void printAll(const NimBLEAdvertisedDevice* d, const AdvParsed& parsed) {
  std::string mac  = d->getAddress().toString();
  std::string name = d->getName();
  int rssi = d->getRSSI();

  Serial.printf("%s  RSSI=%d  type=%s  name=\"%s\"\n",
                mac.c_str(), rssi, classifyAddr(mac), name.c_str());

  if (!parsed.companyIDs.empty()) {
    Serial.print("  MSD: ");
    for (size_t i=0;i<parsed.companyIDs.size();++i) {
      uint16_t cid = parsed.companyIDs[i];
      const char* n = companyName(cid);
      if (n) Serial.printf("%s (0x%04X)%s", n, cid, (i+1<parsed.companyIDs.size())?", ":"");
      else   Serial.printf("0x%04X%s", cid, (i+1<parsed.companyIDs.size())?", ":"");
    }
    Serial.println();
  }

  if (!parsed.serviceUUIDs.empty()) {
    Serial.println("  Services:");
    for (auto& u : parsed.serviceUUIDs) {
      std::string label;
      if (u.size()==36 && u.rfind(BASE_SUFFIX) == (36 - strlen(BASE_SUFFIX))) {
        uint16_t u16 = strtol(u.substr(4,4).c_str(), nullptr, 16);
        label = humanize16(u16);
      }
      Serial.printf("    • %s", u.c_str());
      if (!label.empty()) Serial.printf("  (%s)", label.c_str());
      Serial.println();
    }
  }
  Serial.println();
}

static void printUnique(const NimBLEAdvertisedDevice* d, const AdvParsed& parsed) {
  std::string mac  = d->getAddress().toString();
  std::string name = d->getName();
  int rssi = d->getRSSI();

  Seen* s = findSeen(mac);
  if (!s) {
    Seen ns; ns.mac = mac;
    g_seen.push_back(ns);
    s = findSeen(mac);
    Serial.printf("[NEW addr] %s  RSSI=%d  type=%s  name=\"%s\"\n",
                  mac.c_str(), rssi, classifyAddr(mac), name.c_str());
  }

  if (!name.empty() && !vecHas(s->names, name)) {
    s->names.push_back(name);
    Serial.printf("[NEW name] %s  \"%s\"\n", mac.c_str(), name.c_str());
  }

  for (auto cid : parsed.companyIDs) {
    if (!vecHas16(s->cids, cid)) {
      s->cids.push_back(cid);
      const char* cn = companyName(cid);
      if (cn) Serial.printf("[NEW msd ] %s  %s (0x%04X)\n", mac.c_str(), cn, cid);
      else    Serial.printf("[NEW msd ] %s  0x%04X\n", mac.c_str(), cid);
    }
  }

  for (auto& u : parsed.serviceUUIDs) {
    if (!vecHas(s->uuids, u)) {
      s->uuids.push_back(u);
      std::string label;
      if (u.size()==36 && u.rfind(BASE_SUFFIX) == (36 - strlen(BASE_SUFFIX))) {
        uint16_t u16 = strtol(u.substr(4,4).c_str(), nullptr, 16);
        label = humanize16(u16);
      }
      Serial.printf("[NEW uuid] %s  %s", mac.c_str(), u.c_str());
      if (!label.empty()) Serial.printf("  (%s)", label.c_str());
      Serial.println();
    }
  }
}

// ---------- Setup & Loop ----------
void setup() {
  Serial.begin(115200);
  delay(200);

  NimBLEDevice::init("ESP32 BLE Scanner");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setActiveScan(ACTIVE_SCAN);
  scan->setInterval(SCAN_INTERVAL_MS);
  scan->setWindow(SCAN_WINDOW_MS);
  scan->setDuplicateFilter(FILTER_DUPLICATES);

  Serial.println(">>> Scanner pronto (polling senza callback)");
}

void loop() {
  NimBLEScan* scan = NimBLEDevice::getScan();

  // start(duration_sec, isContinue=false, restart=true)
  bool ok = scan->start(SCAN_SLICE_SEC, /*isContinue=*/false, /*restart=*/false);
  if (!ok) {
    Serial.println("! scan->start fallita");
    delay(1000);
    return;
  }

  NimBLEScanResults results = scan->getResults();
  const int count = results.getCount();
  for (int i=0; i<count; ++i) {
    const NimBLEAdvertisedDevice* dev = results.getDevice(i);

    // payload: string o vector<uint8_t> -> usa data()/size()
    auto payload = dev->getPayload();
    const uint8_t* pData = (const uint8_t*)payload.data();
    size_t pLen = payload.size();

    AdvParsed parsed;
    if (pData && pLen) parsePayload(pData, pLen, parsed);

    if (ONLY_UNIQUE_FACTS) printUnique(dev, parsed);
    else                   printAll(dev, parsed);
  }

  scan->clearResults();
  delay(50);
}
