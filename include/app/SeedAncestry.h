#pragma once

#include <algorithm>
#include <cmath>

#include "Seed.h"

namespace seedbox {
enum SeedMutationDimension : std::uint32_t {
  kPitch = 1u << 0, kDensity = 1u << 1, kProbability = 1u << 2,
  kJitter = 1u << 3, kTone = 1u << 4, kSpread = 1u << 5,
};

inline void recordMutation(const Seed& parent, Seed& child) {
  std::uint32_t mask = 0; float squared = 0.f; unsigned count = 0;
  const auto moved = [&](float before, float after, float scale, std::uint32_t bit) {
    const float delta = after - before;
    if (delta != 0.f) { mask |= bit; squared += (delta / scale) * (delta / scale); ++count; }
  };
  moved(parent.pitch, child.pitch, 24.f, kPitch); moved(parent.density, child.density, 8.f, kDensity);
  moved(parent.probability, child.probability, 1.f, kProbability); moved(parent.jitterMs, child.jitterMs, 50.f, kJitter);
  moved(parent.tone, child.tone, 1.f, kTone); moved(parent.spread, child.spread, 1.f, kSpread);
  if (!mask) return;
  child.ancestry = {parent.id, parent.prng, parent.lineage,
                    std::min(1.f, std::sqrt(squared / static_cast<float>(count))), mask};
}
}  // namespace seedbox
