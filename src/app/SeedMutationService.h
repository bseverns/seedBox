#pragma once

#include <cstdint>
#include <functional>

#include "app/AppState.h"

class SeedMutationService {
public:
  // Centralize focused-seed edits so lock rules, engine sanitization, and cache
  // refresh behavior stay consistent across UI and host paths.
  void setFocusSeed(AppState& app, std::uint8_t index) const;
  void setSeedEngine(AppState& app, std::uint8_t seedIndex, std::uint8_t engineId) const;
  bool applySeedEditFromHost(AppState& app, std::uint8_t seedIndex, const std::function<void(Seed&)>& edit) const;
  void seedPageNudge(AppState& app, std::uint8_t index, const AppState::SeedNudge& nudge) const;
  void seedPageCycleGranularSource(AppState& app, std::uint8_t index, std::int32_t steps) const;

private:
  static std::uint8_t sanitizeEngine(const EngineRouter& router, std::uint8_t engine);
  static void syncSeedState(AppState& app, std::size_t idx);
};
