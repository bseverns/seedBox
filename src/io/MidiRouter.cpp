#include "io/MidiRouter.h"

#ifdef SEEDBOX_HW
#  include <Arduino.h>
#  include <MIDI.h>
#  include <usb_midi.h>
#endif

void MidiRouter::begin() {
#ifdef SEEDBOX_HW
  // Nothing to initialize today. Teensy USB stack already spun up by the
  // framework before we enter here.
#endif
  clockHandler_ = {};
  startHandler_ = {};
  stopHandler_ = {};
  controlChangeHandler_ = {};
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

void MidiRouter::onControlChange(uint8_t ch, uint8_t cc, uint8_t val) {
  if (controlChangeHandler_) {
    controlChangeHandler_(ch, cc, val);
  }
}

void MidiRouter::onClockTick() {
  if (clockHandler_) clockHandler_();
}

void MidiRouter::onStart() {
  if (startHandler_) startHandler_();
}

void MidiRouter::onStop() {
  if (stopHandler_) stopHandler_();
}
