#include "io/MidiRouter.h"
#include "SeedBoxConfig.h"

#include "interop/mn42_map.h"

#ifdef SEEDBOX_HW
  #include <MIDI.h>
  #include <usb_midi.h>
  #include "hal/hal_midi_serial7.h"
#endif

void MidiRouter::begin() {
#ifdef SEEDBOX_HW
  // Nothing to initialize on the USB side: the Teensy stack spins up before we
  // hit firmware code. The TRS mini jacks ride Serial7, so hand that port the
  // same callbacks we expose to the rest of the app.
  hal::midi::serial7::Handlers trsHandlers{};
  trsHandlers.clock = [](void* ctx) {
    static_cast<MidiRouter*>(ctx)->onClockTick();
  };
  trsHandlers.start = [](void* ctx) {
    static_cast<MidiRouter*>(ctx)->onStart();
  };
  trsHandlers.stop = [](void* ctx) {
    static_cast<MidiRouter*>(ctx)->onStop();
  };
  trsHandlers.control_change = [](uint8_t ch, uint8_t cc, uint8_t val, void* ctx) {
    static_cast<MidiRouter*>(ctx)->onControlChange(ch, cc, val);
  };
  hal::midi::serial7::begin(trsHandlers, this);
#endif
  clockHandler_ = {};
  startHandler_ = {};
  stopHandler_ = {};
  controlChangeHandler_ = {};
}

void MidiRouter::onUsbEvent() {
#ifdef SEEDBOX_HW
  if constexpr (SeedBoxConfig::kQuietMode) {
    return;
  }
  // usbMIDI.read() already pulled a fresh packet. Route it based on type so the
  // rest of the app only sees semantic callbacks instead of raw status bytes.
  if (usbMIDI.getType() == midi::Clock) onClockTick();
  else if (usbMIDI.getType() == midi::Start) onStart();
  else if (usbMIDI.getType() == midi::Stop) onStop();
  else if (usbMIDI.getType() == midi::ControlChange) {
    onControlChange(usbMIDI.getChannel(), usbMIDI.getData1(), usbMIDI.getData2());
  }
  poll();
#endif
}

void MidiRouter::poll() {
#ifdef SEEDBOX_HW
  if constexpr (SeedBoxConfig::kQuietMode) {
    return;
  }
  hal::midi::serial7::poll();
#endif
}

void MidiRouter::onControlChange(uint8_t ch, uint8_t cc, uint8_t val) {
  // No behavioral changes yet, but we deliberately touch the MN42 map so both
  // firmware and docs track the same handshake CCs.
  (void)seedbox::interop::mn42::cc::kHandshake;
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
