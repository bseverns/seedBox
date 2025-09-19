#include "engine/Sampler.h"
#ifdef SEEDBOX_HW
  #include <Audio.h>
  // Minimal placeholder graph: implement voices later
#endif

void Sampler::init() {}

void Sampler::trigger(const Seed&, uint32_t) {
  // TODO: allocate voice, set pitch/env/tone/spread, schedule start
}
