#include <LkRadioStructure.h>

struct __attribute__((packed)) MyStructure {
    int va;
    long vb;
    double vc;
    char vd[10];
};

void setup() {
    Serial.begin(115200);
    while (!Serial){
      ;
    }
    // Create an instance of MyStructure
    MyStructure original = {1234, 56789L, 3.14159, "hello"};
    uint8_t buffer[sizeof(MyStructure)];

    // Create an instance of LkArraylize for MyStructure
    LkArraylize<MyStructure> serializer;

    // Serialize the structure to the buffer
    serializer.arraylize(buffer, original);

    // Print the serialized buffer
    Serial.println("Serialized buffer:");
    for (size_t i = 0; i < sizeof(buffer); i++) {
        Serial.print(buffer[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    // Deserialize the buffer back to a structure
    MyStructure deserialized = serializer.deArraylize(buffer);

    // Print the deserialized values
    Serial.println("Deserialized values:");
    Serial.print("va: ");
    Serial.println(deserialized.va);
    Serial.print("vb: ");
    Serial.println(deserialized.vb);
    Serial.print("vc: ");
    Serial.println(deserialized.vc, 5);
    Serial.print("vd: ");
    Serial.println(deserialized.vd);
}

void loop() {
    // Nothing to do in the loop
}
