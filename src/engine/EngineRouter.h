#pragma once
#include <cstdint>
#include "Seed.h"
#include "engine/Sampler.h"
#include "engine/Granular.h"
#include "engine/Resonator.h"

// EngineRouter glues the pattern scheduler to the actual audio engines. Right
// now it simply records deterministic trigger plans so the Option B and C
// roadmaps have living code. When the DSP objects land we can drop them in
// without touching the higher-level scheduler.
class EngineRouter {
public:
  enum class Mode : uint8_t { kSim, kHardware };

  void init(Mode mode);
  void triggerSeed(const Seed& seed, uint32_t whenSamples);

  static void dispatchThunk(void* ctx, const Seed& seed, uint32_t whenSamples);

  GranularEngine& granular() { return granular_; }
  ResonatorBank& resonator() { return resonator_; }

private:
  Mode mode_{Mode::kSim};
  Sampler sampler_{};
  GranularEngine granular_{};
  ResonatorBank resonator_{};
};
