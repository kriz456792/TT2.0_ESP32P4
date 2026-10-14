#include "MockRig.h"

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <math.h>

typedef struct {
  char message[64];
} UsbMessage;

#define MIDDLE_POINT_PWM 1500
#define INCREASE_PWM 507
#define DECREASE_PWM 708
#define STOP_PWM 909
#define RETURN_EXIT 100

extern UsbMessage rx;
extern UsbMessage tx;
extern QueueHandle_t usbRxQueue;
extern QueueHandle_t usbTxQueue;

void MockRig::begin() {}

void MockRig::update() {}

void MockRig::runFullTest() {
  // Fake telemetry while PWM steps 1500 -> 1900.
  for (int pwm = 1500; pwm <= 1900; pwm += 10) {
    float progress = (pwm - 1500) / 400.0f; // 0 at 1500, 1 at 1900
    float amps = sqrtf(progress) * 10.0f;   // parabolic arch curving upward (0 to 10)
    float force = progress * 1000.0f;       // 0 .. 1000

    snprintf(tx.message, sizeof(tx.message),
             "PWM:%d,VOLTS:12,AMPS:%.2f,FORCE:%.0f",
             pwm, amps, force);
    xQueueSend(usbTxQueue, &tx, pdMS_TO_TICKS(10));
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void MockRig::developerMode() {
  int speed_PWM = MIDDLE_POINT_PWM;
  bool active = true;
  unsigned long lastSample = millis();

  while (active) {
    if (xQueueReceive(usbRxQueue, &rx, 0)) {
      char *end;
      long user_input = strtol(rx.message, &end, 10);

      if (*end == '\0') {
        switch (user_input) {
          case INCREASE_PWM:
            speed_PWM += 10;
            break;
          case DECREASE_PWM:
            speed_PWM -= 10;
            break;
          case STOP_PWM:
            speed_PWM = MIDDLE_POINT_PWM;
            break;
          case RETURN_EXIT:
            speed_PWM = MIDDLE_POINT_PWM;
            active = false;
            break;
          default:
            break;
        }
      }
    }

    if (millis() - lastSample >= 50) {
      float progress = fabsf(speed_PWM - MIDDLE_POINT_PWM) / 400.0f;
      float amps = sqrtf(progress) * 10.0f;
      float force = progress * 1000.0f;
      float force_LC1 = speed_PWM > MIDDLE_POINT_PWM ? force : 0.0f;
      float force_LC2 = speed_PWM < MIDDLE_POINT_PWM ? force : 0.0f;

      snprintf(tx.message, sizeof(tx.message),
               "PWM:%d,FORCE_LC1:%.2f,FORCE_LC2:%.2f,VOLTS:12,AMPS:%.2f",
               speed_PWM, force_LC1, force_LC2, amps);
      xQueueSend(usbTxQueue, &tx, pdMS_TO_TICKS(10));
      lastSample = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
