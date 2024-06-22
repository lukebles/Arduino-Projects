void setup() {
  // put your setup code here, to run once:
  pinMode(13,OUTPUT);
  Serial.begin(115200);
}

void loop() {
  // put your main code here, to run repeatedly:
  for (int n = 3000; n< 3200; n+=50){
  tone(13,3000);
  Serial.println(n);
  delay(100);

  }
}
