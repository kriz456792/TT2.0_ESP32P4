#include "HardwareRig.h"

#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include <HX711_ADC.h>
#include <ESP32Servo.h>

#define ESC_PIN 14 //THRUSTER PIN
#define MIDDLE_POINT_PWM 1500 //STOP signal for thruster.
#define FORWARD_ 201
#define REVERSE_ 402

extern HX711_ADC LoadCell_01;
extern HX711_ADC LoadCell_02;
extern Adafruit_ADS1X15 ads_module;
extern Servo thruster_motor;

void runTest(int direction);
void developer_mode();

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

void HardwareRig::runDeveloperMode() {
  developer_mode();
}
