#include "BuildInfo.h"
#include "app/AppState.h"

#ifdef SEEDBOX_HW
  #include "HardwarePrelude.h"
  #include "io/MidiRouter.h"
  #include "engine/Sampler.h"
  AudioControlSGTL5000 sgtl5000;
#endif

AppState app;

void setup() {
#ifdef SEEDBOX_HW
  AudioMemory(64);
  sgtl5000.enable();
  sgtl5000.volume(0.6f);
  app.initHardware();
#else
  app.initSim();
#endif
}

void loop() {
#ifdef SEEDBOX_HW
  while (usbMIDI.read()) { app.midi.onUsbEvent(); }
  app.midi.poll();
#endif
  app.tick();
}
