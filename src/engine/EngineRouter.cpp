#ifdef SEEDBOX_HW
#  include <Arduino.h>
#endif
#include "engine/EngineRouter.h"

void EngineRouter::init(Mode mode) {
  mode_ = mode;
  sampler_.init();
  granular_.init(mode == Mode::kHardware ? GranularEngine::Mode::kHardware
                                          : GranularEngine::Mode::kSim);
  resonator_.init(mode == Mode::kHardware ? ResonatorBank::Mode::kHardware
                                          : ResonatorBank::Mode::kSim);
}

void EngineRouter::triggerSeed(const Seed& seed, uint32_t whenSamples) {
  switch (seed.engine) {
    case 0:
      sampler_.trigger(seed, whenSamples);
      break;
    case 1:
      granular_.trigger(seed, whenSamples);
      break;
    case 2:
      resonator_.trigger(seed, whenSamples);
      break;
    default:
      sampler_.trigger(seed, whenSamples);
      break;
  }
}

void EngineRouter::dispatchThunk(void* ctx, const Seed& seed, uint32_t whenSamples) {
  if (!ctx) return;
  static_cast<EngineRouter*>(ctx)->triggerSeed(seed, whenSamples);
}
