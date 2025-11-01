#pragma once

//
// Preset.h
// --------
// Snapshot of the instrument's state that we can serialize to disk/EEPROM.
// The struct is intentionally verbose — think of it as a teaching tool that
// documents which dials we care to persist.  Serialization helpers live in the
// matching .cpp so we can keep ArduinoJson spillover out of most translation
// units.
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Seed.h"

namespace seedbox {

enum class PageId : std::uint8_t {
  kSeeds = 0,
  kStorage = 1,
  kClock = 2,
};

struct PresetClockSettings {
  float bpm{120.f};
  bool followExternal{false};
  bool debugMeters{false};
  bool transportLatch{false};
};

struct Preset {
  std::string slot;              // human-facing preset label / storage key
  std::uint32_t masterSeed{0};
  std::uint8_t focusSeed{0};
  PresetClockSettings clock{};
  std::vector<Seed> seeds{};
  std::vector<std::uint8_t> engineSelections{};
  PageId page{PageId::kSeeds};

  // Serialize to JSON (UTF-8).  The returned buffer is suitable for dumping to
  // EEPROM, SD, or any other Store backend.
  std::vector<std::uint8_t> serialize() const;

  // Hydrate from JSON.  Returns false when parsing fails or the JSON misses
  // essential fields.
  static bool deserialize(const std::vector<std::uint8_t>& bytes, Preset& out);
};

}  // namespace seedbox
