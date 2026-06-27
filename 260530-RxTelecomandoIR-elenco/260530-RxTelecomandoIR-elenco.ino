#include <IRremote.hpp>

#define IR_RECEIVE_PIN 2

struct ComandoIR {
  const char* nome;
  decode_type_t protocollo;
  uint16_t address;
  uint16_t command;
  uint32_t rawData;
  bool acquisito;
};

ComandoIR comandi[] = {
  {"TASTO_0", UNKNOWN, 0, 0, 0, false},
  {"TASTO_1", UNKNOWN, 0, 0, 0, false},
  {"TASTO_2", UNKNOWN, 0, 0, 0, false},
  {"TASTO_3", UNKNOWN, 0, 0, 0, false},
  {"TASTO_4", UNKNOWN, 0, 0, 0, false},
  {"TASTO_5", UNKNOWN, 0, 0, 0, false},
  {"TASTO_6", UNKNOWN, 0, 0, 0, false},
  {"TASTO_7", UNKNOWN, 0, 0, 0, false},
  {"TASTO_8", UNKNOWN, 0, 0, 0, false},
  {"TASTO_9", UNKNOWN, 0, 0, 0, false},
  {"RADIO_TV", UNKNOWN, 0, 0, 0, false}
};

const int numeroTasti = sizeof(comandi) / sizeof(comandi[0]);

int tastoCorrente = 0;

void stampaRichiesta() {
  Serial.println();
  Serial.println("-----------------------------------");
  Serial.print("Premi sul telecomando il tasto: ");
  Serial.println(comandi[tastoCorrente].nome);
  Serial.println("-----------------------------------");
}

void stampaRisultatoFinale() {
  Serial.println();
  Serial.println("===================================");
  Serial.println("ACQUISIZIONE COMPLETATA");
  Serial.println("===================================");
  Serial.println();

  Serial.println("ELENCO VALORI RILEVATI:");
  Serial.println();

  for (int i = 0; i < numeroTasti; i++) {
    Serial.print(comandi[i].nome);
    Serial.print("  Protocollo: ");
    Serial.print(getProtocolString(comandi[i].protocollo));

    Serial.print("  Address: 0x");
    Serial.print(comandi[i].address, HEX);

    Serial.print("  Command: 0x");
    Serial.print(comandi[i].command, HEX);

    Serial.print("  RawData: 0x");
    Serial.println(comandi[i].rawData, HEX);
  }

  Serial.println();
  Serial.println("===================================");
  Serial.println("CODICE COPIABILE PER ALTRO PROGRAMMA");
  Serial.println("===================================");
  Serial.println();

  Serial.println("struct ComandoIR {");
  Serial.println("  const char* nome;");
  Serial.println("  uint16_t address;");
  Serial.println("  uint16_t command;");
  Serial.println("  uint32_t rawData;");
  Serial.println("};");
  Serial.println();

  Serial.println("ComandoIR comandiTV[] = {");

  for (int i = 0; i < numeroTasti; i++) {
    Serial.print("  {\"");
    Serial.print(comandi[i].nome);
    Serial.print("\", 0x");
    Serial.print(comandi[i].address, HEX);
    Serial.print(", 0x");
    Serial.print(comandi[i].command, HEX);
    Serial.print(", 0x");
    Serial.print(comandi[i].rawData, HEX);
    Serial.print("}");

    if (i < numeroTasti - 1) {
      Serial.print(",");
    }

    Serial.println();
  }

  Serial.println("};");

  Serial.println();
  Serial.println("Per trasmettere un tasto NEC:");
  Serial.println("IrSender.sendNEC(address, command, 0);");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("===================================");
  Serial.println("ACQUISIZIONE COMANDI TELECOMANDO TV");
  Serial.println("===================================");
  Serial.println();
  Serial.println("Collega il ricevitore IR al pin D2.");
  Serial.println("Apri il Monitor Seriale a 115200 baud.");
  Serial.println();

  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

  stampaRichiesta();
}

void loop() {
  if (tastoCorrente >= numeroTasti) {
    return;
  }

  if (IrReceiver.decode()) {

    if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
      IrReceiver.resume();
      return;
    }

    comandi[tastoCorrente].protocollo = IrReceiver.decodedIRData.protocol;
    comandi[tastoCorrente].address = IrReceiver.decodedIRData.address;
    comandi[tastoCorrente].command = IrReceiver.decodedIRData.command;
    comandi[tastoCorrente].rawData = IrReceiver.decodedIRData.decodedRawData;
    comandi[tastoCorrente].acquisito = true;

    Serial.print("Acquisito: ");
    Serial.print(comandi[tastoCorrente].nome);
    Serial.print("  Protocollo: ");
    Serial.print(getProtocolString(comandi[tastoCorrente].protocollo));
    Serial.print("  Address: 0x");
    Serial.print(comandi[tastoCorrente].address, HEX);
    Serial.print("  Command: 0x");
    Serial.println(comandi[tastoCorrente].command, HEX);

    tastoCorrente++;

    IrReceiver.resume();

    delay(2000);

    if (tastoCorrente < numeroTasti) {
      stampaRichiesta();
    } else {
      stampaRisultatoFinale();
    }
  }
}