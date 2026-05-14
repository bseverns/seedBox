#pragma once

#include <cstdint>

class AppState;

class Mn42ControlRouter {
public:
  // Translate the compact MN-42 CC vocabulary into the same runtime mutations
  // the front panel would make locally.
  void route(AppState& app, std::uint8_t channel, std::uint8_t controller, std::uint8_t value) const;

private:
  void applyModeBits(AppState& app, std::uint8_t value) const;
  bool applyParamControl(AppState& app, std::uint8_t controller, std::uint8_t value) const;
  void handleTransportGate(AppState& app, std::uint8_t value) const;
  void cycleFocusedSeedEngine(AppState& app, std::uint8_t value) const;
};
