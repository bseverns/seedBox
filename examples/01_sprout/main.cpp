#ifndef QUIET_MODE
#define QUIET_MODE 1
#endif

#include <cstdio>

#include "SeedboxConfig.h"
#include "app/AppState.h"

int main() {
  AppState app;
  app.initSim();

  // Prime a few ticks so the scheduler wakes up.
  for (int i = 0; i < 8; ++i) {
    app.tick();
  }

  std::printf("SeedBox sprout demo (quiet=%d)\n", seedbox::quietModeEnabled());
  const auto& seeds = app.seeds();
  for (const Seed& seed : seeds) {
    std::printf("Seed %u -> engine %u, density %.2f, prob %.2f\n", seed.id,
                seed.engine, seed.density, seed.probability);
  }

  std::printf("TODO: listen here (sprout.wav).\n");
  return 0;
}
