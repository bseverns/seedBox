#include "BuildInfo.h"
#include "app/AppState.h"
#include "hal/hal_audio.h"
#include "hal/hal_io.h"

#ifdef SEEDBOX_HW
#  include "hal/ArduinoGlue.h"
#  include <MIDI.h>
#  ifdef __GNUC__
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#  include <usb_midi.h>
#  ifdef __GNUC__
#    pragma GCC diagnostic pop
#  endif
#  include "engine/Sampler.h"
#  include "io/MidiRouter.h"
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

#if defined(SEEDBOX_DESKTOP_ENTRY) && !defined(UNIT_TEST) && !defined(PLATFORMIO_UNIT_TEST)
int main() {
  setup();
  while (true) {
    loop();
  }
  return 0;
}
#endif
