#include <Arduino.h>
#include "esp_err.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"

// --------- Utility ------------
static char *bda2str(const uint8_t* bda, char *str, size_t size) {
  if (!bda || !str || size < 18) return nullptr;
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

      int rssi = 0x7FFF;
      const char* name_ptr = nullptr;
      uint8_t name_len = 0;
      const uint8_t* eir = nullptr;

      for (int i = 0; i < param->disc_res.num_prop; i++) {
        esp_bt_gap_dev_prop_t *p = param->disc_res.prop + i;
        switch (p->type) {
          case ESP_BT_GAP_DEV_PROP_RSSI:   rssi = *(int8_t*)p->val; break;
          case ESP_BT_GAP_DEV_PROP_BDNAME: name_ptr = (const char*)p->val; name_len = p->len; break;
          case ESP_BT_GAP_DEV_PROP_EIR:    eir = (const uint8_t*)p->val; break;
          default: break;
        }
      }

      if (!name_ptr && eir) name_ptr = get_eir_name(eir, &name_len);

      char name_buf[249] = {0};
      if (name_ptr && name_len) {
        size_t copy_len = min((size_t)name_len, sizeof(name_buf) - 1);
        memcpy(name_buf, name_ptr, copy_len);
      } else {
        strcpy(name_buf, "(sconosciuto)");
      }

      if (rssi != 0x7FFF)
        Serial.printf("[BT] MAC: %s | Nome: %s | RSSI: %d dBm\n", mac, name_buf, rssi);
      else
        Serial.printf("[BT] MAC: %s | Nome: %s | RSSI: n/d\n", mac, name_buf);

      break;
    }

    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
      if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
        esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
      }
      break;

    default:
      break;
  }
}

static void printErr(const char* what, esp_err_t err) {
  Serial.printf("ERRORE: %s -> %s (0x%X)\n", what, esp_err_to_name(err), (unsigned)err);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== ESP32 Classic BT Scanner: MAC | Nome | RSSI ===");

  // IMPORTANTISSIMO: libera la memoria BLE se vuoi usare solo Classic
  // (se non la liberi, esp_bt_controller_init può fallire per NO_MEM)
  esp_err_t e = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
  if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) { // INVALID_STATE = già rilasciata
    printErr("esp_bt_controller_mem_release(BLE)", e);
  }

  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

  e = esp_bt_controller_init(&bt_cfg);
  if (e != ESP_OK) { printErr("esp_bt_controller_init", e); return; }

  e = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
  if (e != ESP_OK) { printErr("esp_bt_controller_enable(CLASSIC)", e); return; }

  e = esp_bluedroid_init();
  if (e != ESP_OK) { printErr("esp_bluedroid_init", e); return; }

  e = esp_bluedroid_enable();
  if (e != ESP_OK) { printErr("esp_bluedroid_enable", e); return; }

  e = esp_bt_gap_register_callback(gap_cb);
  if (e != ESP_OK) { printErr("esp_bt_gap_register_callback", e); return; }

  esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

  e = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
  if (e == ESP_OK) Serial.println("Scansione avviata… (riavvio automatico ogni 10s)");
  else printErr("esp_bt_gap_start_discovery", e);
}

void loop() {}
