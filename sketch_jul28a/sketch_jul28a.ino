#include <Wire.h>
#include <Adafruit_MCP23X17.h>

Adafruit_MCP23X17 mcp[3];
const uint8_t indirizzi[3] = {0x24, 0x26, 0x27};

void setup() {
  Serial.begin(115200);
  Wire.begin();
  for (int i = 0; i < 3; i++) {
    mcp[i].begin_I2C(indirizzi[i]);
    for (uint8_t pin = 0; pin < 16; pin++) {
      mcp[i].pinMode(pin, INPUT_PULLUP);
      //mcp[i].pullUp(pin, HIGH);  // abilitare se servono pull-up
    }
  }
}

void loop() {
  for (int i = 0; i < 3; i++) {
    Serial.print("Chip 0x");
    Serial.print(indirizzi[i], HEX);
    Serial.print(": ");
    for (int pin = 15; pin >= 0; pin--) {
      Serial.print(mcp[i].digitalRead(pin));
    }
    Serial.println();
  }
  delay(1000);
}
