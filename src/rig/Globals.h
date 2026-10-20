#pragma once

//PINS
#define SIGNAL_PIN_22 22 //TODO: ASSIGN TASK...
#define SIGNAL_PIN_23 23 //TODO: ASSIGN TASK...
#define ESC_PIN 14 //THRUSTER PIN

//ADDRESSES
#define EEPROM_ADDR_VAL_01 0 //EEPROM ADDRESS... Used to store calibration data | TODO: Calibrate & Store Data
#define EEPROM_ADDR_VAL_02 4 //EEPROM ADDRESS... Used to store calibration data | TODO: Calibrate & Store Data

//PROCEDURAL MACROS
#define FORWARD_ 201
#define REVERSE_ 402
#define RUN_TEST 603
#define STOP_TEST 704
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
#define MAX_PWM 1900
#define MIN_PWM 1100
#define MIDDLE_POINT_PWM 1500 //STOP signal for thruster.


#define ADS_00 0 //ADS1115 A0 input pin. Assigned to Voltage module.
#define ADS_01 1 //ADS1115 A1 input pin. Assigned to Amperage module.
#define SAMPLE_RATE_VOLT 12 //Number of samples taken from module to average a reading. Lowest value is 4.
#define SAMPLE_RATE_AMP 12 //Number of samples taken from module to average a reading. Lowest value is 4.
#define HX711_DOUT_01 17 //ESP32 PIN 5. Assigned to Load Cell Chip
#define HX711_SCK_01 18 //ESP32 PIN 18. Assigned to Load Cell Chip
#define HX711_DOUT_02 4 //ESP32 PIN . Assigned to 2nd Load Cell Chip
#define HX711_SCK_02 2 //ESP32 PIN . Assigned to 2nd Load Cell Chip


