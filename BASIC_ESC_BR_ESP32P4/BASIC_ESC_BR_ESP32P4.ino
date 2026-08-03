#include <Arduino.h>

constexpr uint8_t escPin = 25;
constexpr uint32_t pwmFreq = 50;     // 50 Hz for servo/ESC
constexpr uint8_t pwmResolution = 12; // 12-bit: duty 0..4095

uint32_t usToDuty(uint32_t us) {
  // 20,000 us period at 50 Hz
  // duty = (pulse_us / 20000 us) * ((2^resolution)-1)
  return (us * ((1 << pwmResolution) - 1)) / 20000;
}

void writeEscMicroseconds(uint32_t us) {
  ledcWrite(escPin, usToDuty(us));
}

void setup() {
  // Attach PWM to the pin
  if (!ledcAttach(escPin, pwmFreq, pwmResolution)) {
    while (true) {
      delay(1000); // stop here if PWM setup fails
    }
  }

  // Neutral / stop for ESC arming
  writeEscMicroseconds(1500);
  delay(7000);
}

void loop() {
  writeEscMicroseconds(1600);  // run
  delay(20000);

  writeEscMicroseconds(1500);  // stop / neutral
  delay(5000);
}