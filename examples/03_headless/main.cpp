#include <cstdio>
#include "app/AppState.h"
#include "hal/hal_audio.h"
#include "hal/hal_io.h"

int main() {
  seedbox::hal::init_audio();
  seedbox::hal::init_io();

  AppState app;
  app.initSim();

  for (int i = 0; i < 16; ++i) {
    app.tick();
  }

  const auto metrics = seedbox::hal::audio_metrics();
  std::printf("buffers dispatched: %llu\n",
              static_cast<unsigned long long>(metrics.buffers_dispatched));
  std::puts("TODO: emit headless render once audio fixtures exist.");
  return 0;
}
