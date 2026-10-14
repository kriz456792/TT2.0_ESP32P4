#include "HardwareRig.h"

#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include <HX711_ADC.h>
#include <ESP32Servo.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define ESC_PIN 14 //THRUSTER PIN
#define MIDDLE_POINT_PWM 1500 //STOP signal for thruster.
#define FORWARD_ 201
#define REVERSE_ 402
#define INCREASE_PWM 507
#define DECREASE_PWM 708
#define STOP_PWM 909
#define RETURN_EXIT 100
#define ADS_00 0 //ADS1115 A0 input pin. Assigned to Voltage module.
#define ADS_01 1 //ADS1115 A1 input pin. Assigned to Amperage module.
#define SAMPLE_RATE_VOLT 12 //Number of samples taken from module to average a reading. Lowest value is 4.
#define SAMPLE_RATE_AMP 12 //Number of samples taken from module to average a reading. Lowest value is 4.

constexpr float VOLTAGE_RESOLUTION = .2f;
constexpr float ACS712_ZERO_VOLTAGE = 2.55f; //midpoint reading on ads from Amp module
constexpr float AMPERAGE_RESOLUTION = 0.066f;  // 66 mV/A

typedef struct {
  char message[64];
} UsbMessage;

extern HX711_ADC LoadCell_01;
extern HX711_ADC LoadCell_02;
extern Adafruit_ADS1X15 ads_module;
extern Servo thruster_motor;
extern UsbMessage rx;
extern UsbMessage tx;
extern QueueHandle_t usbRxQueue;
extern QueueHandle_t usbTxQueue;
extern uint64_t t;

void HardwareRig::begin() {
  //OTHER OPTIONS: RATE_ADS1115_8SPS | RATE_ADS1115_32SPS | RATE_ADS1115_475SPS | RATE_ADS1115_860SPS...
  //DEFAULT is 128 Samples-Per-Second. Higher rate makes the program faster but less reliable data...

  ads_module.setDataRate(RATE_ADS1115_860SPS); // 250 SPS

  if(!ads_module.begin()){
    Serial.println("Failed to Initialize ADS Module");
    while(1);
  } else{
    Serial.println("ADS1115 Module Initialized Succesfully!!!");
  }

  ads_module.setGain(GAIN_ONE); // Gain sets the ADS1115 input voltage range; higher gain gives better resolution but lowers the maximum measurable voltage.

  float calibrationValue_01 {200.0}, calibrationValue_02 {200.0}; //Calibration Values for Load Cell 1 and 2
  unsigned long stabilizing_time {2000};
  boolean _tare { true };
  byte loadcell_01_ready {0}, loadcell_02_ready {0};

  /*USE TO FOLLOWING TO FETCH VALUES FROM EEPROM IF VALUES EXIST IN EEPROM. //TODO: UNCOMMENT, CALIBRATE LOAD CELLS & SAVE TO EEPROM
  EEPROM.begin(512);
  EEPROM.get(EEPROM_ADDR_VAL_01, calibrationValue_01);
  EEPROM.get(EEPROM_ADDR_VAL_02, calibrationValue_02);
  */

  LoadCell_01.begin();
  LoadCell_02.begin();

  while (loadcell_01_ready + loadcell_02_ready < 2) {
    if (!loadcell_01_ready){ loadcell_01_ready = LoadCell_01.startMultiple(stabilizing_time, _tare); }
    if (!loadcell_02_ready){ loadcell_02_ready = LoadCell_02.startMultiple(stabilizing_time, _tare); }
  }

  if (LoadCell_01.getTareTimeoutFlag()){
    Serial.println("Timeout, check MCU>HX711 no.01 wiring and pins");
  }
  if (LoadCell_02.getTareTimeoutFlag()){
    Serial.println("Timeout, check MCU>HX711 no.02 wiring and pins");
  }

  LoadCell_01.setCalFactor(calibrationValue_01);
  LoadCell_02.setCalFactor(calibrationValue_02);

  thruster_motor.setPeriodHertz(100);
  thruster_motor.attach(ESC_PIN, 1100, 1900); //PIN | min | max 
  thruster_motor.writeMicroseconds(MIDDLE_POINT_PWM); //middlepoint is STOP.
  delay(7000); // allow thruster to settle
}

void HardwareRig::update() {
  LoadCell_01.update();
  LoadCell_02.update();
}

void HardwareRig::runFullTest() {
  runTest(FORWARD_);
  thruster_motor.writeMicroseconds(MIDDLE_POINT_PWM);
  delay(3000); // Allow water to settle.
  runTest(REVERSE_);
  thruster_motor.writeMicroseconds(MIDDLE_POINT_PWM);
}

void HardwareRig::runTest(int direction){
  float amps_ {0.0}, volts_ {0.0}, force_ {0.0};
  int reading_num {10};

  t = millis();
  int speed_PWM = MIDDLE_POINT_PWM;

  for (int j {0}; j < 40; j++){

    switch (direction){
      case FORWARD_:
        speed_PWM += 10;
        break;

      case REVERSE_:
        speed_PWM -= 10;
        break;
    }

    for (int i{0}; i < reading_num; ){
      LoadCell_01.update();
      LoadCell_02.update();

      thruster_motor.writeMicroseconds(speed_PWM); //Thurster running.

      if(millis() > t + 50){

        switch (direction){
          case FORWARD_:
            force_ += LoadCell_01.getData();
            break;

          case REVERSE_:
            force_ += LoadCell_02.getData();
            break;
        }

        volts_ += voltage_Calculation();
        amps_ += amperage_Calculation();

        t = millis();

        i++;
      }

    }

    force_ /= reading_num;
    volts_ /= reading_num;
    amps_ /= reading_num;

    snprintf(tx.message, sizeof(tx.message), "PWM:%d,FORCE:%.2f,VOLTS:%.2f,AMPS:%.2f", speed_PWM, force_, volts_, amps_);
    xQueueSend(usbTxQueue, &tx, pdMS_TO_TICKS(10));
  }
}

void HardwareRig::developerMode() {
  float amps_ {0.0}, volts_ {0.0}, force_LC1 {0.0}, force_LC2 {0.0};
  int speed_PWM {MIDDLE_POINT_PWM};
  bool active_ {true};

  t = millis();

  while(active_){
    LoadCell_01.update();
    LoadCell_02.update();

    if(xQueueReceive(usbRxQueue, &rx, 0)){
      char *end;
      long user_input = strtol(rx.message, &end, 10);

      if (*end == '\0')
      {
        switch(user_input)
        {
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
            active_ = false;
            break;
          default:
            break;
        }
      }
    }

    thruster_motor.writeMicroseconds(speed_PWM);

    if(millis() > t + 50){
      force_LC1 = LoadCell_01.getData();
      force_LC2 = LoadCell_02.getData();
      volts_ = voltage_Calculation();
      amps_ = amperage_Calculation();

      t = millis();
      snprintf(tx.message, sizeof(tx.message), "PWM:%d,FORCE_LC1:%.2f,FORCE_LC2:%.2f,VOLTS:%.2f,AMPS:%.2f", speed_PWM, force_LC1, force_LC2, volts_, amps_);
      xQueueSend(usbTxQueue, &tx, pdMS_TO_TICKS(10));
    }
  }
}

float HardwareRig::voltage_Calculation() {

  if (SAMPLE_RATE_VOLT < 4) {
    return 0.0f;
  }

  float minVoltage = 0.0f;
  float maxVoltage = 0.0f;
  float total = 0.0f;

  for (int i = 0; i < SAMPLE_RATE_VOLT; i++) {

    int16_t rawSample = ads_module.readADC_SingleEnded(ADS_00);
    float voltage = ads_module.computeVolts(rawSample);

    // Initialize min/max from the first real reading
    if (i == 0) {
      minVoltage = voltage;
      maxVoltage = voltage;
    }
    else {
      if (voltage < minVoltage) minVoltage = voltage;
      if (voltage > maxVoltage) maxVoltage = voltage;
    }

    total += voltage;
  }

  float average = (total - minVoltage - maxVoltage) / (SAMPLE_RATE_VOLT - 2);

  return average / VOLTAGE_RESOLUTION;
}

float HardwareRig::amperage_Calculation() {

  if (SAMPLE_RATE_AMP < 4) {
    return 0.0f;
  }

  float minVoltage = 0.0f;
  float maxVoltage = 0.0f;
  float total = 0.0f;

  for (int i = 0; i < SAMPLE_RATE_AMP; i++) {

    int16_t rawSample = ads_module.readADC_SingleEnded(ADS_01);

    // Convert ADC reading to voltage and remove the ACS712 zero-current offset
    float voltage = ads_module.computeVolts(rawSample) - ACS712_ZERO_VOLTAGE;

    if (i == 0) {
      minVoltage = voltage;
      maxVoltage = voltage;
    }
    else {
      if (voltage < minVoltage) minVoltage = voltage;
      if (voltage > maxVoltage) maxVoltage = voltage;
    }

    total += voltage;
  }

  float averageVoltage =
      (total - minVoltage - maxVoltage) /
      (SAMPLE_RATE_AMP - 2);

  return averageVoltage / AMPERAGE_RESOLUTION;
}
