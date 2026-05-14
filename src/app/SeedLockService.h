#pragma once

#include <cstdint>

#include "app/AppState.h"

class SeedLockService {
public:
  void toggleSeedLock(AppState& app, std::uint8_t index) const;
  void toggleGlobalLock(AppState& app) const;
  bool isSeedLocked(const AppState& app, std::uint8_t index) const;
};

