#pragma once

#include <cstdint>

#include "app/Clock.h"

class PatternScheduler;

// ClockTransportController owns the state transitions for sync source
// selection, external transport dominance, and the latch/watchdog rules.
// AppState still decides when to call it, but the messy clock bookkeeping now
// lives behind a smaller, testable seam.
class ClockTransportController {
public:
  ClockTransportController(InternalClock& internalClock, MidiClockIn& midiClockIn, MidiClockOut& midiClockOut,
                           PatternScheduler& scheduler);

  ClockProvider* clock() const { return clock_; }
  bool externalClockDominant() const { return externalClockDominant_; }
  bool followExternalClockEnabled() const { return followExternalClockEnabled_; }
  bool transportLatchEnabled() const { return transportLatchEnabled_; }
  bool transportLatchedRunning() const { return transportLatchedRunning_; }
  bool externalTransportRunning() const { return externalTransportRunning_; }
  bool waitingForExternalClock() const { return waitingForExternalClock_; }

  void selectClockProvider(ClockProvider* provider);
  void toggleClockProvider();
  void applySwing(float swing);
  void resetTransportState();
  void clearTransportGateHold();

  void onExternalClockTick(uint32_t nowMs);
  void onExternalTransportStart();
  void onExternalTransportStop();

  void setTransportLatchEnabled(bool enabled);
  void refreshTransportLatchState();
  void toggleTransportLatchedRunning();

  void setFollowExternalClockEnabled(bool enabled, uint32_t nowMs);
  void setClockSourceExternal(bool external, uint32_t nowMs);
  void applyPresetClockState(bool followExternalClockEnabled, bool transportLatchEnabled,
                             bool externalTransportRunning);
  void updateClockDominance();
  void updateExternalClockWatchdog(uint32_t nowMs, uint32_t timeoutMs);
  void handleTransportGate(uint8_t value);

private:
  void alignProviderRunning();

  InternalClock& internalClock_;
  MidiClockIn& midiClockIn_;
  MidiClockOut& midiClockOut_;
  PatternScheduler& scheduler_;
  ClockProvider* clock_{nullptr};
  float swing_{0.0f};
  bool externalClockDominant_{false};
  bool followExternalClockEnabled_{false};
  bool transportLatchEnabled_{false};
  bool transportLatchedRunning_{false};
  bool externalTransportRunning_{false};
  bool transportGateHeld_{false};
  bool waitingForExternalClock_{false};
  uint32_t lastExternalClockMs_{0};
  uint32_t externalClockWaitStartMs_{0};
};
