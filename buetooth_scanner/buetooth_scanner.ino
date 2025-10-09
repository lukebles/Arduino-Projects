/*
 * ESP32 Classic Bluetooth Scanner – MAC, Nome, RSSI
 * Testato con core Arduino-ESP32.
 * Serial monitor a 115200 bps.
 */

#include <Arduino.h>
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"

// --------- Utility ------------
static char *bda2str(const uint8_t* bda, char *str, size_t size) {
  if (bda == nullptr || str == nullptr || size < 18) return nullptr;
  snprintf(str, size, "%02X:%02X:%02X:%02X:%02X:%02X",
           bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
  return str;
}

static const char* get_eir_name(const uint8_t* eir, uint8_t *len) {
  if (!eir) return nullptr;
  const char* name = (const char*)esp_bt_gap_resolve_eir_data(
      (uint8_t*)eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, len);
  if (name && *len) return name;

  name = (const char*)esp_bt_gap_resolve_eir_data(
      (uint8_t*)eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, len);
  if (name && *len) return name;

  return nullptr;
}

// --------- GAP Callback ------------
static void gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT: {
      char mac[18]; bda2str(param->disc_res.bda, mac, sizeof(mac));

      int rssi = 0x7FFF;          // valore “non disponibile”
      const char* name_ptr = nullptr;
      uint8_t name_len = 0;
      const uint8_t* eir = nullptr;

      // Leggi le proprietà scoperte
      for (int i = 0; i < param->disc_res.num_prop; i++) {
        esp_bt_gap_dev_prop_t *p = param->disc_res.prop + i;
        switch (p->type) {
          case ESP_BT_GAP_DEV_PROP_RSSI:
            rssi = *(int8_t*)p->val; // dBm
            break;

          case ESP_BT_GAP_DEV_PROP_BDNAME:
            name_ptr = (const char*)p->val;
            name_len = p->len;
            break;

          case ESP_BT_GAP_DEV_PROP_EIR:
            eir = (const uint8_t*)p->val;
            break;

          default:
            break;
        }
      }

      if (!name_ptr && eir) {
        // prova a estrarre il nome dall’EIR
        name_ptr = get_eir_name(eir, &name_len);
      }

      // Stampa risultato formattato
      if (name_ptr && name_len) {
        // Assicura stringa terminata
        char name_buf[249];
        size_t copy_len = min((size_t)name_len, sizeof(name_buf) - 1);
        memcpy(name_buf, name_ptr, copy_len);
        name_buf[copy_len] = '\0';

        if (rssi != 0x7FFF) {
          Serial.printf("[BT] MAC: %s | Nome: %s | RSSI: %d dBm\n", mac, name_buf, rssi);
        } else {
          Serial.printf("[BT] MAC: %s | Nome: %s | RSSI: n/d\n", mac, name_buf);
        }
      } else {
        if (rssi != 0x7FFF) {
          Serial.printf("[BT] MAC: %s | Nome: (sconosciuto) | RSSI: %d dBm\n", mac, rssi);
        } else {
          Serial.printf("[BT] MAC: %s | Nome: (sconosciuto) | RSSI: n/d\n", mac);
        }
      }
      break;
    }

    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT: {
      // Quando termina, riavvia la discovery per restare sempre in scan
      if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
        // durata 10 secondi; modalità “general inquiry”, interlaced=0
        esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
      }
      break;
    }

    default:
      break;
  }
}

// --------- Setup / Loop ------------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== ESP32 Classic BT Scanner: MAC | Nome | RSSI ===");

  // Inizializza controller in modalità Classic BT
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
    Serial.println("ERRORE: esp_bt_controller_init");
    return;
  }
  if (esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) != ESP_OK) {
    Serial.println("ERRORE: esp_bt_controller_enable");
    return;
  }

  if (esp_bluedroid_init() != ESP_OK) {
    Serial.println("ERRORE: esp_bluedroid_init");
    return;
  }
  if (esp_bluedroid_enable() != ESP_OK) {
    Serial.println("ERRORE: esp_bluedroid_enable");
    return;
  }

  // Registra callback GAP e avvia discovery
  if (esp_bt_gap_register_callback(gap_cb) != ESP_OK) {
    Serial.println("ERRORE: esp_bt_gap_register_callback");
    return;
  }

  // Chiedi che l’inquiry riporti RSSI quando possibile
  esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

  // Avvio prima scansione
  if (esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0) == ESP_OK) {
    Serial.println("Scansione avviata… (riavvio automatico ogni 10s)");
  } else {
    Serial.println("ERRORE: esp_bt_gap_start_discovery");
  }
}

void loop() {
  // tutto gestito via callback; loop vuoto
}
