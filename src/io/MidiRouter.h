#pragma once

//
// USB + TRS MIDI concierge.
// -------------------------
// This header advertises the friendly, callback-driven surface that the rest of
// the firmware uses.  It deliberately reads like documentation so wiring up new
// controllers or debugging a rogue clock feels approachable.
#include <stdint.h>
#include <functional>
#include <utility>
#include "util/Annotations.h"

// MidiRouter is the only piece that talks directly to usbMIDI. It translates
// raw wire protocol (clock, transport, CC) into high-level callbacks the rest
// of the app can digest. No buffering, no ghost features — just exactly what
// the firmware handles today.
class MidiRouter {
public:
  // Call once during boot. Hardware builds could use this to prime TRS or USB
  // state; the native target keeps it cheap so tests start instantly.
  void begin();

  using ClockHandler = std::function<void()>;
  using TransportHandler = std::function<void()>;
  using ControlChangeHandler = std::function<void(uint8_t, uint8_t, uint8_t)>;

  void setClockHandler(ClockHandler cb) { clockHandler_ = std::move(cb); }
  void setStartHandler(TransportHandler cb) { startHandler_ = std::move(cb); }
  void setStopHandler(TransportHandler cb) { stopHandler_ = std::move(cb); }
  void setControlChangeHandler(ControlChangeHandler cb) {
    controlChangeHandler_ = std::move(cb);
  }

  // Pump this from the main loop while usbMIDI.read() returns events. It will
  // dispatch clock + transport + CC into the matching handlers without storing
  // anything.
  SEEDBOX_MAYBE_UNUSED void onUsbEvent();

  // Poll any non-USB links (currently the TRS Type-A Serial7 port) so they can
  // share the same callback surface as usbMIDI. Hardware builds call this once
  // per loop so a steady stream of UART bytes doesn't depend on USB traffic to
  // stay alive.
  void poll();

  // Stub for mapping CCs to parameters. The signature already exposes channel,
  // controller number, and value so higher layers can build a learn table.
  void onControlChange(uint8_t ch, uint8_t cc, uint8_t val);

  // Mark that the application stack finished booting. Once this flag flips we
  // reply to any MN42 hello traffic and keep the controller convinced we're
  // alive by streaming periodic handshake pulses.
  SEEDBOX_MAYBE_UNUSED void markAppReady();

  // Clock and transport hooks that the scheduler / transport layer wire into.
  void onClockTick();
  void onStart();
  void onStop();

private:
  void handleMn42ControlChange(uint8_t ch, uint8_t cc, uint8_t val);
  void sendMn42Handshake(uint8_t value);
  void maybeSendMn42KeepAlive();
  uint32_t nowMs() const;

  ClockHandler clockHandler_{};
  TransportHandler startHandler_{};
  TransportHandler stopHandler_{};
  ControlChangeHandler controlChangeHandler_{};

  bool mn42HelloSeen_{false};
  bool mn42AppReady_{false};
  bool mn42AckSent_{false};
  uint32_t mn42LastKeepAliveMs_{0};
};
