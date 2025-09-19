#pragma once
#include <cstdint>

struct Seed {
  uint32_t id{0};
  uint32_t prng{0};         // deterministic RNG seed
  float pitch{0.f};         // semitone offset
  float envA{0.001f}, envD{0.08f}, envS{0.6f}, envR{0.12f};
  float density{1.f};       // hits per beat
  float probability{0.85f};
  float jitterMs{7.5f};
  float tone{0.35f};
  float spread{0.2f};
  uint8_t engine{0};        // 0=sampler,1=granular,2=resonator
  uint8_t sampleIdx{0};
  float mutateAmt{0.1f};    // bounded drift 0..1
};
