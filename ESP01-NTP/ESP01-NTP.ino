#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Timezone.h>

const char* ssid     = "iPhone di Luca";     // Sostituisci con il nome della tua rete WiFi
const char* password = "Catgnes-9"; // Sostituisci con la password della tua rete WiFi

const char* ntpServerName = "it.pool.ntp.org";
const int   timeZone = 1;  // Sostituisci con il tuo fuso orario

// Central European Time (Roma, Parigi)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};    // Central European Summer Time (UTC +2 ore)
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};      // Central European Standard Time (UTC +1 ora)
Timezone CE(CEST, CET);

WiFiUDP udp;
unsigned int localPort = 8888;  // Porta locale per ricevere i pacchetti UDP

const int NTP_PACKET_SIZE = 48; // Aggiungi questa riga
byte packetBuffer[NTP_PACKET_SIZE]; // Aggiungi questa riga

const int monthDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.print("Connessione a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" WiFi connesso");

  udp.begin(localPort);
  Serial.print("Porta UDP locale: ");
  Serial.println(udp.localPort());

  setSyncProvider(getNtpTime);
  setSyncInterval(300);
}

void loop() {
  if (timeStatus() != timeNotSet) {
    digitalClockDisplay();
  }
  delay(1000);
}

// void digitalClockDisplay() {
//   time_t localTime = CE.toLocal(now());
//   Serial.print(hour(localTime));
//   printDigits(minute(localTime));
//   printDigits(second(localTime));
//   Serial.println(); 
// }

void digitalClockDisplay() {
    time_t localTime = CE.toLocal(now());
    Serial.print(hour(localTime));
    printDigits(minute(localTime));
    printDigits(second(localTime));

    unsigned long seconds = secondsToNewYear(localTime);
    Serial.print(" Secondi al prossimo ultimo dell'anno: ");
    Serial.println(seconds);
}


void printDigits(int digits) {
  Serial.print(":");
  if (digits < 10) Serial.print('0');
  Serial.print(digits);
}

time_t getNtpTime() {
  IPAddress ntpServerIP;
  while (udp.parsePacket() > 0) ; // scarta eventuali pacchetti UDP precedentemente ricevuti
  Serial.println("Trasmissione richiesta NTP");
  WiFi.hostByName(ntpServerName, ntpServerIP); 
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);

  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= 48) {
      Serial.println("Risposta NTP ricevuta");
      udp.read((char *)packetBuffer, NTP_PACKET_SIZE);
      unsigned long secsSince1900;
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL; // Converti il tempo NTP in tempo UNIX
    }
  }
  Serial.println("Nessuna risposta NTP");
  return 0;
}


void sendNTPpacket(IPAddress& address) {
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;     
  packetBuffer[2] = 6;     
  packetBuffer[3] = 0xEC;  
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  udp.beginPacket(address, 123);
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}


unsigned long secondsToNewYear(time_t localTime) {
    tmElements_t tm;
    breakTime(localTime, tm);

    // Determina se l'anno corrente è un anno bisestile
    bool isLeapYear = ((1970 + tm.Year) % 4 == 0) && (((1970 + tm.Year) % 100 != 0) || ((1970 + tm.Year) % 400 == 0));

    // Calcola i secondi trascorsi in questo anno fino ad ora
    long daysPassed = tm.Day;
    for (int i = 1; i < tm.Month; ++i) {
        daysPassed += monthDays[i - 1];
    }
    if (isLeapYear && tm.Month > 2) daysPassed += 1; // Aggiungi un giorno per l'anno bisestile

    // Calcola i secondi rimanenti fino alla fine dell'anno
    long daysRemaining = (isLeapYear ? 366 : 365) - daysPassed;
    return daysRemaining * 86400L - ((tm.Hour * 3600) + (tm.Minute * 60) + tm.Second);
}

