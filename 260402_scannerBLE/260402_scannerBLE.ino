#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

BLEScan* pBLEScan;

const int MAX_MAC_MEMORIZZATI = 400;
String macVisti[MAX_MAC_MEMORIZZATI];
int numeroMacVisti = 0;

bool macGiaVisto(const String& mac) {
  for (int i = 0; i < numeroMacVisti; i++) {
    if (macVisti[i] == mac) return true;
  }
  return false;
}

void aggiungiMac(const String& mac) {
  if (numeroMacVisti < MAX_MAC_MEMORIZZATI) {
    macVisti[numeroMacVisti++] = mac;
  }
}

String bytesToHexCompatto(const String& s) {
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t b = (uint8_t)s[i];
    if (b < 16) out += "0";
    out += String(b, HEX);
  }
  out.toUpperCase();
  return out;
}

String classificaRandomDaMac(const String& mac) {
  if (mac.length() < 2) return "RANDOM(?)";

  int primoByte = strtol(mac.substring(0, 2).c_str(), NULL, 16);
  int dueBitAlti = (primoByte >> 6) & 0x03;

  if (dueBitAlti == 0x03) return "RANDOM_STATIC";
  if (dueBitAlti == 0x01) return "RESOLVABLE_PRIVATE";
  if (dueBitAlti == 0x00) return "NON_RESOLVABLE_PRIVATE";

  return "RANDOM";
}

String tipoIndirizzo(BLEAdvertisedDevice& d, const String& mac) {
  uint8_t t = d.getAddressType();
  if (t == 0) return "PUBLIC";
  if (t == 1) return classificaRandomDaMac(mac);
  return "UNK";
}

String getBrandFromMFG(const String& mfg) {
  if (mfg.length() < 4) return "Unknown";

  String id = mfg.substring(0, 4);

  if (id == "4C00") return "Apple";
  if (id == "7500") return "Samsung";
  if (id == "4000") return "Epson";

  return "Other";
}

String advTypeToString(uint8_t t) {
  switch (t) {
    case 0x00: return "ADV_IND";
    case 0x01: return "ADV_DIR";
    case 0x02: return "ADV_SCAN";
    case 0x03: return "ADV_NONCONN";
    case 0x04: return "SCAN_RSP";
    default:   return "UNK";
  }
}

// ===== Nuove funzioni per il codice compatto =====

String codiceTipoCompatto(const String& tipo) {
  if (tipo == "RESOLVABLE_PRIVATE") return "R";
  if (tipo == "NON_RESOLVABLE_PRIVATE") return "N";
  if (tipo == "RANDOM_STATIC") return "S";
  if (tipo == "PUBLIC") return "P";
  return "U";
}

String codiceMFGCompatto(const String& mfg) {
  if (mfg.length() >= 4) {
    return mfg.substring(mfg.length() - 4);
  }
  return "----";
}

String codiceADVCompatto(uint8_t t) {
  switch (t) {
    case 0x00: return "A"; // ADV_IND
    case 0x01: return "D"; // ADV_DIRECT_IND
    case 0x02: return "S"; // ADV_SCAN_IND
    case 0x03: return "C"; // ADV_NONCONN_IND
    case 0x04: return "R"; // SCAN_RSP
    default:   return "U";
  }
}

String creaCodiceCompatto(const String& tipo, const String& mfg, BLEAdvertisedDevice& d) {
  String cTipo = codiceTipoCompatto(tipo);
  String cMfg  = codiceMFGCompatto(mfg);
  String cAdv  = codiceADVCompatto(d.getAdvType());
  String cConn = d.isConnectable() ? "Y" : "N";
  String cScan = d.isScannable() ? "Y" : "N";

  return cTipo + "_" + cMfg + "_" + cAdv + "_" + cConn + cScan;
}

class MioCallback : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) override {
    String mac = d.getAddress().toString().c_str();

    if (macGiaVisto(mac)) return;
    aggiungiMac(mac);

    String nome = "-";
    if (d.haveName()) {
      nome = d.getName().c_str();
      if (nome.length() == 0) nome = "-";
    }

    String mfg = "-";
    if (d.haveManufacturerData()) {
      mfg = bytesToHexCompatto(d.getManufacturerData());
    }

    String tipo = tipoIndirizzo(d, mac);
    String brand = getBrandFromMFG(mfg);
    String codiceCompatto = creaCodiceCompatto(tipo, mfg, d);

    Serial.print("Brand=");
    Serial.print(brand);
    Serial.print(" | Codice=");
    Serial.print(codiceCompatto);
    Serial.print(" | MAC=");
    Serial.print(mac);
    Serial.print(" | Tipo=");
    Serial.print(tipo);
    Serial.print(" | RSSI=");
    Serial.print(d.getRSSI());
    Serial.print(" | Nome=");
    Serial.print(nome);
    Serial.print(" | MFG=");
    Serial.print(mfg);
    Serial.print(" | SRV=");
    Serial.print(d.haveServiceUUID() ? d.getServiceUUID().toString().c_str() : "-");
    Serial.print(" | ADV=");
    Serial.print(advTypeToString(d.getAdvType()));
    Serial.print(" | Conn=");
    Serial.print(d.isConnectable() ? "Y" : "N");
    Serial.print(" | Scan=");
    Serial.println(d.isScannable() ? "Y" : "N");
  }
};

void setup() {
  Serial.begin(9600);
  delay(2000);

  Serial.println("Scanner BLE con BRAND e codice compatto");
  BLEDevice::init("");

  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MioCallback());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
}

void loop() {
  pBLEScan->start(5, false);
  pBLEScan->clearResults();
  delay(800);
}