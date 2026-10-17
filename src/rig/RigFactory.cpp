#include "RigFactory.h"
#include "Rig.h"
#include "MockRig.h"
#include "HardwareRig.h"

// Uncomment the next line when you want fake data instead of the real rig.
//#define USE_MOCK_RIG

Rig* createRig() {
#if defined(USE_MOCK_RIG)
  static MockRig rig;
#else
  static HardwareRig rig;
#endif
  return &rig;
}
