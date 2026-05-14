#include "app/AppUiClockService.h"

// This service keeps the "conductor" behaviors together: page/mode posture,
// swing feel, and the rules for when external transport is allowed to seize the
// instrument. AppState still owns the state; this class narrates how that state
// moves when clocks and UI commands arrive.

#include <algorithm>
#include <cmath>

#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  #include <Arduino.h>
#endif

namespace {
constexpr std::uint32_t kExternalClockTimeoutMs = 2000;
}

void AppUiClockService::setModeFromHost(AppState& app, AppState::Mode mode) const {
  if (mode == AppState::Mode::SWING) {
    enterSwingMode(app);
    return;
  }

  // Host callers are allowed to jump directly between top-level pages, but
  // swing edit mode remains special because it piggybacks the previous page.
  app.swingEditing_ = false;
  app.mode_ = mode;
  app.previousModeBeforeSwing_ = app.mode_;
  app.displayDirty_ = true;
}

void AppUiClockService::enterSwingMode(AppState& app) const {
  if (app.mode_ != AppState::Mode::SWING) {
    app.previousModeBeforeSwing_ = app.mode_;
  }
  app.swingEditing_ = true;
  app.mode_ = AppState::Mode::SWING;
  applySwingPercent(app, app.swingPercent_);
  app.displayDirty_ = true;
}

void AppUiClockService::exitSwingMode(AppState& app, AppState::Mode targetMode) const {
  app.swingEditing_ = false;
  app.previousModeBeforeSwing_ = targetMode;
  app.mode_ = targetMode;
  app.displayDirty_ = true;
}

void AppUiClockService::adjustSwing(AppState& app, float delta) const {
  applySwingPercent(app, app.swingPercent_ + delta);
}

void AppUiClockService::applySwingPercent(AppState& app, float value) const {
  // Swing never reaches 100%; leaving a sliver of straight time avoids zeroing
  // out every second subdivision in the transport math.
  const float clamped = std::clamp(value, 0.0f, 0.99f);
  const bool changed = std::fabs(clamped - app.swingPercent_) > 1e-5f;
  app.swingPercent_ = clamped;
  app.clockTransport_.applySwing(clamped);
  if (changed) {
    app.displayDirty_ = true;
  }
}

void AppUiClockService::selectClockProvider(AppState& app, ClockProvider* provider) const {
  app.clockTransport_.selectClockProvider(provider);
}

void AppUiClockService::toggleClockProvider(AppState& app) const {
  app.clockTransport_.toggleClockProvider();
  app.displayDirty_ = true;
}

void AppUiClockService::toggleTransportLatchedRunning(AppState& app) const {
  app.clockTransport_.toggleTransportLatchedRunning();
}

void AppUiClockService::onExternalClockTick(AppState& app) const {
  if (app.panicSkipNextTick_) {
    app.panicSkipNextTick_ = false;
    return;
  }

  const std::uint32_t now = app.board_.nowMillis();

  // External sync may arrive before the instrument has ever primed a seed set.
  // In that case we lazily build the deterministic startup state first so the
  // incoming clock advances something audible rather than an empty scheduler.
  if (!app.seedsPrimed_ && !app.seedPrimeBypassEnabled_) {
    app.primeSeeds(app.masterSeed_);
  }
#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  const bool wasDominant = app.externalClockDominant();
#endif
  app.clockTransport_.onExternalClockTick(now);
#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  if (!wasDominant && app.externalClockDominant()) {
    Serial.println(F("external clock: TRS/USB seized transport"));
  }
#endif
  if (app.clock() == &app.midiClockIn_ || app.externalClockDominant()) {
    // Once the external lane wins, the MIDI clock, gate reseed logic, and
    // preset-boundary commits all advance from the same tick source.
    app.midiClockIn_.onTick();
    app.handleGateTick();
    app.maybeCommitPendingPreset(app.scheduler_.ticks());
  }
}

void AppUiClockService::onExternalTransportStart(AppState& app) const {
#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  const bool wasDominant = app.externalClockDominant();
#endif
  app.clockTransport_.onExternalTransportStart();
#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  if (!wasDominant && app.externalClockDominant()) {
    Serial.println(F("external clock: transport START"));
  }
#endif
}

void AppUiClockService::onExternalTransportStop(AppState& app) const {
#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  const bool wasDominant = app.externalClockDominant();
#endif
  app.clockTransport_.onExternalTransportStop();
#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  if (wasDominant && !app.externalClockDominant()) {
    Serial.println(F("external clock: transport STOP"));
  }
#endif
}

void AppUiClockService::updateClockDominance(AppState& app) const {
  // Dominance is the one-line truth table behind "who is driving right now?".
  app.clockTransport_.updateClockDominance();
}

void AppUiClockService::updateExternalClockWatchdog(AppState& app) const {
  const bool wasFollowing = app.followExternalClockEnabled();
  // If external clock goes silent long enough, we quietly fall back to the
  // internal provider and mark the display dirty so the UI tells the truth.
  app.clockTransport_.updateExternalClockWatchdog(app.board_.nowMillis(), kExternalClockTimeoutMs);
  if (!app.clockTransport_.followExternalClockEnabled() && wasFollowing) {
    app.displayDirty_ = true;
  }
}
