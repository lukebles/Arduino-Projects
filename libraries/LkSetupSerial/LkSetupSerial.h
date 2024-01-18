// LkSetupSerial

/*

The LkSetupSerial library for Arduino is designed to facilitate 
the setup of serial communication. 

Constructor: The class constructor accepts a baudRate parameter 
            with a default value of 115200. 
            This parameter determines the speed of the serial communication.
begin Method: This method initializes the serial communication using the specified 
            baudRate. It also waits for the serial port to be ready before proceeding. 
            Once the connection is established, it sends a message through 
            the serial port. The default message is "setup", 
            but it can be changed by passing a different argument to the method.
Private Variable _baudRate: This variable stores the value of the baudRate 
            used for the serial communication.


=============================

La libreria LkSetupSerial per Arduino è progettata per facilitare 
l'impostazione della comunicazione seriale. 

Costruttore: Il costruttore della classe accetta un parametro baudRate 
            con un valore predefinito di 115200. Questo parametro determina 
            la velocità di comunicazione seriale.
Metodo begin: Questo metodo inizializza la comunicazione seriale utilizzando 
            il baudRate specificato. Inoltre, attende che la porta seriale 
            sia pronta prima di proseguire. 
            Una volta stabilita la connessione, invia un messaggio attraverso 
            la porta seriale. Il messaggio di default è "setup", 
            ma può essere modificato passando un argomento diverso al metodo.
Variabile Privata _baudRate: Questa variabile memorizza il valore del baudRate 
            utilizzato per la comunicazione seriale.


*/

#ifndef LkSetupSerial_h
#define LkSetupSerial_h

#include "Arduino.h"

class LkSetupSerial {
public:
    LkSetupSerial(long baudRate = 115200) {
        _baudRate = baudRate;
    }

    void begin(const String &message = "setup") {
        Serial.begin(_baudRate);
        while (!Serial) {
            ; // aspetta la connessione
        }
        Serial.println("");
        Serial.println(message);
    }

private:
    long _baudRate;
};

#endif
