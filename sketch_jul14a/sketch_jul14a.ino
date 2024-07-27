#include "Button.h"

Button button(0);

void setup() {
  Serial.begin(115200);
}

void loop() {
  button.update();

  if (button.wasPressed()) {
    Serial.print("Button pressed, released duration: ");
    Serial.print(button.releasedDuration());
    Serial.println(" ms");
  }

  if (button.wasReleased()) {
    Serial.print("Button released, pressed duration: ");
    Serial.print(button.pressedDuration());
    Serial.println(" ms");
  }

}
