#include "app/AppState.h"
#ifdef SEEDBOX_HW
  #include "io/Storage.h"
  #include "engine/Sampler.h"
#endif

void AppState::initHardware() {
#ifdef SEEDBOX_HW
  midi.begin();
#endif
}

void AppState::initSim() {
  // nothing yet
}

void AppState::tick() {
  // UI, scheduling, etc.
  ++frame_;
}
