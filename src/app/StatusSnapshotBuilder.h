#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Seed.h"
#include "SeedLock.h"
#include "app/StatusSnapshot.h"
#include "engine/EngineRouter.h"

class StatusSnapshotBuilder {
public:
  struct Input {
    std::string_view mode{};
    std::string_view page{};
    std::uint32_t masterSeed{0};
    std::uint32_t activePresetId{0};
    std::string_view activePresetSlot{};
    float bpm{0.0f};
    std::uint64_t schedulerTick{0};
    seedbox::StatusSnapshot::HostDiagnostics hostDiagnostics{};
    bool externalClockDominant{false};
    bool followExternalClockEnabled{false};
    bool waitingForExternalClock{false};
    bool quietMode{false};
    std::uint8_t focusSeed{0};
    const std::vector<Seed>* seeds{nullptr};
    const EngineRouter* engines{nullptr};
    const SeedLock* seedLock{nullptr};
  };

  void build(seedbox::StatusSnapshot& out, const Input& input) const;
  std::string toJson(const seedbox::StatusSnapshot& status) const;

private:
  static const char* engineLongName(std::uint8_t engine);
};
