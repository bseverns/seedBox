#include "engine/Patterns.h"
#include "util/RNG.h"
#include "util/Units.h"

void PatternScheduler::setBpm(float bpm) { bpm_ = bpm; }

void PatternScheduler::addSeed(const Seed& s) { seeds_.push_back(s); }

void PatternScheduler::setTriggerCallback(void* ctx, void (*fn)(void*, const Seed&, uint32_t)) {
  triggerCtx_ = ctx;
  triggerFn_ = fn;
}

bool PatternScheduler::densityGate(float density, uint64_t tick) {
  // simple model: expected hits per beat, at 24 PPQN => 6 ticks per 16th
  if (density <= 0.f) return false;
  const float ticksPerBeat = 24.f;
  const float period = ticksPerBeat / density; // ticks between hits on average
  return (static_cast<uint64_t>(tick % (uint64_t)(period > 1 ? period : 1)) == 0);
}

uint32_t PatternScheduler::nowSamples() {
  return Units::simNowSamples();
}

uint32_t PatternScheduler::msToSamples(float ms) {
  return Units::msToSamples(ms);
}

void PatternScheduler::onTick() {
  // Seed lifecycle doctrine, MOARkNOBS style:
  // 1) PICK: this scheduler is the authority — we march through seeds_ in
  //    their programmed order every 24 PPQN tick and let densityGate decide if
  //    the seed is even allowed to wake up on this tick.
  // 2) SCHEDULE: once a seed earns a hit we grab the current clock, smear it by
  //    the seed's jitter, and book the trigger time in samples (see nowSamples
  //    and msToSamples) so the render engine can fire with sample-accurate
  //    timing.
  // 3) RENDER: the actual audio engine (sampler / granular / resonator) will
  //    instantiate a voice using the seed's genome when we call into it with
  //    trigger time `t`. That hook lives where the placeholder `(void)t` sits.
  // 4) MUTATE: if mutateAmt > 0 the engine gets to nudge whitelisted params in
  //    a bounded random walk (micro pitch, tone tilt, ±5% density, etc.). We
  //    keep it deterministic by reseeding RNG with s.prng so a hard reset drops
  //    us right back to the original voice. Mutate plumbing lands alongside the
  //    engine trigger call.
  for (auto &s : seeds_) {
    if (densityGate(s.density, tickCount_)) {
      // probability gate
      if (RNG::uniform01(s.prng) < s.probability) {
        const uint32_t t = nowSamples() + msToSamples(s.jitterMs);
        if (triggerFn_) {
          triggerFn_(triggerCtx_, s, t);
        }
      }
    }
  }
  ++tickCount_;
}
