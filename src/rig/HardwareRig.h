#pragma once

#include "Rig.h"
#include <HX711_ADC.h> 

struct HardwareRig : Rig {
  void begin() override;
  void update() override;
  void runFullTest() override;
  void developerMode() override;
  void calibrate_loadCells() override;
  void tare_loadCells() override;

private:
  void runTest(int direction);
  float voltage_Calculation();
  float amperage_Calculation();
  void calibrateLoadCell(HX711_ADC& LoadCell, int LC_num);
};
