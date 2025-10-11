#ifndef QUIET_MODE
#define QUIET_MODE 1
#endif

#include <cstdio>

#include "SeedboxConfig.h"
#include "app/AppState.h"

namespace {
void dumpFocus(const AppState& app, const char* tag) {
  const auto& seeds = app.seeds();
  std::printf("[%s] master=0x%08X focus=%u\n", tag, app.masterSeed(),
              app.focusSeed());
  for (const Seed& seed : seeds) {
    std::printf("  seed %u -> engine %u, tone %.2f, spread %.2f\n", seed.id,
                seed.engine, seed.tone, seed.spread);
  }
}
}

int main() {
  AppState app;
  app.initSim();
  dumpFocus(app, "boot");

  app.reseed(0x1234ABCDu);
  dumpFocus(app, "after reseed");

  std::printf("TODO: listen here (reseed.wav).\n");
  return 0;
}
