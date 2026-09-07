#pragma once

// Higher-order musical intent. A phenotype is not persisted in Seed yet: Seed
// remains the complete executable genome used by engines and storage.
#include <algorithm>

#include "Seed.h"

namespace seedbox {

struct SeedPhenotype {
  float energy{0.5f};
  float stability{0.5f};
  float brightness{0.5f};
  float space{0.5f};
  float roughness{0.5f};
  float recurrence{0.5f};
  float tension{0.5f};
};

inline float phenotypeUnit(float value) { return std::clamp(value, 0.0f, 1.0f); }

inline SeedPhenotype clampPhenotype(SeedPhenotype phenotype) {
  phenotype.energy = phenotypeUnit(phenotype.energy);
  phenotype.stability = phenotypeUnit(phenotype.stability);
  phenotype.brightness = phenotypeUnit(phenotype.brightness);
  phenotype.space = phenotypeUnit(phenotype.space);
  phenotype.roughness = phenotypeUnit(phenotype.roughness);
  phenotype.recurrence = phenotypeUnit(phenotype.recurrence);
  phenotype.tension = phenotypeUnit(phenotype.tension);
  return phenotype;
}

// Map correlated intent into the existing executable genome. Identity,
// provenance, routing, and source selection deliberately remain untouched.
inline void applyPhenotype(Seed& seed, SeedPhenotype phenotype) {
  phenotype = clampPhenotype(phenotype);
  seed.density = 0.5f + 3.5f * phenotype.energy;
  seed.probability = 0.35f + 0.65f * phenotype.recurrence;
  seed.jitterMs = (1.0f - phenotype.stability) * (3.0f + 18.0f * phenotype.roughness);
  seed.mutateAmt = (1.0f - phenotype.stability) * (0.15f + 0.85f * phenotype.roughness);
  seed.tone = 0.1f + 0.9f * phenotype.brightness;
  seed.spread = 0.05f + 0.95f * phenotype.space;
  seed.pitch = -6.0f + 12.0f * phenotype.tension;
  seed.envA = 0.003f + (1.0f - phenotype.energy) * 0.050f;
  seed.envD = 0.04f + (1.0f - phenotype.energy) * 0.30f;
  seed.envS = 0.25f + 0.60f * phenotype.recurrence;
  seed.envR = 0.04f + phenotype.space * 0.76f;
  seed.granular.sprayMs = 4.0f + phenotype.space * 40.0f + phenotype.roughness * 18.0f;
  seed.granular.stereoSpread = seed.spread;
  seed.granular.windowSkew = (phenotype.roughness * 2.0f) - 1.0f;
  seed.resonator.brightness = phenotype.brightness;
  seed.resonator.damping = 0.20f + phenotype.stability * 0.70f;
  seed.resonator.feedback = 0.30f + phenotype.space * 0.55f;
}

}  // namespace seedbox
