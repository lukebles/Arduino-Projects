#include <VirtualWire.h>
#include <LkHexBytes.h>
//#include <LkMultivibrator.h>
#include <LkBlinker.h>
#include <LkArraylize.h>

// 2 feb 2024 - inserimento del distacco ENEL direttamnte nel codice ARDUINO

//
//              |  /| backLightLCM_pin             |-------------|
// |||--/\/\/\--|<  |-----------------------------<|             |
//              |  \|                              |             |
//                                                 | ATMEGA328P  |
//             ------    pushButton_pin            | w/          |
// |||--------O      O---------------------------->| Arduino     |-----> suoneria (badenia) 
//                                                 | boot        |
//                                                 | loader      |
//                                                 |             |
//      |--------------|                           |             |
// GND  |              |                           |             |             |---------------|
// |||--| radio module |                           |             |             |               |
//      |           RX |-----> receive_pin ------->|             |------------>|   P C         |
//      |--------------|                           |             | serial port |               |
//                                                 |             |<------------|               |
//      |--------------|                           |             |             |---------------|
// GND  |          Vcc |----< transmit_en_pin ----<|             |
// |||--| radio module |                           |             |
//      |           TX |----< transmit_pin -------<|             |
//      |--------------|                           |             |
//                                                 |-------------|

bool msgFromPcSerialisComplete = false;  // whether the string is complete

const byte MaxSerialMessageLen = 60;
char msgFromPcSerial[MaxSerialMessageLen];

uint8_t COUNTER=0;

const int transmit_pin = 12;
const int receive_pin = 11;
#define shutdown_pin 7
#define reboot_pin 8
#define badenia_pin 6

LkBlinker allarme_badenia(badenia_pin,true);

#define VIA_RADIO 0xAA
#define POSSIBILE_DISTACCO_ENEL 0x01

struct DataPacket{
  uint8_t sender;
  uint16_t countActiveWh;
  uint16_t countReactiveWh;
};

uint16_t previous_countActiveWh = 0;
unsigned long previous_time_ENEL = 0;

void setup(){
  // dialogue with Host via serial port
  Serial.begin(115200);
  //delay(1000);
  
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // Initialise the IO and ISR
  vw_set_tx_pin(transmit_pin);
  vw_set_rx_pin(receive_pin);
  vw_setup(2000);  // Bits per sec
  vw_rx_start();    // Start the receiver PLL running

  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN,LOW);

  pinMode(shutdown_pin,INPUT_PULLUP);
  pinMode(reboot_pin,INPUT_PULLUP);
}

void loop() {
  // to check the timing of blinkng
  allarme_badenia.loop();

  // reads the status of some pins
  // and sends a string via serial to the computer
  if(digitalRead(shutdown_pin)==LOW){
    Serial.print("SPEGNI\n");
    digitalWrite(LED_BUILTIN,HIGH);
    delay(1000);
  }

  if(digitalRead(reboot_pin)==LOW){
    Serial.print("RIAVVIA\n");
    digitalWrite(LED_BUILTIN,HIGH);
    delay(1000);
  }

  radioMessage2arduino();

  //===============================
  // MESSAGE PC -> ARDUINO
  //===============================
  if (msgFromPcSerialisComplete) {
    byte byteArray[20];
    // conversion from sequence of characters (example: "AAFA12...")
    // into array of bytes
    LkHexBytes::hexCharacterStringToBytes(byteArray, msgFromPcSerial);
    //dumpByteArray(byteArray, MaxByteArraySize);
      if(byteArray[0] == VIA_RADIO){
        // -----------------------------
        // MUST BE SENT BY RADIO
        // -----------------------------
        // each pair of characters equals 1 byte
        uint8_t n_bytes = COUNTER >> 1; 
        // the first character is removed from the count, which is only an 
        // serial communication identifier
        n_bytes -= 1;
        char msg[n_bytes];
        for (int i = 1; i <= n_bytes; i++){
          msg[i-1]=byteArray[i]; 
        }
        
        //digitalWrite(LED_BUILTIN,HIGH);
        vw_send((uint8_t *)msg, n_bytes);
        vw_wait_tx(); // Wait until the whole message is gone
        //delay(100);
        //digitalWrite(LED_BUILTIN,LOW);

        
      } else {
        if(byteArray[0] == POSSIBILE_DISTACCO_ENEL){
          allarme_badenia.enable(); 
        }
        // -------------------
        // test option
        // ------------------
        // digitalWrite(LED_BUILTIN,HIGH);
        // delay(1000);
        // digitalWrite(LED_BUILTIN,LOW);
      }
    // clear the string:
    COUNTER = 0;
    ///msgFromPcSerial[MaxSerialMessageLen] = {0};
    msgFromPcSerialisComplete = false;
  }
}

//=============================================
// PC -> ARDUINO COMMUNICATION
//=============================================
// Event at each reception in the serial port
// the communications that Arduino receives from the serial
// are not received as "bytes" but as pairs of characters
// representing the hexadecimal encoding of the "byte".
// for example
// 0x00 0xab 0xfe 0x12
// are transmitted by the PC as
// "00ABFE12\n"
// the \n indicates the end of the packet
//=============================================
void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    byte theByte = Serial.read();  
    // add it to the msgFromPcSerial:
    // if the incoming character is a newline, 
    // set a flag so the main loop can
    // do something about it:
    if (theByte == '\n') {
      // il \n non viene aggiunto alla stringa
      msgFromPcSerialisComplete = true;
      //Serial.flush();
      //Serial.println();
    } else {
      msgFromPcSerialisComplete = false;
      // only ASCII characters are added without the commands
      // to avoid shattering
      // pairs of characters must be sent to send values from 0 to 255
      // '00' ... 'FF' and terminated with \n (single control character equivalent to 10 in decimal 
      // or to 0a in hexadecimal)
      // example of sending: '0A12BF0067\n'
      if((theByte > 31) && (theByte < 127)){
        msgFromPcSerial[COUNTER]=(char) theByte;
        //Serial.print(msgFromPcSerial[COUNTER]);
        COUNTER++;
      }
    }
  }
}

void radioMessage2arduino(){
    // =================================================
  // RADIO MESSAGE -> ARDUINO -> PC
  //                     |-----> badenia
  // the radio message is always forwarded back to the pc
  // in the form of a "hex_string" character pair
  // example byte 0x0a (\n) is sent as "0A" 
  // =================================================
  uint8_t buf[VW_MAX_MESSAGE_LEN];
  uint8_t buflen = VW_MAX_MESSAGE_LEN;
  String hexstring = "";
  if (vw_get_message(buf, &buflen)) {
    for (int i = 0; i < buflen; i++) {
      if(buf[i] < 0x10) {
        hexstring += '0';
      }      
      hexstring += String(buf[i], HEX);
    }
    hexstring += "\n";        // end message
    
    digitalWrite(LED_BUILTIN,HIGH);
    Serial.print(hexstring);  // sending the message to the PC
    digitalWrite(LED_BUILTIN,LOW);

    // ==========================================
    // GESTIONE DEL POSSIBILE DISTACCO ENEL
    // DIRETTAMENTE DA ARDUINO
    // ==========================================
    // interpreta il contenuto del pacchetto
    // per verificare se è un pacchetto riguardante 
    // il modulo ENEL
    if (buflen == sizeof(DataPacket)) {
      // la dimensione del pacchetto è la stessa
      LkArraylize<DataPacket> dataPacketConverter;
      // estraggo le informazioni dal pacchetto
      DataPacket receivedData = dataPacketConverter.deArraylize(buf);
      if (receivedData.sender == 1){ // ID = 1 è il modulo ENEL
        // dati provenienti dal modulo ENEL
        //
        // differenze con i valori della precedente
        // ricezione via radio
        if (previous_time_ENEL == 0){
          // prima ricezione
          previous_time_ENEL = millis();
          previous_countActiveWh = receivedData.countActiveWh;
        } else {
          // funziona anche la differenza durante il passaggio da 4294967295 a 0, 1, 2, ... eccetera
          unsigned long diffmillis = millis() - previous_time_ENEL;
          previous_time_ENEL = millis();
          // funziona anche la differenza durante il passaggio da 65535 a 0, 1, 2, ... eccetera
          unsigned long diffEnergia = receivedData.countActiveWh - previous_countActiveWh;
          previous_countActiveWh = receivedData.countActiveWh;
          // calcoli
          float delta_tempo_sec = float(diffmillis)/1000.0;
          float delta_energia_wh = float(diffEnergia);

          // evita di fare suonare la campanella in situazioni dubbie
          // cioè quando è passato troppo tempo dall'ultima ricezione o 
          // il conteggio dell'energia consumata nell'intervallo è troppo alta
          // Infatti di solito i conteggi dei secondi sono dell'ordine delle decine di secondi
          // e gli stessi numeri (anche inferiori, dipende dal consumo) riguardano
          // il delta-energia.
          if ((delta_tempo_sec > 3600.0) || (delta_energia_wh > 3600.0)) {
            // non fa niente se mi trovo in una situazione strana
          } else {
            // determinazione della potenza attiva istantanea
            float potenzaAttiva = 3600.0 / float(delta_tempo_sec) * float(delta_energia_wh);
            // verifica se c'è pericolo di distacco energia
            if (potenzaAttiva > 3700.0){
              allarme_badenia.enable(); 
            }
          }
        }
      }
    } // FINE CONTROLLO DISTACCO    
  }
}
