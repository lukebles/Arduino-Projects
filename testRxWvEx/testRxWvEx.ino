#include <VirtualWireEx.h>

int start_speed = 1900;
int end_speed = 2100;
int step_size = 10;
int current_speed;
int packet_count = 0;
unsigned long last_speed_change_time = 0;
const unsigned long interval = 2*60*1000; //tempo di campionamento

void setup()
{
    Serial.begin(115200);    // Debugging only
    Serial.println("setup");

    pinMode(13, OUTPUT);     // Configura il pin LED come output

    // Inizializza il sistema alla velocità iniziale
    current_speed = start_speed;
    vw_set_ptt_inverted(true); // Required for DR3100
    vw_set_rx_pin(0);
    vw_setup(current_speed);   // Set initial bits per second
    vw_rx_start();             // Start the receiver PLL running

    last_speed_change_time = millis();
}

void loop()
{
    uint8_t buf[VW_MAX_MESSAGE_LEN];
    uint8_t buflen = VW_MAX_MESSAGE_LEN;

    // Controlla se è stato ricevuto un messaggio
    if (vw_get_message(buf, &buflen)) // Non-blocking
    {
        packet_count++; // Incrementa il contatore dei pacchetti ricevut

       
    }

    // Verifica se sono trascorsi 5 minuti (300000 ms)
    if (millis() - last_speed_change_time > interval)
    {
        // Stampa la velocità corrente e il numero di pacchetti ricevuti
        Serial.print("Speed: ");
        Serial.print(current_speed);
        Serial.print(" bps, Packets received: ");
        Serial.println(packet_count);

        // Resetta il contatore dei pacchetti
        packet_count = 0;

        // Incrementa la velocità
        current_speed += step_size;

        // Se la velocità supera end_speed, ferma il test
        if (current_speed > end_speed)
        {
            Serial.println("Test completed.");
            while (true); // Ferma il programma
        }

        // Reinizializza il sistema con la nuova velocità
        vw_setup(current_speed);
  
        // Aggiorna il timer per il prossimo intervallo
        last_speed_change_time = millis();
    }
}
