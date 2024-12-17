#include <LkRadioStructureEx.h>

const int LED_PIN = 2;
const int RECEIVE_PIN = 0;

struct __attribute__((packed)) StructureA {
    byte sender; // 1 byte
    float valueA; // 4 bytes
};

struct __attribute__((packed)) StructureB {
    byte sender; // 1 byte
    int16_t valueB;
    int16_t valueC;
};

const size_t SIZE_A = sizeof(StructureA);
const size_t SIZE_B = sizeof(StructureB);

LkRadioStructureEx<StructureA> radio;


#define DEBUG 1

#if DEBUG
	#define prt(x) Serial.print(x);
	#define prtn(x) Serial.println(x);
#else
	#define prt(x) 
	#define prtn(x) 
#endif

void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN,HIGH);
    Serial.begin(115200);
    while (!Serial) {
        ; // aspetta la connessione del porto seriale
    }
    Serial.println();
    Serial.println("setup");
    digitalWrite(LED_PIN,LOW);
    delay(1000);
    digitalWrite(LED_PIN,HIGH);

    radio.globalSetup(2000, -1, RECEIVE_PIN);  // Solo ricezione
}

void loop() {
    if (radio.haveRawMessage()) {
        uint8_t rawBuffer[VW_MAX_MESSAGE_LEN];
        uint8_t rawBufferLen;
        radio.getRawBuffer(rawBuffer, rawBufferLen);
        //
        // // debug
        // prtn();
        // prt("Raw buffer length: ");
        // prtn(rawBufferLen);
        // prt("Raw buffer: ");
        // for (uint8_t i = 0; i < rawBufferLen; i++) {
        //     prt(rawBuffer[i], HEX);
        //     prt(" ");
        // }
        // prtn("------------");
        // byte sender = rawBuffer[rawBufferLen - 1];  // Assuming sender is the last byte
        // prtn(sender);
        // prtn();
        //
        byte sender = rawBuffer[rawBufferLen - 1];  // Assuming sender is the last byte
        if (sender == 1) {
          // contatori enel

          LkArraylize<StructureB> converterB;
          StructureB receivedDataB = converterB.deArraylize(rawBuffer);
          prt("Contatore Energia Attiva: ");
          Serial.println(receivedDataB.valueB);  // Print with 10 decimal places
          prt("Contatore Energia Reattiva: ");
          Serial.println(receivedDataB.valueC);  // Print with 10 decimal places
            
        } else if (sender == 99) {
          LkArraylize<StructureB> converterB;
          StructureB receivedDataB = converterB.deArraylize(rawBuffer);
          prt("contatore acqua calda: ");
          Serial.println(receivedDataB.valueB);  // Print with 10 decimal places
          prt("contatore termo: ");
          Serial.println(receivedDataB.valueC);  // Print with 10 decimal places

        } else if (sender == 100) {
          LkArraylize<StructureB> converterB;
          StructureB receivedDataB = converterB.deArraylize(rawBuffer);
          prt("Temp acqua calda Uscita °C: ");
          Serial.println(float(receivedDataB.valueB)/100);  // Print with 10 decimal places
          prt("Temp acqua calda Entrata °C: ");
          Serial.println(float(receivedDataB.valueC)/100);  // Print with 10 decimal places          

        } else if (sender == 101) {
          LkArraylize<StructureB> converterB;
          StructureB receivedDataB = converterB.deArraylize(rawBuffer);
          prt("Temperatura termo Uscita °C: ");
          Serial.println(float(receivedDataB.valueB)/100.0);  // Print with 10 decimal places
          prt("Temperatura termo Entrata °C: ");
          Serial.println(float(receivedDataB.valueC)/100.0);  // Print with 10 decimal places
        } else if (sender == 102) {
          LkArraylize<StructureB> converterB;
          StructureB receivedDataB = converterB.deArraylize(rawBuffer);
          prt("Stato acqua calda valueB: ");
          Serial.println(receivedDataB.valueB, 10);  // Print with 10 decimal places
          prt("Stato acqua calda valueC: ");
          Serial.println(receivedDataB.valueC, 10);  // Print with 10 decimal places
        } else if (sender == 106) {
          LkArraylize<StructureA> converterA;
          StructureA receivedDataA = converterA.deArraylize(rawBuffer);
          prt("Temperatura Esterna °C: ");
          Serial.println(receivedDataA.valueA);  // Print with 10 decimal places
        } else if (sender == 107) {
          LkArraylize<StructureA> converterA;
          StructureA receivedDataA = converterA.deArraylize(rawBuffer);
          prt("Umidita esterna %: ");
          Serial.println(receivedDataA.valueA);  // Print with 10 decimal places
        } else if (sender == 108) {
          LkArraylize<StructureA> converterA;
          StructureA receivedDataA = converterA.deArraylize(rawBuffer);
          prt("Pressione esterna: ");
          Serial.println(receivedDataA.valueA);  // Print with 10 decimal places
        }
    }
}