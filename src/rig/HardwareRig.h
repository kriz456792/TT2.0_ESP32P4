#pragma once

#include "Rig.h"

struct HardwareRig : Rig {
  void begin() override;
  void update() override;
  void runFullTest() override;
  void developerMode() override;

private:
  void runTest(int direction);
  float voltage_Calculation();
  float amperage_Calculation();
};
