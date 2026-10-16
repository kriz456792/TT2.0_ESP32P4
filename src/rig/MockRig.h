#pragma once

#include "Rig.h"

struct MockRig : Rig {
  void begin() override;
  void update() override;
  void runFullTest() override;
  void developerMode() override;
};
