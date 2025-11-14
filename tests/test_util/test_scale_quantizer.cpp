#include <cassert>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "util/ScaleQuantizer.h"
#include "util/ScaleQuantizerFlow.h"

namespace {

void expectNear(float actual, float expected, float epsilon = 1e-4f) {
  assert(std::fabs(actual - expected) <= epsilon);
}

void test_snap_to_scale_major() {
  using util::ScaleQuantizer;
  const float up = ScaleQuantizer::SnapToScale(1.1f, 0, ScaleQuantizer::Scale::kMajor);
  expectNear(up, 2.0f);

  const float down = ScaleQuantizer::SnapToScale(-3.2f, 0, ScaleQuantizer::Scale::kMajor);
  expectNear(down, -3.0f);

  const float locked = ScaleQuantizer::SnapToScale(7.0f, 0, ScaleQuantizer::Scale::kMajor);
  expectNear(locked, 7.0f);

  const float tie = ScaleQuantizer::SnapToScale(6.5f, 0, ScaleQuantizer::Scale::kMajor);
  expectNear(tie, 7.0f);
}

void test_snap_up_directional() {
  using util::ScaleQuantizer;
  const float resolved = ScaleQuantizer::SnapUp(-0.2f, 0, ScaleQuantizer::Scale::kMinor);
  expectNear(resolved, 0.0f);

  const float octaveCarry = ScaleQuantizer::SnapUp(10.1f, 10, ScaleQuantizer::Scale::kPentatonicMajor);
  expectNear(octaveCarry, 12.0f);

  const float onScale = ScaleQuantizer::SnapUp(8.0f, 0, ScaleQuantizer::Scale::kMinor);
  expectNear(onScale, 8.0f);
}

void test_snap_down_directional() {
  using util::ScaleQuantizer;
  const float resolved = ScaleQuantizer::SnapDown(3.9f, 0, ScaleQuantizer::Scale::kMinor);
  expectNear(resolved, 3.0f);

  const float octaveCarry = ScaleQuantizer::SnapDown(-10.2f, 0, ScaleQuantizer::Scale::kPentatonicMinor);
  expectNear(octaveCarry, -12.0f);

  const float onScale = ScaleQuantizer::SnapDown(0.0f, 0, ScaleQuantizer::Scale::kPentatonicMajor);
  expectNear(onScale, 0.0f);
}

void test_root_wraps() {
  using util::ScaleQuantizer;
  const float wrapped = ScaleQuantizer::SnapToScale(13.4f, 25, ScaleQuantizer::Scale::kMajor);
  expectNear(wrapped, 13.0f);

  const float largeRoot = ScaleQuantizer::SnapToScale(-8.6f, static_cast<std::uint8_t>(250),
                                                     ScaleQuantizer::Scale::kMinor);
  expectNear(largeRoot, -9.0f);
}

void test_drift_samples_and_csv() {
  const std::vector<float> offsets{0.0f, 2.5f};
  const double driftHz = 0.25;
  const float driftDepth = 0.5f;
  const std::size_t frames = 4;

  const auto samples = util::GenerateQuantizerSamples(
      offsets, 0, util::ScaleQuantizer::Scale::kMajor, util::QuantizerMode::kNearest, driftHz,
      driftDepth, frames);

  assert(samples.size() == offsets.size() * frames);

  const util::QuantizerSample& first = samples.front();
  expectNear(static_cast<float>(first.timeSeconds), 0.0f);
  expectNear(first.driftedPitch, offsets[0]);

  const util::QuantizerSample& secondFrame = samples[2];  // frame 1, slot 0
  expectNear(static_cast<float>(secondFrame.timeSeconds), 4.0f / 3.0f, 1e-4f);
  expectNear(secondFrame.driftedPitch, 0.4330127f, 1e-5f);
  expectNear(secondFrame.activePitch, secondFrame.snappedNearest);

  const util::QuantizerSample& thirdFrame = samples[4];  // frame 2, slot 0
  expectNear(static_cast<float>(thirdFrame.timeSeconds), 8.0f / 3.0f, 1e-4f);
  expectNear(thirdFrame.driftedPitch, -0.4330127f, 1e-5f);

  const std::string csv = util::FormatQuantizerCsv(samples, util::QuantizerMode::kNearest);
  std::istringstream stream(csv);
  std::string header;
  std::getline(stream, header);
  assert(header == "time_sec,slot,input_pitch,drifted_pitch,nearest,up,down,active,mode");

  std::string firstRow;
  std::getline(stream, firstRow);
  assert(firstRow == "0.0000,0,0.0000,0.0000,0.0000,0.0000,0.0000,0.0000,nearest");
}

}  // namespace

void test_scale_quantizer_snap_to_scale_major() {
  test_snap_to_scale_major();
}

void test_scale_quantizer_snap_up_directional() {
  test_snap_up_directional();
}

void test_scale_quantizer_snap_down_directional() {
  test_snap_down_directional();
}

void test_scale_quantizer_root_wraps() {
  test_root_wraps();
}

void test_scale_quantizer_drift_samples_and_csv() {
  test_drift_samples_and_csv();
}
