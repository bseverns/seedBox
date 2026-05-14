#pragma once

#include <cstdint>

#include "app/AppState.h"

class GateQuantizeService {
public:
  // Input gate reseeds and pitch quantize edits both change the scheduler's
  // musical grid, so they live together here.
  void handleGateTick(AppState& app) const;
  void stepGateDivision(AppState& app, int delta) const;
  void applyQuantizeControl(AppState& app, std::uint8_t value) const;

private:
  static std::uint32_t gateDivisionTicksFor(AppState::GateDivision div);
};
