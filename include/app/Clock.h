#pragma once

//
// Clock provider abstractions.
// ----------------------------
// Sequencer code toggles between internal and external transports depending on
// who currently owns the groove.  This header introduces a tiny interface that
// captures those differences without forcing the rest of the app to speak MIDI
// or wrangle long-press logic.  Treat it like a control room patch bay: pick a
// provider, wire it into PatternScheduler, and the clock math stays consistent
// regardless of where the pulses originate.
#include <cstdint>
#include <cstddef>
#include "util/Annotations.h"

class PatternScheduler;

// ClockProvider is the common language shared by internal (self clocking) and
// external (MIDI) transports.  The base class mostly stores the parameters we
// care about — tempo, swing, and jitter — and forwards tick pulses into the
// bound PatternScheduler when subclasses decide it's time.
class ClockProvider {
public:
  virtual ~ClockProvider() = default;

  void attachScheduler(PatternScheduler* scheduler) { scheduler_ = scheduler; }

  PatternScheduler* scheduler() const { return scheduler_; }

  virtual void startTransport() { running_ = true; }
  virtual void continueTransport() { running_ = true; }
  virtual void stopTransport() { running_ = false; }

  virtual void onTick();

  virtual void setBpm(float bpm) { bpm_ = bpm; }
  float bpm() const { return bpm_; }

  virtual void setSwing(float swing) { swing_ = swing; }
  float swing() const { return swing_; }

  virtual void setMicroJitterMs(float jitterMs) { jitterMs_ = jitterMs; }
  float microJitterMs() const { return jitterMs_; }

  virtual double swingNudgeSamples(uint64_t tickCount, double baseSamplesPerTick) const;

protected:
  void dispatchTick();
  bool running_{true};
  PatternScheduler* scheduler_{nullptr};
  float bpm_{120.f};
  float swing_{0.f};
  float jitterMs_{0.f};
};

// InternalClock is the "just run" transport used by the simulator and the
// hardware when no external MIDI clock is connected.  Swing lives here because
// it's part of the feel the internal engine needs to advertise to the rest of
// the firmware.
class InternalClock : public ClockProvider {
public:
  void onTick() override;
  double swingNudgeSamples(uint64_t tickCount, double baseSamplesPerTick) const override;
};

// MidiClockIn is the follower.  It only dispatches ticks while the transport is
// running and ignores swing because the upstream source already baked in any
// groove.  We still honour start/stop/continue so PatternScheduler sees the
// same lifecycle regardless of who is in charge.
class MidiClockIn : public ClockProvider {
public:
  void onTick() override;
};

// MidiClockOut keeps the door open for broadcasting transport changes to
// controllers.  For now it mirrors the internal clock behaviour so tests can
// pretend there's an outbound clock without needing real MIDI plumbing.
class MidiClockOut : public ClockProvider {
public:
  void onTick() override;
};

