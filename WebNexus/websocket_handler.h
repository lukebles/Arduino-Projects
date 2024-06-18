#ifndef WEBSOCKET_HANDLER_H
#define WEBSOCKET_HANDLER_H

#include <WebSocketsServer_Generic.h>
#include <TimeLib.h>
#include <EEPROM.h>
#include "data_handling.h"

extern WebSocketsServer webSocket;

int powerLimitValue;
void setPowerLimitValue(int value){
  powerLimitValue = value;
}

void handleWebSocketMessage(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_TEXT) {
    String message = (char*)payload;
    // Serial.println(message);
    if (message.equals("getPowerData")) {
      sendClient_istant(num);
    } else if (message.startsWith("setTime:")) {
      String dateTimeString = message.substring(8);
      struct tm tm;
      if (sscanf(dateTimeString.c_str(), "%d-%d-%d %d:%d:%d",
                  &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                  &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
        tm.tm_year -= 1900;
        tm.tm_mon -= 1;
        tm.tm_isdst = 0;

        time_t t = mktime(&tm);
        setTime(t);

        webSocket.sendTXT(num, "Time updated successfully");
      }
    } else if (message.equals("getHourEnergy")) {
        sendClient_hours(num); 
    } else if (message.equals("getDaysEnergy")){
        sendClient_days(num); 
    } else if (message.equals("getPowerLimit")){
        sendClient_powerLimit(num,powerLimitValue); 
        Serial.println(powerLimitValue);
    } else if (message.startsWith("POWER-LIMIT=")){
        powerLimitValue = message.substring(12).toInt();
        EEPROM.write(0, powerLimitValue & 0xFF);
        EEPROM.write(0 + 1, (powerLimitValue >> 8) & 0xFF);
        Serial.println(message);
    } else if (message.startsWith("ALARM-TEST")){
        Serial.println(message);
    } else {
      //Serial.println(message);
    }
  }
}

void setupWebSocket() {
    webSocket.begin();
    webSocket.onEvent(handleWebSocketMessage);
}

#endif // WEBSOCKET_HANDLER_H
