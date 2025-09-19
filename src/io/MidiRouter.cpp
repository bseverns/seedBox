#include "io/MidiRouter.h"

#ifdef SEEDBOX_HW
  #include <usb_midi.h>
#endif

void MidiRouter::begin() {
#ifdef SEEDBOX_HW
  // Nothing to initialize today. Teensy USB stack already spun up by the
  // framework before we enter here.
#endif
}

void MidiRouter::onUsbEvent() {
#ifdef SEEDBOX_HW
  // usbMIDI.read() already pulled a fresh packet. Route it based on type so the
  // rest of the app only sees semantic callbacks instead of raw status bytes.
  if (usbMIDI.getType() == midi::Clock) onClockTick();
  else if (usbMIDI.getType() == midi::Start) onStart();
  else if (usbMIDI.getType() == midi::Stop) onStop();
  else if (usbMIDI.getType() == midi::ControlChange) {
    onControlChange(usbMIDI.getChannel(), usbMIDI.getData1(), usbMIDI.getData2());
  }
#endif
}

void MidiRouter::onControlChange(uint8_t, uint8_t, uint8_t) {
  // TODO: map CCs to the parameter router once the macro table lands.
}

void MidiRouter::onClockTick() { /* TODO: scheduler tick */ }
void MidiRouter::onStart()     { /* TODO: scheduler start */ }
void MidiRouter::onStop()      { /* TODO: scheduler stop  */ }
