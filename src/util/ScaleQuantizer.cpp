#include "util/ScaleQuantizer.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace util {
namespace {
constexpr float kOctave = 12.0f;

std::vector<int> MakeChromatic() {
  std::vector<int> degrees(12);
  for (int i = 0; i < 12; ++i) {
    degrees[i] = i;
  }
  return degrees;
}

const std::vector<int>& ChromaticDegrees() {
  static const std::vector<int> kChromatic = MakeChromatic();
  return kChromatic;
}

const std::vector<int>& MajorDegrees() {
  static const std::vector<int> kMajor{0, 2, 4, 5, 7, 9, 11};
  return kMajor;
}

const std::vector<int>& MinorDegrees() {
  static const std::vector<int> kMinor{0, 2, 3, 5, 7, 8, 10};
  return kMinor;
}

const std::vector<int>& PentatonicMajorDegrees() {
  static const std::vector<int> kPent{0, 2, 4, 7, 9};
  return kPent;
}

const std::vector<int>& PentatonicMinorDegrees() {
  static const std::vector<int> kPent{0, 3, 5, 7, 10};
  return kPent;
}

int ClampRoot(int root) {
  const int normalized = root % 12;
  return normalized < 0 ? normalized + 12 : normalized;
}

float CandidatePitch(int octave, int root, int degree) {
  return static_cast<float>(octave * 12 + root + degree);
}

}  // namespace

const std::vector<int>& ScaleQuantizer::DegreesFor(Scale scale) {
  switch (scale) {
    case Scale::kChromatic:
      return ChromaticDegrees();
    case Scale::kMajor:
      return MajorDegrees();
    case Scale::kMinor:
      return MinorDegrees();
    case Scale::kPentatonicMajor:
      return PentatonicMajorDegrees();
    case Scale::kPentatonicMinor:
      return PentatonicMinorDegrees();
  }
  return ChromaticDegrees();
}

float ScaleQuantizer::Snap(float semitoneOffset, std::uint8_t root, Scale scale, int direction) {
  const auto& degrees = DegreesFor(scale);
  if (degrees.empty()) {
    return semitoneOffset;
  }

  const int normalizedRoot = ClampRoot(root);
  const float target = semitoneOffset;
  const float relative = target - static_cast<float>(normalizedRoot);
  const int baseOctave = static_cast<int>(std::floor(relative / kOctave));

  float bestPitch = target;
  float bestDistance = std::numeric_limits<float>::infinity();

  for (int octave = baseOctave - 2; octave <= baseOctave + 2; ++octave) {
    for (int degree : degrees) {
      const float candidate = CandidatePitch(octave, normalizedRoot, degree);
      const float delta = candidate - target;
      const float absDelta = std::abs(delta);
      if (direction > 0 && delta < 0.0f) {
        continue;
      }
      if (direction < 0 && delta > 0.0f) {
        continue;
      }
      if (absDelta < bestDistance || (absDelta == bestDistance && candidate < bestPitch)) {
        bestPitch = candidate;
        bestDistance = absDelta;
      }
    }
  }

  if (!std::isfinite(bestDistance)) {
    return target;
  }
  return bestPitch;
}

float ScaleQuantizer::SnapToScale(float semitoneOffset, std::uint8_t root, Scale scale) {
  return Snap(semitoneOffset, root, scale, 0);
}

float ScaleQuantizer::SnapUp(float semitoneOffset, std::uint8_t root, Scale scale) {
  return Snap(semitoneOffset, root, scale, 1);
}

float ScaleQuantizer::SnapDown(float semitoneOffset, std::uint8_t root, Scale scale) {
  return Snap(semitoneOffset, root, scale, -1);
}

}  // namespace util

