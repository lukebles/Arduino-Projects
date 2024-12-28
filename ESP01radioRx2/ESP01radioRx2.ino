#include <LkRadioStructureEx.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "config.h"


// Inizializzazione client e bot
WiFiClientSecure client;
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, client);

unsigned long lastCheckTime = 0;
const unsigned long botRequestDelay = 1000; // Tempo tra richieste in millisecondi

// ================

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

LkRadioStructureEx<StructureA> radio;

bool debugEnabled = true;

void debugPrint(const String& message) {
  if (debugEnabled) {
    Serial.println(message);
  }
}

char prev_potenza[30] = "";
char ora_potenza[30] = "";
char prev_tempEsterna[30] = "";
char ora_tempEsterna[30] = "";
char prev_pressEsterna[30] = "";
char ora_pressEsterna[30] = "";
char prev_umidEsterna[30] = "";
char ora_umidEsterna[30] = "";
char prev_acquaCalda[30] = "";
char ora_acquaCalda[30] = "";
char prev_termosifoni[30] = "";
char ora_termosifoni[30] = "";

uint16_t prev_elettrico = 0;
bool primoAvvio = true;

void inviaMessaggioSeNecessario(char* prevValue, const char* currentValue, const char* descrizione, size_t bufferSize) {
  if (strncmp(prevValue, currentValue, bufferSize) != 0) { // Confronta solo fino alla dimensione del buffer
    strncpy(prevValue, currentValue, bufferSize - 1); // Aggiorna prevValue con currentValue
    prevValue[bufferSize - 1] = '\0'; // Garantisce che il buffer sia terminato correttamente
    if (bot.sendMessage(TELEGRAM_CHAT_ID, String(descrizione) + ": " + currentValue, "")) {
      debugPrint("Messaggio inviato con successo!");
    } else {
      debugPrint("Errore nell'invio del messaggio!");
    }
  }
}


template <typename T>
T deArraylizeData(uint8_t* buffer) {
  LkArraylize<T> converter;
  return converter.deArraylize(buffer);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  Serial.begin(115200);
  debugPrint("Setup iniziato");
  digitalWrite(LED_PIN, LOW);
  delay(1000);
  digitalWrite(LED_PIN, HIGH);

  radio.globalSetup(2000, -1, RECEIVE_PIN);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastCheckTime > 1000) {
      digitalWrite(LED_PIN, LOW);
      delay(30);
      digitalWrite(LED_PIN, HIGH);
      lastCheckTime = millis();
    }
  }
  debugPrint("Connesso al WiFi");
  debugPrint(WiFi.localIP().toString());

  client.setInsecure();

  if (bot.sendMessage(TELEGRAM_CHAT_ID, "ESP01 connesso a Internet", "")) {
    debugPrint("Messaggio inviato con successo!");
  } else {
    debugPrint("Errore nell'invio del messaggio!");
  }  
}

void loop() {
  // static unsigned long previousMillis = 0;
  // unsigned long currentMillis = millis();

  // if (currentMillis - previousMillis >= botRequestDelay) {
  //   previousMillis = currentMillis;
  //   int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  //   while (numNewMessages) {
  //     handleNewMessages(numNewMessages);
  //     numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  //   }
  // }

  if (radio.haveRawMessage()) {
    uint8_t rawBuffer[VW_MAX_MESSAGE_LEN];
    uint8_t rawBufferLen;
    radio.getRawBuffer(rawBuffer, rawBufferLen);

    byte sender = rawBuffer[rawBufferLen - 1];
    if (sender == 100) {
      StructureB receivedDataB = deArraylizeData<StructureB>(rawBuffer);
      int tSanitOut = round(float(receivedDataB.valueB) / 100.0);
      int tSanitIn = round(float(receivedDataB.valueC) / 100.0);
      snprintf(ora_acquaCalda, sizeof(ora_acquaCalda), "%d %d (%d)", tSanitIn, tSanitOut, tSanitOut - tSanitIn);
      //debugPrint(String("acqua: ")+ ora_acquaCalda);

    } else if (sender == 101) {
      StructureB receivedDataB = deArraylizeData<StructureB>(rawBuffer);
      int tTermoOut = round(float(receivedDataB.valueB) / 100.0);
      int tTermoIn = round(float(receivedDataB.valueC) / 100.0);
      snprintf(ora_termosifoni, sizeof(ora_termosifoni), "%d %d (%d)", tTermoIn, tTermoOut, tTermoOut - tTermoIn);
       //debugPrint(String("termo: ") + ora_termosifoni);
      // 102 sono lo stato dei termo
    } else if (sender == 106) {
      StructureA receivedDataA = deArraylizeData<StructureA>(rawBuffer);
      snprintf(ora_tempEsterna, sizeof(ora_tempEsterna), "%d", round(receivedDataA.valueA));
       //debugPrint(String("temp est: ") + ora_tempEsterna);
    } else if (sender == 107) {
      StructureA receivedDataA = deArraylizeData<StructureA>(rawBuffer);
      snprintf(ora_umidEsterna, sizeof(ora_umidEsterna), "%d", round(receivedDataA.valueA));
       //debugPrint(String("umid: ") + ora_umidEsterna);
    } else if (sender == 108) {
      StructureA receivedDataA = deArraylizeData<StructureA>(rawBuffer);
      snprintf(ora_pressEsterna, sizeof(ora_pressEsterna), "%d", round(receivedDataA.valueA));
      //debugPrint(String("press: ") + ora_pressEsterna);
    }

  inviaMessaggioSeNecessario(prev_acquaCalda, ora_acquaCalda, "acqua calda", sizeof(prev_acquaCalda));
  inviaMessaggioSeNecessario(prev_termosifoni, ora_termosifoni, "termosifoni", sizeof(prev_termosifoni));
  inviaMessaggioSeNecessario(prev_potenza, ora_potenza, "potenza", sizeof(prev_potenza));
  inviaMessaggioSeNecessario(prev_tempEsterna, ora_tempEsterna, "temp esterna", sizeof(prev_tempEsterna));
  inviaMessaggioSeNecessario(prev_pressEsterna, ora_pressEsterna, "press esterna", sizeof(prev_pressEsterna));
  inviaMessaggioSeNecessario(prev_umidEsterna, ora_umidEsterna, "umid esterna", sizeof(prev_umidEsterna));

  }
}

// void handleNewMessages(int numNewMessages) {
//   for (int i = 0; i < numNewMessages; i++) {
//     String text = bot.messages[i].text;
//     if (text == "/accendi") {
//       digitalWrite(LED_PIN, LOW);
//       bot.sendMessage(chat_id, "LED acceso!", "");
//     } else if (text == "/spegni") {
//       digitalWrite(LED_PIN, HIGH);
//       bot.sendMessage(chat_id, "LED spento!", "");
//     } else {
//       bot.sendMessage(chat_id, "Comando non riconosciuto. Usa /accendi o /spegni.", "");
//     }
//   }
// }
