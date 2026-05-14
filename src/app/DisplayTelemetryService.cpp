#include "app/DisplayTelemetryService.h"

#include <algorithm>

#include "SeedBoxConfig.h"
#include "app/DisplaySnapshotBuilder.h"
#include "hal/hal_audio.h"
#include "hal/hal_io.h"

// The display and learn-frame snapshots are the repo's public "witness
// statements". They translate dense runtime state into something a student,
// tester, or plugin editor can read without spelunking the engine internals.

namespace {
constexpr hal::io::PinNumber kStatusLedPin = 13;
}

void DisplayTelemetryService::captureDisplaySnapshot(const AppState& app, AppState::DisplaySnapshot& out,
                                                     UiState* ui) const {
  UiState localUi{};
  UiState* uiOut = ui ? ui : &localUi;
  DisplaySnapshotBuilder builder;
  DisplaySnapshotBuilder::Input input{};
  input.masterSeed = app.masterSeed_;
  input.sampleRate = hal::audio::sampleRate();
  input.framesPerBlock = hal::audio::framesPerBlock();
  input.ledOn = hal::io::readDigital(kStatusLedPin);
  input.audioCallbackCount = app.audioRuntime_.audioCallbackCount();
  input.frame = app.frame_;
  input.mode = static_cast<std::uint8_t>(app.mode_);
  input.currentPage = static_cast<std::uint8_t>(app.currentPage_);
  input.seedPrimeMode = static_cast<std::uint8_t>(app.seedPrimeMode_);
  input.gateDivision = static_cast<std::uint8_t>(app.gateDivision_);
  input.focusSeed = app.focusSeed_;
  input.bpm = app.scheduler_.bpm();
  input.swing = app.swingPercent_;
  input.externalClockDominant = app.externalClockDominant();
  input.waitingForExternalClock = app.waitingForExternalClock();
  input.debugMetersEnabled = app.debugMetersEnabled_;
  input.seedPrimeBypassEnabled = app.seedPrimeBypassEnabled_;
  input.quietMode = SeedBoxConfig::kQuietMode;
  input.followExternalClockEnabled = app.followExternalClockEnabled();
  input.inputGateHot = app.inputGate_.hot();
  input.gateEdgePending = app.inputGate_.gateEdgePending();
  input.seeds = &app.seeds_;
  input.engines = &app.engines_;
  input.seedLock = &app.seedLock_;
  input.schedulerSeed = app.debugScheduledSeed(app.focusSeed_);
  input.granularStats = &app.engines_.granular().stats();
  // DisplaySnapshotBuilder owns the rendering language; this service just
  // assembles the truth bundle it needs.
  builder.build(out, *uiOut, input);
}

void DisplayTelemetryService::captureLearnFrame(const AppState& app, AppState::LearnFrame& out) const {
  out = AppState::LearnFrame{};
  out.audio = app.latestAudioMetrics_;

  out.generator.bpm = app.scheduler_.bpm();
  out.generator.clock =
      app.externalClockDominant() ? UiState::ClockSource::kExternal : UiState::ClockSource::kInternal;
  out.generator.tick = app.scheduler_.ticks();
  constexpr std::uint32_t kTicksPerBeat = 24u;
  const std::uint32_t ticksPerBar = AppState::kPresetBoundaryTicksPerBar;
  out.generator.step = static_cast<std::uint32_t>(out.generator.tick % kTicksPerBeat);
  out.generator.bar = static_cast<std::uint32_t>(out.generator.tick / ticksPerBar);
  out.generator.events = app.scheduler_.lastTickTriggerCount();

  // LearnFrame intentionally spotlights the focused seed because that is the
  // one a performer or student is most likely to be editing while reading the
  // telemetry.
  if (!app.seeds_.empty()) {
    const std::size_t index = std::min<std::size_t>(app.focusSeed_, app.seeds_.size() - 1);
    const Seed& s = app.seeds_[index];
    out.generator.focusSeedId = s.id;
    out.generator.focusMutateAmt = s.mutateAmt;
    out.generator.density = s.density;
    out.generator.probability = s.probability;
  } else {
    out.generator.focusSeedId = app.focusSeed_;
  }

  out.generator.mutationCount =
      static_cast<std::uint32_t>(std::count_if(app.seeds_.begin(), app.seeds_.end(), [](const Seed& seed) {
        return seed.mutateAmt > 0.0f;
      }));
  out.generator.primeMode = app.seedPrimeMode_;
  out.generator.tapTempoBpm = app.currentTapTempoBpm();
  out.generator.lastTapIntervalMs = app.tapTempo_.lastIntervalMs();
  out.generator.mutationRate = app.randomnessPanel_.mutationRate;
}
