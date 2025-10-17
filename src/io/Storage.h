#pragma once
#include <vector>
#include "Seed.h"

namespace Storage {

  // Load a seed bank JSON file from disk/SD and hydrate the provided vector.
  // For now the hardware stub just records intent; the simulator returns false
  // so lesson plans do not silently depend on missing media.
  bool loadSeedBank(const char* path, std::vector<Seed>& out);

  // Persist the current scene back to storage.  Again, deliberately stubbed so
  // we can flesh out the data model with students watching.
  bool saveScene(const char* path);

}
