#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
// Quando si accende, nonostante si connetta al wifi e legga la potenza, come motivo di OFF dice "WIFI".
/*
  ============================================================
  LOGICA DI FUNZIONAMENTO - CONTROLLO RICARICA MEGANE
  ============================================================

  Il programma legge periodicamente una pagina web nel formato:

      potenza-valid

  Esempio:

      1230-1

  dove:
    - potenza = potenza istantanea totale dell'impianto, in watt;
    - valid   = 1 se il dato è valido.

  Il relè comanda la ricarica della Megane.

  ------------------------------------------------------------
  RELE ON
  ------------------------------------------------------------

  Quando il relè è ON:
    - viene mostrata su display la media delle ultime letture valide;
    - viene conteggiata l'energia della finestra ON da 15 minuti;
    - se la potenza istantanea arriva a 4000 W, il relè va subito OFF;
    - dopo 15 minuti ON, se l'energia supera 900 Wh, il relè va OFF.

  ------------------------------------------------------------
  RELE OFF
  ------------------------------------------------------------

  Quando il relè è OFF:
    - il programma continua a leggere la pagina web;
    - aggiorna la potenza istantanea;
    - aggiorna la media delle ultime letture valide;
    - NON conteggia energia nella finestra ON;
    - alterna su display ogni 3 secondi:
        * motivo OFF;
        * secondi mancanti alla possibile riaccensione;
        * potenza media delle ultime letture valide.

  ------------------------------------------------------------
  CODICI DISPLAY OFF
  ------------------------------------------------------------

      OFF_DATO_NON_VALIDO        -> DATO
      OFF_SUPERAMENTO_4000W      -> SUPE
      OFF_SUPERAMENTO_ENERGIA    -> ENER
      OFF_WIFI                   -> WIFI
      OFF_MEDIA_ALTA             -> MEDI

  Non esiste un codice OFF generico.

  ------------------------------------------------------------
  TIMER OFF
  ------------------------------------------------------------

  Il tempo minimo OFF -> ON è configurabile con:

      TEMPO_MINIMO_OFF_RELE

  Il display mostra i secondi residui nel formato:

      n300
      n297
      n060
      n005
      n000

  Se il timer arriva a n000 ma il relè non può ancora accendersi,
  viene individuato il nuovo motivo reale di blocco e il timer
  riparte da TEMPO_MINIMO_OFF_RELE.
*/

// ============================================================
// CONFIGURAZIONE DISPLAY / DEBUG
// ============================================================

// Serial.println() viene usato per il display.
// Non usarlo per debug.
#define DEBUG 0

const unsigned long BAUD_DISPLAY = 115200;

// ============================================================
// PIN
// ============================================================

#define PIN_LED_WIFI 2   // GPIO2 - LED ESP-01, di solito attivo LOW
#define PIN_RELE 3       // GPIO3/RX

#define LED_ON  LOW
#define LED_OFF HIGH

#define RELE_ON  HIGH
#define RELE_OFF LOW

// Se il tuo modulo relè è attivo LOW, inverti così:
// #define RELE_ON  LOW
// #define RELE_OFF HIGH

// ============================================================
// WIFI / WEB
// ============================================================

const char* ssid = "sid2";
const char* password = "pw12345678";
const char* url = "http://192.168.4.1/megane.html";

const unsigned long reconnectInterval = 8000UL;
const unsigned long visitInterval = 8000UL;

// ============================================================
// TEMPI / SOGLIE
// ============================================================

const unsigned long MINUTO = 60000UL;

// Modifica liberamente questo valore:
// 3 * MINUTO = 3 minuti
// 5 * MINUTO = 5 minuti
// 6 * MINUTO = 6 minuti
const unsigned long TEMPO_MINIMO_OFF_RELE = 5 * MINUTO;

const unsigned long FINESTRA_ENERGIA_ON = 15 * MINUTO;

const int SOGLIA_POTENZA_ISTANTANEA = 4000;

const int ASSORBIMENTO_MEGANE_W = 2200;
const int SOGLIA_MEDIA_PRE_ACCENSIONE = 3600 - ASSORBIMENTO_MEGANE_W; // 1400 W

const float SOGLIA_ENERGIA_15_MIN = 900.0; // 3600 W medi per 15 minuti

const unsigned long DISPLAY_INTERVAL = 3000UL;

// ============================================================
// MEDIA ULTIME LETTURE VALIDE
// ============================================================

#define MAX_VALORI_MEDIA 10

class MediaUltimeLetture {
  int valori[MAX_VALORI_MEDIA];
  int indice = 0;
  int conta = 0;
  long totale = 0;

public:
  void aggiungi(int valore) {
    if (conta == MAX_VALORI_MEDIA) {
      totale -= valori[indice];
    } else {
      conta++;
    }

    valori[indice] = valore;
    totale += valore;

    indice = (indice + 1) % MAX_VALORI_MEDIA;
  }

  void reset() {
    indice = 0;
    conta = 0;
    totale = 0;
  }

  bool disponibile() {
    return conta > 0;
  }

  int media() {
    if (conta == 0) {
      return 99999;
    }

    return (int)((totale + conta / 2) / conta);
  }
};

MediaUltimeLetture mediaPotenza;

// ============================================================
// MOTIVI OFF
// ============================================================

enum MotivoOffRele {
  OFF_NESSUNO,
  OFF_DATO_NON_VALIDO,
  OFF_SUPERAMENTO_4000W,
  OFF_SUPERAMENTO_ENERGIA,
  OFF_WIFI,
  OFF_MEDIA_ALTA
};

MotivoOffRele motivoOffRele = OFF_MEDIA_ALTA;

// ============================================================
// VARIABILI DI STATO
// ============================================================

bool releAcceso = false;

bool ultimoDatoValido = false;
int potenzaAttuale = 0;

unsigned long ultimoTentativoWifi = 0;
unsigned long ultimaVisitaSito = 0;

unsigned long tempoUltimoOffReale = 0;
bool timerOffAttivo = true;

unsigned long inizioFinestraOn = 0;
unsigned long ultimoAggiornamentoEnergia = 0;

float energiaFinestraOnWh = 0.0;

// ============================================================
// VARIABILI DISPLAY
// ============================================================

unsigned long ultimoAggiornamentoDisplay = 0;

// 0 = motivo OFF
// 1 = secondi residui
// 2 = media potenza
byte displayStepOff = 0;

bool forzaAggiornamentoDisplay = true;

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(BAUD_DISPLAY);
  delay(10);

  pinMode(PIN_LED_WIFI, OUTPUT);
  digitalWrite(PIN_LED_WIFI, LED_OFF);

  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_RELE, RELE_OFF);
  releAcceso = false;

  unsigned long adesso = millis();

  ultimoTentativoWifi = adesso;
  ultimaVisitaSito = adesso;

  tempoUltimoOffReale = adesso;
  timerOffAttivo = true;
  motivoOffRele = OFF_WIFI;

  inizioFinestraOn = adesso;
  ultimoAggiornamentoEnergia = adesso;

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);

  connectToWiFi();
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  unsigned long adesso = millis();

  aggiornaEnergiaSeReleOn();

  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(PIN_LED_WIFI, LED_OFF);

    if (releAcceso) {
      spegniRele(OFF_WIFI);
    } else {
      motivoOffRele = OFF_WIFI;
      gestisciTimerScadutoSeOff();
    }

    if (adesso - ultimoTentativoWifi >= reconnectInterval) {
      connectToWiFi();
    }

    aggiornaDisplay();
    return;
  }

  digitalWrite(PIN_LED_WIFI, LED_ON);

  if (adesso - ultimaVisitaSito >= visitInterval) {
    ultimaVisitaSito = adesso;
    processWebData();
  }

  controllaFinestraEnergeticaOn();

  provaAccensioneRele();

  aggiornaDisplay();
}

// ============================================================
// WIFI
// ============================================================

void connectToWiFi() {
  ultimoTentativoWifi = millis();
  WiFi.begin(ssid, password);
}

// ============================================================
// LETTURA PAGINA WEB
// ============================================================

void processWebData() {
  WiFiClient client;
  HTTPClient http;

  http.setTimeout(5000);

  if (!http.begin(client, url)) {
    segnalaDatoNonValido();
    return;
  }

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    payload.trim();
    handlePayload(payload);
  } else {
    segnalaDatoNonValido();
  }

  http.end();
}

// ============================================================
// PAYLOAD: potenza-valid
// ============================================================

void handlePayload(String payload) {
  int separatorIndex = payload.indexOf('-');

  if (separatorIndex <= 0) {
    segnalaDatoNonValido();
    return;
  }

  String powerStr = payload.substring(0, separatorIndex);
  String validStr = payload.substring(separatorIndex + 1);

  powerStr.trim();
  validStr.trim();

  int nuovaPotenza = powerStr.toInt();
  int valid = validStr.toInt();

  aggiornaEnergiaSeReleOn();

  if (valid != 1) {
    segnalaDatoNonValido();
    return;
  }

  ultimoDatoValido = true;
  potenzaAttuale = nuovaPotenza;

  // Aggiorna sempre la media delle ultime letture valide,
  // sia con relè ON sia con relè OFF.
  mediaPotenza.aggiungi(potenzaAttuale);

  ultimoAggiornamentoEnergia = millis();

  if (releAcceso && potenzaAttuale >= SOGLIA_POTENZA_ISTANTANEA) {
    spegniRele(OFF_SUPERAMENTO_4000W);
    return;
  }
}

// ============================================================
// DATO NON VALIDO
// ============================================================

void segnalaDatoNonValido() {
  aggiornaEnergiaSeReleOn();

  ultimoDatoValido = false;

  if (releAcceso) {
    spegniRele(OFF_DATO_NON_VALIDO);
  } else {
    motivoOffRele = OFF_DATO_NON_VALIDO;
    gestisciTimerScadutoSeOff();
  }
}

// ============================================================
// ENERGIA SOLO CON RELE ON
// ============================================================

void aggiornaEnergiaSeReleOn() {
  unsigned long adesso = millis();

  if (!releAcceso || !ultimoDatoValido) {
    ultimoAggiornamentoEnergia = adesso;
    return;
  }

  unsigned long deltaMs = adesso - ultimoAggiornamentoEnergia;

  if (deltaMs == 0) {
    return;
  }

  energiaFinestraOnWh += ((float)potenzaAttuale * (float)deltaMs) / 3600000.0;

  ultimoAggiornamentoEnergia = adesso;
}

// ============================================================
// CONTROLLO FINESTRA 15 MINUTI ON
// ============================================================

void controllaFinestraEnergeticaOn() {
  if (!releAcceso) {
    return;
  }

  unsigned long adesso = millis();

  if (adesso - inizioFinestraOn < FINESTRA_ENERGIA_ON) {
    return;
  }

  aggiornaEnergiaSeReleOn();

  if (energiaFinestraOnWh <= SOGLIA_ENERGIA_15_MIN) {
    energiaFinestraOnWh = 0.0;
    inizioFinestraOn = adesso;
    ultimoAggiornamentoEnergia = adesso;
  } else {
    spegniRele(OFF_SUPERAMENTO_ENERGIA);
  }
}

// ============================================================
// RELE
// ============================================================

void accendiRele() {
  if (releAcceso) {
    return;
  }

  digitalWrite(PIN_RELE, RELE_ON);
  releAcceso = true;

  motivoOffRele = OFF_NESSUNO;
  timerOffAttivo = false;

  unsigned long adesso = millis();

  energiaFinestraOnWh = 0.0;
  inizioFinestraOn = adesso;
  ultimoAggiornamentoEnergia = adesso;

  displayStepOff = 0;
  forzaAggiornamentoDisplay = true;
}

void spegniRele(MotivoOffRele motivo) {
  unsigned long adesso = millis();

  if (releAcceso) {
    aggiornaEnergiaSeReleOn();

    digitalWrite(PIN_RELE, RELE_OFF);
    releAcceso = false;

    motivoOffRele = motivo;
    tempoUltimoOffReale = adesso;
    timerOffAttivo = true;

    energiaFinestraOnWh = 0.0;
    inizioFinestraOn = adesso;
    ultimoAggiornamentoEnergia = adesso;

    // Dopo lo spegnimento, ricostruisco la media senza la Megane.
    mediaPotenza.reset();

    displayStepOff = 0;
    forzaAggiornamentoDisplay = true;

    return;
  }

  // Se è già OFF, aggiorno solo il motivo visualizzato,
  // ma NON riavvio continuamente il timer.
  motivoOffRele = motivo;
}

// ============================================================
// TIMER OFF
// ============================================================

bool timerOffScaduto() {
  if (!timerOffAttivo) {
    return true;
  }

  return (millis() - tempoUltimoOffReale >= TEMPO_MINIMO_OFF_RELE);
}

unsigned long secondiResiduiOff() {
  if (!timerOffAttivo) {
    return 0;
  }

  unsigned long trascorso = millis() - tempoUltimoOffReale;

  if (trascorso >= TEMPO_MINIMO_OFF_RELE) {
    return 0;
  }

  unsigned long residuoMs = TEMPO_MINIMO_OFF_RELE - trascorso;

  unsigned long secondi = (residuoMs + 999UL) / 1000UL;

  if (secondi > 999UL) {
    secondi = 999UL;
  }

  return secondi;
}

void riavviaTimerOff(MotivoOffRele nuovoMotivo) {
  motivoOffRele = nuovoMotivo;
  tempoUltimoOffReale = millis();
  timerOffAttivo = true;

  displayStepOff = 0;
  forzaAggiornamentoDisplay = true;
}

// ============================================================
// MOTIVO REALE BLOCCO
// ============================================================

MotivoOffRele motivoBloccoAttuale() {
  if (WiFi.status() != WL_CONNECTED) {
    return OFF_WIFI;
  }

  if (!ultimoDatoValido) {
    return OFF_DATO_NON_VALIDO;
  }

  if (potenzaAttuale >= SOGLIA_POTENZA_ISTANTANEA) {
    return OFF_SUPERAMENTO_4000W;
  }

  if (!mediaPotenza.disponibile()) {
    return OFF_DATO_NON_VALIDO;
  }

  if (mediaPotenza.media() >= SOGLIA_MEDIA_PRE_ACCENSIONE) {
    return OFF_MEDIA_ALTA;
  }

  return OFF_NESSUNO;
}

// ============================================================
// ACCENSIONE
// ============================================================

void provaAccensioneRele() {
  if (releAcceso) {
    return;
  }

  if (!timerOffScaduto()) {
    return;
  }

  MotivoOffRele motivo = motivoBloccoAttuale();

  if (motivo == OFF_NESSUNO) {
    accendiRele();
  } else {
    // Il timer è arrivato a n000, ma il relè non può accendersi.
    // Si aggiorna il motivo reale e si ricomincia da TEMPO_MINIMO_OFF_RELE.
    riavviaTimerOff(motivo);
  }
}

void gestisciTimerScadutoSeOff() {
  if (releAcceso) {
    return;
  }

  if (!timerOffScaduto()) {
    return;
  }

  MotivoOffRele motivo = motivoBloccoAttuale();

  if (motivo != OFF_NESSUNO) {
    riavviaTimerOff(motivo);
  }
}

// ============================================================
// DISPLAY
// ============================================================

const char* codiceMotivoOff(MotivoOffRele motivo) {
  switch (motivo) {
    case OFF_DATO_NON_VALIDO:
      return "DATO";

    case OFF_SUPERAMENTO_4000W:
      return "SUPE";

    case OFF_SUPERAMENTO_ENERGIA:
      return "ENER";

    case OFF_WIFI:
      return "WIFI";

    case OFF_MEDIA_ALTA:
      return "MEDI";

    case OFF_NESSUNO:
    default:
      // Non dovrebbe mai essere mostrato con relè OFF.
      // Non usare codice generico "OFF".
      return "DATO";
  }
}

void stampaMediaDisplay() {
  if (!mediaPotenza.disponibile()) {
    Serial.println("----");
    return;
  }

  int media = mediaPotenza.media();

  if (media < 0) {
    media = 0;
  }

  if (media > 9999) {
    media = 9999;
  }

  Serial.println(media);
}

void stampaSecondiResiduiDisplay() {
  char buffer[5];
  unsigned long secondi = secondiResiduiOff();

  snprintf(buffer, sizeof(buffer), "n%03lu", secondi);
  Serial.println(buffer);
}

void aggiornaDisplay() {
  unsigned long adesso = millis();

  if (!forzaAggiornamentoDisplay &&
      adesso - ultimoAggiornamentoDisplay < DISPLAY_INTERVAL) {
    return;
  }

  ultimoAggiornamentoDisplay = adesso;
  forzaAggiornamentoDisplay = false;

  if (releAcceso) {
    stampaMediaDisplay();

    displayStepOff = 0;
    return;
  }

  switch (displayStepOff) {
    case 0:
      Serial.println(codiceMotivoOff(motivoOffRele));
      break;

    case 1:
      stampaSecondiResiduiDisplay();
      break;

    case 2:
      stampaMediaDisplay();
      break;
  }

  displayStepOff++;

  if (displayStepOff > 2) {
    displayStepOff = 0;
  }
}