#include "app/TapTempoTracker.h"

#include <algorithm>
#include <limits>

std::optional<std::uint32_t> TapTempoTracker::noteTap(std::uint64_t timestampUs) {
  std::optional<std::uint32_t> intervalMs;
  if (lastTapUs_ != 0 && timestampUs > lastTapUs_) {
    const std::uint64_t deltaUs = timestampUs - lastTapUs_;
    const std::uint64_t rawIntervalMs = deltaUs / 1000ULL;
    if (rawIntervalMs > 0) {
      const std::uint64_t clamped =
          std::min<std::uint64_t>(rawIntervalMs, static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()));
      intervalMs = static_cast<std::uint32_t>(clamped);
      recordInterval(*intervalMs);
    }
  }

  lastTapUs_ = timestampUs;
  return intervalMs;
}

void TapTempoTracker::resetPendingTap() { lastTapUs_ = 0; }

void TapTempoTracker::recordInterval(std::uint32_t intervalMs) {
  if (intervalMs == 0) {
    return;
  }
  history_.push_back(intervalMs);
  if (history_.size() > kMaxHistory) {
    const std::size_t drop = history_.size() - kMaxHistory;
    history_.erase(history_.begin(), history_.begin() + static_cast<std::ptrdiff_t>(drop));
  }
}

float TapTempoTracker::currentBpm() const {
  if (history_.empty()) {
    return 120.f;
  }

  double total = 0.0;
  std::size_t count = 0;
  for (std::uint32_t interval : history_) {
    if (interval == 0) {
      continue;
    }
    total += static_cast<double>(interval);
    ++count;
  }
  if (count == 0) {
    return 120.f;
  }

  const double averageMs = total / static_cast<double>(count);
  if (averageMs <= 0.0) {
    return 120.f;
  }
  return static_cast<float>(60000.0 / averageMs);
}

std::uint32_t TapTempoTracker::lastIntervalMs() const {
  return history_.empty() ? 0u : history_.back();
}
