#ifndef WEBSOCKET_HANDLER_H
#define WEBSOCKET_HANDLER_H

#include <WebSocketsServer_Generic.h>
#include <TimeLib.h>
#include <EEPROM.h>
#include "data_handling.h"

extern WebSocketsServer webSocket;

template <typename T>
void sendToMultiCatch(T message) {
    Serial.println(message); // trasmette ad Arduino su seriale
}

int powerLimitValue;
void setPowerLimitValue(int value){
  powerLimitValue = value;
  prt("impostato limite di potenza: ");
  prtn(powerLimitValue);
}

void handleWebSocketMessage(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_TEXT) {
    String message = (char*)payload;
    prt("\tMessaggio da pagina web: ");
    prtn(message);
    if (message.equals("getPowerData")) {
      sendClient_istant(num);
    } else if (message.startsWith("setTime:")) {
      String dateTimeString = message.substring(8);
      prtn("\tRichiesto aggiornamento data-ora");
      struct tm tm;
      if (sscanf(dateTimeString.c_str(), "%d-%d-%d %d:%d:%d",
                  &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                  &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
        tm.tm_year -= 1900;
        tm.tm_mon -= 1;
        tm.tm_isdst = 0;
        //
        time_t t = mktime(&tm);
        setTime(t);
        //
        webSocket.sendTXT(num, "Time updated successfully");
        sendToMultiCatch("DATETIME-OK"); // invio comando a MultiCatch
        prtn("\tData ora aggiornata con quella del browser");
      }
    } else if (message.equals("getHourEnergy")) {
        sendClient_hours(num); 
        prtn("\tInviata la tabella oraria");
    } else if (message.equals("getDaysEnergy")){
        sendClient_days(num);
        prtn("\tInviata la tabella giornaliera");
    } else if (message.equals("getPowerLimit")){
        sendClient_powerLimit(num,powerLimitValue);
        prt("\tRichiesto limite di potenza: ");
        prtn(powerLimitValue);
        // Serial.println(powerLimitValue);
    } else if (message.startsWith("POWER-LIMIT=")){
        powerLimitValue = message.substring(12).toInt();
        EEPROM.write(0, powerLimitValue & 0xFF);
        EEPROM.write(0 + 1, (powerLimitValue >> 8) & 0xFF);
        EEPROM.commit(); 
        sendToMultiCatch(message); // invio comando a MultiCatch
        prt("\tRegistrato limite di potenza su ESP01 e inviato anche ad Arduino: ");
        prtn(powerLimitValue);
    } else if (message.startsWith("ALARM-TEST")){
        sendToMultiCatch(message); // invio comando a MultiCatch
    } else if (message.startsWith("SAVE")){
        saveData();
    } else {
      prtn("\tNON RICONOSCIUTO: questo messaggio non è stato riconosciuto");
    }
  }
}

void setupWebSocket() {
    webSocket.begin();
    webSocket.onEvent(handleWebSocketMessage);
    prtn("\tWebsocket attivato: pronto a gestire i messaggi da web");
}

#endif // WEBSOCKET_HANDLER_H
