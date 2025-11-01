#pragma once

//
// PatternScheduler
// -----------------
// Sequencer logic stripped of UI noise.  The public API mirrors what the rest
// of the firmware needs: set a tempo, feed it seeds, and call `onTick()` once
// per MIDI clock (24 pulses per quarter note).  The dense commentary makes it a
// perfect whiteboard companion for explaining probability gates, density, and
// jitter to curious students.  Native builds now advance an internal
// samples-per-tick cursor so BPM finally dictates the simulated transport math.
#include <vector>
#include <cstddef>
#include <cstdint>
#include "Seed.h"
#include "util/Annotations.h"

class PatternScheduler {
public:
  PatternScheduler();
  // Define the global tempo.  We keep the unit in BPM because it lines up with
  // the incoming MIDI clock rate.
  SEEDBOX_MAYBE_UNUSED void setBpm(float bpm);
  float bpm() const { return bpm_; }

  // Hardware builds can feed us a real sample clock so the scheduler stays in
  // sync with the audio ISR.  Native builds leave this null and lean on the
  // internal cursor.
  SEEDBOX_MAYBE_UNUSED void setSampleClockFn(uint32_t (*fn)());

  // Pulse this once per 24 PPQN tick.  Internally `onTick` checks density,
  // probability, jitter, and schedules engine triggers.
  SEEDBOX_MAYBE_UNUSED void onTick();

  // Manage the scheduler's seed roster.  Seeds are stored by value so the
  // scheduler can mutate its own copies without racing callers.
  SEEDBOX_MAYBE_UNUSED void addSeed(const Seed& s);
  SEEDBOX_MAYBE_UNUSED bool updateSeed(std::size_t index, const Seed& s);

  // Register a callback that will fire whenever a seed earns a trigger.  The
  // callback receives the sample timestamp so downstream code can render in
  // lockstep with the audio device.
  SEEDBOX_MAYBE_UNUSED void setTriggerCallback(void* ctx, void (*fn)(void*, const Seed&, uint32_t));
  uint64_t ticks() const { return tickCount_; }

  SEEDBOX_MAYBE_UNUSED uint32_t nowSamples() const;

  // Peek at the scheduler's internal copy of a seed for debugging / teaching.
  // Returns nullptr if you wander off the end of the list.
  SEEDBOX_MAYBE_UNUSED const Seed* seedForDebug(std::size_t index) const;

private:
  // Implementation helpers documented in Patterns.cpp.  We keep declarations
  // clustered here so readers know what machinery hides behind `onTick`.
  bool densityGate(std::size_t seedIndex, float density);
  void recalcSamplesPerTick();
  uint32_t latchTickSample();
  uint32_t msToSamples(float ms);
private:
  std::vector<Seed> seeds_;
  std::vector<float> densityAccumulators_;
  uint64_t tickCount_{0};
  float bpm_{120.f};
  void* triggerCtx_{nullptr};
  void (*triggerFn_)(void*, const Seed&, uint32_t){nullptr};
  uint32_t (*sampleClockFn_)() = nullptr;
  double sampleCursor_{0.0};
  double samplesPerTick_{0.0};
  uint32_t latchedTickSample_{0};
};
