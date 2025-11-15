#pragma once
#include <vector>
#include "Seed.h"

class AppState;

namespace Storage {

  // Crack open a preset snapshot from either the EEPROM store or the SD/host
  // filesystem and hydrate a seed vector. `path` accepts prefixes in the form
  // `"eeprom:<slot>"` or `"sd:<folder/file.json>"`; bare strings default to the
  // EEPROM contract so legacy callers keep working.  Loads stay available even in
  // quiet mode — classrooms can browse banks while writes remain locked down.
  bool loadSeedBank(const char* path, std::vector<Seed>& out);

  // Serialize the live scene to JSON and persist it via the same path scheme as
  // `loadSeedBank`.  Quiet mode still blocks hardware writes, mirroring the
  // guardrails baked into the `Store` backends, while native builds land files in
  // a deterministic shim directory so tests can assert on the resulting blobs.
  bool saveScene(const char* path);

  // Allow AppState instances to advertise themselves as the live runtime.  The
  // storage helpers only ever need one active pointer — whichever AppState is
  // currently shepherding the performance — so constructors register on entry
  // and destructors unregister when they leave scope.
  void registerApp(AppState& app);
  void unregisterApp(const AppState& app);

}
