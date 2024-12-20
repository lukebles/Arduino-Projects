#include <LkRadioStructureEx.h>

const int LED_PIN = 2;
const int RECEIVE_PIN = 0;

struct __attribute__((packed)) StructureA {
  byte sender;   // 1 byte
  float valueA;  // 4 bytes
};

struct __attribute__((packed)) StructureB {
  byte sender;  // 1 byte
  int16_t valueB;
  int16_t valueC;
};

struct __attribute__((packed)) StructureC {
  byte sender;  // 1 byte
  uint16_t valueD;
  uint16_t valueE;
};

LkRadioStructureEx<StructureA> radio; // necessario per radio

#define DEBUG 0

#if DEBUG
#define prt(x) Serial.print(x);
#define prtn(x) Serial.println(x);
#else
#define prt(x)
#define prtn(x)
#endif

int i_tSanitOut = 99; 
int i_tSanitIn = 99; 
int i_tTermoOut = 99; 
int i_tTermoIn = 99; 
int i_EsternaTemp = 99;
int i_EsternaPress = 9999;
int i_EsternaUmidita = 99;


void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  Serial.begin(115200);
  while (!Serial) {
    ;  // aspetta la connessione del porto seriale
  }
  prtn();
  prtn("setup");
  digitalWrite(LED_PIN, LOW);
  delay(1000);
  digitalWrite(LED_PIN, HIGH);

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

      LkArraylize<StructureC> converterC;
      StructureC receivedDataC = converterC.deArraylize(rawBuffer);
      prt("Contatore Energia Attiva: ");
      prtn(receivedDataC.valueD);  
      prt("Contatore Energia Reattiva: ");
      prtn(receivedDataC.valueE);  
    } else if (sender == 99) {
      LkArraylize<StructureC> converterC;
      StructureC receivedDataC = converterC.deArraylize(rawBuffer);
      prt("contatore acqua calda: ");
      prtn(receivedDataC.valueD);  
      prt("contatore termo: ");
      prtn(receivedDataC.valueE);  
    } else if (sender == 100) {
      LkArraylize<StructureB> converterB;
      StructureB receivedDataB = converterB.deArraylize(rawBuffer);
      float tSanitOut = float(receivedDataB.valueB) / 100.0; 
      float tSanitIn = float(receivedDataB.valueC) / 100.0; 
      //
      prt("Temp acqua calda Uscita °C: ");
      prtn(tSanitOut);  
      prt("Temp acqua calda Entrata °C: ");
      prtn(tSanitIn);  
      //
      i_tSanitOut = round(tSanitOut); 
      i_tSanitIn = round(tSanitIn); 
    } else if (sender == 101) {
      LkArraylize<StructureB> converterB;
      StructureB receivedDataB = converterB.deArraylize(rawBuffer);
      float tTermoOut = float(receivedDataB.valueB) / 100.0; 
      float tTermoIn = float(receivedDataB.valueC) / 100.0; 
      prt("Temperatura termo Uscita °C: ");
      prtn(tTermoOut);  
      prt("Temperatura termo Entrata °C: ");
      prtn(tTermoIn);  
      i_tTermoOut = round(tTermoOut); 
      i_tTermoIn = round(tTermoIn); 
    } else if (sender == 102) {
      LkArraylize<StructureB> converterB;
      StructureB receivedDataB = converterB.deArraylize(rawBuffer);
      prt("Stato acqua calda valueB: ");
      prtn(receivedDataB.valueB);  
      prt("Stato acqua calda valueC: ");
      prtn(receivedDataB.valueC);  
    } else if (sender == 106) {
      LkArraylize<StructureA> converterA;
      StructureA receivedDataA = converterA.deArraylize(rawBuffer);
      prt("Temperatura Esterna °C: ");
      prtn(receivedDataA.valueA);  
      i_EsternaTemp = round(receivedDataA.valueA);
    } else if (sender == 107) {
      LkArraylize<StructureA> converterA;
      StructureA receivedDataA = converterA.deArraylize(rawBuffer);
      prt("Umidita esterna %: ");
      prtn(receivedDataA.valueA);  
      i_EsternaUmidita = round(receivedDataA.valueA);
    } else if (sender == 108) {
      LkArraylize<StructureA> converterA;
      StructureA receivedDataA = converterA.deArraylize(rawBuffer);
      prt("Pressione esterna: ");
      prtn(receivedDataA.valueA);  
      i_EsternaPress = round(receivedDataA.valueA);
    }

    Serial.print(i_tSanitIn);
    Serial.print(" ");
    Serial.print(i_tSanitOut);
    Serial.print(" ");
    Serial.print(i_tTermoIn);
    Serial.print(" ");
    Serial.print(i_tTermoOut);
    Serial.print(" ");
    Serial.print(i_EsternaTemp);
    Serial.print(" ");
    Serial.print(i_EsternaUmidita);
    Serial.print(" ");
    Serial.println(i_EsternaPress);


  }
}