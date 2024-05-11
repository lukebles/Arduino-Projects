#include <ESP8266WiFi.h>
#include <espnow.h>

#define LED_PIN 2

bool ledState = false;

void onDataReceive(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
    uint8_t data = ledState;
    esp_now_send(mac, &data, sizeof(data));
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != 0) {
        Serial.println("ESP-NOW initialization failed");
        return;
    }
    esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
    esp_now_register_recv_cb(onDataReceive);
}

void loop() {
    // No actions needed in loop for DISP2
}
