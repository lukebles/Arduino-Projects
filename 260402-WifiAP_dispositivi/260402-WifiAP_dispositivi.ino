#include <WiFi.h>
#include <esp_wifi.h>

#define SERIAL_BAUD               9600
#define MAX_APS                   80
#define MAX_CLIENTS               250
#define AP_SCAN_INTERVAL_MS       20000UL
#define PRINT_INTERVAL_MS         15000UL
#define CHANNEL_HOP_INTERVAL_MS     700UL

#define WIFI_MIN_CHANNEL 1
#define WIFI_MAX_CHANNEL 13

struct APRecord {
  bool used;
  String ssid;
  uint8_t bssid[6];
  int32_t rssi;
  int32_t channel;
  uint8_t enc;
};

struct ClientRecord {
  bool used;
  uint8_t mac[6];
  int32_t bestRssi;
  int32_t lastRssi;
  int32_t channel;
  uint32_t frames;
  uint32_t lastSeenMs;
  bool hasAp;
  uint8_t apBssid[6];
};

struct ChannelStats {
  bool seen;
  int32_t maxRssi;
  uint16_t apCount;
  uint16_t clientCount;
};

APRecord aps[MAX_APS];
ClientRecord clients[MAX_CLIENTS];
ChannelStats channelStats[WIFI_MAX_CHANNEL + 1];

unsigned long lastApScanMs = 0;
unsigned long lastPrintMs = 0;
unsigned long lastHopMs = 0;
uint8_t currentChannel = 1;
bool promiscuousEnabled = false;

String macToString(const uint8_t *mac) {
  char buf[18];
  snprintf(buf, sizeof(buf),
           "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

bool macEquals(const uint8_t *a, const uint8_t *b) {
  for (int i = 0; i < 6; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

bool isBroadcastOrMulticast(const uint8_t *mac) {
  if ((mac[0] & 0x01) != 0) return true;

  bool allFF = true;
  for (int i = 0; i < 6; i++) {
    if (mac[i] != 0xFF) {
      allFF = false;
      break;
    }
  }
  return allFF;
}

void parseMacString(const char *s, uint8_t out[6]) {
  unsigned int b[6];
  int n = sscanf(s, "%2X:%2X:%2X:%2X:%2X:%2X",
                 &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
  if (n == 6) {
    for (int i = 0; i < 6; i++) out[i] = (uint8_t)b[i];
  } else {
    memset(out, 0, 6);
  }
}

bool macEqualsString(const uint8_t *mac, const char *macStr) {
  uint8_t tmp[6];
  parseMacString(macStr, tmp);
  return macEquals(mac, tmp);
}

String getHardcodedBrandModel(const uint8_t *mac) {
  if (macEqualsString(mac, "B0:59:47:2F:F7:03")) return "Shenzhen Qihu Intelligent Technology Company Limited";
  if (macEqualsString(mac, "3E:78:5F:84:2D:95")) return "";
  if (macEqualsString(mac, "30:32:35:A4:DB:34")) return "Qingdao Intelligent&Precise Electronics Co., Ltd.";

  if (macEqualsString(mac, "00:4B:12:9A:25:30")) return "Espressif Inc.";
  if (macEqualsString(mac, "AA:42:A1:07:2C:81")) return "";

  if (macEqualsString(mac, "DC:CD:2F:B0:7D:BD")) return "Seiko Epson Corporation";
  if (macEqualsString(mac, "EC:FA:BC:37:99:F3")) return "Espressif Inc.";
  if (macEqualsString(mac, "8C:FA:BA:91:D1:80")) return "";
  if (macEqualsString(mac, "D4:9C:DD:AC:E8:D4")) return "";
  if (macEqualsString(mac, "C8:12:0B:35:61:E4")) return "";

  return "";
}

String encTypeToStr(uint8_t enc) {
  switch ((wifi_auth_mode_t)enc) {
    case WIFI_AUTH_OPEN: return "OPEN";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
    case WIFI_AUTH_WAPI_PSK: return "WAPI";
    default: return "UNKNOWN";
  }
}

void clearAPs() {
  for (int i = 0; i < MAX_APS; i++) {
    aps[i].used = false;
    aps[i].ssid = "";
    aps[i].rssi = 0;
    aps[i].channel = 0;
    aps[i].enc = 0;
    memset(aps[i].bssid, 0, 6);
  }
}

void clearClients() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    clients[i].used = false;
    memset(clients[i].mac, 0, 6);
    clients[i].bestRssi = -127;
    clients[i].lastRssi = -127;
    clients[i].channel = 0;
    clients[i].frames = 0;
    clients[i].lastSeenMs = 0;
    clients[i].hasAp = false;
    memset(clients[i].apBssid, 0, 6);
  }
}

void clearChannelStats() {
  for (int ch = WIFI_MIN_CHANNEL; ch <= WIFI_MAX_CHANNEL; ch++) {
    channelStats[ch].seen = false;
    channelStats[ch].maxRssi = -127;
    channelStats[ch].apCount = 0;
    channelStats[ch].clientCount = 0;
  }
}

int findAPByBssid(const uint8_t *bssid) {
  for (int i = 0; i < MAX_APS; i++) {
    if (aps[i].used && macEquals(aps[i].bssid, bssid)) return i;
  }
  return -1;
}

int findFreeClientSlot() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (!clients[i].used) return i;
  }
  return -1;
}

int findClientByMac(const uint8_t *mac) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].used && macEquals(clients[i].mac, mac)) return i;
  }
  return -1;
}

void addOrUpdateClient(const uint8_t *clientMac, int32_t rssi, int32_t channel,
                       const uint8_t *apBssid, bool hasAp) {
  if (!clientMac) return;
  if (isBroadcastOrMulticast(clientMac)) return;

  int idx = findClientByMac(clientMac);
  if (idx < 0) {
    idx = findFreeClientSlot();
    if (idx < 0) return;

    clients[idx].used = true;
    memcpy(clients[idx].mac, clientMac, 6);
    clients[idx].bestRssi = rssi;
    clients[idx].lastRssi = rssi;
    clients[idx].channel = channel;
    clients[idx].frames = 1;
    clients[idx].lastSeenMs = millis();
    clients[idx].hasAp = false;
    memset(clients[idx].apBssid, 0, 6);

    if (hasAp && apBssid) {
      clients[idx].hasAp = true;
      memcpy(clients[idx].apBssid, apBssid, 6);
    }
  } else {
    clients[idx].frames++;
    clients[idx].lastRssi = rssi;
    if (rssi > clients[idx].bestRssi) clients[idx].bestRssi = rssi;
    clients[idx].channel = channel;
    clients[idx].lastSeenMs = millis();

    if (hasAp && apBssid) {
      clients[idx].hasAp = true;
      memcpy(clients[idx].apBssid, apBssid, 6);
    }
  }
}

void disablePromiscuous() {
  if (!promiscuousEnabled) return;
  esp_wifi_set_promiscuous(false);
  promiscuousEnabled = false;
}

void enablePromiscuous() {
  wifi_promiscuous_filter_t filt;
  filt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous(true);
  promiscuousEnabled = true;
}

void scanAPs() {
  disablePromiscuous();
  clearAPs();

  Serial.println();
  Serial.println("=== Scansione AP WiFi in corso ===");

  int n = WiFi.scanNetworks(false, true);

  if (n > 0) {
    if (n > MAX_APS) n = MAX_APS;

    for (int i = 0; i < n; i++) {
      String ssid;
      uint8_t enc;
      int32_t rssi;
      uint8_t *bssid;
      int32_t channel;

      if (WiFi.getNetworkInfo(i, ssid, enc, rssi, bssid, channel)) {
        aps[i].used = true;
        aps[i].ssid = ssid;
        aps[i].rssi = rssi;
        aps[i].channel = channel;
        aps[i].enc = enc;
        memcpy(aps[i].bssid, bssid, 6);
      }
    }
  }

  WiFi.scanDelete();

  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
  enablePromiscuous();
}

void IRAM_ATTR snifferCallback(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA) return;

  const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
  const uint8_t *payload = ppkt->payload;
  const wifi_pkt_rx_ctrl_t &rx = ppkt->rx_ctrl;

  int32_t rssi = rx.rssi;
  int32_t channel = rx.channel;

  uint16_t fc = payload[0] | (payload[1] << 8);
  uint8_t frameType = (fc >> 2) & 0x03;
  bool toDS   = (fc >> 8) & 0x01;
  bool fromDS = (fc >> 9) & 0x01;

  const uint8_t *addr1 = payload + 4;
  const uint8_t *addr2 = payload + 10;
  const uint8_t *addr3 = payload + 16;

  if (frameType == 2) {
    if (toDS && !fromDS) {
      addOrUpdateClient(addr2, rssi, channel, addr1, true);
    } else if (!toDS && fromDS) {
      addOrUpdateClient(addr1, rssi, channel, addr2, true);
    } else if (!toDS && !fromDS) {
      addOrUpdateClient(addr2, rssi, channel, addr3, true);
    }
  } else if (frameType == 0) {
    uint8_t subtype = (fc >> 4) & 0x0F;

    if (subtype == 4) {
      addOrUpdateClient(addr2, rssi, channel, nullptr, false);
    }
  }
}

void buildChannelStats() {
  clearChannelStats();

  for (int i = 0; i < MAX_APS; i++) {
    if (!aps[i].used) continue;
    int ch = aps[i].channel;
    if (ch < WIFI_MIN_CHANNEL || ch > WIFI_MAX_CHANNEL) continue;

    channelStats[ch].seen = true;
    channelStats[ch].apCount++;

    if (aps[i].rssi > channelStats[ch].maxRssi) {
      channelStats[ch].maxRssi = aps[i].rssi;
    }
  }

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (!clients[i].used) continue;
    int ch = clients[i].channel;
    if (ch < WIFI_MIN_CHANNEL || ch > WIFI_MAX_CHANNEL) continue;

    channelStats[ch].seen = true;
    channelStats[ch].clientCount++;

    if (clients[i].bestRssi > channelStats[ch].maxRssi) {
      channelStats[ch].maxRssi = clients[i].bestRssi;
    }
  }
}

void printChannelSummary() {
  buildChannelStats();

  Serial.println();
  Serial.println("========== RESOCONTO CANALI ==========");

  Serial.println("Canali occupati:");
  bool anyBusy = false;
  for (int ch = WIFI_MIN_CHANNEL; ch <= WIFI_MAX_CHANNEL; ch++) {
    if (!channelStats[ch].seen) continue;

    anyBusy = true;
    Serial.print("  CH ");
    Serial.print(ch);
    Serial.print(" -> max RSSI=");
    Serial.print(channelStats[ch].maxRssi);
    Serial.print(" dBm, dispositivi=");
    Serial.print((int)channelStats[ch].apCount + (int)channelStats[ch].clientCount);
    Serial.print(" (AP=");
    Serial.print(channelStats[ch].apCount);
    Serial.print(", client=");
    Serial.print(channelStats[ch].clientCount);
    Serial.println(")");
  }
  if (!anyBusy) {
    Serial.println("  Nessuno");
  }

  Serial.println("Canali liberi:");
  bool anyFree = false;
  for (int ch = WIFI_MIN_CHANNEL; ch <= WIFI_MAX_CHANNEL; ch++) {
    if (channelStats[ch].seen) continue;

    anyFree = true;
    Serial.print("  CH ");
    Serial.println(ch);
  }
  if (!anyFree) {
    Serial.println("  Nessuno");
  }

  Serial.println("======================================");
}

void printAPWithClients() {
  Serial.println();
  Serial.println("==============================================");
  Serial.println("Reti WiFi e client rilevati");
  Serial.println("==============================================");

  for (int i = 0; i < MAX_APS; i++) {
    if (!aps[i].used) continue;

    String ssid = aps[i].ssid.length() ? aps[i].ssid : String("<hidden>");
    String apMac = macToString(aps[i].bssid);

    Serial.print(ssid);
    Serial.print(" (");
    Serial.print(apMac);
    Serial.print(") (CH ");
    Serial.print(aps[i].channel);
    Serial.print(") (RSSI ");
    Serial.print(aps[i].rssi);
    Serial.println(")");

    bool foundAny = false;

    for (int j = 0; j < MAX_CLIENTS; j++) {
      if (!clients[j].used) continue;
      if (!clients[j].hasAp) continue;
      if (!macEquals(clients[j].apBssid, aps[i].bssid)) continue;

      foundAny = true;

      String clientMac = macToString(clients[j].mac);
      String brandModel = getHardcodedBrandModel(clients[j].mac);

      Serial.print("    ");
      Serial.print(clientMac);

      if (brandModel.length() > 0) {
        Serial.print(" (");
        Serial.print(brandModel);
        Serial.print(")");
      }

      Serial.print("  RSSI=");
      Serial.print(clients[j].bestRssi);
      Serial.print("  Frames=");
      Serial.println(clients[j].frames);
    }

    if (!foundAny) {
      Serial.println("    ");
    }
  }

  Serial.println();
  Serial.println("Client senza AP dedotto");
  Serial.println("----------------------------------------------");

  bool anyUnknown = false;

  for (int j = 0; j < MAX_CLIENTS; j++) {
    if (!clients[j].used) continue;
    if (clients[j].hasAp) continue;

    anyUnknown = true;

    String clientMac = macToString(clients[j].mac);
    String brandModel = getHardcodedBrandModel(clients[j].mac);

    Serial.print("    ");
    Serial.print(clientMac);

    if (brandModel.length() > 0) {
      Serial.print(" (");
      Serial.print(brandModel);
      Serial.print(")");
    }

    Serial.print("  CH=");
    Serial.print(clients[j].channel);
    Serial.print("  RSSI=");
    Serial.print(clients[j].bestRssi);
    Serial.print("  Frames=");
    Serial.println(clients[j].frames);
  }

  if (!anyUnknown) {
    Serial.println("    Nessuno");
  }

  printChannelSummary();
  Serial.println("==============================================");
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1000);

  clearAPs();
  clearClients();
  clearChannelStats();

  Serial.println();
  Serial.println("ESP32 Devkit V1 - Scanner WiFi raggruppato");
  Serial.println("Baud seriale: 9600");

  WiFi.mode(WIFI_STA);
  delay(200);

  esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous_rx_cb(&snifferCallback);

  scanAPs();

  lastApScanMs = millis();
  lastPrintMs = millis();
  lastHopMs = millis();
}

void loop() {
  unsigned long now = millis();

  if (now - lastHopMs >= CHANNEL_HOP_INTERVAL_MS) {
    lastHopMs = now;
    currentChannel++;
    if (currentChannel > WIFI_MAX_CHANNEL) currentChannel = WIFI_MIN_CHANNEL;
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
  }

  if (now - lastApScanMs >= AP_SCAN_INTERVAL_MS) {
    lastApScanMs = now;
    scanAPs();
  }

  if (now - lastPrintMs >= PRINT_INTERVAL_MS) {
    lastPrintMs = now;
    printAPWithClients();
  }

  delay(20);
}