
//
// programma per l'invio di numeri random su porta seriale
//
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while(!Serial){;}
}

void loop() {
  long valore = random(-4000,4000);
  Serial.print(String(valore)+"\n");
  delay(100);
}
