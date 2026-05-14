#include "app/HostAudioRealtimeService.h"

#include "app/AppState.h"

void HostAudioRealtimeService::tick(AppState& app) const {
  // The callback contract is intentionally short:
  // 1. sync transport/tempo state
  // 2. advance one clock edge if this block owns one
  // 3. publish frame-level runtime stats
  syncTransportState(app);
  advanceClockEdgeIfNeeded(app);
  publishFrameState(app);
}

void HostAudioRealtimeService::syncTransportState(AppState& app) {
  app.updateTempoSmoothing();
  app.updateExternalClockWatchdog();
}

void HostAudioRealtimeService::advanceClockEdgeIfNeeded(AppState& app) {
  if (!app.externalClockDominant()) {
    if (!app.clock()) {
      app.selectClockProvider(&app.internalClock_);
    }
    if (app.panicSkipNextTick_) {
      app.panicSkipNextTick_ = false;
      return;
    }
    app.clock()->onTick();
    app.handleGateTick();
    return;
  }

  if (app.panicSkipNextTick_) {
    app.panicSkipNextTick_ = false;
  }
}

void HostAudioRealtimeService::publishFrameState(AppState& app) {
  app.stepPresetCrossfade();
  ++app.frame_;
}
