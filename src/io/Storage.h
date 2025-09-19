#pragma once
#include <vector>
#include "Seed.h"

namespace Storage {
  bool loadSeedBank(const char* path, std::vector<Seed>& out);
  bool saveScene(const char* path);
}
