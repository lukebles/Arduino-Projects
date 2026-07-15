
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <LkMultivibrator.h>

#define MAX_VALORI 10

class MediaUltimi10 {
  int valori[MAX_VALORI];
  int indice = 0;
  int totale = 0;
  int conta = 0;

public:
  // Aggiunge un numero e restituisce la media intera
  int aggiungi(int numero) {
    // Se l'array è pieno, togliamo il valore più vecchio dal totale
    if (conta == MAX_VALORI) {
      totale -= valori[indice];
    } else {
      conta++;
    }

    valori[indice] = numero;
    totale += numero;

    indice = (indice + 1) % MAX_VALORI;

    int media = (totale + conta / 2) / conta; // media intera arrotondata al più vicino
    return ((media + 5) / 10) * 10;           // arrotondamento alle decine

  }
};


const char* ssid = "sid2";
const char* password = "pw12345678";
const char* url = "http://192.168.4.1/megane.html";

const int pinLedWifi = 2;  // GPIO2
const int pinRele = 3;  // GPIO2
const unsigned long reconnectInterval = 8000; // 8 seconds
const unsigned long visitInterval = 8000; // 8 seconds
const unsigned long releOnDuration = 10000;// 1200000;  // 20 minutes in milliseconds
const unsigned long releOffDuration = 5000; //60000;   // 1 minute in milliseconds

// ==========================================
// mdoficare i tempi in base alle necessità
// ==========================================
const unsigned long MINUTO = 60000;

#define MEZZORA 15 * MINUTO 
#define TREMINUTI 3 * MINUTO
#define UNORA 10 * MINUTO   
#define TEMPO_RIACCENSIONE_RELE 3 * MINUTO // 3 minuti in millisecondi

unsigned long tempoSpentoRele = TEMPO_RIACCENSIONE_RELE; // Memorizza il tempo di spegnimento del relè

MediaUltimi10 media;


bool meganeInCarica = false;
bool stopRicaricaMegane = false;
//
LkMultivibrator reconnectTimer(reconnectInterval, MONOSTABLE);
LkMultivibrator visitSiteTimer(visitInterval, MONOSTABLE);
LkMultivibrator releOnTimer(releOnDuration, MONOSTABLE);
LkMultivibrator releOffTimer(releOffDuration, MONOSTABLE);

unsigned long tempoAccessoMegane = 0;
unsigned long tempoSpentoMegane = 0;
unsigned long tempoCaricoGenericoALTO = 0;

bool releSiPuoAccendere = true;

bool statoPrecedente = LOW;

const int MAX_DATI_NON_VALIDI = 2; // numero di dati non validi consecutivi
int contatoreDatiNonValidi = 0;

// ===============
#define DEBUG 0
// ===============
#if DEBUG
#define prt(x) Serial.print(x)
#define prtn(x) Serial.println(x)
#else
#define prt(x)
#define prtn(x)
#endif


void setup() {
  Serial.begin(115200);
  delay(10);
  
  pinMode(pinLedWifi, OUTPUT);
  digitalWrite(pinLedWifi, HIGH); // Turn LED off

  pinMode(pinRele, OUTPUT);
  digitalWrite(pinRele, LOW); // Turn RELE off

  // disattiva
  releOffTimer.stop();
  releOnTimer.stop();

  reconnectTimer.start();

  //connectToWiFi();

}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(pinLedWifi, HIGH); // Turn LED off if not connected
    // tenta di ricollegarsi al wifi dopo
    // ogni 8 secondi
    if (reconnectTimer.expired()) {
      connectToWiFi();
      reconnectTimer.start();
    }
  } else {
    // il wifi è collegato
    digitalWrite(pinLedWifi, LOW); // accende il LED del wifi
    if (visitSiteTimer.expired()) {
      // ogni 8 secondi visita la pagina
      processWebData();
      visitSiteTimer.start();
    }
  }

}

void connectToWiFi() {
  prt("Connecting to ");
  prtn(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    prt(".");
  }
  
  prtn("");
  prtn("WiFi connected");
  prtn("IP address: ");
  prtn(WiFi.localIP());
}

void processWebData() {
  WiFiClient client;
  HTTPClient http;

  if (!http.begin(client, url)) {
    prtn("HTTP begin failed");
    return;
  } else {
    http.begin(client, url);
    int httpCode = http.GET();    
    if (httpCode > 0) {
      String payload = http.getString();
      prtn("HTTP Response:");
      prtn(payload);
      handlePayload(payload);
    } else {
      prtn("Error on HTTP request");
    }
    http.end();
  }
}

void spegniRele(String motivo) {
  if (digitalRead(pinRele) == HIGH) {
    digitalWrite(pinRele, LOW);
  }

  tempoSpentoRele = millis();
  prtn(motivo);
}


bool stringaNumerica(String s) {
  if (s.length() == 0) return false;

  for (int i = 0; i < s.length(); i++) {
    if (!isDigit(s[i])) {
      return false;
    }
  }

  return true;
}


void handlePayload(String payload) {
  payload.trim();

  int separatorIndex = payload.indexOf('-');

  if (separatorIndex <= 0) {
    spegniRele("Payload non valido: spengo il relè");
    return;
  }

  String powerStr = payload.substring(0, separatorIndex);
  String validStr = payload.substring(separatorIndex + 1);

  powerStr.trim();
  validStr.trim();

  if (!stringaNumerica(powerStr) || !stringaNumerica(validStr)) {
    spegniRele("Valori non numerici: spengo il relè");
    return;
  }

  int power = powerStr.toInt();
  int valid = validStr.toInt();

  if (valid != 1) {
      contatoreDatiNonValidi++;

      prt("Dato non valido consecutivo n. ");
      prtn(contatoreDatiNonValidi);

      if (contatoreDatiNonValidi >= MAX_DATI_NON_VALIDI) {

          if (digitalRead(pinRele) == HIGH) {
              digitalWrite(pinRele, LOW);
              tempoSpentoRele = millis();

              prtn("Dati non validi per circa 1 minuto: spengo il relè");
          } else {
              prtn("Dati ancora non validi: relè già spento");
          }
      }

      return;
  }

  // Se arrivo qui, il dato è valido
  contatoreDatiNonValidi = 0;

  // Da qui in poi il dato è valido
  int mediaAttuale = media.aggiungi(power);
  Serial.println(mediaAttuale);

  bool releAcceso = digitalRead(pinRele) == HIGH;

  if (releAcceso) {
    if (power > 3990) {
      spegniRele("Superato il limite di potenza: spengo il relè");
      return;
    }
  }

  if (!releAcceso) {
    if (mediaAttuale < 1400) {
      if (millis() - tempoSpentoRele >= TEMPO_RIACCENSIONE_RELE) {
        digitalWrite(pinRele, HIGH);
        prtn("Riaccendo il relè per la ricarica Megane");
      }
    }
  }
}