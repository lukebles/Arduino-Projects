#include "LkSetupSerial.h"

// LkSetupSerial mySerial(9600); // You can specify a different baud rate if necessary
LkSetupSerial mySerial;          // otherwise it is set to 115200 (don't put parentheses here)

void setup() {
    // mySerial.begin("Avvio"); // Enter your personalized message here
    mySerial.begin();           // the standard is "setup"
}

void loop() {
    // insert your code
}
