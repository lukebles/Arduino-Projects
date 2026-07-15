#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_system.h>

// ======================================================
// FORMATI PACCHETTO
// ======================================================

#define NODE_TX1 1
#define NODE_TX2 2
#define NODE_TX3 3

#define TYPE_TWO_UINT16 1
#define TYPE_DOUBLE     2

// Canale ESP-NOW
// Deve essere uguale su TX e CENTRALE
#define ESPNOW_CHANNEL 6

struct __attribute__((packed)) PacketHeader {
  uint8_t nodeId;
  uint8_t payloadType;
  uint32_t seq;
};

struct __attribute__((packed)) PacketTwoUint16 {
  uint8_t nodeId;
  uint8_t payloadType;
  uint32_t seq;
  uint16_t value1;
  uint16_t value2;
};

struct __attribute__((packed)) PacketDouble {
  uint8_t nodeId;
  uint8_t payloadType;
  uint32_t seq;
  double value;
};

// ======================================================
// UTILITY
// ======================================================

void stampaMacBytes(const uint8_t *mac) {
  char macStr[18];

  snprintf(
    macStr,
    sizeof(macStr),
    "%02X:%02X:%02X:%02X:%02X:%02X",
    mac[0], mac[1], mac[2],
    mac[3], mac[4], mac[5]
  );

  Serial.print(macStr);
}

void stampaMacCentrale() {
  Serial.print("MAC centrale WiFi.macAddress(): ");
  Serial.println(WiFi.macAddress());

  uint8_t macSta[6];

  esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, macSta);

  if (err == ESP_OK) {
    Serial.print("MAC centrale esp_wifi_get_mac(): ");
    stampaMacBytes(macSta);
    Serial.println();
  } else {
    Serial.print("Errore esp_wifi_get_mac(): ");
    Serial.println(err);
  }
}

void stampaCanaleCorrente() {
  uint8_t primaryChannel;
  wifi_second_chan_t secondChannel;

  esp_err_t err = esp_wifi_get_channel(&primaryChannel, &secondChannel);

  Serial.print("Canale WiFi corrente: ");

  if (err == ESP_OK) {
    Serial.println(primaryChannel);
  } else {
    Serial.print("errore ");
    Serial.println(err);
  }
}

// ======================================================
// CALLBACK RICEZIONE ESP-NOW
// ======================================================

void onDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  Serial.println();
  Serial.println("======================================");

  Serial.print("Ricevuto da MAC: ");
  stampaMacBytes(recv_info->src_addr);
  Serial.println();

  Serial.print("Lunghezza pacchetto: ");
  Serial.println(len);

  if (len < (int)sizeof(PacketHeader)) {
    Serial.println("Pacchetto troppo corto");
    return;
  }

  PacketHeader header;
  memcpy(&header, incomingData, sizeof(header));

  Serial.print("Nodo ID: ");
  Serial.println(header.nodeId);

  Serial.print("Tipo payload: ");
  Serial.println(header.payloadType);

  Serial.print("Sequenza: ");
  Serial.println(header.seq);

  if (header.payloadType == TYPE_TWO_UINT16) {
    if (len != (int)sizeof(PacketTwoUint16)) {
      Serial.println("Errore: lunghezza non compatibile con PacketTwoUint16");
      return;
    }

    PacketTwoUint16 p;
    memcpy(&p, incomingData, sizeof(p));

    Serial.println("Tipo dati: due interi uint16_t");

    Serial.print("Valore 1: ");
    Serial.println(p.value1);

    Serial.print("Valore 2: ");
    Serial.println(p.value2);
  }

  else if (header.payloadType == TYPE_DOUBLE) {
    if (len != (int)sizeof(PacketDouble)) {
      Serial.println("Errore: lunghezza non compatibile con PacketDouble");
      return;
    }

    PacketDouble p;
    memcpy(&p, incomingData, sizeof(p));

    Serial.println("Tipo dati: double");

    Serial.print("Valore: ");
    Serial.println(p.value, 6);
  }

  else {
    Serial.println("Tipo payload sconosciuto");
  }
}

// ======================================================
// SETUP WIFI / CANALE
// ======================================================

bool inizializzaWiFiCanale() {
  WiFi.persistent(false);

  // Modalità station necessaria per ESP-NOW.
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  // Disconnette da eventuali reti precedenti senza spegnere il WiFi.
  WiFi.disconnect(false, true);

  delay(300);

  // Forza avvio interfaccia WiFi.
  esp_err_t startResult = esp_wifi_start();

  Serial.print("Risultato esp_wifi_start(): ");
  Serial.println(startResult);

  delay(300);

  // Imposta il canale ESP-NOW.
  // Uso promiscuous true/false perché su ESP32 spesso rende il cambio canale più affidabile.
  esp_wifi_set_promiscuous(true);
  esp_err_t chResult = esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  Serial.print("Risultato set canale: ");
  Serial.println(chResult);

  Serial.print("Canale impostato richiesto: ");
  Serial.println(ESPNOW_CHANNEL);

  stampaCanaleCorrente();

  if (chResult != ESP_OK) {
    Serial.println("Errore impostazione canale WiFi");
    return false;
  }

  return true;
}

// ======================================================
// SETUP
// ======================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("======================================");
  Serial.println("Centrale ESP-NOW - Ricevitore 3 sensori");
  Serial.println("Channel 6");
  Serial.println("======================================");

  if (!inizializzaWiFiCanale()) {
    Serial.println("Inizializzazione WiFi/canale fallita");
    while (true) {
      delay(1000);
    }
  }

  stampaMacCentrale();

  if (WiFi.macAddress() == "00:00:00:00:00:00") {
    Serial.println("ATTENZIONE: MAC ancora nullo. Controlla scheda selezionata / alimentazione / core ESP32.");
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("Errore inizializzazione ESP-NOW");
    while (true) {
      delay(1000);
    }
  }

  esp_err_t cbResult = esp_now_register_recv_cb(onDataRecv);

  Serial.print("Risultato registrazione callback RX: ");
  Serial.println(cbResult);

  if (cbResult != ESP_OK) {
    Serial.println("Errore registrazione callback RX");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("Ricevitore ESP-NOW pronto");
}

// ======================================================
// LOOP
// ======================================================

void loop() {
  delay(1000);
}