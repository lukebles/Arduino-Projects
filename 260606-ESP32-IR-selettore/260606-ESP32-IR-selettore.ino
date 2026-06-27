#include <WiFi.h>
#include <WebServer.h>
#include <IRremote.hpp>

#define IR_SEND_PIN 26   // Consigliato su ESP32. Eviterei GPIO12.

const char* ssid = "teoles";
const char* password = "Nanoun-9";

WebServer server(80);

struct ComandoIR {
  const char* nome;
  uint16_t address;
  uint16_t command;
  uint32_t rawData;
};

ComandoIR comandiTV[] = {
  { "TASTO_0", 0xDF, 0x9A, 0x659A00DF },
  { "TASTO_1", 0xDF, 0x8E, 0x718E00DF },
  { "TASTO_2", 0xDF, 0x86, 0x798600DF },
  { "TASTO_3", 0xDF, 0x8F, 0x708F00DF },
  { "TASTO_4", 0xDF, 0x92, 0x6D9200DF },
  { "TASTO_5", 0xDF, 0x87, 0x788700DF },
  { "TASTO_6", 0xDF, 0x93, 0x6C9300DF },
  { "TASTO_7", 0xDF, 0x96, 0x699600DF },
  { "TASTO_8", 0xDF, 0x82, 0x7D8200DF },
  { "TASTO_9", 0xDF, 0x97, 0x689700DF },

  { "RADIO_TV", 0xDF, 0xCC, 0x33CC00DF },
  { "PROGRAMMI_SU", 0xDF, 0xDE, 0x21DE00DF },
  { "PROGRAMMI_GIU", 0xDF, 0xD6, 0x29D600DF }
};

const char paginaHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">

  <title>Telecomando TV</title>

  <style>
    body {
      margin: 0;
      padding: 15px;
      background-color: #111111;
      color: white;
      font-family: Arial, Helvetica, sans-serif;
      text-align: center;
      -webkit-user-select: none;
      user-select: none;
    }

    h1 {
      font-size: 26px;
      margin: 10px 0 20px 0;
    }

    #display {
      width: 240px;
      height: 60px;
      margin: 0 auto 20px auto;
      background-color: #222222;
      border: 2px solid #555555;
      border-radius: 10px;
      font-size: 36px;
      line-height: 60px;
      letter-spacing: 6px;
      color: #00ff88;
      overflow: hidden;
    }

    table {
      margin: 0 auto;
      border-collapse: separate;
      border-spacing: 10px;
    }

    button {
      width: 70px;
      height: 65px;
      font-size: 26px;
      border: 0;
      border-radius: 12px;
      background-color: #333333;
      color: white;
      -webkit-appearance: none;
      appearance: none;
    }

    button:active {
      background-color: #777777;
    }

    .invio {
      width: 150px;
      background-color: #0066cc;
      font-size: 24px;
    }

    .funzione {
      width: 70px;
      height: 65px;
      font-size: 18px;
      background-color: #cc5500;
    }

    .prog {
      background-color: #444444;
      font-size: 24px;
    }

    .nota {
      margin-top: 18px;
      font-size: 14px;
      color: #aaaaaa;
    }
  </style>
</head>

<body>

  <h1>Telecomando TV</h1>

  <div id="display"></div>

  <table>
    <tr>
      <td><button onclick="premiNumero('1')">1</button></td>
      <td><button onclick="premiNumero('2')">2</button></td>
      <td><button onclick="premiNumero('3')">3</button></td>
    </tr>

    <tr>
      <td><button onclick="premiNumero('4')">4</button></td>
      <td><button onclick="premiNumero('5')">5</button></td>
      <td><button onclick="premiNumero('6')">6</button></td>
    </tr>

    <tr>
      <td><button onclick="premiNumero('7')">7</button></td>
      <td><button onclick="premiNumero('8')">8</button></td>
      <td><button onclick="premiNumero('9')">9</button></td>
    </tr>

    <tr>
      <td><button onclick="premiNumero('0')">0</button></td>
      <td colspan="2"><button class="invio" onclick="inviaCanale()">INVIO</button></td>
    </tr>

    <tr>
      <td><button class="funzione" onclick="tvRadio()">TV<br>RADIO</button></td>
      <td><button class="funzione prog" onclick="programmiSu()">P+</button></td>
      <td><button class="funzione prog" onclick="programmiGiu()">P-</button></td>
    </tr>
  </table>

  <div class="nota">
    Massimo 4 cifre. Dopo 3 secondi senza INVIO si cancella.
  </div>

  <script type="text/javascript">
    var digit = "";
    var timerCancella = null;

    function aggiornaDisplay() {
      document.getElementById("display").innerHTML = digit;
    }

    function resetTimer() {
      if (timerCancella !== null) {
        clearTimeout(timerCancella);
      }

      timerCancella = setTimeout(function() {
        digit = "";
        aggiornaDisplay();
      }, 3000);
    }

    function premiNumero(n) {
      if (digit.length >= 4) {
        return;
      }

      digit = digit + n;
      aggiornaDisplay();
      resetTimer();
    }

    function richiestaHttp(url) {
      var xhr = new XMLHttpRequest();

      xhr.open("GET", url, true);

      xhr.onreadystatechange = function() {
        if (xhr.readyState == 4) {
          // risposta ricevuta
        }
      };

      xhr.send(null);
    }

    function inviaCanale() {
      if (digit.length == 0) {
        return;
      }

      richiestaHttp("/canale?num=" + encodeURIComponent(digit));

      digit = "";
      aggiornaDisplay();

      if (timerCancella !== null) {
        clearTimeout(timerCancella);
        timerCancella = null;
      }
    }

    function tvRadio() {
      richiestaHttp("/tvradio");
    }

    function programmiSu() {
      richiestaHttp("/programmi_su");
    }

    function programmiGiu() {
      richiestaHttp("/programmi_giu");
    }
  </script>

</body>
</html>
)rawliteral";

void inviaTasto(int indice) {
  Serial.print("Invio: ");
  Serial.println(comandiTV[indice].nome);

  IrSender.sendNEC(
    comandiTV[indice].address,
    comandiTV[indice].command,
    0
  );
}

void inviaCanale(String canale) {
  canale.trim();

  if (canale.length() == 0) {
    return;
  }

  if (canale.length() > 4) {
    return;
  }

  Serial.print("Canale richiesto: ");
  Serial.println(canale);

  for (int i = 0; i < canale.length(); i++) {
    char c = canale.charAt(i);

    if (c >= '0' && c <= '9') {
      int cifra = c - '0';

      inviaTasto(cifra);

      delay(100);   // 1/10 di secondo tra un numero e l'altro
    }
  }
}

void handleRoot() {
  server.send_P(200, "text/html", paginaHTML);
}

void handleCanale() {
  if (!server.hasArg("num")) {
    server.send(400, "text/plain", "Parametro num mancante");
    return;
  }

  String canale = server.arg("num");
  canale.trim();

  if (canale.length() == 0) {
    server.send(400, "text/plain", "Canale vuoto");
    return;
  }

  if (canale.length() > 4) {
    server.send(400, "text/plain", "Massimo 4 cifre");
    return;
  }

  for (int i = 0; i < canale.length(); i++) {
    if (!isDigit(canale.charAt(i))) {
      server.send(400, "text/plain", "Sono ammesse solo cifre");
      return;
    }
  }

  inviaCanale(canale);

  server.send(200, "text/plain", "Canale inviato: " + canale);
}

void handleTvRadio() {
  inviaTasto(10);   // RADIO_TV
  server.send(200, "text/plain", "Comando TV/RADIO inviato");
}

void handleProgrammiSu() {
  inviaTasto(11);   // PROGRAMMI_SU
  server.send(200, "text/plain", "Comando PROGRAMMI_SU inviato");
}

void handleProgrammiGiu() {
  inviaTasto(12);   // PROGRAMMI_GIU
  server.send(200, "text/plain", "Comando PROGRAMMI_GIU inviato");
}

void handleNotFound() {
  server.send(404, "text/plain", "Pagina non trovata");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("Avvio trasmettitore IR...");
  IrSender.begin(IR_SEND_PIN);

  Serial.println("Connessione al router WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connesso.");
  Serial.print("Indirizzo IP assegnato dal router: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/canale", handleCanale);
  server.on("/tvradio", handleTvRadio);
  server.on("/programmi_su", handleProgrammiSu);
  server.on("/programmi_giu", handleProgrammiGiu);
  server.onNotFound(handleNotFound);

  server.begin();

  Serial.println("Server web avviato.");
  Serial.println("Apri dal browser l'indirizzo IP mostrato sopra.");
}

void loop() {
  server.handleClient();
}