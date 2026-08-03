#include <ESP32Servo.h>
#define ESC_PIN 25

Servo esc;

void setup() {
  esc.setPeriodHertz(100);           // servo/ESC frequency
  esc.attach(ESC_PIN, 1100, 1900);   // min/max pulse width
  esc.writeMicroseconds(1500);      // neutral / stop
  delay(7000);                      // allow ESC to arm
}

void loop() {
  esc.writeMicroseconds(1600);
  delay(10000);

  esc.writeMicroseconds(1500);
  delay(2000);

  esc.writeMicroseconds(1400);
  delay(5000);

  esc.writeMicroseconds(1500);
  delay(3000);
}