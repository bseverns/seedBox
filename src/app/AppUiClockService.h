#pragma once

#include <cstdint>

#include "app/AppState.h"

class AppUiClockService {
public:
  void setModeFromHost(AppState& app, AppState::Mode mode) const;
  void enterSwingMode(AppState& app) const;
  void exitSwingMode(AppState& app, AppState::Mode targetMode) const;
  void adjustSwing(AppState& app, float delta) const;
  void applySwingPercent(AppState& app, float value) const;
  void selectClockProvider(AppState& app, ClockProvider* provider) const;
  void toggleClockProvider(AppState& app) const;
  void toggleTransportLatchedRunning(AppState& app) const;
  void onExternalClockTick(AppState& app) const;
  void onExternalTransportStart(AppState& app) const;
  void onExternalTransportStop(AppState& app) const;
  void updateClockDominance(AppState& app) const;
  void updateExternalClockWatchdog(AppState& app) const;
};

