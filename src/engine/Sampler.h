#pragma once
#include "Seed.h"
#include <stdint.h>

class Sampler {
public:
  void init();
  void trigger(const Seed& s, uint32_t whenSamples);
};
