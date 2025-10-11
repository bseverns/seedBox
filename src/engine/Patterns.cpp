#include "engine/Patterns.h"
#include <cmath>
#include <cstdint>
#include "util/RNG.h"
#include "util/Units.h"

void PatternScheduler::setBpm(float bpm) { bpm_ = bpm; }

void PatternScheduler::addSeed(const Seed& s) {
  seeds_.push_back(s);
  densityAccumulators_.push_back(0.f);
}

bool PatternScheduler::updateSeed(size_t index, const Seed& s) {
  if (index >= seeds_.size()) {
    return false;
  }
  seeds_[index] = s;
  return true;
}

void PatternScheduler::setTriggerCallback(void* ctx, void (*fn)(void*, const Seed&, uint32_t)) {
  triggerCtx_ = ctx;
  triggerFn_ = fn;
}

const Seed* PatternScheduler::seedForDebug(size_t index) const {
  if (index >= seeds_.size()) {
    return nullptr;
  }
  return &seeds_[index];
}

bool PatternScheduler::densityGate(size_t seedIndex, float density) {
  if (density <= 0.f) return false;
  if (seedIndex >= densityAccumulators_.size()) return false;

  static constexpr float kTicksPerBeat = 24.f;
  float& accumulator = densityAccumulators_[seedIndex];
  accumulator += density / kTicksPerBeat;
  if (accumulator >= 1.f) {
    accumulator -= 1.f;
    return true;
  }
  return false;
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
  for (size_t i = 0; i < seeds_.size(); ++i) {
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
