#include <Adafruit_ADS1X15.h>
/*
* This program uses the ADS1115 module to read analog inputs up to 4 chanels A0, A1, A2, A3.
* The intended use is for a Voltage module that outputs analog signals and Current module ACS712. 
*/
Adafruit_ADS1115 ads; 

void setup() {
  Serial.begin(57600);
  Serial.println("Setting UP...");

  //ads.setDataRate(RATE_ADS1115_8SPS);
  //ads.setDataRate(RATE_ADS1115_32SPS);   //Conversion time ~31.25ms More accurate but slower.
  ads.setDataRate(RATE_ADS1115_250SPS);  //Conversion time ~4.0ms Goldie Locks
  //ads.setDataRate(RATE_ADS1115_860SPS);    //Conversion time ~1.16ms Faster but less accuracy.
  

  if(!ads.begin()){
    Serial.println("Failed to initizalize ADS Module..");
    while(1);
  } else{
    Serial.println("ADS1115 Module Initialized Successfully!!!");
  }
}

void loop() {

  //Module successfully reads the analog signal
  Serial.print("Voltage: ");
  //Serial.println(ads.computeVolts(ads.readADC_SingleEnded(0)));
  Serial.println(ads.readADC_SingleEnded(0));

  Serial.print("Current: ");
  Serial.println(ads.readADC_SingleEnded(1));

  delay(100);

}
