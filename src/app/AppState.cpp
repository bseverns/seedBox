#include "app/AppState.h"
#include <algorithm>
#include <cstdio>
#include "util/RNG.h"
#ifdef SEEDBOX_HW
  #include "io/Storage.h"
  #include "engine/Sampler.h"
#endif

namespace {
const char* engineLabel(uint8_t engine) {
  switch (engine) {
    case 0: return "SMP";
    case 1: return "GRA";
    case 2: return "PING";
    default: return "UNK";
  }
}
}

void AppState::initHardware() {
#ifdef SEEDBOX_HW
  midi.begin();
#endif
  primeSeeds(masterSeed_);
}

void AppState::initSim() {
  primeSeeds(masterSeed_);
}

void AppState::tick() {
  if (!seedsPrimed_) {
    primeSeeds(masterSeed_);
  }
  scheduler_.onTick();
  ++frame_;
}

void AppState::primeSeeds(uint32_t masterSeed) {
  masterSeed_ = masterSeed ? masterSeed : 0x5EEDB0B1u;
  seeds_.clear();
  scheduler_ = PatternScheduler{};
  scheduler_.setBpm(120.f);

  uint32_t state = masterSeed_;
  constexpr size_t kSeedCount = 4;
  for (size_t i = 0; i < kSeedCount; ++i) {
    Seed seed{};
    seed.id = static_cast<uint32_t>(i);
    seed.prng = RNG::xorshift(state);
    seed.engine = static_cast<uint8_t>(i % 3);
    seed.sampleIdx = static_cast<uint8_t>(i % 16);
    seed.pitch = static_cast<float>(static_cast<int32_t>(RNG::xorshift(state) % 25) - 12);
    seed.density = 0.5f + 0.75f * RNG::uniform01(state);
    seed.probability = 0.6f + 0.4f * RNG::uniform01(state);
    seed.jitterMs = 2.0f + 12.0f * RNG::uniform01(state);
    seed.tone = RNG::uniform01(state);
    seed.spread = 0.1f + 0.8f * RNG::uniform01(state);
    seed.mutateAmt = 0.05f + 0.15f * RNG::uniform01(state);
    seeds_.push_back(seed);
    scheduler_.addSeed(seeds_.back());
  }

  focusSeed_ = 0;
  seedsPrimed_ = true;
}

void AppState::reseed(uint32_t masterSeed) {
  primeSeeds(masterSeed);
}

void AppState::setFocusSeed(uint8_t index) {
  if (seeds_.empty()) {
    focusSeed_ = 0;
    return;
  }
  const uint8_t count = static_cast<uint8_t>(seeds_.size());
  focusSeed_ = static_cast<uint8_t>(index % count);
}

void AppState::captureDisplaySnapshot(DisplaySnapshot& out) const {
  std::snprintf(out.title, sizeof(out.title), "SeedBox %06X", masterSeed_ & 0xFFFFFFu);

  if (seeds_.empty()) {
    std::snprintf(out.status, sizeof(out.status), "no seeds loaded");
    std::snprintf(out.metrics, sizeof(out.metrics), "tap reseed to wake");
    std::snprintf(out.nuance, sizeof(out.nuance), "frame %08lu", static_cast<unsigned long>(frame_));
    return;
  }

  const Seed& s = seeds_[std::min<size_t>(focusSeed_, seeds_.size() - 1)];
  std::snprintf(out.status, sizeof(out.status), "#%02u %s %+0.1fst", s.id, engineLabel(s.engine), s.pitch);
  std::snprintf(out.metrics, sizeof(out.metrics), "Den %.2f Prob %.2f", s.density, s.probability);
  std::snprintf(out.nuance, sizeof(out.nuance), "Jit %.1fms Mu %.2f", s.jitterMs, s.mutateAmt);
}
