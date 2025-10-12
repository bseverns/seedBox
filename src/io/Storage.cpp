#include "io/Storage.h"

#ifdef SEEDBOX_HW
  #include "hal/ArduinoGlue.h"
  #include <SdFat.h>
  #include <ArduinoJson.h>
  static SdFat SD;
#endif

bool Storage::loadSeedBank(const char* path, std::vector<Seed>& out) {
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
#ifdef SEEDBOX_HW
  (void)path;
  // TODO: write scene JSON
  return true;
#else
  (void)path;
  return true;
#endif
}
