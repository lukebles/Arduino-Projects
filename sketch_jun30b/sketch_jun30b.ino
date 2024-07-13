#include <Bounce2.h>

#define BUTTON_PIN 0  
#define LED_PIN 2     

// Crea un'istanza della classe Bounce per il debouncing del pulsante
Bounce debouncer = Bounce();

bool ledState = false;

void setup() {
  // Configura il pin del pulsante come input pullup
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  // Configura il pin del LED come output
  pinMode(LED_PIN, OUTPUT);
  
  // Associa il pin del pulsante all'oggetto debouncer
  debouncer.attach(BUTTON_PIN);
  // Imposta l'intervallo di debouncing (millisecondi)
  debouncer.interval(25);
  
  // Spegne il LED inizialmente
  digitalWrite(LED_PIN, LOW);
}

void loop() {
  // Aggiorna lo stato del debouncer
  debouncer.update();
  
  // Controlla se il pulsante è stato premuto
  if (debouncer.fell()) {
    // Inverti lo stato del LED
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  }
}
