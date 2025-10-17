#include "io/MidiRouter.h"
#include "SeedBoxConfig.h"

#include "interop/mn42_map.h"

#ifdef SEEDBOX_HW
  #include <MIDI.h>
  #include <usb_midi.h>
  #include "hal/hal_midi_serial7.h"
  #include <Arduino.h>
#endif

namespace {
constexpr uint32_t kMn42KeepAliveIntervalMs = 3000;
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
  mn42AppReady_ = false;
  mn42AckSent_ = false;
  mn42LastKeepAliveMs_ = nowMs();
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
    const uint8_t raw_channel = usbMIDI.getChannel();
    const uint8_t ch = seedbox::interop::mn42::NormalizeUsbChannel(raw_channel);
    const uint8_t cc = usbMIDI.getData1();
    const uint8_t val = usbMIDI.getData2();
    handleMn42ControlChange(ch, cc, val);
    onControlChange(ch, cc, val);
  }
  poll();
#endif
}

void MidiRouter::poll() {
#ifdef SEEDBOX_HW
  if constexpr (SeedBoxConfig::kQuietMode) {
    return;
  }
  maybeSendMn42KeepAlive();
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

void MidiRouter::markAppReady() {
  mn42AppReady_ = true;
#ifdef SEEDBOX_HW
  if constexpr (SeedBoxConfig::kQuietMode) {
    return;
  }
  mn42LastKeepAliveMs_ = nowMs();
  if (mn42HelloSeen_ && !mn42AckSent_) {
    sendMn42Handshake(seedbox::interop::mn42::handshake::kAck);
  }
#endif
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

void MidiRouter::handleMn42ControlChange(uint8_t ch, uint8_t cc, uint8_t val) {
  (void)ch;
  (void)cc;
  (void)val;
#ifdef SEEDBOX_HW
  if constexpr (SeedBoxConfig::kQuietMode) {
    return;
  }
  using namespace seedbox::interop::mn42;
  if (ch != kDefaultChannel) {
    return;
  }
  if (cc != cc::kHandshake) {
    return;
  }
  if (val == handshake::kHello) {
    mn42HelloSeen_ = true;
    mn42LastKeepAliveMs_ = nowMs();
    if (mn42AppReady_ && !mn42AckSent_) {
      sendMn42Handshake(handshake::kAck);
    }
  } else if (val == handshake::kKeepAlive) {
    mn42LastKeepAliveMs_ = nowMs();
  }
#endif
}

void MidiRouter::sendMn42Handshake(uint8_t value) {
#ifdef SEEDBOX_HW
  if constexpr (SeedBoxConfig::kQuietMode) {
    return;
  }
  usbMIDI.sendControlChange(seedbox::interop::mn42::cc::kHandshake, value,
                            seedbox::interop::mn42::kDefaultChannel + 1);
  if (value == seedbox::interop::mn42::handshake::kAck ||
      value == seedbox::interop::mn42::handshake::kKeepAlive) {
    mn42AckSent_ = true;
    mn42LastKeepAliveMs_ = nowMs();
  }
#else
  (void)value;
#endif
}

void MidiRouter::maybeSendMn42KeepAlive() {
#ifdef SEEDBOX_HW
  if constexpr (SeedBoxConfig::kQuietMode) {
    return;
  }
  if (!mn42AppReady_ || !mn42AckSent_ || !mn42HelloSeen_) {
    return;
  }
  const uint32_t now = nowMs();
  if (now - mn42LastKeepAliveMs_ >= kMn42KeepAliveIntervalMs) {
    sendMn42Handshake(seedbox::interop::mn42::handshake::kKeepAlive);
  }
#endif
}

uint32_t MidiRouter::nowMs() const {
#ifdef SEEDBOX_HW
  return millis();
#else
  return 0;
#endif
}
