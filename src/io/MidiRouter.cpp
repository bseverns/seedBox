// MidiRouter.cpp is where we translate low-level MIDI plumbing into the tidy
// callbacks that AppState consumes.  The verbosity is intentional — USB, TRS,
// and MN42 quirks all get spelled out so debugging a classroom rig doesn't
// require diving into Teensy core code.
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
    // MN42 handshake CCs get intercepted here so we can maintain the keep-alive
    // dance before passing the message on to AppState.
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
  // Keep the MN42 heartbeat alive and slurp any UART bytes waiting on Serial7.
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
    // Don't leave the controller hanging — once we finish booting we answer the
    // handshake immediately so its LEDs light up in sync.
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
  const bool isMn42Handshake =
      (ch == seedbox::interop::mn42::kDefaultChannel &&
       cc == seedbox::interop::mn42::cc::kHandshake);

  if (isMn42Handshake && val == seedbox::interop::mn42::handshake::kHello) {
    // A fresh HELLO means the controller wants to restart the handshake.
    // Drop the ACK flag so the next branch re-sends a greeting instead of
    // assuming the previous session is still alive.
    mn42AckSent_ = false;
  }

#ifdef SEEDBOX_HW
  if constexpr (SeedBoxConfig::kQuietMode) {
    return;
  }
  using namespace seedbox::interop::mn42;
  if (!isMn42Handshake) {
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
#endif
  if (value == seedbox::interop::mn42::handshake::kAck ||
      value == seedbox::interop::mn42::handshake::kKeepAlive) {
    mn42AckSent_ = true;
    mn42LastKeepAliveMs_ = nowMs();
  }
#ifndef SEEDBOX_HW
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
    // Send a friendly "yup, still here" every few seconds so the MN42 display
    // never assumes we've crashed mid-gig.
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
