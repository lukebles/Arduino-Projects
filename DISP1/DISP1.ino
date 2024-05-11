#include <ESP8266WiFi.h>
#include <espnow.h>

#define BUTTON_PIN 0
#define LED_PIN 2

uint8_t peerAddress[] = {0x24, 0x0A, 0xC4, 0xXX, 0xXX, 0xXX}; // MAC address of DISP2
bool ledState = false;

void onDataReceive(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
    ledState = incomingData[0];
    digitalWrite(LED_PIN, ledState);
}

void sendCommand() {
    ledState = !ledState;
    uint8_t data = ledState;
    esp_now_send(peerAddress, &data, sizeof(data));
}

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != 0) {
        Serial.println("ESP-NOW initialization failed");
        return;
    }
    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
    esp_now_add_peer(peerAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
    esp_now_register_recv_cb(onDataReceive);
}

void loop() {
    if (digitalRead(BUTTON_PIN) == LOW) {
        delay(50); // debounce
        if (digitalRead(BUTTON_PIN) == LOW) {
            sendCommand();
            while (digitalRead(BUTTON_PIN) == LOW) {
                // Wait until button is released
            }
        }
    }
}
