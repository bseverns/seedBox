#include "wav_helpers.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <type_traits>

#ifndef ENABLE_GOLDEN
#define ENABLE_GOLDEN 0
#endif

namespace golden {

bool write_wav_16(const WavWriteRequest &request) {
#if ENABLE_GOLDEN
    if (request.path.empty() || request.samples.empty() || request.sample_rate_hz == 0) {
        return false;
    }

    std::filesystem::path path(request.path);
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            return false;
        }
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }

    const std::uint16_t kChannels = 1;
    const std::uint16_t kBitsPerSample = 16;
    const std::uint32_t data_bytes = static_cast<std::uint32_t>(request.samples.size() * sizeof(std::int16_t));
    const std::uint32_t byte_rate = request.sample_rate_hz * kChannels * (kBitsPerSample / 8);
    const std::uint16_t block_align = static_cast<std::uint16_t>(kChannels * (kBitsPerSample / 8));

    auto write_tag = [&](const char (&tag)[5]) {
        out.write(tag, 4);
    };

    auto write_le = [&](auto value) {
        using T = std::decay_t<decltype(value)>;
        using UnsignedT = std::make_unsigned_t<T>;
        UnsignedT v = static_cast<UnsignedT>(value);
        for (std::size_t i = 0; i < sizeof(UnsignedT); ++i) {
            const char byte = static_cast<char>((v >> (8u * i)) & 0xFFu);
            out.put(byte);
        }
    };

    write_tag("RIFF");
    const std::uint32_t riff_size = 36u + data_bytes;
    write_le(riff_size);
    write_tag("WAVE");
    write_tag("fmt ");
    const std::uint32_t fmt_size = 16u;
    write_le(fmt_size);
    const std::uint16_t audio_format = 1u;  // PCM
    write_le(audio_format);
    write_le(kChannels);
    write_le(request.sample_rate_hz);
    write_le(byte_rate);
    write_le(block_align);
    write_le(kBitsPerSample);
    write_tag("data");
    write_le(data_bytes);
    out.write(reinterpret_cast<const char*>(request.samples.data()), data_bytes);

    return out.good();
#else
    (void)request;
    return false;
#endif
}

std::string hash_pcm16(const std::vector<int16_t> &samples) {
    constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
    constexpr std::uint64_t kFnvPrime = 1099511628211ull;

    std::uint64_t state = kFnvOffset;
    for (std::int16_t sample : samples) {
        std::uint16_t value = static_cast<std::uint16_t>(sample);
        const std::uint8_t lo = static_cast<std::uint8_t>(value & 0xFFu);
        const std::uint8_t hi = static_cast<std::uint8_t>((value >> 8u) & 0xFFu);
        state ^= lo;
        state *= kFnvPrime;
        state ^= hi;
        state *= kFnvPrime;
    }

    std::ostringstream oss;
    oss << std::hex << std::nouppercase << std::setfill('0') << std::setw(16) << state;
    return oss.str();
}

}  // namespace golden
