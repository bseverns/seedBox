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

// A small reversible performance state. The caller owns the branch; this keeps
// history out of the real-time engine and preserves the executable Seed.
struct SeedBranch {
  Seed ancestor{};
  Seed current{};
  bool hasAncestor{false};
  void forkFrom(const Seed& seed) { ancestor = seed; current = seed; hasAncestor = true; }
  bool returnToAncestor() { if (!hasAncestor) return false; current = ancestor; return true; }
};

// Crossing happens in phenotype space: stable traits blend, while roughness
// and tension choose one parent deterministically so a child retains character.
inline SeedPhenotype crossPhenotypes(SeedPhenotype a, SeedPhenotype b, std::uint32_t branch) {
  a = clampPhenotype(a); b = clampPhenotype(b);
  SeedPhenotype child{};
  child.energy = (a.energy + b.energy) * .5f;
  child.stability = (a.stability + b.stability) * .5f;
  child.brightness = (a.brightness + b.brightness) * .5f;
  child.space = (a.space + b.space) * .5f;
  child.recurrence = (a.recurrence + b.recurrence) * .5f;
  child.roughness = lineageUnit(branch) < .5f ? a.roughness : b.roughness;
  child.tension = lineageUnit(branch) < .5f ? a.tension : b.tension;
  return child;
}

}  // namespace seedbox
