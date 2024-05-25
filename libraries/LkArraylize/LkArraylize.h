// LkArraylize.h

// This template class provides the functionality to convert variables or structures
// into an array of bytes and vice versa. This functionality is particularly useful
// in communication scenarios, such as transmitting complex data through
// means like radio frequency or serial connections. Using this class,
// data of any type (primitives, structures, classes, etc.) can be 
// converted into a byte array for transmission. Upon reception, 
// these data can be reconverted from the byte array format 
// back to their original type, preserving the integrity and structure of the data.
//
// The class utilizes an internal union to handle the conversion between the generic
// data type T and its byte array representation. This method ensures
// efficient and direct access to the binary data of type T, simplifying 
// processes like data serialization and deserialization for transmission.
//
// Examples of use include, but are not limited to, communication between 
// Arduino devices, storing complex data in EEPROM, or 
// transmitting structured data over network protocols.
//
// Note: Caution is advised when using this class with data types containing 
// pointers or references, as simple byte-wise conversion and reconversion may not 
// preserve the validity of such references.

// ===================================================================================

// Questa classe template fornisce la funzionalità di convertire variabili o strutture
// in un array di byte e viceversa. Tale funzionalità è particolarmente utile
// in scenari di comunicazione, come la trasmissione di dati complessi attraverso
// mezzi come la radiofrequenza o connessioni seriali. Utilizzando questa classe,
// i dati di qualsiasi tipo (primitive, strutture, classi, ecc.) possono essere 
// convertiti in un array di byte per la trasmissione. Dopo la ricezione, 
// questi dati possono essere riconvertiti dal formato di array di byte 
// al loro tipo originale, preservando l'integrità e la struttura dei dati.
//
// La classe utilizza una union interna per gestire la conversione tra il tipo di dato
// generico T e la sua rappresentazione in array di byte. Questo metodo assicura
// un accesso efficiente e diretto ai dati binari del tipo T, semplificando 
// processi come la serializzazione e deserializzazione dei dati per la trasmissione.
//
// Esempi di utilizzo includono, ma non sono limitati a, la comunicazione tra 
// dispositivi Arduino, la memorizzazione di dati complessi in EEPROM, o 
// l'invio di dati strutturati attraverso protocolli di rete.
//
// Nota: Si consiglia di usare questa classe con cautela quando si lavora con tipi
// di dati contenenti puntatori o riferimenti, poiché la semplice conversione 
// a byte e viceversa potrebbe non preservare la validità di tali riferimenti.


#ifndef LKARRAYLIZE_H
#define LKARRAYLIZE_H

#include <Arduino.h>

// declaration
template<class T>
class LkArraylize {
public:
    void arraylize(uint8_t *array, T value);
    T deArraylize(uint8_t *array);

private:
    union WorkBasket {
        T value;
        uint8_t byteArray[sizeof(T)];
    };
    void swapBytes(uint8_t *byteArray, size_t size);
};

// implementation
template<class T>
void LkArraylize<T>::arraylize(uint8_t *array, T value) {
    WorkBasket basket;
    basket.value = value;
    swapBytes(basket.byteArray, sizeof(T)); // Inverti i byte se necessario
    for (size_t i = 0; i < sizeof(T); i++) {
        array[i] = basket.byteArray[i];
    }
}

template<class T>
T LkArraylize<T>::deArraylize(uint8_t *array) {
    WorkBasket basket;
    for (size_t i = 0; i < sizeof(T); i++) {
        basket.byteArray[i] = array[i];
    }
    swapBytes(basket.byteArray, sizeof(T)); // Inverti i byte se necessario
    return basket.value;
}

template<class T>
void LkArraylize<T>::swapBytes(uint8_t *byteArray, size_t size) {
    for (size_t i = 0; i < size / 2; i++) {
        uint8_t temp = byteArray[i];
        byteArray[i] = byteArray[size - 1 - i];
        byteArray[size - 1 - i] = temp;
    }
}

#endif

