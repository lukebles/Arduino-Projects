#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEEddystoneURL.h>
#include <BLEEddystoneTLM.h>
#include <BLEBeacon.h>

int scanTime = 5;  // In seconds
BLEScan *pBLEScan;

static inline bool startsWith(const uint8_t* d, size_t len, const uint8_t* pfx, size_t pfxlen) {
  return (len >= pfxlen) && (memcmp(d, pfx, pfxlen) == 0);
}

static bool shouldSkipManufacturerFrame(const uint8_t* d, size_t len) {
  // Escludi 4C 00 12 02 00 01/02/03 (len=6)
  if (len == 6 && d[0] == 0x4C && d[1] == 0x00 && d[2] == 0x12 && d[3] == 0x02 && d[4] == 0x00) {
    return true;
  }

    // 
  if (len == 11 && d[0] == 0x4C && d[1] == 0x00 && d[2] == 0x10 && d[3] == 0x07 && d[4] == 0x1C) {
    return true;
  }

  if (len == 26 && d[0] == 0x75 && d[1] == 0x00 && d[2] == 0x42 && d[3] == 0x04 && d[4] == 0x01) {
    return true;
  }


  if (len == 10 && d[0] == 0x4C && d[1] == 0x00 && d[2] == 0x10 && d[3] == 0x06 ) {
    return true;
  }

  if (len == 11 && d[0] == 0x4C && d[1] == 0x00 && d[2] == 0x10 && d[3] == 0x07 && d[4] == 0x3E) {
    return true;
  }

  // Escludi 4C 00 09 ... (len=22)
  if (d[0] == 0x4C && d[1] == 0x00 ) {
    return true;
  }
  // Escludi 4C 00 0C ... (len=26)
  if (len == 26 && d[0] == 0x4C && d[1] == 0x00 && d[2] == 0x0C) {
    return true;
  }
  // Escludi esattamente 87 00 0C D2 (len=4)
  {
    static const uint8_t pat_87[] = {0x87, 0x00, 0x0C, 0xD2};
    if (len == sizeof(pat_87) && startsWith(d, len, pat_87, sizeof(pat_87))) {
      return true;
    }
  }

  return false;
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // Manufacturer Data: prima cosa, filtra
    if (advertisedDevice.haveManufacturerData()) {
      String md = advertisedDevice.getManufacturerData();
      const uint8_t* d = reinterpret_cast<const uint8_t*>(md.c_str());
      size_t len = md.length();

      if (shouldSkipManufacturerFrame(d, len)) {
        return; // silenzio su questi frame
      }

      // iBeacon Apple (4C 00, len=25)
      if (len == 25 && d[0] == 0x4C && d[1] == 0x00) {
        BLEBeacon oBeacon;
        oBeacon.setData(md); // BLEBeacon::setData(String)
        Serial.println("Found an iBeacon!");
        Serial.printf("iBeacon Frame\n");
        Serial.printf(
          "ID: %04X Major: %d Minor: %d UUID: %s Power: %d\n",
          oBeacon.getManufacturerId(),
          ENDIAN_CHANGE_U16(oBeacon.getMajor()),
          ENDIAN_CHANGE_U16(oBeacon.getMinor()),
          oBeacon.getProximityUUID().toString().c_str(),
          oBeacon.getSignalPower()
        );
        return;
      }

      // Altri Manufacturer Data ammessi: stampa compatta
      Serial.println("Found another manufacturers beacon!");
      Serial.printf("strManufacturerData: %d ", (int)len);
      for (size_t i = 0; i < len; i++) Serial.printf("[%X]", d[i]);
      Serial.printf("\n");
    }

    if (advertisedDevice.haveName()) {
      Serial.print("Device name: ");
      Serial.println(advertisedDevice.getName().c_str());
      Serial.println();
    }

    if (advertisedDevice.haveServiceUUID()) {
      BLEUUID devUUID = advertisedDevice.getServiceUUID();
      Serial.print("Found ServiceUUID: ");
      Serial.println(devUUID.toString().c_str());
      Serial.println();
    }

    if (advertisedDevice.getFrameType() == BLE_EDDYSTONE_URL_FRAME) {
      Serial.println("Found an EddystoneURL beacon!");
      BLEEddystoneURL EddystoneURL(&advertisedDevice);
      Serial.printf("URL bytes: 0x");
      String url = EddystoneURL.getURL();
      for (auto ch : url) Serial.printf("%02X", (uint8_t)ch);
      Serial.printf("\n");
      Serial.printf("Decoded URL: %s\n", EddystoneURL.getDecodedURL().c_str());
      Serial.printf("EddystoneURL.getDecodedURL(): %s\n", EddystoneURL.getDecodedURL().c_str());
      Serial.printf("TX power %d (Raw 0x%02X)\n", EddystoneURL.getPower(), EddystoneURL.getPower());
      Serial.println();
    }

    if (advertisedDevice.getFrameType() == BLE_EDDYSTONE_TLM_FRAME) {
      Serial.println("Found an EddystoneTLM beacon!");
      BLEEddystoneTLM EddystoneTLM(&advertisedDevice);
      Serial.printf("Reported battery voltage: %dmV\n", EddystoneTLM.getVolt());
      Serial.printf("Reported temperature: %.2f°C (raw data=0x%04X)\n", EddystoneTLM.getTemp(), EddystoneTLM.getRawTemp());
      Serial.printf("Reported advertise count: %lu\n", EddystoneTLM.getCount());
      Serial.printf("Reported time since last reboot: %lus\n", EddystoneTLM.getTime());
      Serial.println();
      Serial.print(EddystoneTLM.toString().c_str());
      Serial.println();
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Scanning...");

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
}

void loop() {
  BLEScanResults *foundDevices = pBLEScan->start(scanTime, false);
  Serial.print("Devices found: ");
  Serial.println(foundDevices->getCount());
  Serial.println("Scan done!");
  pBLEScan->clearResults();
  delay(2000);
}
