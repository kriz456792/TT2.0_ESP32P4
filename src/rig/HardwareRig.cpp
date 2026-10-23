#include "HardwareRig.h"
#include <EEPROM.h> 
#include <Arduino.h>
#include <Adafruit_ADS1X15.h> 
#include <HX711_ADC.h> //NOTE: config.h sample rate must be set low for higher speeds.
#include <ESP32Servo.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "src/rig/Globals.h"
#include "src/rig/UsbMessage.h"

constexpr float VOLTAGE_RESOLUTION = .2f;
constexpr float ACS712_ZERO_VOLTAGE = 2.55f; //midpoint reading on ads from Amp module
constexpr float AMPERAGE_RESOLUTION = 0.066f;  // 66 mV/A
bool stopTestFlag = false;

HX711_ADC LoadCell_01(HX711_DOUT_01, HX711_SCK_01);
HX711_ADC LoadCell_02(HX711_DOUT_02, HX711_SCK_02);
Adafruit_ADS1X15 ads_module;
Servo thruster_motor;

uint64_t t{ 0 }; //t keeps track of current millis the program has been running.

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

  if (stopTestFlag) {
	  stopTestFlag = false; // Reset the flag for future tests
	  return; // Exit if stopTestFlag is set
  }

  delay(3000); // Allow water to settle.
  runTest(REVERSE_);
  thruster_motor.writeMicroseconds(MIDDLE_POINT_PWM);

  if (stopTestFlag) {
      stopTestFlag = false; // Reset the flag for future tests
      return; // Exit if stopTestFlag is set
  }
}

void HardwareRig::runTest(int direction){
  float amps_ {0.0}, volts_ {0.0}, force_ {0.0};

  t = millis();
  int speed_PWM = MIDDLE_POINT_PWM;

  for (int j {0}; j <= 40; j++){

    for (int i{0}; i < 10; ){
      thruster_motor.writeMicroseconds(speed_PWM); //Thurster running.

      LoadCell_01.update();
      LoadCell_02.update();

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

      if (xQueueReceive(usbRxQueue, &rx, 0)) {
		  char* end;
		  long user_input = strtol(rx.message, &end, 10);
		  if (*end == '\0' && user_input == STOP_TEST) {
			  thruster_motor.writeMicroseconds(MIDDLE_POINT_PWM); // Stop the thruster
              stopTestFlag = true;
			  return; // Exit the function if STOP_TEST is received
		  }
      }

    }

    force_ /= 10;
    volts_ /= 10;
    amps_ /= 10;

    snprintf(tx.message, sizeof(tx.message), "PWM:%d,FORCE:%.2f,VOLTS:%.2f,AMPS:%.2f", speed_PWM, force_, volts_, amps_);
    xQueueSend(usbTxQueue, &tx, pdMS_TO_TICKS(10));

    switch (direction) {
    case FORWARD_:
        speed_PWM += 10;
        break;

    case REVERSE_:
        speed_PWM -= 10;
        break;
    }
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
              if (speed_PWM < MAX_PWM) {
                  speed_PWM += 10;
              }
            break;
          case DECREASE_PWM:
            if (speed_PWM > MIN_PWM) {
              speed_PWM -= 10;
            }
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

void HardwareRig::calibrate_loadCells() {
    calibrateSingleLoadCell(LoadCell_01, LC_01);
    calibrateSingleLoadCell(LoadCell_02, LC_02);
}

void HardwareRig::calibrateSingleLoadCell(HX711_ADC& LoadCell, int LC_num) {
 
    long known_mass{};

    // switch (LC_num) {
    // case LC_01:
    //     strcpy(tx.message, "Place Load Cell 1 on a level stable surface");
    //     xQueueSend(usbTxQueue, &tx, pdMS_TO_TICKS(10));
    //     break;
    // case LC_02:
    //     strcpy(tx.message, "Place Load Cell 2 on a level stable surface");
    // xQueueSend(usbTxQueue, &tx, pdMS_TO_TICKS(10));
    //     break;
    // }

    while (true) {
        //LOOP WILL WAIT UNTIL USER SETS THE TT ON IT'S SIDE AND REMOVES ANY LOAD FROM THE LOADCELL & CONFIRMS.
        LoadCell_01.update();
        LoadCell_02.update();

        if (xQueueReceive(usbRxQueue, &rx, 0)) {

            char* end;
            long user_input = strtol(rx.message, &end, 10);  //convert message to intiger 

            if (*end == '\0' && user_input == READY_)
            {
                LoadCell.tareNoDelay();
            }

            if (LoadCell.getDataSetStatus() == true) {
                strcpy(tx.message, "READY_");
                xQueueSend(usbTxQueue, &tx, 0);
                break;
            }
            else {
                strcpy(tx.message, "ERROR_");
                xQueueSend(usbTxQueue, &tx, 0);
                return;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    //Serial.println("Now place known mass on the loadcell 01.");
    //Serial.println("Then send the weight of this mass (i.e 100.0) from serial monitor.");

    while (true) {
        //LOOP WILL WAIT UNTIL KNOWN MASS IS ON TT AND USER HAS CONFIRMED KNOWN MASS IS ON LOADCELL
        LoadCell.update();

        if (xQueueReceive(usbRxQueue, &rx, 0)) {

            char* end;
            known_mass = strtol(rx.message, &end, 10);  //convert message to intiger 

            if (*end == '\0')
            {
                strcpy(tx.message, "SUCCESS_");
                xQueueSend(usbTxQueue, &tx, 0);
                break;
            }
        }
    }

    LoadCell.refreshDataSet();
    float newCalibrationValue = LoadCell.getNewCalibration(known_mass);

#if defined(ESP8266) || defined(ESP32)
    EEPROM.begin(512);
#endif
    switch (LC_num) {
    case LC_01:
        EEPROM.put(EEPROM_ADDR_VAL_01, newCalibrationValue);
        break;
    case LC_02:
        EEPROM.put(EEPROM_ADDR_VAL_02, newCalibrationValue);
        break;
    }
#if defined(ESP8266) || defined(ESP32)
    EEPROM.commit();
#endif
    switch (LC_num) {
    case LC_01:
        EEPROM.get(EEPROM_ADDR_VAL_01, newCalibrationValue);
        break;
    case LC_02:
        EEPROM.get(EEPROM_ADDR_VAL_02, newCalibrationValue);
        break;
    }
}

void HardwareRig::tare_loadCells() {
	LoadCell_01.tare();
	LoadCell_02.tare();

	while (true) {
		LoadCell_01.update();
		LoadCell_02.update();
		if (LoadCell_01.getTareStatus() && LoadCell_02.getTareStatus()) {
			strcpy(tx.message, "TARE_SUCCESS");
			xQueueSend(usbTxQueue, &tx, 0);
			break;
		}
		else {
			strcpy(tx.message, "TARE_ERROR");
			xQueueSend(usbTxQueue, &tx, 0);
			return;
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}


