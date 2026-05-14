#include "app/SeedPrimeRuntimeService.h"

#include <algorithm>

#include "SeedBoxConfig.h"
#include "app/SeedPrimeController.h"
#include "engine/Granular.h"
#include "hal/hal_audio.h"
#include "hal/hal_io.h"
#include "util/RNG.h"

// SeedPrimeRuntimeService is where SeedBox re-builds its little universe.
// Different prime modes supply different "stories" for the next seed set, but
// every story has to preserve the same runtime invariants: locked seeds stay
// put, the scheduler is rebuilt deterministically, and the display/transport
// are told about the new world immediately.

namespace {
constexpr std::uint32_t kLiveCaptureLineageSalt = 131u;
constexpr hal::io::PinNumber kStatusLedPin = 13;
constexpr std::uint32_t kSaltRepeatBias = 0xD667788Au;
constexpr float kNormalizedRange = 1.0f / 16777216.0f;

SeedPrimeController::Mode toPrimeControllerMode(AppState::SeedPrimeMode mode) {
  switch (mode) {
    case AppState::SeedPrimeMode::kTapTempo:
      return SeedPrimeController::Mode::kTapTempo;
    case AppState::SeedPrimeMode::kPreset:
      return SeedPrimeController::Mode::kPreset;
    case AppState::SeedPrimeMode::kLiveInput:
      return SeedPrimeController::Mode::kLiveInput;
    case AppState::SeedPrimeMode::kLfsr:
    default:
      return SeedPrimeController::Mode::kLfsr;
  }
}

SeedPrimeController gSeedPrimeController{};

float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}
}

float SeedPrimeRuntimeService::deterministicNormalizedValue(std::uint32_t masterSeed, std::size_t slot,
                                                            std::uint32_t salt) {
  // This is a tiny hash, not a musical RNG stream. We use it to ask stable
  // yes/no questions like "should repeat bias keep slot 2 this time?".
  std::uint32_t state = masterSeed ? masterSeed : kDefaultMasterSeed;
  state ^= static_cast<std::uint32_t>(slot) * 0x9E3779B9u;
  state ^= salt;
  const std::uint32_t hashed = RNG::xorshift(state);
  return (hashed >> 8) * kNormalizedRange;
}

Seed SeedPrimeRuntimeService::blendSeeds(const Seed& from, const Seed& to, float t) {
  // Crossfades are parameter blends, not event-history blends. Discrete fields
  // snap toward whichever side of the fade is currently dominant.
  const float mix = std::clamp(t, 0.0f, 1.0f);
  Seed blended = from;
  blended.id = to.id;
  blended.prng = to.prng;
  blended.pitch = lerp(from.pitch, to.pitch, mix);
  blended.envA = lerp(from.envA, to.envA, mix);
  blended.envD = lerp(from.envD, to.envD, mix);
  blended.envS = lerp(from.envS, to.envS, mix);
  blended.envR = lerp(from.envR, to.envR, mix);
  blended.density = lerp(from.density, to.density, mix);
  blended.probability = lerp(from.probability, to.probability, mix);
  blended.jitterMs = lerp(from.jitterMs, to.jitterMs, mix);
  blended.tone = lerp(from.tone, to.tone, mix);
  blended.spread = lerp(from.spread, to.spread, mix);
  blended.engine = (mix < 0.5f) ? from.engine : to.engine;
  blended.sampleIdx = (mix < 0.5f) ? from.sampleIdx : to.sampleIdx;
  blended.mutateAmt = lerp(from.mutateAmt, to.mutateAmt, mix);

  blended.granular.grainSizeMs = lerp(from.granular.grainSizeMs, to.granular.grainSizeMs, mix);
  blended.granular.sprayMs = lerp(from.granular.sprayMs, to.granular.sprayMs, mix);
  blended.granular.transpose = lerp(from.granular.transpose, to.granular.transpose, mix);
  blended.granular.windowSkew = lerp(from.granular.windowSkew, to.granular.windowSkew, mix);
  blended.granular.stereoSpread = lerp(from.granular.stereoSpread, to.granular.stereoSpread, mix);
  blended.granular.source = (mix < 0.5f) ? from.granular.source : to.granular.source;
  blended.granular.sdSlot = (mix < 0.5f) ? from.granular.sdSlot : to.granular.sdSlot;

  blended.resonator.exciteMs = lerp(from.resonator.exciteMs, to.resonator.exciteMs, mix);
  blended.resonator.damping = lerp(from.resonator.damping, to.resonator.damping, mix);
  blended.resonator.brightness = lerp(from.resonator.brightness, to.resonator.brightness, mix);
  blended.resonator.feedback = lerp(from.resonator.feedback, to.resonator.feedback, mix);
  blended.resonator.mode = (mix < 0.5f) ? from.resonator.mode : to.resonator.mode;
  blended.resonator.bank = (mix < 0.5f) ? from.resonator.bank : to.resonator.bank;
  return blended;
}

void SeedPrimeRuntimeService::applyRepeatBias(AppState& app, const std::vector<Seed>& previousSeeds,
                                              std::vector<Seed>& generated) {
  if (app.randomnessPanel_.resetBehavior == AppState::RandomnessPanel::ResetBehavior::Hard) {
    return;
  }
  if (previousSeeds.empty() || generated.empty()) {
    return;
  }
  const float bias = std::clamp(app.randomnessPanel_.repeatBias, 0.0f, 1.0f);
  if (bias <= 0.0f) {
    return;
  }
  const std::size_t limit = std::min(previousSeeds.size(), generated.size());
  for (std::size_t i = 0; i < limit; ++i) {
    if (app.seedLock_.seedLocked(i)) {
      continue;
    }
    const float roll = deterministicNormalizedValue(app.masterSeed_, i, kSaltRepeatBias);
    if (roll >= bias) {
      continue;
    }
    // "Drift" keeps identity but lets entropy pull parameters toward the new
    // target. "Soft" keeps the whole previous seed verbatim.
    if (app.randomnessPanel_.resetBehavior == AppState::RandomnessPanel::ResetBehavior::Drift) {
      generated[i] = blendSeeds(previousSeeds[i], generated[i], app.randomnessPanel_.entropy);
    } else {
      generated[i] = previousSeeds[i];
    }
  }
}

void SeedPrimeRuntimeService::handleReseedRequest(AppState& app) const {
  app.reseedRequested_ = true;
  app.displayDirty_ = true;
}

void SeedPrimeRuntimeService::triggerLiveCaptureReseed(AppState& app) const {
  // Live capture is deterministic on purpose: the counter, variation offset,
  // and lineage salt together decide which short-bank slot becomes "now".
  ++app.liveCaptureCounter_;
  app.liveCaptureSlot_ =
      static_cast<std::uint8_t>((app.liveCaptureCounter_ + app.liveCaptureVariation_) % kShortCaptureSlots);
  app.liveCaptureLineage_ = app.masterSeed_ ^ (app.liveCaptureCounter_ * kLiveCaptureLineageSalt) ^ app.liveCaptureVariation_;
  if (app.liveCaptureLineage_ == 0) {
    app.liveCaptureLineage_ = app.masterSeed_ ? app.masterSeed_ : kDefaultMasterSeed;
  }
  const std::uint32_t reseedValue = RNG::xorshift(app.liveCaptureLineage_);
  seedPageReseed(app, reseedValue, AppState::SeedPrimeMode::kLiveInput);
}

void SeedPrimeRuntimeService::setSeedPreset(AppState& app, std::uint32_t presetId, const std::vector<Seed>& seeds) const {
  app.presetBuffer_.id = presetId;
  app.presetBuffer_.seeds = seeds;
}

void SeedPrimeRuntimeService::setSeedPrimeMode(AppState& app, AppState::SeedPrimeMode mode) const {
  if (app.seedPrimeMode_ != mode) {
    app.seedPrimeMode_ = mode;
    app.displayDirty_ = true;
  } else {
    app.seedPrimeMode_ = mode;
  }
  app.tapTempo_.resetPendingTap();
}

void SeedPrimeRuntimeService::seedPageReseed(AppState& app, std::uint32_t masterSeed, AppState::SeedPrimeMode mode) const {
  setSeedPrimeMode(app, mode);
  reseed(app, masterSeed);
}

void SeedPrimeRuntimeService::reseed(AppState& app, std::uint32_t masterSeed) const {
  // A reseed is more than "make new seeds": it also clears any old preset
  // crossfade and re-binds the scheduler to the engine/router reality.
  primeSeeds(app, masterSeed);
  app.clearPresetCrossfade();
  app.presetController_.setActivePresetSlot(std::string{});
  app.engines_.reseed(app.masterSeed_);
  const bool hardwareMode = (app.engines_.granular().mode() == GranularEngine::Mode::kHardware);
  app.scheduler_.setSampleClockFn(hardwareMode ? &hal::audio::sampleClock : nullptr);
}

void SeedPrimeRuntimeService::primeSeeds(AppState& app, std::uint32_t masterSeed) const {
  app.masterSeed_ = masterSeed ? masterSeed : kDefaultMasterSeed;

  const std::vector<std::uint8_t> previousSelections = app.seedEngineSelections_;
  const std::vector<Seed> previousSeeds = app.seeds_;
  const std::uint8_t previousFocus = app.focusSeed_;
  const bool allowRepeatBias = (app.seedPrimeMode_ == AppState::SeedPrimeMode::kLfsr);
  const auto controllerMode = toPrimeControllerMode(app.seedPrimeMode_);
  const auto buildBatch = [&](std::size_t count) {
    return gSeedPrimeController.buildSeeds(controllerMode, app.masterSeed_, count, app.randomnessPanel_.entropy,
                                           app.randomnessPanel_.mutationRate, app.currentTapTempoBpm(),
                                           app.liveCaptureLineage_, app.liveCaptureSlot_, app.presetBuffer_.seeds,
                                           app.presetBuffer_.id);
  };

  // SeedLock tracks logical slots, so trim it before any generation logic
  // decides whether a particular position should be preserved.
  app.seedLock_.resize(kSeedSlotCount);
  app.seedLock_.trim(kSeedSlotCount);

  std::vector<Seed> generated;
  if (app.seedPrimeBypassEnabled_) {
    // Bypass mode keeps the grid visually stable but only populates the focus
    // slot with real content. The other slots stay intentionally empty.
    generated.assign(kSeedSlotCount, Seed{});
    std::vector<Seed> focusOnly = buildBatch(1);
    if (focusOnly.empty()) {
      focusOnly = gSeedPrimeController.buildSeeds(SeedPrimeController::Mode::kLfsr, app.masterSeed_, 1,
                                                  app.randomnessPanel_.entropy, app.randomnessPanel_.mutationRate,
                                                  app.currentTapTempoBpm(), app.liveCaptureLineage_, app.liveCaptureSlot_,
                                                  app.presetBuffer_.seeds, app.presetBuffer_.id);
    }
    if (!focusOnly.empty()) {
      const std::size_t targetIndex = std::min<std::size_t>(app.focusSeed_, kSeedSlotCount - 1);
      Seed focused = focusOnly.front();
      focused.id = static_cast<std::uint32_t>(targetIndex);
      if (focused.prng == 0) {
        focused.prng = RNG::xorshift(app.masterSeed_);
      }
      generated[targetIndex] = focused;
    }
    if (allowRepeatBias) {
      SeedPrimeRuntimeService::applyRepeatBias(app, previousSeeds, generated);
    }
  } else if (!app.seedLock_.globalLocked() || previousSeeds.empty()) {
    // Normal path: generate a fresh batch, then patch locked slots back in.
    generated = buildBatch(kSeedSlotCount);
    if (generated.empty()) {
      generated = gSeedPrimeController.buildSeeds(SeedPrimeController::Mode::kLfsr, app.masterSeed_, kSeedSlotCount,
                                                  app.randomnessPanel_.entropy, app.randomnessPanel_.mutationRate,
                                                  app.currentTapTempoBpm(), app.liveCaptureLineage_, app.liveCaptureSlot_,
                                                  app.presetBuffer_.seeds, app.presetBuffer_.id);
    }
    if (generated.size() < kSeedSlotCount) {
      generated.resize(kSeedSlotCount);
    }
    if (generated.size() > kSeedSlotCount) {
      generated.resize(kSeedSlotCount);
    }
    if (allowRepeatBias) {
      SeedPrimeRuntimeService::applyRepeatBias(app, previousSeeds, generated);
    }
    for (std::size_t i = 0; i < generated.size(); ++i) {
      if (app.seedLock_.seedLocked(i) && i < previousSeeds.size()) {
        generated[i] = previousSeeds[i];
      } else {
        generated[i].id = static_cast<std::uint32_t>(i);
        if (generated[i].prng == 0) {
          generated[i].prng = RNG::xorshift(app.masterSeed_);
        }
      }
    }
  } else {
    // Global lock means "do not rewrite the current world". We still top up to
    // the expected slot count so downstream UI and scheduler code see a full
    // fixed-width seed table.
    generated = previousSeeds;
    if (generated.size() < kSeedSlotCount) {
      const auto topUp = gSeedPrimeController.buildSeeds(SeedPrimeController::Mode::kLfsr, app.masterSeed_,
                                                         kSeedSlotCount, app.randomnessPanel_.entropy,
                                                         app.randomnessPanel_.mutationRate, app.currentTapTempoBpm(),
                                                         app.liveCaptureLineage_, app.liveCaptureSlot_,
                                                         app.presetBuffer_.seeds, app.presetBuffer_.id);
      for (std::size_t i = generated.size(); i < kSeedSlotCount && i < topUp.size(); ++i) {
        generated.push_back(topUp[i]);
      }
    }
    if (generated.size() > kSeedSlotCount) {
      generated.resize(kSeedSlotCount);
    }
    for (std::size_t i = 0; i < generated.size(); ++i) {
      generated[i].id = static_cast<std::uint32_t>(i);
    }
  }

  app.seeds_ = generated;

  app.scheduler_ = PatternScheduler{};
  const float bpm = (app.seedPrimeMode_ == AppState::SeedPrimeMode::kTapTempo) ? app.currentTapTempoBpm() : 120.f;
  app.setTempoTarget(bpm, true);
  app.scheduler_.setTriggerCallback(&app.engines_, &EngineRouter::dispatchThunk);

  // Only non-empty seeds become scheduled voices. Empty placeholder seeds still
  // exist for UI shape, but they should not advance any pattern state.
  for (const Seed& seed : app.seeds_) {
    if (seed.prng != 0) {
      app.scheduler_.addSeed(seed);
    }
  }

  app.inputGate_.syncGateTick(app.scheduler_.ticks());

  app.seedEngineSelections_.assign(app.seeds_.size(), 0);
  for (std::size_t i = 0; i < app.seeds_.size(); ++i) {
    app.seeds_[i].id = static_cast<std::uint32_t>(i);
    if (app.seeds_[i].prng == 0) {
      app.seedEngineSelections_[i] = app.seeds_[i].engine;
      continue;
    }
    const std::uint8_t desired =
        (i < previousSelections.size()) ? previousSelections[i] : app.seeds_[i].engine;
    // Engine choice is part of the player's intent, so after generation we
    // restore the last chosen engine routing on top of the fresh seed payload.
    app.setSeedEngine(static_cast<std::uint8_t>(i), desired);
  }

  const bool hasSeedContent =
      std::any_of(app.seeds_.begin(), app.seeds_.end(), [](const Seed& s) { return s.prng != 0; });
  if (!app.seeds_.empty()) {
    const std::size_t maxIndex = app.seeds_.size() - 1;
    const std::uint8_t targetFocus =
        previousSeeds.empty() ? 0 : static_cast<std::uint8_t>(std::min<std::size_t>(previousFocus, maxIndex));
    app.setFocusSeed(targetFocus);
  } else {
    app.focusSeed_ = 0;
  }

  app.seedsPrimed_ = hasSeedContent;
  app.clockTransport_.resetTransportState();
  const bool ledOn = !SeedBoxConfig::kQuietMode;
  hal::io::writeDigital(kStatusLedPin, ledOn);
  app.displayDirty_ = true;
}
