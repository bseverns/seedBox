#pragma once

#include "app/AppState.h"

class DisplayTelemetryService {
public:
  void captureDisplaySnapshot(const AppState& app, AppState::DisplaySnapshot& out, UiState* ui) const;
  void captureLearnFrame(const AppState& app, AppState::LearnFrame& out) const;
};

