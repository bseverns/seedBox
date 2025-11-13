#include "util/ScaleQuantizerFlow.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace util {

namespace {

float ActiveForMode(float nearest, float up, float down, QuantizerMode mode) {
  switch (mode) {
    case QuantizerMode::kNearest:
      return nearest;
    case QuantizerMode::kUp:
      return up;
    case QuantizerMode::kDown:
      return down;
  }
  return nearest;
}

}  // namespace

const char* ToString(QuantizerMode mode) {
  switch (mode) {
    case QuantizerMode::kNearest:
      return "nearest";
    case QuantizerMode::kUp:
      return "up";
    case QuantizerMode::kDown:
      return "down";
  }
  return "nearest";
}

std::vector<QuantizerSample> GenerateQuantizerSamples(
    const std::vector<float>& offsets,
    std::uint8_t root,
    ScaleQuantizer::Scale scale,
    QuantizerMode mode,
    double driftHz,
    float driftDepth,
    std::size_t frameCount) {
  if (offsets.empty()) {
    return {};
  }
  if (frameCount == 0) {
    frameCount = 1;
  }

  const bool applyDrift = driftHz > 0.0 && driftDepth != 0.0f && frameCount > 1;
  const double cycleDuration = applyDrift ? (1.0 / driftHz) : 0.0;
  const double stepCount = static_cast<double>(std::max<std::size_t>(frameCount - 1, 1));

  std::vector<QuantizerSample> samples;
  samples.reserve(offsets.size() * frameCount);

  for (std::size_t frame = 0; frame < frameCount; ++frame) {
    const double timelinePosition = applyDrift ? (frame / stepCount) : 0.0;
    const double timeSeconds = applyDrift ? (timelinePosition * cycleDuration) : 0.0;
    const double phase = applyDrift ? (2.0 * 3.14159265358979323846 * driftHz * timeSeconds) : 0.0;
    const float driftValue = applyDrift ? static_cast<float>(std::sin(phase) * driftDepth) : 0.0f;

    for (std::size_t slot = 0; slot < offsets.size(); ++slot) {
      const float inputPitch = offsets[slot];
      const float driftedPitch = inputPitch + driftValue;
      const float snappedNearest = ScaleQuantizer::SnapToScale(driftedPitch, root, scale);
      const float snappedUp = ScaleQuantizer::SnapUp(driftedPitch, root, scale);
      const float snappedDown = ScaleQuantizer::SnapDown(driftedPitch, root, scale);

      QuantizerSample sample;
      sample.timeSeconds = timeSeconds;
      sample.slot = slot;
      sample.inputPitch = inputPitch;
      sample.driftedPitch = driftedPitch;
      sample.snappedNearest = snappedNearest;
      sample.snappedUp = snappedUp;
      sample.snappedDown = snappedDown;
      sample.activePitch = ActiveForMode(snappedNearest, snappedUp, snappedDown, mode);

      samples.push_back(sample);
    }
  }

  return samples;
}

std::string FormatQuantizerCsv(const std::vector<QuantizerSample>& samples,
                               QuantizerMode mode) {
  std::ostringstream oss;
  oss << "time_sec,slot,input_pitch,drifted_pitch,nearest,up,down,active,mode" << '\n';
  oss << std::fixed << std::setprecision(4);

  for (const QuantizerSample& sample : samples) {
    oss << sample.timeSeconds << ',';
    oss << sample.slot << ',';
    oss << std::setprecision(4) << sample.inputPitch << ',';
    oss << std::setprecision(4) << sample.driftedPitch << ',';
    oss << std::setprecision(4) << sample.snappedNearest << ',';
    oss << std::setprecision(4) << sample.snappedUp << ',';
    oss << std::setprecision(4) << sample.snappedDown << ',';
    oss << std::setprecision(4) << sample.activePitch << ',';
    oss << ToString(mode) << '\n';
  }

  return oss.str();
}

}  // namespace util
