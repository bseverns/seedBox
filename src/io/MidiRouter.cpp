#include "io/MidiRouter.h"
#include "SeedBoxConfig.h"

#include "interop/mn42_map.h"

#ifdef SEEDBOX_HW
  #include <Arduino.h>
  #include <MIDI.h>
  #include <usb_midi.h>
  #include "hal/hal_midi_serial7.h"
#endif

namespace {
#ifdef SEEDBOX_HW
constexpr uint32_t kMn42KeepAliveMs = 2000;
#endif
}

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
  mn42HelloSeen_ = false;
  mn42AckSent_ = false;
  mn42LastKeepAliveMs_ = 0;
}

void MidiRouter::onUsbEvent() {
#ifdef SEEDBOX_HW
  if constexpr (SeedBoxConfig::kQuietMode) {
    return;
  }
  // usbMIDI.read() already pulled a fresh packet. Route it based on type so the
  // rest of the app only sees semantic callbacks instead of raw status bytes.
  const auto type = usbMIDI.getType();
  if (type == midi::Clock) onClockTick();
  else if (type == midi::Start) onStart();
  else if (type == midi::Stop) onStop();
  else if (type == midi::ControlChange) {
    const uint8_t ch = usbMIDI.getChannel();
    const uint8_t cc = usbMIDI.getData1();
    const uint8_t val = usbMIDI.getData2();
    if (ch == seedbox::interop::mn42::kDefaultChannel + 1 &&
        cc == seedbox::interop::mn42::cc::kHandshake) {
      if (val == seedbox::interop::mn42::handshake::kHello) {
        mn42HelloSeen_ = true;
        mn42AckSent_ = false;
      }
    }
    onControlChange(ch, cc, val);
    maybeSendMn42Ack();
  }

  if (mn42AckSent_ && controlChangeHandler_) {
    const uint32_t now = millis();
    if (now - mn42LastKeepAliveMs_ >= kMn42KeepAliveMs) {
      usbMIDI.sendControlChange(seedbox::interop::mn42::cc::kHandshake,
                                seedbox::interop::mn42::handshake::kKeepAlive,
                                seedbox::interop::mn42::kDefaultChannel + 1);
      mn42LastKeepAliveMs_ = now;
    }
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

void MidiRouter::maybeSendMn42Ack() {
#ifdef SEEDBOX_HW
  if constexpr (SeedBoxConfig::kQuietMode) {
    return;
  }
  if (!mn42HelloSeen_ || mn42AckSent_ || !controlChangeHandler_) {
    return;
  }
  usbMIDI.sendControlChange(seedbox::interop::mn42::cc::kHandshake,
                            seedbox::interop::mn42::handshake::kAck,
                            seedbox::interop::mn42::kDefaultChannel + 1);
  mn42AckSent_ = true;
  mn42LastKeepAliveMs_ = millis();
#endif
}
