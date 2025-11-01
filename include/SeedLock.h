#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

//
// SeedLock
// --------
// A tiny manager that keeps track of which seeds are protected from mutation.
// The Seed page in the UI can flip individual locks (per-seed) or slam the
// emergency brake (global lock) and this helper makes it easy for AppState to
// respect those wishes without peppering the code with bespoke bookkeeping.
// The implementation is intentionally boring: std::vector for per-seed flags,
// a single boolean for the global guard, and a couple of helpers to grow/shrink
// the roster as seeds come and go.
class SeedLock {
public:
  void clear();
  void resize(std::size_t count);
  void trim(std::size_t count);

  void setSeedLocked(std::size_t index, bool locked);
  void toggleSeedLock(std::size_t index);
  bool seedLocked(std::size_t index) const;

  void setGlobalLock(bool locked);
  void toggleGlobalLock();
  bool globalLocked() const { return globalLock_; }

private:
  void ensureSize(std::size_t count);
  bool rawLock(std::size_t index) const;

  std::vector<uint8_t> perSeed_{};
  bool globalLock_{false};
};

