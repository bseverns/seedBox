#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class InputGateMonitor {
 public:
  void setFloor(float floor);
  float level() const { return level_; }
  float peak() const { return peak_; }
  bool hot() const { return hot_; }
  bool gateEdgePending() const { return gateEdgePending_; }

  void setDryInput(const float* left, const float* right, std::size_t frames);
  void refreshFromDryInput(std::size_t probeFrames);

  bool hasDryInput() const { return !dryLeft_.empty(); }
  std::size_t dryFrames() const { return dryLeft_.size(); }
  const float* dryLeft() const { return dryLeft_.empty() ? nullptr : dryLeft_.data(); }
  const float* dryRight(std::size_t frames) const;

  bool gateReady(std::uint64_t tick, std::uint32_t divisionTicks) const;
  void cancelPendingGateEdge() { gateEdgePending_ = false; }
  void syncGateTick(std::uint64_t tick) {
    lastGateTick_ = tick;
    gateEdgePending_ = false;
  }

 private:
  void updateInputGateState(float rms, float peak);

  float floor_{0.0f};
  float level_{0.0f};
  float peak_{0.0f};
  bool hot_{false};
  bool gateEdgePending_{false};
  std::uint64_t lastGateTick_{0};
  std::vector<float> dryLeft_{};
  std::vector<float> dryRight_{};
};
