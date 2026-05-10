#include <Arduino.h>
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_err.h"

volatile bool richiestaNuovaScansione = false;
volatile bool scansioneInCorso = false;

void avviaScansione();

void bt_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT: {
      Serial.println("---- Dispositivo trovato ----");

      char mac[18];
      snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
               param->disc_res.bda[0], param->disc_res.bda[1],
               param->disc_res.bda[2], param->disc_res.bda[3],
               param->disc_res.bda[4], param->disc_res.bda[5]);
      Serial.print("MAC: ");
      Serial.println(mac);

      bool nomeStampato = false;

      for (int i = 0; i < param->disc_res.num_prop; i++) {
        esp_bt_gap_dev_prop_t *p = &param->disc_res.prop[i];

        if (p->type == ESP_BT_GAP_DEV_PROP_RSSI) {
          int8_t rssi = *(int8_t *)(p->val);
          Serial.print("RSSI: ");
          Serial.println(rssi);
        }

        if (p->type == ESP_BT_GAP_DEV_PROP_EIR) {
          uint8_t len = 0;

          uint8_t *name = esp_bt_gap_resolve_eir_data(
            (uint8_t *)p->val,
            ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME,
            &len
          );

          if (!name) {
            name = esp_bt_gap_resolve_eir_data(
              (uint8_t *)p->val,
              ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME,
              &len
            );
          }

          if (name && len > 0) {
            char nome[249];
            size_t n = len;
            if (n > sizeof(nome) - 1) n = sizeof(nome) - 1;
            memcpy(nome, name, n);
            nome[n] = '\0';

            Serial.print("Nome: ");
            Serial.println(nome);
            nomeStampato = true;
          }
        }
      }

      if (!nomeStampato) {
        Serial.println("Nome: (non disponibile)");
      }

      Serial.println("-----------------------------");
      break;
    }

    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
      if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
        scansioneInCorso = true;
        Serial.println("Scansione avviata.");
      } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
        scansioneInCorso = false;
        Serial.println("Scansione terminata.");
        richiestaNuovaScansione = true;
      }
      break;

    default:
      break;
  }
}

void avviaScansione() {
  esp_err_t err = esp_bt_gap_start_discovery(
    ESP_BT_INQ_MODE_GENERAL_INQUIRY,
    10,   // 10 * 1.28 s = circa 12.8 secondi
    0     // 0 = numero risposte illimitato
  );

  if (err != ESP_OK) {
    Serial.print("Errore start discovery: ");
    Serial.println((int)err);
  } else {
    Serial.println("Richiesta nuova scansione...");
  }
}

void setup() {
  Serial.begin(9600);
  delay(2000);

  Serial.println();
  Serial.println("Avvio scanner Bluetooth Classic continuo...");

  if (!btStart()) {
    Serial.println("Errore: btStart() fallita");
    return;
  }

  esp_bluedroid_status_t stato = esp_bluedroid_get_status();

  if (stato == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
    esp_err_t err = esp_bluedroid_init();
    if (err != ESP_OK) {
      Serial.print("Errore esp_bluedroid_init(): ");
      Serial.println((int)err);
      return;
    }
  }

  stato = esp_bluedroid_get_status();
  if (stato != ESP_BLUEDROID_STATUS_ENABLED) {
    esp_err_t err = esp_bluedroid_enable();
    if (err != ESP_OK) {
      Serial.print("Errore esp_bluedroid_enable(): ");
      Serial.println((int)err);
      return;
    }
  }

  esp_err_t err = esp_bt_gap_register_callback(bt_callback);
  if (err != ESP_OK) {
    Serial.print("Errore callback GAP: ");
    Serial.println((int)err);
    return;
  }

  err = esp_bt_dev_set_device_name("ESP32_SCANNER");
  if (err != ESP_OK) {
    Serial.print("Errore set nome dispositivo: ");
    Serial.println((int)err);
    return;
  }

  Serial.println("Bluetooth Classic pronto.");
  avviaScansione();
}

void loop() {
  if (richiestaNuovaScansione && !scansioneInCorso) {
    richiestaNuovaScansione = false;
    delay(500);
    avviaScansione();
  }

  delay(50);
}