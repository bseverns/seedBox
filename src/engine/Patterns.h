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
#include "app/Clock.h"

class PatternScheduler {
public:
  explicit PatternScheduler(ClockProvider* clock = nullptr);
  // Define the global tempo.  We keep the unit in BPM because it lines up with
  // the incoming MIDI clock rate.
  SEEDBOX_MAYBE_UNUSED void setBpm(float bpm);
  float bpm() const { return bpm_; }

  // Hardware builds can feed us a real sample clock so the scheduler stays in
  // sync with the audio ISR.  Native builds leave this null and lean on the
  // internal cursor.
  SEEDBOX_MAYBE_UNUSED void setSampleClockFn(uint32_t (*fn)());

  void setClockProvider(ClockProvider* clock);
  ClockProvider* clockProvider() const { return clock_; }

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

  SEEDBOX_MAYBE_UNUSED uint32_t lastTickTriggerCount() const { return lastTickTriggerCount_; }

  SEEDBOX_MAYBE_UNUSED uint32_t nowSamples() const;

  // Peek at the scheduler's internal copy of a seed for debugging / teaching.
  // Returns nullptr if you wander off the end of the list.
  SEEDBOX_MAYBE_UNUSED const Seed* seedForDebug(std::size_t index) const;

  void triggerImmediate(std::size_t seedIndex, uint32_t whenSamples);
  void clearPendingTriggers();

  struct Diagnostics {
    uint32_t immediateQueueOverflows{0};
    uint32_t quantizedQueueOverflows{0};
    uint32_t missedTicks{0};
    uint32_t schedulingLag{0};
  };

  void setDiagnosticsEnabled(bool enabled) { diagnosticsEnabled_ = enabled; }
  bool diagnosticsEnabled() const { return diagnosticsEnabled_; }
  const Diagnostics& diagnostics() const { return diagnostics_; }
  void resetDiagnostics() { diagnostics_ = {}; }

#if !defined(ENABLE_GOLDEN)
#define ENABLE_GOLDEN 0
#endif
  const std::vector<uint32_t>& tickLog() const { return tickLog_; }
  void clearTickLog() { tickLog_.clear(); }

private:
  // Implementation helpers documented in Patterns.cpp.  We keep declarations
  // clustered here so readers know what machinery hides behind `onTick`.
  bool densityGate(std::size_t seedIndex, float density);
  void recalcSamplesPerTick();
  uint32_t latchTickSample();
  uint32_t msToSamples(float ms);
  void dispatchQueues();

private:
  std::vector<Seed> seeds_;
  std::vector<float> densityAccumulators_;
  uint64_t tickCount_{0};
  ClockProvider* clock_{nullptr};
  float bpm_{120.f};
  void* triggerCtx_{nullptr};
  void (*triggerFn_)(void*, const Seed&, uint32_t){nullptr};
  uint32_t (*sampleClockFn_)() = nullptr;
  double sampleCursor_{0.0};
  double samplesPerTick_{0.0};
  uint32_t latchedTickSample_{0};
  uint32_t lastTickSample_{0};
  struct QueuedTrigger {
    std::size_t seedIndex;
    uint32_t when;
  };
  static constexpr std::size_t kMaxQueuedTriggers = 256;
  std::vector<QueuedTrigger> quantizedQueue_;
  std::vector<QueuedTrigger> immediateQueue_;
  std::vector<uint32_t> tickLog_;
  uint32_t lastTickTriggerCount_{0};
  Diagnostics diagnostics_{};
  bool diagnosticsEnabled_{false};
};
