#include <TM1637.h>
TM1637 tm(D1, D2);
void setup() {
  Serial.begin(115200);
  digitalWrite(D3, HIGH);
  digitalWrite(D4, LOW);
  pinMode(D3, OUTPUT);
  pinMode(D4, OUTPUT);
  tm.begin();
  tm.setBrightness(4);
}

void loop() {
  int Vt = analogRead(A0);
  float Rt = (float)(1023 * 10000) / Vt - 10000;
  float ln = log(Rt / 10000);
  float  temp = (1 / ((ln / 3950) + (1 / (25 + 273.15))));
  temp -= 273.15;
  Serial.println(temp);
  tm.display(String(temp,0) + " C");
  delay(200);
}
