#include <ESP8266WiFi.h>
#include <RH_ASK.h>

// Create an instance of the ASK driver
// RH_ASK driver(2000, RX_PIN, TX_PIN, PTT_PIN, PTT_INVERT);
RH_ASK driver(2000, 2, -1, -1);  // Specifica 2000 bps, RX su GPIO2

void setup()
{
    // Initialize the serial port for debugging
    Serial.begin(115200);
    // Ensure driver is initialized properly
    if (!driver.init())
    {
        Serial.println("Initialization failed");
        while (1);
    }
}

void loop()
{
    uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
    uint8_t buflen = sizeof(buf);

    // Non-blocking receive message
    if (driver.recv(buf, &buflen))
    {
        // Flash an LED or do something to indicate a successful message reception
        pinMode(2, OUTPUT);  // Ensure pin is set as output
        digitalWrite(2, HIGH); // Turn on the LED

        Serial.print("Got: ");
        for (int i = 0; i < buflen; i++)
        {
            Serial.print(buf[i], HEX);
            Serial.print(" ");
        }
        Serial.println("");

        digitalWrite(2, LOW); // Turn off the LED
    }
}
