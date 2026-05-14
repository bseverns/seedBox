#pragma once

#include <cstdint>

namespace seedbox {
struct StatusSnapshot {
  char mode[12];
  char page[12];
  std::uint32_t masterSeed{0};
  std::uint32_t activePresetId{0};
  char activePresetSlot[33];
  float bpm{0.0f};
  std::uint64_t schedulerTick{0};
  bool externalClockDominant{false};
  bool followExternalClockEnabled{false};
  bool waitingForExternalClock{false};
  bool quietMode{false};
  bool globalSeedLocked{false};
  bool focusSeedLocked{false};
  bool hasFocusedSeed{false};
  std::uint8_t focusSeedIndex{0};
  std::uint32_t focusSeedId{0};
  std::uint8_t focusSeedEngineId{0};
  char focusSeedEngineName[16];
};
}  // namespace seedbox
