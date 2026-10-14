#pragma once

struct Rig {
  virtual void begin() = 0;
  virtual void update() = 0;
  virtual void runFullTest() = 0;
  virtual void runDeveloperMode() = 0;
  virtual ~Rig() = default;
};
