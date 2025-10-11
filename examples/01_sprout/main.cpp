#include <cstdio>
#include "app/AppState.h"

int main() {
  AppState app;
  app.initSim();

  std::puts("SeedBox sprout demo (Quiet Mode)");
  for (const auto& seed : app.seeds()) {
    std::printf("seed %u -> engine %u density %.2f\n",
                seed.id,
                seed.engine,
                seed.density);
  }

  std::puts("TODO: render 1s WAV preview once fixtures exist.");
  return 0;
}
