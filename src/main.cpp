#include "BuildInfo.h"
#include "app/AppState.h"
#include "hal/hal_audio.h"
#include "hal/hal_io.h"

#ifdef SEEDBOX_HW
  #include "io/MidiRouter.h"
  #include "engine/Sampler.h"
#endif

AppState app;

void setup() {
#ifdef SEEDBOX_HW
  seedbox::hal::init_audio({64, 0.6f, 48000.0f, 128});
  seedbox::hal::init_io();
  app.initHardware();
#else
  seedbox::hal::init_audio();
  seedbox::hal::init_io();
  app.initSim();
#endif
}

void loop() {
#ifdef SEEDBOX_HW
  while (usbMIDI.read()) { app.midi.onUsbEvent(); }
#endif
  app.tick();
}
