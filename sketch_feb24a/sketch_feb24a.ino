
//
// programma per l'invio di numeri random su porta seriale
//
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while(!Serial){;}
}

void loop() {
  long valore = random(5,31);
  Serial.print(String(valore)+"\n");
  delay(10);
}
