#pragma once
#include <stdint.h>
#include <vector>
#include "Seed.h"
#include "engine/Patterns.h"
#ifdef SEEDBOX_HW
#include "io/MidiRouter.h"
#endif

class AppState {
public:
  void initHardware();
  void initSim();
  void tick();

  struct DisplaySnapshot {
    char title[17];
    char status[17];
    char metrics[17];
    char nuance[17];
  };

  void captureDisplaySnapshot(DisplaySnapshot& out) const;
  void reseed(uint32_t masterSeed);
  void setFocusSeed(uint8_t index);
  const std::vector<Seed>& seeds() const { return seeds_; }
  uint32_t masterSeed() const { return masterSeed_; }
  uint8_t focusSeed() const { return focusSeed_; }
  uint64_t schedulerTicks() const { return scheduler_.ticks(); }

  void onExternalClockTick();
  void onExternalTransportStart();
  void onExternalTransportStop();
  void onExternalControlChange(uint8_t ch, uint8_t cc, uint8_t val);

#ifdef SEEDBOX_HW
  MidiRouter midi;
#endif

private:
  void primeSeeds(uint32_t masterSeed);
  uint32_t frame_{0};
  std::vector<Seed> seeds_{};
  PatternScheduler scheduler_{};
  uint32_t masterSeed_{0x5EEDB0B1u};
  uint8_t focusSeed_{0};
  bool seedsPrimed_{false};
  bool externalClockDominant_{false};
};
