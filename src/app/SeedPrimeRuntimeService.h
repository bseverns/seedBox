#pragma once

#include <cstdint>
#include <vector>

#include "app/AppState.h"

class SeedPrimeRuntimeService {
public:
  void handleReseedRequest(AppState& app) const;
  void triggerLiveCaptureReseed(AppState& app) const;
  void setSeedPreset(AppState& app, std::uint32_t presetId, const std::vector<Seed>& seeds) const;
  void setSeedPrimeMode(AppState& app, AppState::SeedPrimeMode mode) const;
  void seedPageReseed(AppState& app, std::uint32_t masterSeed, AppState::SeedPrimeMode mode) const;
  void reseed(AppState& app, std::uint32_t masterSeed) const;
  void primeSeeds(AppState& app, std::uint32_t masterSeed) const;

private:
  static constexpr std::uint32_t kDefaultMasterSeed = 0x5EEDB0B1u;
  static constexpr std::size_t kSeedSlotCount = 4u;
  static constexpr std::uint8_t kShortCaptureSlots = 4u;
  static float deterministicNormalizedValue(std::uint32_t masterSeed, std::size_t slot, std::uint32_t salt);
  static Seed blendSeeds(const Seed& from, const Seed& to, float t);
  static void applyRepeatBias(AppState& app, const std::vector<Seed>& previousSeeds, std::vector<Seed>& generated);
};
