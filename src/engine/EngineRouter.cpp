// The implementation side of EngineRouter is intentionally dead simple — it is
// the glue, not the headliner.  Keeping things transparent here makes future
// engine swaps approachable for students.
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
      // Unknown engine IDs fall back to the sampler so we never silently drop a
      // trigger.  That graceful failure makes it easier to prototype new engine
      // IDs without bricking the show.
      sampler_.trigger(seed, whenSamples);
      break;
  }
}

void EngineRouter::dispatchThunk(void* ctx, const Seed& seed, uint32_t whenSamples) {
  if (!ctx) return;
  // Plain C callback entry point so PatternScheduler can stay ignorant of
  // EngineRouter's concrete type.
  static_cast<EngineRouter*>(ctx)->triggerSeed(seed, whenSamples);
}
