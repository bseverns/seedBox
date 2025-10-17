#include "engine/Patterns.h"
#include <cmath>
#include <cstdint>
#include "util/RNG.h"
#include "util/Units.h"

// PatternScheduler keeps the macro timing math honest. Seeds live inside this
// object until they're triggered, and every method tries to narrate what part
// of the groove machine it's touching. Students can trace the timeline by
// following the call sites in AppState.

// Set the global tempo. We leave the unit intentionally boring (BPM) so the
// teaching demo can focus on density/probability instead of fancy transport
// math.
void PatternScheduler::setBpm(float bpm) { bpm_ = bpm; }

// Add a seed to the scheduling roster and make sure it has a matching density
// accumulator slot. The accumulator tracks fractional hits until densityGate
// decides it's time to trigger.
void PatternScheduler::addSeed(const Seed& s) {
  seeds_.push_back(s);
  densityAccumulators_.push_back(0.f);
}

// Update the copy of the seed the scheduler owns. Returns false if the caller
// fat-fingered the index so the tests can assert deterministic behaviour.
bool PatternScheduler::updateSeed(std::size_t index, const Seed& s) {
  if (index >= seeds_.size()) {
    return false;
  }
  seeds_[index] = s;
  return true;
}

void PatternScheduler::setTriggerCallback(void* ctx, void (*fn)(void*, const Seed&, uint32_t)) {
  // The callback gets called inside `onTick` whenever a seed survives density +
  // probability filtering.  Keeping both the context pointer and function
  // pointer explicit (instead of `std::function`) makes this trivial to bridge
  // into C-style APIs like the Teensy Audio library.
  triggerCtx_ = ctx;
  triggerFn_ = fn;
}

const Seed* PatternScheduler::seedForDebug(std::size_t index) const {
  if (index >= seeds_.size()) {
    return nullptr;
  }
  return &seeds_[index];
}

bool PatternScheduler::densityGate(std::size_t seedIndex, float density) {
  if (density <= 0.f) return false;
  if (seedIndex >= densityAccumulators_.size()) return false;

  // Each seed accrues fractional hits according to its density. Once the
  // accumulator crosses 1.0 we let the note through and subtract 1.0 so the
  // groove stays phase-aligned. Think of it as a poor-person's Bernoulli clock
  // that stays deterministic for tests.
  static constexpr float kTicksPerBeat = 24.f;
  float& accumulator = densityAccumulators_[seedIndex];
  accumulator += density / kTicksPerBeat;
  if (accumulator >= 1.f) {
    accumulator -= 1.f;
    return true;
  }
  return false;
}

// Utility conversions keep the scheduling math agnostic from whether we're in
// sim or hardware land. Units::simNowSamples and Units::msToSamples hide the
// platform specifics so lectures can focus on timing concepts instead of
// transport glue.
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
  for (std::size_t i = 0; i < seeds_.size(); ++i) {
    Seed& s = seeds_[i];
    if (densityGate(i, s.density)) {
      // probability gate
      if (RNG::uniform01(s.prng) < s.probability) {
        const uint32_t baseSamples = nowSamples();
        int32_t jitterSamples = 0;
        if (s.jitterMs != 0.f) {
          const float jitterMs = RNG::uniformSigned(s.prng) * s.jitterMs;
          const uint32_t magnitude = msToSamples(std::abs(jitterMs));
          jitterSamples = static_cast<int32_t>(magnitude);
          if (jitterMs < 0.f) {
            jitterSamples = -jitterSamples;
          }
        }
        int64_t scheduled = static_cast<int64_t>(baseSamples) + static_cast<int64_t>(jitterSamples);
        if (scheduled < 0) {
          scheduled = 0;
        }
        const uint32_t t = static_cast<uint32_t>(scheduled);
        if (triggerFn_) {
          triggerFn_(triggerCtx_, s, t);
        }
      }
    }
  }
  ++tickCount_;
}
