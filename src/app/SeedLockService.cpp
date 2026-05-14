#include "app/SeedLockService.h"

// Lock behavior is intentionally tiny and explicit: a lock means "do not let
// procedural reseed logic rewrite this slot", and the global lock means that
// rule applies to the whole table.

void SeedLockService::toggleSeedLock(AppState& app, std::uint8_t index) const {
  if (app.seeds_.empty()) {
    return;
  }
  const std::size_t idx = static_cast<std::size_t>(index) % app.seeds_.size();
  app.seedLock_.toggleSeedLock(idx);
  app.displayDirty_ = true;
}

void SeedLockService::toggleGlobalLock(AppState& app) const {
  app.seedLock_.toggleGlobalLock();
  app.displayDirty_ = true;
}

bool SeedLockService::isSeedLocked(const AppState& app, std::uint8_t index) const {
  if (app.seedLock_.globalLocked()) {
    return true;
  }
  if (app.seeds_.empty()) {
    return false;
  }
  const std::size_t idx = static_cast<std::size_t>(index) % app.seeds_.size();
  return app.seedLock_.seedLocked(idx);
}
