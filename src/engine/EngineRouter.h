#pragma once

//
// EngineRouter
// ------------
// This class is the switchboard between the transport layer and the DSP toys.
// When PatternScheduler decides a seed should fire, it calls back into
// EngineRouter with a timestamp.  EngineRouter then picks the right audio engine
// (sampler, granular, resonator) and forwards the seed along.  No hidden state,
// no secret sauce — we want students to trace the signal path without getting
// lost.
#include <cstdint>
#include "Seed.h"
#include "engine/Sampler.h"
#include "engine/Granular.h"
#include "engine/Resonator.h"
#include "util/Annotations.h"

// EngineRouter glues the pattern scheduler to the actual audio engines. Right
// now it simply records deterministic trigger plans so the Option B and C
// roadmaps have living code. When the DSP objects land we can drop them in
// without touching the higher-level scheduler.
class EngineRouter {
public:
  enum class Mode : uint8_t { kSim, kHardware };

  // Point the router at hardware or simulator backends.  Each engine exposes a
  // matching Mode enum so we can keep the control surfaces identical even when
  // the DSP graph changes.
  void init(Mode mode);

  // Fan a seed out to whichever engine it currently owns.  The scheduler hands
  // over a sample-accurate timestamp (`whenSamples`) so downstream code can stay
  // tight with the transport.
  void triggerSeed(const Seed& seed, uint32_t whenSamples);

  SEEDBOX_MAYBE_UNUSED static void dispatchThunk(void* ctx, const Seed& seed, uint32_t whenSamples);

  Sampler& sampler() { return sampler_; }
  const Sampler& sampler() const { return sampler_; }
  GranularEngine& granular() { return granular_; }
  const GranularEngine& granular() const { return granular_; }
  ResonatorBank& resonator() { return resonator_; }
  const ResonatorBank& resonator() const { return resonator_; }

private:
  Mode mode_{Mode::kSim};
  Sampler sampler_{};
  GranularEngine granular_{};
  ResonatorBank resonator_{};
};
