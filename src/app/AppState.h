#pragma once
#include <stdint.h>
#ifdef SEEDBOX_HW
#include "io/MidiRouter.h"
#endif

class AppState {
public:
  void initHardware();
  void initSim();
  void tick();

#ifdef SEEDBOX_HW
  MidiRouter midi;
#endif

private:
  uint32_t frame_{0};
};
