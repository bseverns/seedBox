#include "app/AppState.h"
#include <algorithm>
#include <cstdio>
#include "util/RNG.h"
#ifdef SEEDBOX_HW
  #include "io/Storage.h"
  #include "engine/Sampler.h"
#endif

namespace {
constexpr uint8_t kEngineCount = 3;
constexpr uint8_t kEngineCycleCc = 20;

uint8_t sanitizeEngine(uint8_t engine) {
  if (kEngineCount == 0) {
    return 0;
  }
  return static_cast<uint8_t>(engine % kEngineCount);
}

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
  midi.setClockHandler([this]() { onExternalClockTick(); });
  midi.setStartHandler([this]() { onExternalTransportStart(); });
  midi.setStopHandler([this]() { onExternalTransportStop(); });
  midi.setControlChangeHandler(
      [this](uint8_t ch, uint8_t cc, uint8_t val) {
        onExternalControlChange(ch, cc, val);
      });
#endif
  engines_.init(EngineRouter::Mode::kHardware);
  engines_.granular().setMaxActiveVoices(36);
  engines_.granular().armLiveInput(true);
  engines_.resonator().setMaxVoices(10);
  primeSeeds(masterSeed_);
}

void AppState::initSim() {
  engines_.init(EngineRouter::Mode::kSim);
  engines_.granular().setMaxActiveVoices(12);
  engines_.granular().armLiveInput(false);
  engines_.resonator().setMaxVoices(4);
  primeSeeds(masterSeed_);
}

void AppState::tick() {
  if (!seedsPrimed_) {
    primeSeeds(masterSeed_);
  }
  if (!externalClockDominant_) {
    scheduler_.onTick();
  }
  ++frame_;
}

void AppState::primeSeeds(uint32_t masterSeed) {
  masterSeed_ = masterSeed ? masterSeed : 0x5EEDB0B1u;
  const std::vector<uint8_t> previousSelections = seedEngineSelections_;
  seeds_.clear();
  scheduler_ = PatternScheduler{};
  scheduler_.setBpm(120.f);
  scheduler_.setTriggerCallback(&engines_, &EngineRouter::dispatchThunk);

  uint32_t state = masterSeed_;
  constexpr size_t kSeedCount = 4;
  seedEngineSelections_.assign(kSeedCount, 0);
  for (size_t i = 0; i < kSeedCount; ++i) {
    Seed seed{};
    seed.id = static_cast<uint32_t>(i);
    seed.prng = RNG::xorshift(state);
    seed.engine = 0;
    seed.sampleIdx = static_cast<uint8_t>(i % 16);
    seed.pitch = static_cast<float>(static_cast<int32_t>(RNG::xorshift(state) % 25) - 12);
    seed.density = 0.5f + 0.75f * RNG::uniform01(state);
    seed.probability = 0.6f + 0.4f * RNG::uniform01(state);
    seed.jitterMs = 2.0f + 12.0f * RNG::uniform01(state);
    seed.tone = RNG::uniform01(state);
    seed.spread = 0.1f + 0.8f * RNG::uniform01(state);
    seed.mutateAmt = 0.05f + 0.15f * RNG::uniform01(state);

    seed.granular.grainSizeMs = 35.f + 120.f * RNG::uniform01(state);
    seed.granular.sprayMs = 4.f + 24.f * RNG::uniform01(state);
    seed.granular.transpose = static_cast<float>(static_cast<int32_t>(RNG::xorshift(state) % 13) - 6);
    seed.granular.windowSkew = (RNG::uniform01(state) * 2.f) - 1.f;
    seed.granular.stereoSpread = 0.2f + 0.7f * RNG::uniform01(state);
    seed.granular.source = (RNG::uniform01(state) > 0.4f)
                               ? static_cast<uint8_t>(GranularEngine::Source::kSdClip)
                               : static_cast<uint8_t>(GranularEngine::Source::kLiveInput);
    seed.granular.sdSlot = static_cast<uint8_t>(RNG::xorshift(state) % GranularEngine::kSdClipSlots);

    seed.resonator.exciteMs = 2.0f + 10.0f * RNG::uniform01(state);
    seed.resonator.damping = RNG::uniform01(state);
    seed.resonator.brightness = RNG::uniform01(state);
    seed.resonator.feedback = 0.55f + 0.4f * RNG::uniform01(state);
    if (seed.resonator.feedback > 0.99f) seed.resonator.feedback = 0.99f;
    seed.resonator.mode = static_cast<uint8_t>(i % 2);
    seed.resonator.bank = static_cast<uint8_t>(RNG::xorshift(state) % 6);

    seeds_.push_back(seed);
    scheduler_.addSeed(seeds_.back());
  }

  for (size_t i = 0; i < kSeedCount; ++i) {
    const uint8_t desired = (i < previousSelections.size())
                                ? previousSelections[i]
                                : seeds_[i].engine;
    setSeedEngine(static_cast<uint8_t>(i), desired);
  }

  focusSeed_ = 0;
  seedsPrimed_ = true;
  externalClockDominant_ = false;
}

void AppState::onExternalClockTick() {
  if (!seedsPrimed_) {
    primeSeeds(masterSeed_);
  }
  externalClockDominant_ = true;
  scheduler_.onTick();
}

void AppState::onExternalTransportStart() {
  externalClockDominant_ = true;
}

void AppState::onExternalTransportStop() {
  externalClockDominant_ = false;
}

void AppState::onExternalControlChange(uint8_t, uint8_t cc, uint8_t val) {
  if (cc == kEngineCycleCc) {
    if (kEngineCount == 0) {
      return;
    }
    if (seeds_.empty()) {
      return;
    }
    const size_t count = seeds_.size();
    const uint8_t focus = static_cast<uint8_t>(std::min<size_t>(focusSeed_, count - 1));
    const uint8_t current = seeds_[focus].engine;
    uint8_t target = current;
    if (val >= 64) {
      target = static_cast<uint8_t>((current + 1) % kEngineCount);
    } else {
      target = static_cast<uint8_t>((current + kEngineCount - 1) % kEngineCount);
    }
    setSeedEngine(focus, target);
    return;
  }

  // Future CC maps will route through here once the macro table lands.
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

void AppState::setSeedEngine(uint8_t seedIndex, uint8_t engineId) {
  if (seeds_.empty()) {
    return;
  }
  const size_t count = seeds_.size();
  const size_t idx = static_cast<size_t>(seedIndex) % count;
  const uint8_t sanitized = sanitizeEngine(engineId);

  if (seedEngineSelections_.size() < count) {
    seedEngineSelections_.resize(count, 0);
  }

  Seed& seed = seeds_[idx];
  seed.engine = sanitized;
  seedEngineSelections_[idx] = sanitized;
  scheduler_.updateSeed(idx, seed);
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

const Seed* AppState::debugScheduledSeed(uint8_t index) const {
  return scheduler_.seedForDebug(static_cast<size_t>(index));
}
