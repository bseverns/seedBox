#include "hal/hal_io.h"

#include "SeedboxConfig.h"
#include "app/AppState.h"

#ifdef SEEDBOX_HW
  #include <usbMIDI.h>
#endif

namespace seedbox::hal {

void bootIo(AppState&) {
  // Intentionally empty. MIDI routers bootstrap themselves inside AppState so
  // tests can stub them without mocking global state.
}

void pollIo(AppState& app) {
#ifdef SEEDBOX_HW
  if (quietModeEnabled()) {
    return;
  }
  while (usbMIDI.read()) {
    app.midi.onUsbEvent();
  }
#else
  (void)app;
#endif
}

}  // namespace seedbox::hal
