// Storage.cpp stays intentionally skeletal right now — the SD card story is a
// future lab.  Still, we document the intended control flow so the TODOs read
// like a roadmap rather than a shrug.
#include "io/Storage.h"
#include "SeedBoxConfig.h"

#if SEEDBOX_HW
  #include <Arduino.h>
  #include <SdFat.h>
  #include <ArduinoJson.h>
  static SdFat SD;
#endif

bool Storage::loadSeedBank(const char* path, std::vector<Seed>& out) {
  if constexpr (SeedBoxConfig::kQuietMode) {
    (void)path;
    out.clear();
    return false;
  }
#if SEEDBOX_HW
  // Eventually this block will crack open `path`, parse JSON, and pour seed
  // genomes into `out`.  For now we acknowledge the call and return true so the
  // UI flow can be rehearsed without a card inserted.
  (void)path; (void)out;
  return true;
#else
  // Simulator builds lean on baked-in seed tables, so loading is a no-op.  We
  // still return true to keep call sites simple.
  (void)path; (void)out;
  return true;
#endif
}

bool Storage::saveScene(const char* path) {
  if constexpr (SeedBoxConfig::kQuietMode) {
    (void)path;
    return false;
  }
#if SEEDBOX_HW
  (void)path;
  // Placeholder for future JSON writer that mirrors loadSeedBank.
  return true;
#else
  (void)path;
  return true;
#endif
}
