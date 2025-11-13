#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "util/ScaleQuantizer.h"

namespace util {

enum class QuantizerMode { kNearest, kUp, kDown };

const char* ToString(QuantizerMode mode);

struct QuantizerSample {
  double timeSeconds = 0.0;
  std::size_t slot = 0;
  float inputPitch = 0.0f;
  float driftedPitch = 0.0f;
  float snappedNearest = 0.0f;
  float snappedUp = 0.0f;
  float snappedDown = 0.0f;
  float activePitch = 0.0f;
};

std::vector<QuantizerSample> GenerateQuantizerSamples(
    const std::vector<float>& offsets,
    std::uint8_t root,
    ScaleQuantizer::Scale scale,
    QuantizerMode mode,
    double driftHz,
    float driftDepth,
    std::size_t frameCount);

std::string FormatQuantizerCsv(const std::vector<QuantizerSample>& samples,
                               QuantizerMode mode);

}  // namespace util
