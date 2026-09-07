#pragma once

#include <cmath>
#include <cstdint>

#include "app/SeedPhenotype.h"

namespace seedbox {

inline std::uint32_t lineageStep(std::uint32_t state) {
  state ^= state << 13u; state ^= state >> 17u; state ^= state << 5u;
  return state;
}

inline float lineageUnit(std::uint32_t& state) {
  state = lineageStep(state);
  return static_cast<float>(state & 0xffffu) / 65535.0f;
}

inline SeedPhenotype siblingPhenotype(SeedPhenotype parent, std::uint32_t branch, float amount) {
  parent = clampPhenotype(parent); amount = phenotypeUnit(amount);
  float* values[] = {&parent.energy, &parent.stability, &parent.brightness, &parent.space,
                     &parent.roughness, &parent.recurrence, &parent.tension};
  for (float* value : values) *value = phenotypeUnit(*value + ((lineageUnit(branch) * 2.0f) - 1.0f) * amount);
  return parent;
}

inline float phenotypeDistance(SeedPhenotype a, SeedPhenotype b) {
  a = clampPhenotype(a); b = clampPhenotype(b);
  const float delta[] = {a.energy-b.energy, a.stability-b.stability, a.brightness-b.brightness,
                         a.space-b.space, a.roughness-b.roughness, a.recurrence-b.recurrence, a.tension-b.tension};
  float squared = 0.0f; for (float value : delta) squared += value * value;
  return std::sqrt(squared / 7.0f);
}

}  // namespace seedbox
