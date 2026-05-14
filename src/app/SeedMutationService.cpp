#include "app/SeedMutationService.h"

#include <algorithm>
#include <cstring>

#include "engine/Granular.h"

// SeedMutationService owns "surgical" edits to individual seeds: focusing a
// slot, swapping its engine, nudging a few parameters, or cycling granular
// source material. The rule is simple: if a seed changes, scheduler and engine
// state must be told immediately.

std::uint8_t SeedMutationService::sanitizeEngine(const EngineRouter& router, std::uint8_t engine) {
  // Host and MIDI surfaces are allowed to overrun the engine range; we wrap the
  // ID back into the valid registry instead of rejecting the gesture outright.
  const std::size_t count = router.engineCount();
  if (count == 0) {
    return 0;
  }
  return static_cast<std::uint8_t>(engine % count);
}

void SeedMutationService::syncSeedState(AppState& app, std::size_t idx) {
  // Scheduler and engine caches both mirror the seed table, so any successful
  // mutation has to refresh both or the next trigger will speak stale data.
  app.scheduler_.updateSeed(idx, app.seeds_[idx]);
  app.engines_.onSeed(app.seeds_[idx]);
  app.displayDirty_ = true;
}

void SeedMutationService::setFocusSeed(AppState& app, std::uint8_t index) const {
  if (app.seeds_.empty()) {
    app.focusSeed_ = 0;
    return;
  }
  const std::uint8_t count = static_cast<std::uint8_t>(app.seeds_.size());
  // Focus wraps forever so encoder travel feels circular, not bounded.
  app.focusSeed_ = static_cast<std::uint8_t>(index % count);
}

void SeedMutationService::setSeedEngine(AppState& app, std::uint8_t seedIndex, std::uint8_t engineId) const {
  if (app.seeds_.empty()) {
    return;
  }
  const std::size_t count = app.seeds_.size();
  const std::size_t idx = static_cast<std::size_t>(seedIndex) % count;
  const std::uint8_t sanitized = sanitizeEngine(app.engines_, engineId);

  if (app.seedEngineSelections_.size() < count) {
    app.seedEngineSelections_.resize(count, 0);
  }

  Seed& seed = app.seeds_[idx];
  if (seed.prng == 0) {
    // Empty placeholder seeds still remember which engine they should wake up
    // with later, but there is no live scheduler state to refresh yet.
    seed.engine = sanitized;
    app.seedEngineSelections_[idx] = sanitized;
    app.displayDirty_ = true;
    return;
  }

  seed.engine = sanitized;
  app.seedEngineSelections_[idx] = sanitized;
  app.engines_.assignSeed(idx, sanitized);
  syncSeedState(app, idx);
}

bool SeedMutationService::applySeedEditFromHost(AppState& app, std::uint8_t seedIndex,
                                                const std::function<void(Seed&)>& edit) const {
  if (app.seeds_.empty()) {
    return false;
  }

  const std::size_t idx = static_cast<std::size_t>(seedIndex) % app.seeds_.size();
  if (app.seedLock_.globalLocked() || app.seedLock_.seedLocked(idx)) {
    return false;
  }

  Seed before = app.seeds_[idx];
  edit(app.seeds_[idx]);

  // Bytewise equality is enough here because Seed is treated as a plain state
  // packet; if nothing changed, we avoid pointless scheduler churn.
  if (std::memcmp(&before, &app.seeds_[idx], sizeof(Seed)) == 0) {
    return false;
  }

  syncSeedState(app, idx);
  return true;
}

void SeedMutationService::seedPageNudge(AppState& app, std::uint8_t index, const AppState::SeedNudge& nudge) const {
  if (app.seeds_.empty()) {
    return;
  }
  const std::size_t idx = static_cast<std::size_t>(index) % app.seeds_.size();
  if (app.seedLock_.seedLocked(idx)) {
    return;
  }

  Seed& seed = app.seeds_[idx];
  // Nudges are intentionally local and clamp-heavy: they should feel like safe
  // performance gestures, not arbitrary structure edits.
  if (nudge.pitchSemitones != 0.f) {
    seed.pitch += nudge.pitchSemitones;
  }
  if (nudge.densityDelta != 0.f) {
    seed.density = std::max(0.f, seed.density + nudge.densityDelta);
  }
  if (nudge.probabilityDelta != 0.f) {
    seed.probability = std::clamp(seed.probability + nudge.probabilityDelta, 0.f, 1.f);
  }
  if (nudge.jitterDeltaMs != 0.f) {
    seed.jitterMs = std::max(0.f, seed.jitterMs + nudge.jitterDeltaMs);
  }
  if (nudge.toneDelta != 0.f) {
    seed.tone = std::clamp(seed.tone + nudge.toneDelta, 0.f, 1.f);
  }
  if (nudge.spreadDelta != 0.f) {
    seed.spread = std::clamp(seed.spread + nudge.spreadDelta, 0.f, 1.f);
  }

  syncSeedState(app, idx);
}

void SeedMutationService::seedPageCycleGranularSource(AppState& app, std::uint8_t index, std::int32_t steps) const {
  if (app.seeds_.empty() || steps == 0) {
    return;
  }
  const std::size_t idx = static_cast<std::size_t>(index) % app.seeds_.size();
  if (app.seedLock_.seedLocked(idx)) {
    return;
  }

  Seed& seed = app.seeds_[idx];
  const std::uint8_t originalSource = seed.granular.source;
  const std::uint8_t originalSlot = seed.granular.sdSlot;

  constexpr std::uint8_t kClipSlots = GranularEngine::kSdClipSlots;
  const bool sdClipsAvailable = kClipSlots > 1;

  if (!sdClipsAvailable) {
    // Without SD clips, the source selector collapses to "live input only".
    const std::uint8_t liveEncoded = static_cast<std::uint8_t>(GranularEngine::Source::kLiveInput);
    seed.granular.source = liveEncoded;
    seed.granular.sdSlot = 0;
    if (seed.granular.source == originalSource && seed.granular.sdSlot == originalSlot) {
      return;
    }
    syncSeedState(app, idx);
    return;
  }

  seed.granular.sdSlot = static_cast<std::uint8_t>(seed.granular.sdSlot % kClipSlots);

  GranularEngine::Source source = static_cast<GranularEngine::Source>(seed.granular.source);
  if (source != GranularEngine::Source::kSdClip) {
    source = GranularEngine::Source::kLiveInput;
  }

  const int direction = (steps > 0) ? 1 : -1;
  const int stepCount = static_cast<int>((steps > 0) ? steps : -steps);

  // Cycling alternates between the live lane and the numbered SD clip lane.
  // The clip slot helper keeps slot zero reserved for "not currently on SD".
  const auto cycleSlot = [&](std::uint8_t current) {
    int slot = static_cast<int>(current % kClipSlots);
    if (slot <= 0) {
      slot = (direction > 0) ? 1 : (kClipSlots - 1);
    } else {
      slot += direction;
      if (slot >= kClipSlots) {
        slot = 1;
      } else if (slot <= 0) {
        slot = kClipSlots - 1;
      }
    }
    return static_cast<std::uint8_t>(slot);
  };

  for (int i = 0; i < stepCount; ++i) {
    if (source == GranularEngine::Source::kLiveInput) {
      source = GranularEngine::Source::kSdClip;
      seed.granular.sdSlot = cycleSlot(seed.granular.sdSlot);
    } else {
      source = GranularEngine::Source::kLiveInput;
    }
  }

  if (source == GranularEngine::Source::kSdClip && seed.granular.sdSlot == 0) {
    seed.granular.sdSlot = 1;
  }

  const std::uint8_t encodedSource = static_cast<std::uint8_t>(source);
  if (encodedSource == originalSource && seed.granular.sdSlot == originalSlot) {
    return;
  }

  seed.granular.source = encodedSource;
  syncSeedState(app, idx);
}
