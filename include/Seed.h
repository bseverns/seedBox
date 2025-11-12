#pragma once

//
// Seed genome.
// ------------
// One struct to rule them all — every engine reads its marching orders from a
// Seed.  That means we get to teach procedural music systems by poking at this
// file and watching how parameters ripple through the sampler/granular/resonator
// code.
#include <cstdint>

struct Seed {
  // Identity -----------------
  // Each voice and debug readout keys off `id`.  AppState assigns this as the
  // index into its seed table so unit tests can reach in deterministically.
  uint32_t id{0};
  // `prng` is the raw RNG state captured when the seed was minted.  Engines use
  // it as provenance when they need to re-roll per-seed variations (e.g.
  // Euclid patterns fanning out from the same genome).
  uint32_t prng{0};         // deterministic RNG seed
  enum class Source : uint8_t { kLfsr = 0, kTapTempo, kPreset, kLiveInput };
  Source source{Source::kLfsr};
  // `lineage` traces the prime mover: master seed value, preset slot, tap tempo
  // BPM tag, etc.  UI views surface it so students can talk about "seed
  // families" instead of magic numbers.
  uint32_t lineage{0};      // mode-specific provenance (master seed, preset id, etc.)

  // Musical DNA -------------
  // Pitch lives in semitone offsets from concert A.  Sampler::configureVoice
  // converts this with pitchToPlaybackRate.
  float pitch{0.f};         // semitone offset
  // ADSR envelope in seconds.  Engines clamp/convert as needed: Sampler goes
  // to milliseconds for Teensy, Resonator feeds these into modal envelopes.
  float envA{0.001f}, envD{0.08f}, envS{0.6f}, envR{0.12f};
  // Density and probability drive the scheduler.  Density is hits/beat while
  // probability is the Bernoulli gate the Euclid engine consults.
  float density{1.f};       // hits per beat
  float probability{0.85f};
  // Groove + tone controls; jitter is milliseconds of timing spray, tone is a
  // tilt EQ macro, and spread widens the stereo field.
  float jitterMs{7.5f};
  float tone{0.35f};
  float spread{0.2f};       // 0 = mono center, 1 = hard-pan width (right biased for now)

  // Routing ------------------
  // Engines treat this as a "which DSP lane should fire" enum.  EngineRouter
  // keeps the IDs aligned with Engine::Type.
  uint8_t engine{0};        // 0=sampler,1=granular,2=resonator
  uint8_t sampleIdx{0};
  // Mutation amount is the guard-rail for random walk editors.  SeedLock uses
  // it to decide how aggressive to be when the mutate encoder is twisted.
  float mutateAmt{0.1f};    // bounded drift 0..1

  struct GranularParams {
    float grainSizeMs{90.f};     // grain length envelope target
    float sprayMs{18.f};         // random offset per grain
    float transpose{0.f};        // semitone shift relative to seed pitch
    float windowSkew{0.f};       // -1 saw, 0 hann, +1 exponential-ish
    float stereoSpread{0.5f};    // 0 = mono center .. 1 = wide/right-lean
    uint8_t source{0};           // 0=live input,1=sd clip table
    uint8_t sdSlot{0};           // SD clip slot when source==1
  } granular;

  struct ResonatorParams {
    float exciteMs{3.5f};        // duration of excitation burst
    float damping{0.35f};        // 0 overdamped .. 1 infinite sustain
    float brightness{0.6f};      // tone tilt into modal bank
    float feedback{0.78f};       // global feedback amount
    uint8_t mode{0};             // 0=KarplusStrong,1=Modal bank
    uint8_t bank{0};             // modal preset index
  } resonator;
};
