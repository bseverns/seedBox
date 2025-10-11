#include <cstdio>
#include "app/AppState.h"
#include "interop/mn42_map.h"

int main() {
  AppState app;
  app.initSim();

  std::puts("SeedBox reseed ritual (Quiet Mode)");
  app.onExternalControlChange(0, seedbox::interop::mn42::kCcReseed, 64);
  app.reseed(app.masterSeed());

  for (const auto& seed : app.seeds()) {
    std::printf("seed %u -> prob %.2f jitter %.2f\n",
                seed.id,
                seed.probability,
                seed.jitterMs);
  }

  std::puts("TODO: capture MN42-driven WAV once fixtures exist.");
  return 0;
}
