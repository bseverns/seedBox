#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "Seed.h"
#include "SeedLock.h"
#include "app/DisplaySnapshot.h"
#include "app/Preset.h"
#include "app/UiState.h"
#include "engine/EngineRouter.h"
#include "engine/Granular.h"

class DisplaySnapshotBuilder {
public:
  struct Input {
    std::uint32_t masterSeed{0};
    float sampleRate{0.0f};
    std::size_t framesPerBlock{0};
    bool ledOn{false};
    std::uint64_t audioCallbackCount{0};
    std::uint64_t frame{0};
    std::uint8_t mode{0};
    std::uint8_t currentPage{0};
    std::uint8_t seedPrimeMode{0};
    std::uint8_t gateDivision{0};
    std::uint8_t focusSeed{0};
    float bpm{0.0f};
    float swing{0.0f};
    bool externalClockDominant{false};
    bool waitingForExternalClock{false};
    bool debugMetersEnabled{false};
    bool seedPrimeBypassEnabled{false};
    bool quietMode{false};
    bool followExternalClockEnabled{false};
    bool inputGateHot{false};
    bool gateEdgePending{false};
    const std::vector<Seed>* seeds{nullptr};
    const EngineRouter* engines{nullptr};
    const SeedLock* seedLock{nullptr};
    const Seed* schedulerSeed{nullptr};
    const GranularEngine::Stats* granularStats{nullptr};
  };

  // Build the human-facing "what is SeedBox doing right now?" frame used by
  // OLED, simulator, and debugging surfaces.
  void build(seedbox::DisplaySnapshot& out, UiState& uiOut, const Input& input) const;

private:
  static const char* modeLabel(std::uint8_t mode);
  static const char* gateDivisionLabel(std::uint8_t division);
  static const char* primeModeLabel(std::uint8_t primeMode);
  static const char* engineLongName(std::uint8_t engine);
  static std::string_view engineLabel(const EngineRouter& router, std::uint8_t engine);
};
