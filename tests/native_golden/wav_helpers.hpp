#pragma once

#include <cstddef>
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

//! Hashes arbitrary byte payloads (logs, manifests, etc.) via the same 64-bit
//! FNV-1a routine so control logs can be verified without touching disk.
std::string hash_bytes(const std::string &payload);

struct SpatialRender {
    std::vector<int16_t> samples;
    std::string control_log;
    std::uint32_t sample_rate_hz = 48000u;
    std::uint16_t channels = 1u;
    std::size_t frames = 0u;
};

//! Synthesizes a 7.1 stage render that slams every mixer lane with deterministic
//! content. The helper keeps the layout self-documenting so reviewers can point
//! meters at each channel without guessing.
SpatialRender render_stage71_scene();
//! Renders the layered Euclid/Burst engine capture that glues the sampler,
//! resonator, and granular lanes together. Returns interleaved stereo PCM16
//! samples (L/R) so callers can dump the buffer straight to disk.
std::vector<int16_t> render_layered_euclid_burst_fixture();

}  // namespace golden
