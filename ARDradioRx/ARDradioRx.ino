#include <LkRadioStructure.h>
// include the library code:
#include <LiquidCrystal.h>

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 8, en = 10, d4 = 7, d5 = 6, d6 = 5, d7 = 4;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const int pin_pushb1 = 2;

const int LED_PIN = 13;
const int RECEIVE_PIN = 11;

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

LkRadioStructure<StructureA> radio; // necessario per radio

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

uint16_t prev_elettrico =0 ;
bool primoAvvio = true;

String prev_potenza = "";
String prev_tempEsterna = "";
String prev_pressEsterna = "";
String prev_umidEsterna = "";
String prev_acquaCalda = "";
String prev_termosifoni = "";

String ora_potenza = "";
String ora_tempEsterna = "";
String ora_pressEsterna = "";
String ora_umidEsterna = "";
String ora_acquaCalda = "";
String ora_termosifoni = "";

void setup() {
  pinMode(pin_pushb1, INPUT_PULLUP);
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
  lcd.begin(20, 4);
  // Print a message to the LCD.
  lcd.setCursor(0, 0);
  lcd.print("Radio Rx");  
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
      
      if (primoAvvio){
        prev_elettrico = receivedDataC.valueD;
        primoAvvio = false;
      }
      else {
        uint16_t diff = receivedDataC.valueD - prev_elettrico;
        if (diff > 12){
          ora_potenza = String(diff);
        } else {
          ora_potenza = "";
        }
      }
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
      int i_diff = round(tSanitOut - tSanitIn);
      //
      ora_acquaCalda = String(i_tSanitIn) + " " + String(i_tSanitOut) + " (" + String(i_diff) + ")";
    
    
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
      int i_diff = round(i_tTermoOut - i_tTermoIn);
      //
      ora_termosifoni = String(i_tTermoIn) + " " + String(i_tTermoOut) + " (" + String(i_diff) + ")" ;     
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
      //
      ora_tempEsterna = String(i_EsternaTemp);
    } else if (sender == 107) {
      LkArraylize<StructureA> converterA;
      StructureA receivedDataA = converterA.deArraylize(rawBuffer);
      prt("Umidita esterna %: ");
      prtn(receivedDataA.valueA);  
      i_EsternaUmidita = round(receivedDataA.valueA);
      //
      ora_umidEsterna = String(i_EsternaUmidita);
    } else if (sender == 108) {
      LkArraylize<StructureA> converterA;
      StructureA receivedDataA = converterA.deArraylize(rawBuffer);
      prt("Pressione esterna: ");
      prtn(receivedDataA.valueA);  
      i_EsternaPress = round(receivedDataA.valueA);
      //
      ora_pressEsterna = String(i_EsternaPress);
    }

    char outputString[20];
    snprintf(outputString, sizeof(outputString), "%d %d %d %d %d %d", 
           i_tSanitIn, i_tSanitOut, i_tTermoIn, i_tTermoOut, 
           i_EsternaTemp, i_EsternaUmidita);

    lcd.setCursor(0, 0);
    lcd.print("                    ");  
    lcd.setCursor(0, 0);
    lcd.print(outputString);

    lcd.setCursor(0, 1);
    lcd.print("                    ");  
    lcd.setCursor(0, 1);
    lcd.print(i_EsternaPress);

    if (!digitalRead(pin_pushb1)){
      lcd.setCursor(0, 2);
      lcd.print("                    ");  
      lcd.setCursor(0, 2);
      lcd.print(outputString);

      lcd.setCursor(0, 3);
      lcd.print("                    ");  
      lcd.setCursor(0, 3);
      lcd.print(i_EsternaPress);
    }



  }
}