#pragma once
#include <stdint.h>

// MidiRouter is the only piece that talks directly to usbMIDI. It translates
// raw wire protocol (clock, transport, CC) into high-level callbacks the rest
// of the app can digest. No buffering, no ghost features — just exactly what
// the firmware handles today.
class MidiRouter {
public:
  // Call once during boot. Hardware builds could use this to prime DIN or USB
  // state; the native target keeps it cheap so tests start instantly.
  void begin();

  // Pump this from the main loop while usbMIDI.read() returns events. It will
  // dispatch clock + transport + CC into the matching handlers without storing
  // anything.
  void onUsbEvent();

  // Stub for mapping CCs to parameters. The signature already exposes channel,
  // controller number, and value so higher layers can build a learn table.
  void onControlChange(uint8_t ch, uint8_t cc, uint8_t val);

  // Clock and transport hooks that the scheduler / transport layer wire into.
  void onClockTick();
  void onStart();
  void onStop();
};
