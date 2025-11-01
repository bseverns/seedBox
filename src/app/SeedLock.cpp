#include "SeedLock.h"

#include <algorithm>

void SeedLock::clear() {
  std::fill(perSeed_.begin(), perSeed_.end(), 0);
  globalLock_ = false;
}

void SeedLock::ensureSize(std::size_t count) {
  if (perSeed_.size() < count) {
    perSeed_.resize(count, 0);
  }
}

void SeedLock::resize(std::size_t count) {
  ensureSize(count);
}

void SeedLock::trim(std::size_t count) {
  if (perSeed_.size() > count) {
    perSeed_.resize(count);
  }
}

bool SeedLock::rawLock(std::size_t index) const {
  if (index >= perSeed_.size()) {
    return false;
  }
  return perSeed_[index] != 0;
}

bool SeedLock::seedLocked(std::size_t index) const {
  if (globalLock_) {
    return true;
  }
  return rawLock(index);
}

void SeedLock::setSeedLocked(std::size_t index, bool locked) {
  ensureSize(index + 1);
  perSeed_[index] = locked ? 1 : 0;
}

void SeedLock::toggleSeedLock(std::size_t index) {
  const bool locked = rawLock(index);
  setSeedLocked(index, !locked);
}

void SeedLock::setGlobalLock(bool locked) { globalLock_ = locked; }

void SeedLock::toggleGlobalLock() { globalLock_ = !globalLock_; }

