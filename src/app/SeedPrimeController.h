#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "Seed.h"
#include "engine/Granular.h"

// SeedPrimeController owns the recipe book for turning a master seed into a
// repeatable seed table. AppState still decides when priming happens; this
// helper just packages the named prime modes into a small, testable unit.
class SeedPrimeController {
public:
  enum class Mode : std::uint8_t {
    kLfsr = 0,
    kTapTempo,
    kPreset,
    kLiveInput,
  };

  Mode rotateMode(Mode current, int step) const;
  const char* modeLabel(Mode mode) const;

  std::vector<Seed> buildSeeds(Mode mode,
                               std::uint32_t masterSeed,
                               std::size_t count,
                               float entropy,
                               float mutationRate,
                               float tapTempoBpm,
                               std::uint32_t liveCaptureLineage,
                               std::uint8_t liveCaptureSlot,
                               const std::vector<Seed>& presetSeeds,
                               std::uint32_t presetId) const;

private:
  std::vector<Seed> buildLfsrSeeds(std::uint32_t masterSeed,
                                   std::size_t count,
                                   float entropy,
                                   float mutationRate) const;
  std::vector<Seed> buildTapTempoSeeds(std::uint32_t masterSeed,
                                       std::size_t count,
                                       float bpm,
                                       float entropy,
                                       float mutationRate) const;
  std::vector<Seed> buildPresetSeeds(std::uint32_t masterSeed,
                                     std::size_t count,
                                     const std::vector<Seed>& presetSeeds,
                                     std::uint32_t presetId,
                                     float entropy,
                                     float mutationRate) const;
  std::vector<Seed> buildLiveInputSeeds(std::uint32_t masterSeed,
                                        std::size_t count,
                                        std::uint32_t liveCaptureLineage,
                                        std::uint8_t liveCaptureSlot,
                                        float entropy,
                                        float mutationRate) const;
};
