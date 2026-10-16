#include <Adafruit_ADS1X15.h> 
#include <HX711_ADC.h> //NOTE: config.h sample rate must be set low for higher speeds.
#include <ESP32Servo.h>
#include <EEPROM.h> 
#include "USB.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "src/rig/Rig.h"
#include "src/rig/RigFactory.h"

//PINS
#define HX711_DOUT_01 17 //ESP32 PIN 5. Assigned to Load Cell Chip
#define HX711_SCK_01 18 //ESP32 PIN 18. Assigned to Load Cell Chip
#define HX711_DOUT_02 4 //ESP32 PIN . Assigned to 2nd Load Cell Chip
#define HX711_SCK_02 2 //ESP32 PIN . Assigned to 2nd Load Cell Chip
#define SIGNAL_PIN_22 22 //TODO: ASSIGN TASK...
#define SIGNAL_PIN_23 23 //TODO: ASSIGN TASK...
#define ESC_PIN 14 //THRUSTER PIN

//ADDRESSES
#define EEPROM_ADDR_VAL_01 0 //EEPROM ADDRESS... Used to store calibration data | TODO: Calibrate & Store Data
#define EEPROM_ADDR_VAL_02 4 //EEPROM ADDRESS... Used to store calibration data | TODO: Calibrate & Store Data

//PROCEDURAL MACROS
#define MIDDLE_POINT_PWM 1500 //STOP signal for thruster.

#define FORWARD_ 201
#define REVERSE_ 402
#define RUN_TEST 603
#define CALIBRATE_L_CELLS 804
#define DEVELOPER_MODE 105
#define TARE_CELLS 306
#define INCREASE_PWM 507
#define DECREASE_PWM 708
#define STOP_PWM 909
#define RETURN_EXIT 100
#define LC_01 111
#define LC_02 222
#define READY_ 333
#define SUCCESS_ 444
#define ERROR_ 555

//GLOBAL OBJECTS...
HX711_ADC LoadCell_01(HX711_DOUT_01, HX711_SCK_01);
HX711_ADC LoadCell_02(HX711_DOUT_02, HX711_SCK_02);
QueueHandle_t usbRxQueue; //Receiver Queue
QueueHandle_t usbTxQueue; //Transmission Queue
Adafruit_ADS1X15 ads_module;
Servo thruster_motor;

//STRUCTURES...
typedef struct{
  char message[64];
} UsbMessage;

//OTHER VARIABLES...
uint64_t t {0}; //t keeps track of current millis the program has been running.
int speed_percentage {0}; 
UsbMessage rx, tx, msg; //Structures to hold messages.

//FUNCTION SIGNATURES...
void getUserInputs();
void calibrate_loadCell(HX711_ADC& LoadCell);

Rig* rig = createRig();

//PARALLEL PROGRAMS RUNNING IN A CORE EACH---------------------------------------------------------------------------------------------------------------
void usbTask(void *parameter){

  while(true){
    if(Serial.available()){
      String input = Serial.readStringUntil('\n');

      input.toCharArray(msg.message, sizeof(msg.message));

      xQueueSend(usbRxQueue, &msg, pdMS_TO_TICKS(10));
    }

    if(xQueueReceive(usbTxQueue, &msg, 0)){
      Serial.println(msg.message);
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void applicationTask(void *parameter){

  while(true){

    rig->update();

    if(xQueueReceive(usbRxQueue, &rx, 0)){
      
      char *end;
      long user_input = strtol(rx.message, &end, 10);  //convert message to intiger 
      int speed_temp {0};

      if (*end == '\0')
      {
        switch(user_input)
        {
        case RUN_TEST:
          rig->runFullTest();
          break;
        case CALIBRATE_L_CELLS:
          strcpy(tx.message, "TODO: MODIFY CALIBRATION POSITION.");
          xQueueSend(usbTxQueue, &tx, pdMS_TO_TICKS(10));
          //calibrate_loadCell(LoadCell_01, LC_01);
          //calibrate_loadCell(LoadCell_02, LC_02);
          break;
        case DEVELOPER_MODE:
          rig->developerMode();
          break;
        case TARE_CELLS:
          LoadCell_01.tare();
          LoadCell_02.tare();

          //check if last tare operation is complete
          if (LoadCell_01.getTareStatus() == true) {
            strcpy(tx.message, "TARE LOAD CELL #1 COMPLETE");
            xQueueSend(usbTxQueue, &tx, pdMS_TO_TICKS(10));
          }
          else{
            strcpy(tx.message, "TARE FAILED LC_1");
            xQueueSend(usbTxQueue, &tx, pdMS_TO_TICKS(10));
          }
          if (LoadCell_02.getTareStatus() == true) {
            strcpy(tx.message, "TARE LOAD CELL #2 COMPLETE");
            xQueueSend(usbTxQueue, &tx, pdMS_TO_TICKS(10));
          }
          else {
            strcpy(tx.message, "TARE FAILED LC_2");
            xQueueSend(usbTxQueue, &tx, pdMS_TO_TICKS(10));
          }

          break;
        default:
          strcpy(tx.message, "ENTER A VALID CODE");
          xQueueSend(usbTxQueue, &tx, pdMS_TO_TICKS(10));
        }
      }else {
        strcpy(tx.message, "INVALID INPUT");
        xQueueSend(usbTxQueue, &tx, pdMS_TO_TICKS(10));
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------
void setup() {
  Serial.begin(115200); delay(10);
  Serial.print("Setting UP...");
  rig->begin();

  Serial.println("Startup is complete...");
  usbTxQueue = xQueueCreate(100, sizeof(UsbMessage));
  usbRxQueue = xQueueCreate(100, sizeof(UsbMessage));

  xTaskCreatePinnedToCore(usbTask, "USB", 4096, NULL, 3, NULL,0);
  xTaskCreatePinnedToCore(applicationTask, "Application", 4096, NULL, 2, NULL, 1);
}

//MAIN LOOP ***********************************************************
void loop() {
  
  //RAW CODE TO TEST FOR FUNCTIONALITY..


  // LoadCell_01.update();
  // LoadCell_02.update();

  // thruster_motor.writeMicroseconds(1560);
  
  // Serial.print("LOAD_CELL #1: "); Serial.print(LoadCell_01.getData());
  // Serial.print("  LOAD_CELL #2: "); Serial.print(LoadCell_02.getData());
  // Serial.print("  VOLTAGE: "); Serial.print(voltage_Calculation());
  // Serial.print("  AMPERAGE: "); Serial.println(amperage_Calculation());
  
}

//METHODS ******************************************************************

void calibrate_loadCell(HX711_ADC& LoadCell, int LC_num){
  long known_mass {};

  switch (LC_num) {
    case LC_01:
      strcpy(tx.message, "Place Load Cell 1 on a level stable surface");
      xQueueSend(usbTxQueue, &tx, pdMS_TO_TICKS(10));
      break;
    case LC_02:
      strcpy(tx.message, "Place Load Cell 2 on a level stable surface");
      xQueueSend(usbTxQueue, &tx, pdMS_TO_TICKS(10));
      break;
  }

  while(true){
    //LOOP WILL WAIT UNTIL USER SETS THE TT ON IT'S SIDE AND REMOVES ANY LOAD FROM THE LOADCELL & CONFIRMS.
    LoadCell_01.update();
    LoadCell_02.update();

    if(xQueueReceive(usbRxQueue, &rx, 0)){
      
      char *end;
      long user_input = strtol(rx.message, &end, 10);  //convert message to intiger 

      if (*end == '\0' && user_input == READY_)
      {
        LoadCell.tareNoDelay();
      }

      if (LoadCell.getDataSetStatus() == true) {
        strcpy(tx.message, "READY_");
        xQueueSend(usbTxQueue, &tx, 0); 
        break;
      }else{
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

    if(xQueueReceive(usbRxQueue, &rx, 0)){
      
      char *end;
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

































