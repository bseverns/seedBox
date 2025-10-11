#include "BuildInfo.h"
#include "app/AppState.h"
#include "hal/hal_audio.h"
#include "hal/hal_io.h"

AppState app;

void setup() {
  seedbox::hal::bootAudioBackend();
  seedbox::hal::bootIo(app);
#ifdef SEEDBOX_HW
  app.initHardware();
#else
  app.initSim();
#endif
}

void loop() {
  seedbox::hal::pollIo(app);
  app.tick();
}
