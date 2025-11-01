#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace util {

//
// ScaleQuantizer
// --------------
// Helpers that snap arbitrary semitone offsets back onto musical scales. These
// live in util/ so both the UI layer and the engines can reach for the same
// math when a performer slaps the Quantize control. We expose three variants:
// nearest-note snapping, force-up, and force-down. All of them work with the
// same tiny Scale enum so classroom demos can focus on ear training instead of
// bit masks.
class ScaleQuantizer {
public:
  enum class Scale : std::uint8_t {
    kChromatic = 0,
    kMajor,
    kMinor,
    kPentatonicMajor,
    kPentatonicMinor,
  };

  static float SnapToScale(float semitoneOffset, std::uint8_t root, Scale scale);
  static float SnapUp(float semitoneOffset, std::uint8_t root, Scale scale);
  static float SnapDown(float semitoneOffset, std::uint8_t root, Scale scale);

private:
  static float Snap(float semitoneOffset, std::uint8_t root, Scale scale, int direction);
  static const std::vector<int>& DegreesFor(Scale scale);
};

}  // namespace util

