#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace golden {

struct WavWriteRequest {
    std::string path;
    uint32_t sample_rate_hz = 48000;
    uint16_t channels = 1;
    std::vector<int16_t> samples;
};

//! Minimal 16-bit PCM WAV writer used by the golden harness. When
//! `ENABLE_GOLDEN` is flipped on we emit a proper header + payload to disk so
//! humans can inspect the render alongside the stored hash.
bool write_wav_16(const WavWriteRequest &request);

//! Produces a stable hash of the WAV payload (PCM data only) using 64-bit
//! FNV-1a.
std::string hash_pcm16(const std::vector<int16_t> &samples);

}  // namespace golden
