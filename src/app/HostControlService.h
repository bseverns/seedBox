#pragma once

#include <cstddef>
#include <cstdint>

#include "app/AppState.h"

class HostControlService {
public:
  // Host/editor entrypoints mirror panel controls but speak in transport- and
  // automation-friendly units.
  void setSwingPercent(AppState& app, float value) const;
  void applyQuantizeControl(AppState& app, std::uint8_t value) const;
  void setDebugMetersEnabled(AppState& app, bool enabled) const;
  void setTransportLatch(AppState& app, bool enabled) const;
  void setFollowExternalClock(AppState& app, bool enabled) const;
  void setClockSourceExternal(AppState& app, bool external) const;
  void setInternalBpm(AppState& app, float bpm) const;
  void setDiagnosticsEnabled(AppState& app, bool enabled) const;
  AppState::DiagnosticsSnapshot diagnosticsSnapshot(const AppState& app) const;
  void setSeedPrimeBypass(AppState& app, bool enabled) const;
  void setLiveCaptureVariation(AppState& app, std::uint8_t variationSteps) const;
  void setInputGateDivision(AppState& app, std::uint8_t division) const;
  void setInputGateFloor(AppState& app, float floor) const;
  void setDryInput(AppState& app, const float* left, const float* right, std::size_t frames) const;

private:
  static void resetSchedulerForBypass(AppState& app, float bpm);
};
