#include "app/HostControlService.h"

#include <algorithm>

#include "app/AppState.h"
#include "engine/Granular.h"
#include "hal/hal_audio.h"

// HostControlService is the DAW/editor handshake layer. It keeps external UI
// commands thin and predictable so host automation touches the same runtime
// invariants as the front panel instead of inventing a second control model.

void HostControlService::setSwingPercent(AppState& app, float value) const {
  app.applySwingPercent(value);
}

void HostControlService::applyQuantizeControl(AppState& app, std::uint8_t value) const {
  app.applyQuantizeControl(value);
}

void HostControlService::setDebugMetersEnabled(AppState& app, bool enabled) const {
  if (app.debugMetersEnabled_ == enabled) {
    return;
  }
  // Meters are pure presentation state, so there is no scheduler or engine
  // side effect here beyond telling the UI to redraw.
  app.debugMetersEnabled_ = enabled;
  app.displayDirty_ = true;
}

void HostControlService::setTransportLatch(AppState& app, bool enabled) const {
  if (app.transportLatchEnabled() == enabled) {
    return;
  }
  app.clockTransport_.setTransportLatchEnabled(enabled);
  app.displayDirty_ = true;
}

void HostControlService::setFollowExternalClock(AppState& app, bool enabled) const {
  if (app.followExternalClockEnabled() == enabled) {
    return;
  }
  app.clockTransport_.setFollowExternalClockEnabled(enabled, app.board_.nowMillis());
  app.displayDirty_ = true;
}

void HostControlService::setClockSourceExternal(AppState& app, bool external) const {
  app.clockTransport_.setClockSourceExternal(external, app.board_.nowMillis());
  app.displayDirty_ = true;
}

void HostControlService::setInternalBpm(AppState& app, float bpm) const {
  // Host BPM is sanitized before it ever reaches the smoother so automation
  // cannot drive the transport into nonsense values.
  const float sanitized = std::clamp(bpm, 20.0f, 999.0f);
  app.setTempoTarget(sanitized, false);
  app.displayDirty_ = true;
}

void HostControlService::setDiagnosticsEnabled(AppState& app, bool enabled) const {
  app.diagnosticsEnabled_ = enabled;
  app.scheduler_.setDiagnosticsEnabled(enabled);
  if (enabled) {
    // Enabling diagnostics resets the counters so a fresh host capture starts
    // from zero rather than inheriting whatever the last session left behind.
    app.scheduler_.resetDiagnostics();
  }
}

AppState::DiagnosticsSnapshot HostControlService::diagnosticsSnapshot(const AppState& app) const {
  AppState::DiagnosticsSnapshot snap{};
  snap.scheduler = app.scheduler_.diagnostics();
  snap.audioCallbackCount = app.audioRuntime_.audioCallbackCount();
  return snap;
}

void HostControlService::resetSchedulerForBypass(AppState& app, float bpm) {
  // Seed-prime bypass still needs a living scheduler so transport, clock, and
  // host safety logic keep running even while the seed table is mostly empty.
  app.scheduler_ = PatternScheduler{};
  app.setTempoTarget(bpm, true);
  app.scheduler_.setTriggerCallback(&app.engines_, &EngineRouter::dispatchThunk);
  const bool hardwareMode = (app.enginesReady_ && app.engines_.granular().mode() == GranularEngine::Mode::kHardware);
  app.scheduler_.setSampleClockFn(hardwareMode ? &hal::audio::sampleClock : nullptr);
}

void HostControlService::setSeedPrimeBypass(AppState& app, bool enabled) const {
  if (app.seedPrimeBypassEnabled_ == enabled) {
    return;
  }
  app.seedPrimeBypassEnabled_ = enabled;
  if (app.seedPrimeBypassEnabled_) {
    // Entering bypass preserves the shell of the current machine but clears the
    // audible seed content, leaving only the focused slot pathway available.
    const float bpm = (app.seedPrimeMode_ == AppState::SeedPrimeMode::kTapTempo) ? app.currentTapTempoBpm() : 120.f;
    resetSchedulerForBypass(app, bpm);
    app.seeds_.assign(app.seeds_.size(), Seed{});
    app.seedEngineSelections_.assign(app.seedEngineSelections_.size(), 0);
    app.seedsPrimed_ = false;
    app.setFocusSeed(app.focusSeed_);
  } else {
    // Leaving bypass always reseeds from the current master seed so the user
    // returns to a coherent world rather than half-cleared state.
    app.reseed(app.masterSeed_);
  }
  app.displayDirty_ = true;
}

void HostControlService::setLiveCaptureVariation(AppState& app, std::uint8_t variationSteps) const {
  // Variation is a ring over the short capture slots, not an open-ended value.
  app.liveCaptureVariation_ = static_cast<std::uint8_t>(variationSteps % 4u);
}

void HostControlService::setInputGateDivision(AppState& app, std::uint8_t division) const {
  app.stepGateDivision(static_cast<int>(division) - static_cast<int>(app.gateDivision_));
}

void HostControlService::setInputGateFloor(AppState& app, float floor) const {
  app.inputGate_.setFloor(floor);
}

void HostControlService::setDryInput(AppState& app, const float* left, const float* right, std::size_t frames) const {
  // Host dry input is staged into the InputGateMonitor first; the audio callback
  // decides later whether that input becomes effect material or passthrough.
  app.inputGate_.setDryInput(left, right, frames);
}
