#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace golden {

struct WavWriteRequest {
    std::string path;
    uint32_t sample_rate_hz = 48000;
    std::vector<int16_t> samples;
};

//! Placeholder 16-bit PCM WAV writer.
//! When ENABLE_GOLDEN is flipped on, implement this to dump buffers for hashing.
bool write_wav_16(const WavWriteRequest &request);

//! Produces a stable hash of the WAV payload (PCM data only).
//! Replace the body with your preferred hashing routine when goldens go live.
std::string hash_pcm16(const std::vector<int16_t> &samples);

}  // namespace golden
