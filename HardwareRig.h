#pragma once

#include "Rig.h"

struct HardwareRig : Rig {
  void begin() override;
  void update() override;
  void runFullTest() override;
  void runDeveloperMode() override;
};
