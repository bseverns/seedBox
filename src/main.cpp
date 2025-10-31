//
// SeedBox boot script.
// ---------------------
// This is the lone translation unit that the Arduino/Teensy toolchain
// recognises as the program entry point.  It wires up the high-level
// application state container (`AppState`) to the platform specific runtime
// (`setup`/`loop`) that the Teensy microcontroller expects.  The goal is to keep
// this file tiny so students can immediately map the firmware life-cycle to the
// place where work actually gets done.
//
#include "BuildInfo.h"
#include "app/AppState.h"

#ifdef SEEDBOX_HW
  #include "HardwarePrelude.h"
  #include "io/MidiRouter.h"
  #include "engine/Sampler.h"
  AudioControlSGTL5000 sgtl5000;
#endif

// Global application brain.  The object is intentionally static so that we have
// one long-lived state machine that outlives the setup/loop churn.
AppState app;

void setup() {
#ifdef SEEDBOX_HW
  // On real hardware we have to wake the SGTL5000 audio codec before touching
  // any of the custom logic.  Think of this as switching on the studio mixer
  // before routing instruments.
  sgtl5000.enable();
  sgtl5000.volume(0.6f);
  // With the codec alive, hand the controls over to the board-specific init
  // path.  This configures GPIO, audio paths, etc.
  app.initHardware();
#else
  // When we are building for the simulator there is no codec to wake up, so we
  // jump straight into a software-only bootstrap.
  app.initSim();
#endif
}

void loop() {
#ifdef SEEDBOX_HW
  app.midi.poll();
#endif
  // Finally, tick the app.  This pumps the state machine, audio engine, and any
  // UI logic.  `loop` runs perpetually, so `tick` needs to be lean and
  // deterministic.
  app.tick();
}
