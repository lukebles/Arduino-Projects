
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
unsigned long tempoSpentoRele = 0; // Memorizza il tempo di spegnimento del relè

MediaUltimi10 media;

// ==========================================
// mdoficare i tempi in base alle necessità
// ==========================================
const unsigned long MINUTO = 60000;

#define MEZZORA 15 * MINUTO 
#define TREMINUTI 3 * MINUTO
#define UNORA 10 * MINUTO   
#define TEMPO_RIACCENSIONE_RELE 3 * MINUTO // 3 minuti in millisecondi
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

void handlePayload(String payload) {
  int separatorIndex = payload.indexOf('-');
  
  if (separatorIndex > 0) {
    String powerStr = payload.substring(0, separatorIndex);
    String validStr = payload.substring(separatorIndex + 1);
    
    int power = powerStr.toInt();
    int valid = validStr.toInt();
    // stampa su display esterno (seriale altrimenti)

    int mediaAttuale = media.aggiungi(power);
    Serial.println(mediaAttuale);

    if (digitalRead(pinRele) == HIGH) {
        if (power > 3600) {
            digitalWrite(pinRele, LOW);
            tempoSpentoRele = millis(); // Registra il momento dello spegnimento
            prtn("Superato il limite di potenza, spengo il relè e attendo 3 minuti prima di riaccenderlo");
        }
    }
    // Controllo per riaccendere la ricarica dopo 3 minuti se le condizioni lo permettono
    if (power < 1100 && valid == 1) {
        if ((millis() - tempoSpentoRele >= TEMPO_RIACCENSIONE_RELE)) {
            digitalWrite(pinRele, HIGH);
            prtn("Riaccendo il relè per la ricarica Megane dopo il tempo di attesa");
        }
    } else {
        if (valid != 1) {
            digitalWrite(pinRele, LOW);
            tempoSpentoRele = millis(); // Registra il momento dello spegnimento
            prtn("Dati non validi: spengo il relè e attendo 3 minuti prima di riaccenderlo");
        }
    }
  }
}


