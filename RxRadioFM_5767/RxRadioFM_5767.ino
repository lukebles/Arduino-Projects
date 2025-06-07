#include <Wire.h>

#define TEA5767_I2C_ADDR  0x60

// Max stazioni da memorizzare
#define MAX_STAZIONI 60

// Memorizza frequenze e contatore
double stazioni[MAX_STAZIONI];
int stazioniCount = 0;

// Indice corrente selezionato dal potenziometro
int currentIndex = 0;

// -------------------------------------------------------------------
// Impostazione della frequenza sul TEA5767
// -------------------------------------------------------------------
void setFrequency(double freq)
{
  // Calcolo dei registri da inviare al TEA5767 (vedi datasheet)
  unsigned int frequencyB = 4 * (freq * 1000000 + 225000) / 32768;
  
  byte frequencyH = frequencyB >> 8;
  byte frequencyL = frequencyB & 0xFF;
  
  // Invio dei dati via I2C
  Wire.beginTransmission(TEA5767_I2C_ADDR);
  Wire.write(frequencyH);
  Wire.write(frequencyL);
  
  // Settaggi standard: Mute off (B0 = 10110000), Mono/Stereo automatico
  Wire.write(0xB0); 
  // PLL reference freq e altro (0x10 di solito: 32.768kHz ref)
  Wire.write(0x10);
  // Ultimo byte (non sempre usato, a seconda del datasheet)
  Wire.write((byte)0x00);
  
  Wire.endTransmission();
  
  // Breve attesa per stabilizzare la sintonizzazione
  delay(50);
}

// -------------------------------------------------------------------
// Legge il livello segnale dal TEA5767 (0..15)
// -------------------------------------------------------------------
byte getSignalLevel()
{
  // Richiede 5 byte di stato
  Wire.requestFrom(TEA5767_I2C_ADDR, 5);
  
  if (Wire.available() == 5)
  {
    byte data[5];
    for(int i = 0; i < 5; i++){
      data[i] = Wire.read();
    }
    
    // I 4 bit alti di data[3] sono il livello del segnale (0..15)
    byte signalLevel = (data[3] & 0xF0) >> 4;
    return signalLevel;
  }
  else {
    // Se non arrivano dati, restituiamo 0
    return 0;
  }
}

// -------------------------------------------------------------------
// Scansione della banda FM (es. 87.5 -> 108.0 a passi di 0.1 MHz)
// Salva le frequenze con intensità > 9 fino a 30 stazioni
// -------------------------------------------------------------------
void scanStations()
{
  stazioniCount = 0; // Reset conteggio

  for(double freq = 87.5; freq <= 108.0; freq += 0.1)
  {
    setFrequency(freq);         // Sintonizza
    delay(80);                  // Dai tempo di agganciare il segnale
    byte level = getSignalLevel(); // Leggi intensità

    if(level > 9) // Se è un segnale “forte”
    {
      // Memorizza frequenza se abbiamo ancora spazio
      if(stazioniCount < MAX_STAZIONI) {
        stazioni[stazioniCount] = freq;
        stazioniCount++;
      }
    }
  }
}

// -------------------------------------------------------------------
// Setup
// -------------------------------------------------------------------
void setup()
{
  Wire.begin();
  Serial.begin(115200);

  Serial.println("Inizio scansione stazioni...");
  scanStations();

  Serial.print("Trovate ");
  Serial.print(stazioniCount);
  Serial.println(" stazioni forti.");

  // Se troviamo almeno una stazione, sintonizziamoci
  if(stazioniCount > 0) {
    currentIndex = 0; 
    setFrequency(stazioni[currentIndex]);
    Serial.print("Prima stazione: ");
    Serial.println(stazioni[currentIndex]);
  } else {
    Serial.println("Nessuna stazione forte trovata.");
  }
}

// -------------------------------------------------------------------
// Loop - Seleziona stazioni con il potenziometro
// -------------------------------------------------------------------
void loop()
{
  // Se non abbiamo stazioni salvate, non facciamo nulla
  if(stazioniCount == 0) {
    return;
  }

  // Leggi potenziometro
  int potValue = analogRead(A0);

  // Mappa pot (0..1023) al range (0..(stazioniCount-1))
  int newIndex = map(potValue, 0, 1023, 0, stazioniCount - 1);

  // Se cambia stazione, aggiorna
  if(newIndex != currentIndex) {
    currentIndex = newIndex;
    setFrequency(stazioni[currentIndex]);
    Serial.print("Nuova stazione (indice ");
    Serial.print(currentIndex);
    Serial.print("): ");
    Serial.println(stazioni[currentIndex]);

    // Puoi anche leggere la potenza dopo il set
    byte level = getSignalLevel();
    Serial.print("Intensita': ");
    Serial.println(level);
  }

  delay(200);
}
