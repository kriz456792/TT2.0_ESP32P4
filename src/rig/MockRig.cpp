#include "MockRig.h"
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <math.h>
#include "src/rig/Globals.h"
#include "src/rig/UsbMessage.h"

extern UsbMessage rx;
extern UsbMessage tx;
extern QueueHandle_t usbRxQueue;
extern QueueHandle_t usbTxQueue;

void MockRig::begin() {}
void MockRig::update() {}
void MockRig::tare_loadCells() {}

void MockRig::calibrate_loadCells() {
  for (int cell = 0; cell < 2; cell++) {
    while (true) {
      if (xQueueReceive(usbRxQueue, &rx, 0)) {
        char* end;
        long user_input = strtol(rx.message, &end, 10);

        if (end != rx.message && *end == '\0' && user_input == READY_) {
          snprintf(tx.message, sizeof(tx.message), "%d", SUCCESS_);
          xQueueSend(usbTxQueue, &tx, 0);
          break;
        }
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    while (true) {
      if (xQueueReceive(usbRxQueue, &rx, 0)) {
        char* end;
        float input = strtof(rx.message, &end);

        if (end != rx.message && *end == '\0' &&
            isfinite(input) && input > 0.0f) {
          snprintf(tx.message, sizeof(tx.message), "%d", SUCCESS_);
          xQueueSend(usbTxQueue, &tx, 0);
          break;
        } else {
          snprintf(tx.message, sizeof(tx.message), "%d", ERROR_);
          xQueueSend(usbTxQueue, &tx, 0);
        }
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Same default factor HardwareRig starts with before EEPROM has a saved value.
    const float calibrationValue = 200.0f;
    strcpy(tx.message, cell == 0 ? "LC_01_CALIBRATED:" : "LC_02_CALIBRATED:");
    dtostrf(calibrationValue, 1, 2, tx.message + strlen(tx.message));
    xQueueSend(usbTxQueue, &tx, 0);
  }
}

void MockRig::runFullTest() {
  // Fake telemetry while PWM steps 1500 -> 1900 | FORWARD.
  for (int pwm = 1500; pwm <= 1900; pwm += 10) {
    float progress = (pwm - 1500) / 400.0f; // 0 at 1500, 1 at 1900
    float amps = sqrtf(progress) * 10.0f;   // parabolic arch curving upward (0 to 10)
    float force = progress * 1000.0f;       // 0 .. 1000

    if (xQueueReceive(usbRxQueue, &rx, 0)) {
        char* end;
        long user_input = strtol(rx.message, &end, 10);
        if (*end == '\0' && user_input == STOP_TEST) {
            return; // Exit the function if STOP_TEST is received
        }
    }

    snprintf(tx.message, sizeof(tx.message),
             "PWM:%d,VOLTS:12,AMPS:%.2f,FORCE:%.0f",
             pwm, amps, force);
    xQueueSend(usbTxQueue, &tx, pdMS_TO_TICKS(10));
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  // Fake telemetry while PWM steps 1500 -> 1100 | REVERSE.
  for (int pwm = 1500; pwm >= 1100; pwm -= 10) {
      float progress = abs(pwm - 1500) / 400.0f; // 0 at 1500, 1 at 1100
      float amps = sqrtf(progress) * 10.0f;   // parabolic arch curving upward (0 to 10)
      float force = progress * 1000.0f;       // 0 .. 1000

      if (xQueueReceive(usbRxQueue, &rx, 0)) {
          char* end;
          long user_input = strtol(rx.message, &end, 10);
          if (*end == '\0' && user_input == STOP_TEST) {
              return; // Exit the function if STOP_TEST is received
          }
      }

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
      float progress = abs(fabsf(speed_PWM - MIDDLE_POINT_PWM)) / 400.0f;
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
