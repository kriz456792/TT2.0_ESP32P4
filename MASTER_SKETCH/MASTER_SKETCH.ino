#include <Adafruit_ADS1X15.h> 
#include <HX711_ADC.h> //NOTE: config.h sample rate must be set low for higher speeds.
#include <ESP32Servo.h>
#include <EEPROM.h> //TODO: Calibrate Individual Load Cells and save to EEPROM.


//PINS
#define ADS_00 0 //ADS1115 A0 input pin. Assigned to Voltage module.
#define ADS_01 1 //ADS1115 A1 input pin. Assigned to Amperage module.
#define HX711_DOUT_01 5 //ESP32 PIN 5. Assigned to Load Cell Chip
#define HX711_SCK_01 18 //ESP32 PIN 18. Assigned to Load Cell Chip
#define HX711_DOUT_02 2 //ESP32 PIN 2. Assigned to 2nd Load Cell Chip
#define HX711_SCK_02 4 //ESP32 PIN 4. Assigned to 2nd Load Cell Chip
#define SIGNAL_PIN_22 22 //TODO: ASSIGN TASK...
#define SIGNAL_PIN_23 23 //TODO: ASSIGN TASK...
#define ESC_PIN 25 //THRUSTER PIN

//ADDRESSES
#define EEPROM_ADDR_VAL_01 0 //EEPROM ADDRESS... Used to store calibration data | TODO: Calibrate & Store Data
#define EEPROM_ADDR_VAL_02 4 //EEPROM ADDRESS... Used to store calibration data | TODO: Calibrate & Store Data

//PROCEDURAL MACROS
#define SAMPLE_RATE_VOLT 10 //Number of samples taken from module to average a reading. Lowest value is 4.
#define SAMPLE_RATE_AMP 10 //Number of samples taken from module to average a reading. Lowest value is 4.
#define MIDDLE_POINT_PWM 1500 //STOP signal to thruster.
#define DATA_INTERVAL 800 //Every second we collect data based on the number of times we want, default is 20 times per second.
#define FORWARD_ 201
#define REVERSE_ 402
#define RUN_TEST 603
#define CALIBRATE_L_CELLS 804
#define DEVELOPER_MODE 105
#define TARE_CELLS 306

const double VOLTAGE_RESOLUTION {.2}; //TODO: Enter the resolution per voltage. Must wire things up and test.
const double AMPERAGE_RESOLUTION {.060}; //TODO: Enter the resolution per amp. Must wire things up and test.

//OBJECTS...
Adafruit_ADS1X15 ads_module;
HX711_ADC LoadCell_01(HX711_DOUT_01, HX711_SCK_01);
HX711_ADC LoadCell_02(HX711_DOUT_02, HX711_SCK_02);
Servo thruster_motor;

//OTHER VARIABLES...
uint64_t t {0}; //t keeps track of current millis the program has been running.
float volts_{0.0},amps_{0.0};
int speed_PWM {1500};
int speed_percentage {0}; //Change manually for testing. 
int reading_num {10}; //20 collection per second. DEFAULT is 10/Sec

float voltage_Calculation();
float amperage_Calculation();

void setup() {
  Serial.begin(57600); delay(10);
  Serial.print("Setting UP...");

  //OTHER OPTIONS: RATE_ADS1115_8SPS | RATE_ADS1115_32SPS | RATE_ADS1115_475SPS | RATE_ADS1115_860SPS...
  //DEFAULT is 128 Samples-Per-Second. Higher rate makes the program faster but less reliable data...
  ads_module.setDataRate(RATE_ADS1115_860SPS); // 250 SPS

  if(!ads_module.begin()){
    Serial.println("Failed to Initialize ADS Module");
    while(1);
  } else{
    Serial.println("ADS1115 Module Initialized Succesfully!!!");
  }

  ads_module.setGain(GAIN_ONE);

  float calibrationValue_01 {200.0}, calibrationValue_02 {200.0}; //Calibration Values for Load Cell 1 and 2
  unsigned long stabilizing_time {2000};
  boolean _tare { true };
  byte loadcell_01_ready {0}, loadcell_02_ready {0};

  /*USE TO FOLLOWING TO FETCH VALUES FROM EEPROM IF VALUES EXIST IN EEPROM.
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

  Serial.println("Startup is complete...");
}

//MAIN LOOP ***********************************************************
void loop() {

  //OPTIONALITY: RUN TEST -> RUN_TEST | CALIBRATE LOAD CELLS -> CALIBRATE_L_CELLS | DEVELOPER MODE -> DEVELOPER_MODE | TARE CELLS -> TARE_CELLS
  Serial.println("ENTER 603 TO RUN TEST.");
  Serial.println("ENTER 804 TO CALIBRATE LOAD CELLS");
  Serial.println("ENTER 105 TO ENTER DEVELOPER MODE");
  Serial.println("ENTER 306 TO TARE CELLS");
    
  while (Serial.available() == 0) {
    delay(10);
  }

  int user_input = Serial.parseInt();
  int speed_temp {0};

  switch(user_input){
    case RUN_TEST: 
      getUserInputs();

      speed_PWM = MIDDLE_POINT_PWM + speed_percentage * 4;
      runTest(FORWARD_);

      thruster_motor.writeMicroseconds(MIDDLE_POINT_PWM);
      delay(7000);

      speed_PWM = MIDDLE_POINT_PWM - speed_percentage * 4;
      runTest(REVERSE_);

      thruster_motor.writeMicroseconds(MIDDLE_POINT_PWM);   

      break;
    case CALIBRATE_L_CELLS:
          //TODO: DEVELOP CALIBRATION METHOD.
      break;
    case DEVELOPER_MODE:
      while (1) {
        Serial.print("Enter Power %: ");
    
        while (Serial.available() == 0) {
          delay(100);
        }
    
        speed_temp = Serial.parseInt();

        if (speed_temp >= 1 && speed_temp <= 100) {
          Serial.println(speed_temp);
          break;
        }
        else{
          Serial.println(speed_temp);
          Serial.println(" Input NOT Valid. Enter # 1 - 100");
        }
      }
      developer_mode(MIDDLE_POINT_PWM + (speed_temp * 4));

      break;
    case TARE_CELLS:
      // receive command from serial terminal, send 't' to initiate tare operation:
      if (Serial.available() > 0) {
        char inByte = Serial.read();
        if (inByte == 't') {
          LoadCell_01.tareNoDelay();
          LoadCell_02.tareNoDelay();
        }
      }

      //check if last tare operation is complete
      if (LoadCell_01.getTareStatus() == true) {
        Serial.println("TARE LOAD CELL #1 COMPLETE");
      }
      if (LoadCell_02.getTareStatus() == true) {
        Serial.println("TARE LOAD CELL #2 COMPLETE");
      }

      break;
    default:
      Serial.println("ENTER VALID CODE");
  }

}

//METHODS ******************************************************************

void runTest(int direction){

  for (int i{0}; i < reading_num; ){
    LoadCell_01.update();
    LoadCell_02.update();
    
    thruster_motor.writeMicroseconds(speed_PWM); //Thurster running.
    
    if(millis() > t + (DATA_INTERVAL / reading_num)){
      
      switch (direction){
        case FORWARD_: 
          Serial.print("LOAD_CELL 1: "); Serial.print(LoadCell_01.getData());
          break;
        
        case REVERSE_:
          Serial.print("  LOAD_CELL #2:"); Serial.print(LoadCell_02.getData());
          break;
      }

      volts_ = voltage_Calculation();
      amps_ = amperage_Calculation();
      
      Serial.print("  VOLTAGE: "); Serial.print(volts_);
      Serial.print("  AMPERAGE: "); Serial.println(amps_);
      
      t = millis();
      
      i++;
    }

  }

}

void developer_mode(int speed_){

  while(1){
    LoadCell_01.update();
    LoadCell_02.update();

    thruster_motor.writeMicroseconds(speed_);
  
    Serial.print("LOAD_CELL #1: "); Serial.print(LoadCell_01.getData());
    Serial.print("  LOAD_CELL #2: "); Serial.print(LoadCell_02.getData());
    Serial.print("  VOLTAGE: "); Serial.print(voltage_Calculation());
    Serial.print("  AMPERAGE: "); Serial.println(amperage_Calculation());

    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 'e') {
        break;
      }
    }
  }
}

float voltage_Calculation(){
  double min{10.0}, max{0.0}, sample{0.0}, total{0.0}, voltage{0.0};

  for (int i {0}; i < SAMPLE_RATE_VOLT; i++){
    sample = ads_module.readADC_SingleEnded(ADS_00);
    voltage = ads_module.computeVolts(sample);

    if (SAMPLE_RATE_VOLT >= 4){ //Rate must be greater than or equal to 4.
      if (voltage < min) { min = voltage;}
      if (voltage > max) { max = voltage;}
    }
    else{
      Serial.println("Sample rate too low...");
      while(1);
    }

    total += voltage;
  }
  return ((total - (min + max)) / (SAMPLE_RATE_VOLT - 2)) / VOLTAGE_RESOLUTION;
}

float amperage_Calculation(){
  double min{10.0}, max{0.0}, sample{0.0}, total{0.0}, voltage{0.0};

  for (int i {0}; i < SAMPLE_RATE_AMP; i++){
    sample = ads_module.readADC_SingleEnded(ADS_01);
    voltage = ads_module.computeVolts(sample) - (5.1 / 2);

    if(SAMPLE_RATE_AMP >=4){
      if (voltage < min) {min = voltage;}
      if (voltage > max) {max = voltage;}
    }
    else{
      Serial.println("Sample rate too low...");
      while(1);
    }

    total += voltage;

  }

  return (total - (min + max)) / (SAMPLE_RATE_AMP - 2) / AMPERAGE_RESOLUTION;
}

void getUserInputs(){
  while (1) {
    Serial.print("Enter Power %: ");
    
    while (Serial.available() == 0) {
      delay(10);
    }
    
    speed_percentage = Serial.parseInt();

    if (speed_percentage >= 1 && speed_percentage <= 100) {
      Serial.println(speed_percentage);
      break;
    }
    else{
      Serial.println(speed_percentage);
      Serial.println(" Input NOT Valid. Enter # 1 - 100");
    }

  }

  while (1) {
    Serial.print("Enter Desired # of Readings: ");

    while (Serial.available() == 0) {
      delay(10);
    }

    reading_num = Serial.parseInt();

    if (reading_num >= 5 && reading_num <= 30){
      Serial.println(reading_num);
      break;
    }
    else{
      Serial.println(reading_num);
      Serial.println("Input NOT Valid. Enter # 1 - 100");
    }

  }

}









