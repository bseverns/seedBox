#include "app/PresetTransitionRunner.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "SeedBoxConfig.h"
#include "app/AppState.h"
#include "engine/Granular.h"
#include "hal/hal_audio.h"

// PresetTransitionRunner owns the answer to "when does a new scene become the
// truth?" Sometimes the answer is immediate, sometimes it is a bar boundary,
// and sometimes it is a temporary crossfade where both scenes coexist.

namespace {
constexpr std::string_view kDefaultPresetSlot = "default";

PresetController::Boundary toPresetControllerBoundary(std::uint8_t boundary) {
  switch (static_cast<AppState::PresetBoundary>(boundary)) {
    case AppState::PresetBoundary::Bar: return PresetController::Boundary::kBar;
    case AppState::PresetBoundary::Step:
    default: return PresetController::Boundary::kStep;
  }
}

float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

Seed blendSeeds(const Seed& from, const Seed& to, float t) {
  // Crossfades treat seeds like parameter snapshots. Continuous fields glide;
  // discrete fields snap toward whichever side of the fade currently dominates.
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

}  // namespace

void PresetTransitionRunner::rebuildScheduler(AppState& app, float bpm) {
  // Any time a preset takes over without a same-size crossfade, we rebuild the
  // scheduler from scratch so it reflects the new seed table exactly once.
  app.scheduler_ = PatternScheduler{};
  app.setTempoTarget(bpm, true);
  app.scheduler_.setTriggerCallback(&app.engines_, &EngineRouter::dispatchThunk);
  const bool hardwareMode = (app.engines_.granular().mode() == GranularEngine::Mode::kHardware);
  app.scheduler_.setSampleClockFn(hardwareMode ? &hal::audio::sampleClock : nullptr);
  for (const Seed& seed : app.seeds_) {
    app.scheduler_.addSeed(seed);
  }
}

void PresetTransitionRunner::requestChange(AppState& app, const seedbox::Preset& preset, bool crossfade,
                                           std::uint8_t boundary) const {
  // Requesting a preset is cheap: we queue the intent plus its boundary policy,
  // then let the main tick decide when that intent becomes audible reality.
  app.presetController_.requestPresetChange(preset, crossfade, toPresetControllerBoundary(boundary), app.scheduler_.ticks());
  app.displayDirty_ = true;
}

void PresetTransitionRunner::maybeCommitPending(AppState& app, std::uint64_t currentTick) const {
  auto pending = app.presetController_.takePendingPreset(currentTick);
  if (!pending) {
    return;
  }
  // Once the boundary fires, preset application is intentionally the same code
  // path regardless of whether the request came from storage, host, or UI.
  apply(app, pending->preset, pending->crossfade);
}

seedbox::Preset PresetTransitionRunner::snapshot(const AppState& app, std::string_view slot) const {
  // Snapshotting is the inverse of apply(): capture the current machine state
  // into a portable scene without exposing AppState internals directly.
  PresetController::SnapshotInput input{};
  input.slot = slot.empty() ? kDefaultPresetSlot : slot;
  input.masterSeed = app.masterSeed_;
  input.focusSeed = app.focusSeed_;
  input.bpm = app.scheduler_.bpm();
  input.followExternal = app.followExternalClockEnabled();
  input.debugMeters = app.debugMetersEnabled_;
  input.transportLatch = app.transportLatchEnabled();
  input.page = static_cast<seedbox::PageId>(app.currentPage_);
  input.seeds = &app.seeds_;
  input.engineSelections = &app.seedEngineSelections_;
  return app.presetController_.snapshotPreset(input);
}

void PresetTransitionRunner::apply(AppState& app, const seedbox::Preset& preset, bool crossfade) const {
  // Applying a preset updates both the obvious musical state (seeds, BPM, page)
  // and the operational state around it (locks, crossfade state, gate timing).
  app.presetController_.setActivePresetSlot(preset.slot.empty() ? std::string(kDefaultPresetSlot) : preset.slot);
  app.masterSeed_ = preset.masterSeed;
  app.clockTransport_.applyPresetClockState(preset.clock.followExternal, preset.clock.transportLatch,
                                            app.clockTransport_.externalTransportRunning());
  app.debugMetersEnabled_ = preset.clock.debugMeters;
  app.setTempoTarget(preset.clock.bpm, true);
  app.currentPage_ = static_cast<AppState::Page>(preset.page);
  app.storageButtonHeld_ = false;
  app.storageLongPress_ = false;

  if (!preset.engineSelections.empty()) {
    app.seedEngineSelections_ = preset.engineSelections;
  }
  if (app.seedEngineSelections_.size() < preset.seeds.size()) {
    app.seedEngineSelections_.resize(preset.seeds.size(), 0);
    for (std::size_t i = 0; i < preset.seeds.size(); ++i) {
      app.seedEngineSelections_[i] = preset.seeds[i].engine;
    }
  }

  const bool haveSeeds = !preset.seeds.empty();
  const bool doCrossfade = crossfade && haveSeeds && !app.seeds_.empty() && preset.seeds.size() == app.seeds_.size();
  if (doCrossfade) {
    // Crossfade only makes sense when both worlds have the same slot geometry.
    // Otherwise we fall back to a hard cut plus scheduler rebuild.
    app.presetController_.beginCrossfade(app.seeds_, preset.seeds, AppState::kPresetCrossfadeTicks);
  } else {
    app.seeds_ = preset.seeds;
    app.presetController_.clearCrossfade();
    rebuildScheduler(app, preset.clock.bpm);
  }

  app.setFocusSeed(preset.focusSeed);
  app.seedsPrimed_ = haveSeeds;
  app.setSeedPreset(preset.masterSeed, preset.seeds);
  app.inputGate_.syncGateTick(app.scheduler_.ticks());
  app.displayDirty_ = true;
}

void PresetTransitionRunner::stepCrossfade(AppState& app) const {
  if (!app.presetController_.crossfadeActive()) {
    return;
  }
  const auto& crossfade = app.presetController_.crossfade();
  if (crossfade.from.size() != crossfade.to.size() || crossfade.to.size() != app.seeds_.size()) {
    // A malformed crossfade state is resolved by committing the destination
    // scene outright; silent corruption is worse than an abrupt but correct cut.
    app.seeds_ = crossfade.to;
    app.presetController_.clearCrossfade();
    rebuildScheduler(app, app.scheduler_.bpm());
    return;
  }

  const float total = static_cast<float>(crossfade.total);
  const float remaining = static_cast<float>(crossfade.remaining);
  const float mix = (total <= 0.f) ? 1.0f : (1.0f - (remaining / total));
  for (std::size_t i = 0; i < app.seeds_.size(); ++i) {
    // We update the scheduler slot-by-slot during the fade so any currently
    // running transport reflects the in-between world, not just the endpoints.
    app.seeds_[i] = blendSeeds(crossfade.from[i], crossfade.to[i], mix);
    app.scheduler_.updateSeed(i, app.seeds_[i]);
  }

  app.presetController_.decrementCrossfade();
  if (!app.presetController_.crossfadeActive()) {
    // The final tick snaps to the exact destination preset so no accumulated
    // float drift from the blend math survives after the fade is done.
    app.seeds_ = crossfade.to;
    for (std::size_t i = 0; i < app.seeds_.size(); ++i) {
      app.scheduler_.updateSeed(i, app.seeds_[i]);
    }
    app.presetController_.clearCrossfade();
  }
}

void PresetTransitionRunner::clearCrossfade(AppState& app) const {
  app.presetController_.clearCrossfade();
}
