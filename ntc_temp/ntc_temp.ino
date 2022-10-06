

void setup() {
  Serial.begin(115200);
}

void loop() {
  int Vt = analogRead(A0);
  float Rt = (float)(1023 * 10000) / Vt - 10000;
  float ln = log(Rt / 10000);
  float  temp = (1 / ((ln / 3950) + (1 / (25 + 273.15))));
  temp -= 273.15;
  Serial.println(temp);
  delay(200);
}
