#pragma once

struct Rig {
  virtual void begin() = 0;
  virtual void update() = 0;
  virtual void runFullTest() = 0;
  virtual void developerMode() = 0;
  virtual void calibrate_loadCells() = 0;
  virtual void tare_loadCells() = 0;
  virtual ~Rig() = default;
};
