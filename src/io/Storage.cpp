#include "io/Storage.h"
#include "SeedBoxConfig.h"

#ifdef SEEDBOX_HW
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
#ifdef SEEDBOX_HW
  // TODO: parse JSON from SD
  (void)path; (void)out;
  return true;
#else
  (void)path; (void)out;
  return true;
#endif
}

bool Storage::saveScene(const char* path) {
  if constexpr (SeedBoxConfig::kQuietMode) {
    (void)path;
    return false;
  }
#ifdef SEEDBOX_HW
  (void)path;
  // TODO: write scene JSON
  return true;
#else
  (void)path;
  return true;
#endif
}
