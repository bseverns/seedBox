#ifndef QUIET_MODE
#define QUIET_MODE 1
#endif

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>

#include "SeedboxConfig.h"
#include "app/AppState.h"

int main() {
  AppState app;
  app.initSim();

  constexpr std::uint32_t kTicks = 240;  // 2 measures at 120 BPM (approx)
  for (std::uint32_t i = 0; i < kTicks; ++i) {
    app.tick();
    if (i % 60 == 0) {
      std::printf("Tick %u (scheduler=%llu)\n", i,
                  static_cast<unsigned long long>(app.schedulerTicks()));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  AppState::DisplaySnapshot snapshot{};
  app.captureDisplaySnapshot(snapshot);
  std::printf("Display title: %s | status: %s\n", snapshot.title, snapshot.status);
  std::printf("TODO: listen here (headless.wav).\n");
  return 0;
}
