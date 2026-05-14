#include "app/InputGateMonitor.h"

#include <algorithm>
#include <cmath>

namespace {

struct EnergyProbe {
  float rms{0.0f};
  float peak{0.0f};
};

EnergyProbe measureEnergy(const float* left, const float* right, std::size_t frames) {
  EnergyProbe probe{};
  if (!left || frames == 0) {
    return probe;
  }

  double sumSquares = 0.0;
  float peak = 0.0f;
  const double samples = static_cast<double>(frames) * (right ? 2.0 : 1.0);
  for (std::size_t i = 0; i < frames; ++i) {
    const float l = left[i];
    peak = std::max(peak, std::fabs(l));
    sumSquares += static_cast<double>(l) * static_cast<double>(l);
    if (right) {
      const float r = right[i];
      peak = std::max(peak, std::fabs(r));
      sumSquares += static_cast<double>(r) * static_cast<double>(r);
    }
  }

  probe.peak = peak;
  if (samples > 0.0) {
    probe.rms = static_cast<float>(std::sqrt(sumSquares / samples));
  }
  return probe;
}

}  // namespace

void InputGateMonitor::setFloor(float floor) { floor_ = std::max(1e-6f, floor); }

void InputGateMonitor::setDryInput(const float* left, const float* right, std::size_t frames) {
  if (!left || frames == 0) {
    dryLeft_ = nullptr;
    dryRight_ = nullptr;
    dryFrames_ = 0;
    updateInputGateState(0.0f, 0.0f);
    return;
  }

  dryLeft_ = left;
  dryRight_ = right;
  dryFrames_ = frames;
  refreshFromDryInput(frames);
}

void InputGateMonitor::refreshFromDryInput(std::size_t probeFrames) {
  if (!hasDryInput()) {
    updateInputGateState(0.0f, 0.0f);
    return;
  }

  const std::size_t frames = std::min(probeFrames, dryFrames_);
  if (frames == 0) {
    updateInputGateState(0.0f, 0.0f);
    return;
  }

  const float* dryLeft = dryLeft_;
  const float* dryRight = dryRight_ ? dryRight_ : dryLeft;
  const EnergyProbe probe = measureEnergy(dryLeft, dryRight, frames);
  updateInputGateState(probe.rms, probe.peak);
}

const float* InputGateMonitor::dryRight(std::size_t frames) const {
  if (!hasDryInput() || frames == 0) {
    return nullptr;
  }
  return dryRight_ ? dryRight_ : dryLeft_;
}

bool InputGateMonitor::gateReady(std::uint64_t tick, std::uint32_t divisionTicks) const {
  if (!gateEdgePending_ || divisionTicks == 0u) {
    return false;
  }
  const std::uint64_t elapsed = (tick >= lastGateTick_) ? (tick - lastGateTick_) : 0u;
  return elapsed >= static_cast<std::uint64_t>(divisionTicks);
}

void InputGateMonitor::updateInputGateState(float rms, float peak) {
  level_ = rms;
  peak_ = peak;
  const bool hot = (rms >= floor_) || (peak >= floor_);
  if (hot && !hot_) {
    gateEdgePending_ = true;
  }
  hot_ = hot;
}
