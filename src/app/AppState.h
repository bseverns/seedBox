#pragma once
#include <stdint.h>
#include <vector>
#include "Seed.h"
#include "engine/Patterns.h"
#include "engine/EngineRouter.h"
#ifdef SEEDBOX_HW
#include "io/MidiRouter.h"
#endif

// AppState is the mothership for everything the performer can poke at run time.
// It owns the seed table, orchestrates scheduling, and provides a place for the
// UI layer (physical or simulated) to read back human-friendly snapshots. The
// teaching intent: students should be able to glance at this header and see
// what control hooks exist without spelunking the implementation.
class AppState {
public:
  // Boot the full hardware stack. We spin up MIDI routing, initialise the
  // engine router in hardware mode, and prime the deterministic seed table so
  // the Teensy build behaves exactly like the simulator.
  void initHardware();

  // Boot the simulator variant. Same story as initHardware, just without the
  // hardware drivers so unit tests (and classroom demos) run fast and
  // repeatably on a laptop.
  void initSim();

  // Pump one control-tick. In sim builds tests call this manually; on hardware
  // the main loop does the honours. Either way the scheduler decides whether a
  // seed should trigger on this frame.
  void tick();

  struct DisplaySnapshot {
    char title[32];
    char status[32];
    char metrics[32];
    char nuance[32];
  };

  // Populate the snapshot struct with text destined for the OLED / debug
  // display. Think of this as the "mixing console" view for teaching labs.
  void captureDisplaySnapshot(DisplaySnapshot& out) const;

  // Re-roll the procedural seeds using a supplied master seed. Students can use
  // this hook to explore how the scheduler reacts to different pseudo-random
  // genomes without rebooting the whole device.
  void reseed(uint32_t masterSeed);

  // Set which seed the UI is focused on. The focus influences which engine gets
  // cycled when the performer mashes the CC encoder.
  void setFocusSeed(uint8_t index);

  // Core teaching hook: explicitly assign a playback engine (sampler, granular,
  // resonator) to a seed. We expose it so integration tests — and students — can
  // reason about deterministic engine swaps.
  void setSeedEngine(uint8_t seedIndex, uint8_t engineId);
  const std::vector<Seed>& seeds() const { return seeds_; }
  uint32_t masterSeed() const { return masterSeed_; }
  uint8_t focusSeed() const { return focusSeed_; }
  uint64_t schedulerTicks() const { return scheduler_.ticks(); }

  const Seed* debugScheduledSeed(uint8_t index) const;

  // MIDI ingress points. Each handler maps 1:1 with incoming transport/clock
  // events so lessons about external sync can point here directly.
  void onExternalClockTick();
  void onExternalTransportStart();
  void onExternalTransportStop();
  void onExternalControlChange(uint8_t ch, uint8_t cc, uint8_t val);

#ifdef SEEDBOX_HW
  MidiRouter midi;
#endif

private:
  // Helper that hydrates the seeds_ array deterministically from masterSeed_.
  // The implementation leans heavily on comments so students can watch the RNG
  // state machine do its thing.
  void primeSeeds(uint32_t masterSeed);
  uint32_t frame_{0};
  std::vector<Seed> seeds_{};
  PatternScheduler scheduler_{};
  EngineRouter engines_{};
  std::vector<uint8_t> seedEngineSelections_{};
  uint32_t masterSeed_{0x5EEDB0B1u};
  uint8_t focusSeed_{0};
  bool seedsPrimed_{false};
  bool externalClockDominant_{false};
};
