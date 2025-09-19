#include "engine/Patterns.h"
#include "src/util/RNG.h"
#include "src/util/Units.h"

void PatternScheduler::setBpm(float bpm) { bpm_ = bpm; }

void PatternScheduler::addSeed(const Seed& s) { seeds_.push_back(s); }

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
  for (auto &s : seeds_) {
    if (densityGate(s.density, tickCount_)) {
      // probability gate
      if (RNG::uniform01(s.prng) < s.probability) {
        const uint32_t t = nowSamples() + msToSamples(s.jitterMs);
        (void)t; // placeholder: would call engine.trigger(s, t);
      }
    }
  }
  ++tickCount_;
}
