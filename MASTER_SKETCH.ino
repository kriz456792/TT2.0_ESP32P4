#include "USB.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "src/rig/Rig.h"
#include "src/rig/RigFactory.h"
#include "src/rig/Globals.h"
#include "src/rig/UsbMessage.h"

//GLOBAL OBJECTS...
QueueHandle_t usbRxQueue; //Receiver Queue
QueueHandle_t usbTxQueue; //Transmission Queue

//OTHER VARIABLES...
UsbMessage rx, tx, msg; //Structures to hold messages.

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
          rig->calibrate_loadCells();
          break;
        case DEVELOPER_MODE:
          rig->developerMode();
          break;
        case TARE_CELLS:
          rig->tare_loadCells();
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
void loop() {}

































