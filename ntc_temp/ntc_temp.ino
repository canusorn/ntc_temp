#include <TM1637.h>

TM1637 tm(D0, D5);

void setup() {
  Serial.begin(115200);
  digitalWrite(D6, HIGH);
  digitalWrite(D7, LOW);
  pinMode(D6, OUTPUT);
  pinMode(D7, OUTPUT);
  tm.begin();
  tm.setBrightness(4);
}

void loop() {
  int Vt = analogRead(A0);
  float Rt = (float)(1023 * 9900) / Vt - 9900;
  float ln = log(Rt / 10000);
  float  temp = (1 / ((ln / 3950) + (1 / (25 + 273.15))));
  temp -= 273.15;
  Serial.println(temp);
  tm.display(String(temp,0) + " C");
  delay(200);
}
