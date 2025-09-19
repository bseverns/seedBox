#include "io/MidiRouter.h"

#ifdef SEEDBOX_HW
  #include <usb_midi.h>
#endif

void MidiRouter::begin() {
#ifdef SEEDBOX_HW
  // nothing yet
#endif
}

void MidiRouter::onUsbEvent() {
#ifdef SEEDBOX_HW
  if (usbMIDI.getType() == midi::Clock) onClockTick();
  else if (usbMIDI.getType() == midi::Start) onStart();
  else if (usbMIDI.getType() == midi::Stop) onStop();
  else if (usbMIDI.getType() == midi::ControlChange) {
    onControlChange(usbMIDI.getChannel(), usbMIDI.getData1(), usbMIDI.getData2());
  }
#endif
}

void MidiRouter::onControlChange(uint8_t, uint8_t, uint8_t) {
  // TODO: map CC to params
}

void MidiRouter::onClockTick() { /* TODO: scheduler tick */ }
void MidiRouter::onStart()     { /* TODO: scheduler start */ }
void MidiRouter::onStop()      { /* TODO: scheduler stop  */ }
