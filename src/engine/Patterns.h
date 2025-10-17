#pragma once

//
// PatternScheduler
// -----------------
// Sequencer logic stripped of UI noise.  The public API mirrors what the rest
// of the firmware needs: set a tempo, feed it seeds, and call `onTick()` once
// per MIDI clock (24 pulses per quarter note).  The dense commentary makes it a
// perfect whiteboard companion for explaining probability gates, density, and
// jitter to curious students.
#include <vector>
#include <cstddef>
#include <cstdint>
#include <cstddef>
#include "Seed.h"

class PatternScheduler {
public:
  // Define the global tempo.  We keep the unit in BPM because it lines up with
  // the incoming MIDI clock rate.
  void setBpm(float bpm);

  // Pulse this once per 24 PPQN tick.  Internally `onTick` checks density,
  // probability, jitter, and schedules engine triggers.
  void onTick();

  // Manage the scheduler's seed roster.  Seeds are stored by value so the
  // scheduler can mutate its own copies without racing callers.
  void addSeed(const Seed& s);
  bool updateSeed(std::size_t index, const Seed& s);

  // Register a callback that will fire whenever a seed earns a trigger.  The
  // callback receives the sample timestamp so downstream code can render in
  // lockstep with the audio device.
  void setTriggerCallback(void* ctx, void (*fn)(void*, const Seed&, uint32_t));
  uint64_t ticks() const { return tickCount_; }

  // Peek at the scheduler's internal copy of a seed for debugging / teaching.
  // Returns nullptr if you wander off the end of the list.
  const Seed* seedForDebug(std::size_t index) const;

private:
  // Implementation helpers documented in Patterns.cpp.  We keep declarations
  // clustered here so readers know what machinery hides behind `onTick`.
  bool densityGate(std::size_t seedIndex, float density);
  uint32_t nowSamples();
  uint32_t msToSamples(float ms);
private:
  std::vector<Seed> seeds_;
  std::vector<float> densityAccumulators_;
  uint64_t tickCount_{0};
  float bpm_{120.f};
  void* triggerCtx_{nullptr};
  void (*triggerFn_)(void*, const Seed&, uint32_t){nullptr};
};
