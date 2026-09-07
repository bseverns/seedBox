#pragma once

#include <algorithm>

#include "Seed.h"

namespace seedbox {
// One deliberately bounded relationship: Burst energy excites Resonator.
// It only touches resonator-facing values and is deterministic per trigger.
inline Seed burstExcitesResonator(Seed seed, float burstEnergy) {
  const float energy = std::clamp(burstEnergy, 0.f, 1.f);
  seed.resonator.exciteMs = 2.f + energy * 18.f;
  seed.resonator.brightness = std::clamp(seed.resonator.brightness + energy * .25f, 0.f, 1.f);
  seed.resonator.feedback = std::clamp(seed.resonator.feedback + energy * .12f, 0.f, .99f);
  return seed;
}
}  // namespace seedbox
