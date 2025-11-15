#pragma once

//
// MN42 → SeedBox parameter map skeleton.
// -------------------------------------
// The controller integration is still cooking, but we expose the structure now
// so future PRs can drop real mappings without re-litigating design decisions.
// Treat this as a studio notebook: enough scaffolding to explain the intent,
// with plenty of TODO space left for the actual patchwork of parameters.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace seedbox::interop::mn42 {

namespace param {
//
// MN42 front-panel → SeedBox seed parameter map.
// ------------------------------------------------
// The controller ships with a ring of eight endless encoders whose CC numbers
// live in the low 20s.  They target whichever seed AppState has focused:
//   * 20 keeps its legacy “engine cycle” behaviour so remote rigs can rotate
//     engines without touching the local UI.
//   * 21 picks the focused seed (divide 0–127 into as many zones as there are
//     seeds and clamp the top edge).
//   * 22–28 drive the actual genome knobs — pitch, density, probability,
//     jitter, tone, spread, and mutation depth.
//
// The constants below intentionally read like a crash course so docs/tests can
// copy-paste without re-deriving the lore.
constexpr std::uint8_t kEngineCycle = 20;   // val >= 64 spins forward, <64 back.
constexpr std::uint8_t kFocusSeed = 21;     // value buckets select active seed.
constexpr std::uint8_t kSeedPitch = 22;     // 0 → -24 semitones, 127 → +24.
constexpr std::uint8_t kSeedDensity = 23;   // 0 → 0 hits/beat, 127 → ~8.
constexpr std::uint8_t kSeedProbability = 24;  // Linear 0..1 gate probability.
constexpr std::uint8_t kSeedJitter = 25;    // Linear 0..30ms swing spray.
constexpr std::uint8_t kSeedTone = 26;      // Tilt EQ macro 0..1.
constexpr std::uint8_t kSeedSpread = 27;    // Stereo width 0 (mono) .. 1 (wide).
constexpr std::uint8_t kSeedMutate = 28;    // Mutation depth guard rail 0..1.
}  // namespace param

struct ParamDescriptor {
  std::uint8_t controller = 0;
  std::string_view label{};
  std::string_view notes{};
};

struct ParamMap {
  std::array<ParamDescriptor, 32> entries{};
  std::size_t size = 0;

  const ParamDescriptor* find(std::uint8_t controller) const {
    for (std::size_t i = 0; i < size; ++i) {
      if (entries[i].controller == controller) {
        return &entries[i];
      }
    }
    return nullptr;
  }
};

inline ParamMap BuildDefaultParamMap() {
  ParamMap map{};
  map.entries[0] = {param::kEngineCycle, "Engine cycle",
                    "Encoder press sends >=64 to advance, <64 to reverse."};
  map.entries[1] = {param::kFocusSeed, "Focus seed",
                    "Divide 0–127 into equal slices and clamp to the last seed."};
  map.entries[2] = {param::kSeedPitch, "Seed pitch",
                    "Map 0..127 → -24..+24 semitones around concert A."};
  map.entries[3] = {param::kSeedDensity, "Seed density",
                    "Linear 0..127 ramp into 0..8 hits per beat."};
  map.entries[4] = {param::kSeedProbability, "Seed probability",
                    "Set the Bernoulli gate weight 0..1."};
  map.entries[5] = {param::kSeedJitter, "Seed jitter",
                    "Translate 0..127 into 0..30ms timing spray."};
  map.entries[6] = {param::kSeedTone, "Seed tone",
                    "Tilt the EQ macro between dark (0) and bright (1)."};
  map.entries[7] = {param::kSeedSpread, "Seed spread",
                    "Stereo width macro from mono (0) to wide (1)."};
  map.entries[8] = {param::kSeedMutate, "Seed mutate",
                    "Mutation depth guard rail, linear 0..1."};
  map.size = 9;
  return map;
}

inline ParamMap& GetMutableParamMap() {
  static ParamMap map = BuildDefaultParamMap();
  return map;
}

inline const ParamMap& GetParamMap() {
  return GetMutableParamMap();
}

inline const ParamDescriptor* LookupParam(std::uint8_t controller) {
  return GetParamMap().find(controller);
}

}  // namespace seedbox::interop::mn42
