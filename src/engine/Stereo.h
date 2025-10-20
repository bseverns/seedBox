#pragma once

#include <algorithm>
#include <cmath>

namespace stereo {

struct Gains {
  float left;
  float right;
};

// Map a 0..1 "spread" knob to a constant-power pan curve that starts centered
// (equal left/right) and blooms toward a hard pan as the knob approaches 1.0.
// The math intentionally skews toward the right channel because the current
// engines don't juggle a separate pan polarity yet — we only expose a "how
// wide" control.
inline Gains constantPowerWidth(float spread) {
  const float clamped = std::max(0.0f, std::min(1.0f, spread));
  constexpr float kHalfPi = 1.57079637f;
  constexpr float kCenterAngle = kHalfPi * 0.5f;  // pi/4 keeps equal power.
  const float angle = kCenterAngle + (kHalfPi - kCenterAngle) * clamped;
  return Gains{std::cos(angle), std::sin(angle)};
}

}  // namespace stereo
