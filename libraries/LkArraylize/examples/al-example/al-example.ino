#include <LkArraylize.h>

struct MyStructure {
   int va;
   long vb;
   double vc;
   char vd[10];
};

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // aspetta la connessione del porto seriale
  }
  Serial.println();
  Serial.println("Examples");

  exampleInt();
  exampleLong();
  exampleUnsignedLong();
  exampleDouble();
  exampleStructure();
}

void loop() {
  // put your main code here, to run repeatedly:
}

void exampleInt() {
  int intValue = -32760;
  uint8_t int_buffer[sizeof(int)];  
  LkArraylize<int> int_o;
  int_o.arraylize(int_buffer, intValue);
  int int_retvalue = int_o.deArraylize(int_buffer);
  Serial.println(int_retvalue); 
}

void exampleLong() {
  long longValue = -2147483648;
  uint8_t long_buffer[sizeof(long)];
  LkArraylize<long> long_o;
  long_o.arraylize(long_buffer, longValue);
  long long_retvalue = long_o.deArraylize(long_buffer);
  Serial.println(long_retvalue);
}

void exampleUnsignedLong() {
  unsigned long ulongValue = 4294967295;
  uint8_t ulong_buffer[sizeof(unsigned long)];
  LkArraylize<unsigned long> ulong_o;
  ulong_o.arraylize(ulong_buffer, ulongValue);
  unsigned long ulong_retvalue = ulong_o.deArraylize(ulong_buffer);
  Serial.println(ulong_retvalue);
}

void exampleDouble() {
  double doubleValue = 3.256712;
  uint8_t double_buffer[sizeof(double)];
  LkArraylize<double> double_o;
  double_o.arraylize(double_buffer, doubleValue);
  double double_retvalue = double_o.deArraylize(double_buffer);
  Serial.println(double_retvalue, 6);
}

void exampleStructure() {
  MyStructure mystructure_value = {12345, 100000, -101000.234, "ciao"};
  uint8_t mystructure_buffer[sizeof(MyStructure)];
  LkArraylize<MyStructure> mystructure_o;
  mystructure_o.arraylize(mystructure_buffer, mystructure_value);
  
  MyStructure mystructure_retvalue = mystructure_o.deArraylize(mystructure_buffer);
  Serial.println(mystructure_retvalue.va);
  Serial.println(mystructure_retvalue.vb);
  Serial.println(mystructure_retvalue.vc);
  Serial.println(mystructure_retvalue.vd);
}
