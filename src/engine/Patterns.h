#pragma once
#include <vector>
#include <cstdint>
#include "Seed.h"

class PatternScheduler {
public:
  void setBpm(float bpm);
  void onTick();                 // call at 24 PPQN
  void addSeed(const Seed& s);
  void setTriggerCallback(void* ctx, void (*fn)(void*, const Seed&, uint32_t));
  uint64_t ticks() const { return tickCount_; }

private:
  bool densityGate(size_t seedIndex, float density);
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
