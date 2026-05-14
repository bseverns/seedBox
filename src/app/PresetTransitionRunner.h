#pragma once

#include <cstdint>
#include <string_view>

#include "app/Preset.h"

class AppState;

class PresetTransitionRunner {
public:
  void requestChange(AppState& app, const seedbox::Preset& preset, bool crossfade, std::uint8_t boundary) const;
  void maybeCommitPending(AppState& app, std::uint64_t currentTick) const;
  seedbox::Preset snapshot(const AppState& app, std::string_view slot) const;
  void apply(AppState& app, const seedbox::Preset& preset, bool crossfade) const;
  void stepCrossfade(AppState& app) const;
  void clearCrossfade(AppState& app) const;

private:
  static void rebuildScheduler(AppState& app, float bpm);
};
