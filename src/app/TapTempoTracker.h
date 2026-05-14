#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

class TapTempoTracker {
public:
  std::optional<std::uint32_t> noteTap(std::uint64_t timestampUs);
  void resetPendingTap();
  void recordInterval(std::uint32_t intervalMs);

  float currentBpm() const;
  std::uint32_t lastIntervalMs() const;

private:
  static constexpr std::size_t kMaxHistory = 8;

  std::vector<std::uint32_t> history_{};
  std::uint64_t lastTapUs_{0};
};
