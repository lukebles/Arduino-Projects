/*
    Questo sistema garantisce che la Megane si ricarichi in modo efficiente, senza superare i limiti di consumo domestico.

    **Gestione della Ricarica della Megane (Auto Ibrida)**

    Questo codice controlla la ricarica dell'auto ibrida ("Megane") in base agli assorbimenti elettrici della casa.
    Il sistema ha le seguenti regole:

    1) **Spegnimento per Priorità agli Altri Carichi:**
       - Se il consumo totale della casa supera **3600W**, il sistema interrompe immediatamente la ricarica della Megane.

    2) **Riconoscimento della Ricarica Megane:**
       - Se il consumo rientra tra **2000W e 3500W**, si presume che la Megane sia in ricarica.
       - Se questo stato dura **almeno 30 minuti senza interruzioni**, viene confermato che il carico appartiene alla Megane.

    3) **Gestione delle Interruzioni Brevi (<3 minuti):**
       - Se il consumo supera **3500W** per meno di **3 minuti**, il tempo di carica è considerata in corso e non viene interrotta.
       - Se il consumo scende sotto **2000W** per meno di **3 minuti**, il tempo di carica è considerata in corso e il timer di ricarica continua.

    4) **Gestione delle Interruzioni Lunghe (>3 minuti):**
       - Se il consumo **supera 3500W per più di 3 minuti** e la carica NON è ancora durata 30 minuti, il timer di ricarica si resetta.
       - Se il consumo **scende sotto 2000W per più di 3 minuti**, il sistema aspetta per verificare se la Megane sta ancora caricando.

    5) **Spegnimento Definitivo:**
       - Se il consumo resta **sotto 2000W per più di 1 ora**, significa che la Megane ha terminato la ricarica e il relè viene disattivato in modo definitivo.

    6) **Riaccensione del Relè:**
       - Se il consumo totale è **inferiore a 1100W** e i dati sono validi, il relè si riaccende per continuare la carica **a meno che non sia stato già spento definitivamente**.

    7) **Tempo di Carica Totale:**
       - La Megane può ricaricarsi per un **minimo di 30 minuti** fino a un **massimo di 6 ore**.
*/


#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <LkMultivibrator.h>

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

// ==========================================
// mdoficare i tempi in base alle necessità
// ==========================================
#define MEZZORA 15*60*1000 // 15 minuti
#define TREMINUTI 3*60*1000
#define UNORA 10*60*1000   // 10 minuti
#define TEMPO_RIACCENSIONE_RELE 180000 // 3 minuti in millisecondi
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

  connectToWiFi();

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

void handlePayload(String payload) {
  int separatorIndex = payload.indexOf('-');
  
  if (separatorIndex > 0) {
    String powerStr = payload.substring(0, separatorIndex);
    String validStr = payload.substring(separatorIndex + 1);
    
    int power = powerStr.toInt();
    int valid = validStr.toInt();
    // stampa su display esterno (seriale altrimenti)
    Serial.println(power);

    // =====================================================
    // gestione dello spegnimento definitivo rele quando la
    // megane ha finito di caricarsi
    // =====================================================
    // if (power >= 2000 && power < 3500) {
    //   // Assorbimento simile a quello della Megane
    //   tempoCaricoGenericoALTO = 0;

    //   if (tempoAccessoMegane == 0) {
    //     // La carica inizia ora
    //     tempoAccessoMegane = millis();
    //     prtn("Assorbimento simile caricamento Megane: rilevato per la 1 volta");
    //   } else if (millis() - tempoAccessoMegane >= MEZZORA) {
    //     // Dopo mezz'ora di carica continua confermiamo che è la Megane
    //     tempoSpentoMegane = 0;
    //     if (!meganeInCarica) {
    //       meganeInCarica = true;
    //       prtn("La Megane è in carica da mezz'ora: confermato");
    //     }
    //   }
    // } else if (power >= 3500) {
    //   // Carico generico ALTO: possibile interruzione della carica Megane
    //   if (tempoCaricoGenericoALTO == 0) {
    //     tempoCaricoGenericoALTO = millis();
    //     prtn("Assorbimento generico ALTO rilevato per la 1 volta");
    //   } else if (millis() - tempoCaricoGenericoALTO >= TREMINUTI) {
    //     if (!meganeInCarica) {
    //       // Resetto la carica perché non ha ancora raggiunto mezz'ora
    //       prtn("Resetto il timer di identificazione carica-megane");
    //       tempoAccessoMegane = 0;
    //     } else {
    //       prtn("La carica della Megane è stata temporaneamente interrotta");
    //     }
    //   }
    // } else {
    //   // Assorbimento sotto i 2000W
    //   if (meganeInCarica) {
    //     if (tempoSpentoMegane == 0) {
    //       tempoSpentoMegane = millis();
    //     } else if (millis() - tempoSpentoMegane > UNORA) {
    //       // La Megane ha finito di caricarsi
    //       meganeInCarica = false;
    //       stopRicaricaMegane = true;
    //       digitalWrite(pinRele, LOW);
    //       prtn("Megane ha terminato la carica: spengo il relè");
    //     }
    //   }
    // }
    // ========================================================
    // Gestione del relè in base alla potenza totale assorbita
    // il relè non si accenderà e spegnerà continuamente se il consumo supera temporaneamente i 3600W, 
    // evitando usura del relè e instabilità nel sistema di ricarica.
    // ========================================================
    // - Attesa di 3 minuti prima di riaccendere il relè
    // Ora, dopo ogni spegnimento, il relè non si riaccende prima di 3 minuti, anche se le condizioni di ricarica tornano favorevoli.
    // - Memorizzazione del tempo di spegnimento
    // Quando il relè viene spento, viene registrato il tempo di spegnimento in tempoSpentoRele.
    // - Verifica prima della riaccensione
    // Il relè si riaccende solo se sono passati almeno 3 minuti dallo spegnimento e se le condizioni di carica lo permettono.
    //
    if (digitalRead(pinRele)) {
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
