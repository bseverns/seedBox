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
    dryLeft_.clear();
    dryRight_.clear();
    updateInputGateState(0.0f, 0.0f);
    return;
  }

  dryLeft_.assign(left, left + frames);
  if (right) {
    dryRight_.assign(right, right + frames);
  } else {
    dryRight_.assign(dryLeft_.begin(), dryLeft_.end());
  }

  refreshFromDryInput(frames);
}

void InputGateMonitor::refreshFromDryInput(std::size_t probeFrames) {
  if (dryLeft_.empty()) {
    updateInputGateState(0.0f, 0.0f);
    return;
  }

  const std::size_t frames = std::min(probeFrames, dryLeft_.size());
  if (frames == 0) {
    updateInputGateState(0.0f, 0.0f);
    return;
  }

  const float* dryLeft = dryLeft_.data();
  const float* dryRight = (!dryRight_.empty() && dryRight_.size() >= frames) ? dryRight_.data() : dryLeft;
  const EnergyProbe probe = measureEnergy(dryLeft, dryRight, frames);
  updateInputGateState(probe.rms, probe.peak);
}

const float* InputGateMonitor::dryRight(std::size_t frames) const {
  if (dryLeft_.empty()) {
    return nullptr;
  }
  return (!dryRight_.empty() && dryRight_.size() >= frames) ? dryRight_.data() : dryLeft_.data();
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
