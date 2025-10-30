#pragma once

#ifdef SEEDBOX_HW

// AudioMemoryBudget centralizes the buffer math that keeps the Teensy audio
// graph fed. Each block equals 128 samples at 44.1 kHz, and we divvy them up
// across the big three engines plus a router/mixer reserve. Documenting the
// counts here makes it obvious how to rebalance things the next time someone
// brings a new DSP toy to the party.
namespace AudioMemoryBudget {

// Sampler chews through envelopes and dual-source playback, but it stays fairly
// lean: 88 blocks still cover the legacy stress tests while handing a few blocks
// back to the global pool. Less RAM pressure, no audible compromise.
constexpr uint16_t kSamplerBlocks = 88;

// Granular is the monster. Window overlap and SD streaming slammed the pool,
// but we can dial the ceiling to 144 blocks and keep the "max chaos" lessons
// intact. The tighter budget buys us the RAM we need to actually ship.
constexpr uint16_t kGranularBlocks = 144;

// Resonator banks light up burst envelopes, delay lines, and a spray of modal
// filters. Shaving this down to 88 blocks still leaves plenty of sustain while
// easing the linker's RAM panic.
constexpr uint16_t kResonatorBlocks = 88;

// The old setup.cpp call to AudioMemory(64) effectively funded the shared I/O
// spine: input bus, final mix, and whatever utility nodes sneak in around the
// engines. We preserve that headroom explicitly so nobody quietly steals it.
constexpr uint16_t kSharedGraphBlocks = 64;

// Total budget: one righteous AudioMemory() call during hardware bootstrap.
constexpr uint16_t kTotalBlocks = kSamplerBlocks + kGranularBlocks +
                                   kResonatorBlocks + kSharedGraphBlocks;

}  // namespace AudioMemoryBudget

#endif  // SEEDBOX_HW
