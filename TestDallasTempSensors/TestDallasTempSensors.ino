#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2
#define MAX_RESOLUTION 12

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// Variabili per memorizzare la temperatura minima e massima
float minTemperature;
float maxTemperature;

void setup(void)
{
  // Start serial communication
  Serial.begin(115200);
  Serial.println("Dallas Temperature IC Control Library Demo");

  // Start up the library
  sensors.begin();

  // Locate devices on the bus
  Serial.print("Locating devices...");
  int numberOfDevices = sensors.getDeviceCount();
  Serial.print("Found ");
  Serial.print(numberOfDevices, DEC);
  Serial.println(" devices.");

  // Report parasite power requirements for all devices
  Serial.print("Parasite power is: ");
  if (sensors.isParasitePowerMode()) 
    Serial.println("ON");
  else 
    Serial.println("OFF");

  // Check and print the initial resolution and parasite power status for all devices
  for (int i = 0; i < numberOfDevices; i++) {
    DeviceAddress deviceAddress;
    if (sensors.getAddress(deviceAddress, i)) {
      // Print the initial resolution
      Serial.print("Device ");
      Serial.print(i);
      Serial.print(" Initial Resolution: ");
      Serial.print(sensors.getResolution(deviceAddress), DEC);
      Serial.println();

      // Set resolution to maximum (12 bit)
      sensors.setResolution(deviceAddress, MAX_RESOLUTION);

      // Print the parasite power status for each device
      Serial.print("Device ");
      Serial.print(i);
      Serial.print(" Parasite power: ");
      if (sensors.isParasitePowerMode()) 
        Serial.println("ON");
      else 
        Serial.println("OFF");
    } else {
      Serial.print("Unable to find address for Device ");
      Serial.println(i);
    }
  }
}

// Function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

// Function to print the temperature for a device and update min/max values
void printTemperature(DeviceAddress deviceAddress)
{
  float tempC = sensors.getTempC(deviceAddress);
  if (tempC == DEVICE_DISCONNECTED_C) 
  {
    Serial.println("Error: Could not read temperature data");
    return;
  }
  
  // Stampa la temperatura corrente
  Serial.print("Temp C: ");
  Serial.print(tempC);
  Serial.print(" Temp F: ");
  Serial.print(DallasTemperature::toFahrenheit(tempC));

  // Aggiorna la temperatura minima e massima solo per il ciclo corrente
  if (tempC < minTemperature) {
    minTemperature = tempC;
  }
  if (tempC > maxTemperature) {
    maxTemperature = tempC;
  }
}

// Main function to print information about all devices
void printAllData()
{
  int numberOfDevices = sensors.getDeviceCount();
  for (int i = 0; i < numberOfDevices; i++) {
    DeviceAddress deviceAddress;
    if (sensors.getAddress(deviceAddress, i)) {
      Serial.print("Device ");
      Serial.print(i);
      Serial.print(" Address: ");
      printAddress(deviceAddress);
      Serial.print(" ");
      printTemperature(deviceAddress);
      Serial.println();
    } else {
      Serial.print("Unable to find address for Device ");
      Serial.println(i);
    }
  }
}

// Function to print the minimum and maximum temperatures recorded in the current cycle
void printMinMaxTemperatures() {
  Serial.print("Minimum temperature this cycle: ");
  Serial.print(minTemperature);
  Serial.println(" C");

  Serial.print("Maximum temperature this cycle: ");
  Serial.print(maxTemperature);
  Serial.println(" C");
}

void loop(void)
{
  // Reinizializza la temperatura minima e massima per il ciclo corrente
  minTemperature = 1000.0; // Un valore alto che verrà ridotto subito
  maxTemperature = -1000.0; // Un valore basso che verrà aumentato subito

  // Request temperatures from all devices on the bus
  Serial.print("Requesting temperatures...");
  sensors.requestTemperatures();
  Serial.println("DONE");

  // Print data for all devices
  printAllData();

  // Print min and max temperatures for this cycle
  printMinMaxTemperatures();

  delay(1000);
}
