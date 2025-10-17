#include "app/AppState.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <utility>
#include "SeedBoxConfig.h"
#include "util/RNG.h"
#include "interop/mn42_map.h"
#ifdef SEEDBOX_HW
  #include "io/Storage.h"
  #include "engine/Sampler.h"
#endif

namespace {
constexpr uint8_t kEngineCount = 3;
constexpr uint8_t kEngineCycleCc = 20;

template <size_t N>
void writeDisplayField(char (&dst)[N], std::string_view text) {
  static_assert(N > 0, "Display field must have space for a terminator");
  const size_t maxCopy = N - 1;
  const size_t copyLen = std::min(text.size(), maxCopy);
  std::memcpy(dst, text.data(), copyLen);
  dst[copyLen] = '\0';
  if (copyLen < maxCopy) {
    std::fill(dst + copyLen + 1, dst + N, '\0');
  }
}

template <typename... Args>
std::string_view formatScratch(std::array<char, 64>& scratch, const char* fmt, Args&&... args) {
  const int written = std::snprintf(scratch.data(), scratch.size(), fmt, std::forward<Args>(args)...);
  if (written <= 0) {
    scratch[0] = '\0';
    return std::string_view{};
  }
  const size_t len = std::min(static_cast<size_t>(written), scratch.size() - 1);
  return std::string_view(scratch.data(), len);
}

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

// initHardware wires up the physical instrument: MIDI ingress, the engine
// router in hardware mode, and finally the deterministic seed prime. The code
// mirrors initSim because we want both paths to be interchangeable in class —
// students can run the simulator on a laptop and trust that the Teensy build
// behaves the same.
void AppState::initHardware() {
#ifdef SEEDBOX_HW
  if constexpr (!SeedBoxConfig::kQuietMode) {
    midi.begin();
    midi.setClockHandler([this]() { onExternalClockTick(); });
    midi.setStartHandler([this]() { onExternalTransportStart(); });
    midi.setStopHandler([this]() { onExternalTransportStop(); });
    midi.setControlChangeHandler(
        [this](uint8_t ch, uint8_t cc, uint8_t val) {
          onExternalControlChange(ch, cc, val);
        });
  } else {
    midi.begin();
    midi.setClockHandler(nullptr);
    midi.setStartHandler(nullptr);
    midi.setStopHandler(nullptr);
    midi.setControlChangeHandler(nullptr);
  }
#endif
  engines_.init(EngineRouter::Mode::kHardware);
  engines_.granular().setMaxActiveVoices(36);
  engines_.granular().armLiveInput(true);
  engines_.resonator().setMaxVoices(10);
  primeSeeds(masterSeed_);
}

// initSim is the lab-friendly twin of initHardware. No MIDI bootstrap, but the
// rest of the wiring is identical so deterministic behaviour survives unit
// tests and lecture demos.
void AppState::initSim() {
  engines_.init(EngineRouter::Mode::kSim);
  engines_.granular().setMaxActiveVoices(12);
  engines_.granular().armLiveInput(false);
  engines_.resonator().setMaxVoices(4);
  primeSeeds(masterSeed_);
}

// tick is the heartbeat. Every call either primes seeds (first-run), lets the
// internal scheduler drive things when we own the transport, or just counts
// frames so the OLED can display a ticking counter.
void AppState::tick() {
  if constexpr (!SeedBoxConfig::kQuietMode) {
    if (!seedsPrimed_) {
      primeSeeds(masterSeed_);
    }
  }
  if (!externalClockDominant_) {
    scheduler_.onTick();
  }
  ++frame_;
}

// primeSeeds is where the deterministic world-building happens. We treat
// masterSeed as the only source of entropy, then spin a tiny RNG to fill each
// Seed with musically interesting defaults. The idea: instructors can step
// through this function and show how density, jitter, and engine-specific
// parameters fall out of simple pseudo-random math.
void AppState::primeSeeds(uint32_t masterSeed) {
  masterSeed_ = masterSeed ? masterSeed : 0x5EEDB0B1u;

  if constexpr (SeedBoxConfig::kQuietMode) {
    seeds_.clear();
    scheduler_ = PatternScheduler{};
    scheduler_.setBpm(0.f);
    scheduler_.setTriggerCallback(&engines_, &EngineRouter::dispatchThunk);
    seedEngineSelections_.clear();
    focusSeed_ = 0;
    seedsPrimed_ = false;
    externalClockDominant_ = false;
    return;
  }

  const std::vector<uint8_t> previousSelections = seedEngineSelections_;
  seeds_.clear();
  scheduler_ = PatternScheduler{};
  scheduler_.setBpm(120.f);
  scheduler_.setTriggerCallback(&engines_, &EngineRouter::dispatchThunk);

  uint32_t state = masterSeed_;
  constexpr size_t kSeedCount = 4;
  seedEngineSelections_.assign(kSeedCount, 0);
  for (size_t i = 0; i < kSeedCount; ++i) {
    // Every loop iteration births a new Seed and walks RNG::xorshift / uniform
    // helpers forward. Because the RNG is deterministic we can rerun
    // primeSeeds with the same masterSeed and always land on the same genome.
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

    // --- Granular controls ---
    // Grain size controls the duration of a single voice. We skew the random
    // walk toward shorter grains so the scheduler can demonstrate density
    // stacking without smearing the entire texture.
    seed.granular.grainSizeMs = 35.f + 120.f * RNG::uniform01(state);
    seed.granular.sprayMs = 4.f + 24.f * RNG::uniform01(state);
    seed.granular.transpose = static_cast<float>(static_cast<int32_t>(RNG::xorshift(state) % 13) - 6);
    seed.granular.windowSkew = (RNG::uniform01(state) * 2.f) - 1.f;
    seed.granular.stereoSpread = 0.2f + 0.7f * RNG::uniform01(state);
    seed.granular.source = (RNG::uniform01(state) > 0.4f)
                               ? static_cast<uint8_t>(GranularEngine::Source::kSdClip)
                               : static_cast<uint8_t>(GranularEngine::Source::kLiveInput);
    seed.granular.sdSlot = static_cast<uint8_t>(RNG::xorshift(state) % GranularEngine::kSdClipSlots);

    // --- Resonator controls ---
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
    // Restore any explicit engine selections we captured before reseeding.
    // That keeps lesson plans deterministic: once a class sets seed 2 to the
    // resonator they can reseed the parameters without losing the engine
    // choice.
    const uint8_t desired = (i < previousSelections.size())
                                ? previousSelections[i]
                                : seeds_[i].engine;
    setSeedEngine(static_cast<uint8_t>(i), desired);
  }

  focusSeed_ = 0;
  seedsPrimed_ = true;
  transportLatched_ = false;
  externalClockDominant_ = false;
  updateExternalDominance();
}

void AppState::onExternalClockTick() {
  if constexpr (SeedBoxConfig::kQuietMode) {
    updateExternalDominance();
    if (!followExternalClock_ || !transportLatched_) {
      return;
    }
    scheduler_.onTick();
    return;
  }
  if (!followExternalClock_) {
    updateExternalDominance();
    return;
  }
  if (!seedsPrimed_) {
    primeSeeds(masterSeed_);
  }
  updateExternalDominance();
  if (!transportLatched_) {
    return;
  }
  scheduler_.onTick();
}

void AppState::onExternalTransportStart() {
  transportLatched_ = true;
  updateExternalDominance();
}

void AppState::onExternalTransportStop() {
  transportLatched_ = false;
  updateExternalDominance();
}

void AppState::onExternalControlChange(uint8_t ch, uint8_t cc, uint8_t val) {
  using namespace seedbox::interop::mn42;
  const bool matchesMn42Channel = (ch == mn42::kDefaultChannel) ||
                                  (ch == static_cast<uint8_t>(mn42::kDefaultChannel + 1));
  if (matchesMn42Channel) {
    if (cc == mn42::cc::kHandshake) {
      handleMn42Handshake(val);
      return;
    }
    if (cc == mn42::cc::kMode) {
      applyMn42ModeBits(val);
      return;
    }
    if (cc == mn42::cc::kTransportGate) {
      handleMn42TransportGate(val);
      return;
    }
  }
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
    // Our rule-of-thumb: CC values >= 64 spin the encoder clockwise (advance to
    // the next engine) while lower values back up one slot. The math is tiny
    // but worth spelling out for clarity.
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
  // Modulo arithmetic means you can spin the encoder forever without throwing
  // the app off a cliff. It also makes for a nice classroom demo about wrap-
  // around indexing.
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
  // Re-register the seed with the scheduler so downstream engines fire the
  // right trigger callback. This keeps PatternScheduler as the single source of
  // truth for playback order.
  scheduler_.updateSeed(idx, seed);
}

void AppState::captureDisplaySnapshot(DisplaySnapshot& out) const {
  // OLED real estate is tiny, so we jam the master seed into the title and then
  // use status/metrics/nuance rows to narrate what's happening with the focus
  // seed. Think of it as a glorified logcat for the front panel.
  std::array<char, 64> scratch{};
  writeDisplayField(out.title, formatScratch(scratch, "SeedBox %06X", masterSeed_ & 0xFFFFFFu));

  if (seeds_.empty()) {
    if constexpr (SeedBoxConfig::kQuietMode) {
      writeDisplayField(out.status, "quiet mode zzz");
      writeDisplayField(out.metrics, "flip QUIET=0");
    } else {
      writeDisplayField(out.status, "no seeds loaded");
      writeDisplayField(out.metrics, "tap reseed to wake");
    }
    writeDisplayField(out.nuance, formatScratch(scratch, "frame %08lu", static_cast<unsigned long>(frame_)));
    return;
  }

  const Seed& s = seeds_[std::min<size_t>(focusSeed_, seeds_.size() - 1)];
  writeDisplayField(out.status, formatScratch(scratch, "#%02u %s %+0.1fst", s.id, engineLabel(s.engine), s.pitch));
  const float density = std::clamp(s.density, 0.0f, 99.99f);
  const float probability = std::clamp(s.probability, 0.0f, 1.0f);
  writeDisplayField(out.metrics, formatScratch(scratch, "D%.2f P%.2f", density, probability));
  const float jitter = std::clamp(s.jitterMs, 0.0f, 999.9f);
  const float mutate = std::clamp(s.mutateAmt, 0.0f, 1.0f);
  writeDisplayField(out.nuance, formatScratch(scratch, "J%.1fms Mu%.2f", jitter, mutate));
}

const Seed* AppState::debugScheduledSeed(uint8_t index) const {
  return scheduler_.seedForDebug(static_cast<size_t>(index));
}

void AppState::handleMn42Handshake(uint8_t value) {
  mn42HandshakeValue_ = value;
}

void AppState::applyMn42ModeBits(uint8_t bits) {
  using namespace seedbox::interop::mn42;
  mn42ModeBits_ = bits;
  followExternalClock_ = (bits & mode::kFollowExternalClock) != 0;
  debugMetersExposed_ = (bits & mode::kExposeDebugMeters) != 0;
  transportLatchEnabled_ = (bits & mode::kLatchTransport) != 0;
  if (!followExternalClock_ || !transportLatchEnabled_) {
    transportLatched_ = false;
  }
  updateExternalDominance();
}

void AppState::handleMn42TransportGate(uint8_t value) {
  const bool gateActive = value > 0;
  if (transportLatchEnabled_) {
    if (gateActive) {
      if (transportLatched_) {
        onExternalTransportStop();
      } else {
        onExternalTransportStart();
      }
    }
    return;
  }

  if (gateActive) {
    onExternalTransportStart();
  } else {
    onExternalTransportStop();
  }
}

void AppState::updateExternalDominance() {
  externalClockDominant_ = followExternalClock_;
}
