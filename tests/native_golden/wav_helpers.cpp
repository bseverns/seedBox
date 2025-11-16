#include "wav_helpers.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <type_traits>

#include "SeedBoxConfig.h"

namespace golden {

bool write_wav_16(const WavWriteRequest &request) {
#if ENABLE_GOLDEN
    if (request.path.empty() || request.samples.empty() || request.sample_rate_hz == 0) {
        return false;
    }

    if (request.channels == 0) {
        return false;
    }

    if (request.samples.size() % request.channels != 0) {
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

    const std::uint16_t kBitsPerSample = 16;
    const std::uint16_t channels = request.channels;
    const std::uint32_t data_bytes = static_cast<std::uint32_t>(request.samples.size() * sizeof(std::int16_t));
    const std::uint32_t byte_rate = request.sample_rate_hz * channels * (kBitsPerSample / 8);
    const std::uint16_t block_align = static_cast<std::uint16_t>(channels * (kBitsPerSample / 8));

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
    write_le(channels);
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

namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

std::string format_fnv(std::uint64_t state) {
    std::ostringstream oss;
    oss << std::hex << std::nouppercase << std::setfill('0') << std::setw(16) << state;
    return oss.str();
}

}  // namespace

std::string hash_pcm16(const std::vector<int16_t> &samples) {
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

    return format_fnv(state);
}

std::string hash_bytes(const std::string &payload) {
    std::uint64_t state = kFnvOffset;
    for (unsigned char byte : payload) {
        state ^= static_cast<std::uint8_t>(byte);
        state *= kFnvPrime;
    }
    return format_fnv(state);
}

namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;

template <std::size_t ChannelCount>
struct StageStem {
    const char* name;
    double freq_hz;
    double amplitude;
    std::array<double, ChannelCount> base_gains;
    double pan_speed_hz;
    double mid_side_rate_hz;
    double surround_rate_hz;
};

}  // namespace

SpatialRender render_stage71_scene() {
    constexpr std::size_t kChannels = 8u;
    constexpr std::size_t kFrames = 48000u;
    constexpr double kSampleRate = 48000.0;

    using Stem = StageStem<kChannels>;
    const std::array<Stem, 4> stems = {Stem{"front beam",
                                              110.0,
                                              0.75,
                                              {1.00, 0.95, 0.60, 0.10, 0.30, 0.18, 0.12, 0.08},
                                              0.08,
                                              0.20,
                                              0.05},
                                       Stem{"center pad",
                                              164.81,
                                              0.55,
                                              {0.50, 0.50, 1.00, 0.25, 0.22, 0.22, 0.18, 0.18},
                                              0.03,
                                              0.16,
                                              0.04},
                                       Stem{"rear bloom",
                                              220.0,
                                              0.65,
                                              {0.35, 0.35, 0.25, 0.15, 0.60, 0.55, 0.70, 0.70},
                                              0.05,
                                              0.12,
                                              0.09},
                                       Stem{"side grit",
                                              329.63,
                                              0.45,
                                              {0.42, 0.48, 0.20, 0.18, 0.40, 0.42, 0.30, 0.30},
                                              0.11,
                                              0.28,
                                              0.13}};

    std::array<std::vector<double>, kChannels> lanes;
    for (auto& lane : lanes) {
        lane.assign(kFrames, 0.0);
    }

    for (std::size_t idx = 0; idx < stems.size(); ++idx) {
        const auto& stem = stems[idx];
        for (std::size_t frame = 0; frame < kFrames; ++frame) {
            const double t = static_cast<double>(frame) / kSampleRate;
            const double oscillator = std::sin(kTwoPi * stem.freq_hz * t);
            const double envelope = stem.amplitude *
                                    (0.7 + 0.3 * std::sin(kTwoPi * (0.17 + 0.03 * idx) * t));
            const double pan = 0.5 * (1.0 + std::sin(kTwoPi * (stem.pan_speed_hz + 0.01 * idx) * t));
            const double mid_side =
                std::sin(kTwoPi * (stem.mid_side_rate_hz + 0.007 * idx) * t);
            const double surround_swirl = 0.5 +
                                          0.5 * std::sin(kTwoPi * (stem.surround_rate_hz + 0.02 * idx) * t +
                                                         0.35 * idx);
            const double rear_pivot = 0.5 +
                                      0.5 * std::cos(kTwoPi * (stem.surround_rate_hz * 0.8 + 0.015 * idx) * t);

            const double sample = oscillator * envelope;
            std::array<double, kChannels> modulated = stem.base_gains;
            modulated[0] *= 0.6 + (1.0 - pan) + 0.35 * mid_side;
            modulated[1] *= 0.6 + pan - 0.35 * mid_side;
            modulated[2] *= 0.7 + 0.3 * (1.0 - std::abs(mid_side));
            modulated[3] *= 0.25 + 0.75 * std::abs(mid_side);
            modulated[4] *= 0.5 + 0.8 * surround_swirl;
            modulated[5] *= 0.5 + 0.8 * (1.0 - surround_swirl);
            modulated[6] *= 0.4 + 0.6 * rear_pivot + 0.2 * mid_side;
            modulated[7] *= 0.4 + 0.6 * (1.0 - rear_pivot) - 0.2 * mid_side;

            for (std::size_t ch = 0; ch < kChannels; ++ch) {
                lanes[ch][frame] += sample * modulated[ch];
            }
        }
    }

    for (std::size_t frame = 0; frame < kFrames; ++frame) {
        const double front_mid = 0.5 * (lanes[0][frame] + lanes[1][frame]);
        const double front_side = lanes[0][frame] - lanes[1][frame];
        const double surround_mid = 0.5 * (lanes[4][frame] + lanes[5][frame]);
        const double rear_mid = 0.5 * (lanes[6][frame] + lanes[7][frame]);

        lanes[2][frame] += front_mid * 0.55;
        lanes[3][frame] += 0.35 * front_mid + 0.25 * std::abs(front_side);
        lanes[4][frame] += front_side * 0.22;
        lanes[5][frame] -= front_side * 0.22;

        const double rear_glue = 0.18 * (rear_mid - surround_mid);
        lanes[6][frame] -= rear_glue;
        lanes[7][frame] += rear_glue;

        const double crossfeed = 0.12 * (lanes[0][frame] - lanes[1][frame]);
        lanes[0][frame] -= crossfeed;
        lanes[1][frame] += crossfeed;

        lanes[3][frame] = std::tanh(lanes[3][frame] * 1.20);
        lanes[6][frame] = std::tanh(lanes[6][frame] * 1.05);
        lanes[7][frame] = std::tanh(lanes[7][frame] * 1.05);
    }

    double max_abs = 0.0;
    for (const auto& lane : lanes) {
        for (double sample : lane) {
            max_abs = std::max(max_abs, std::abs(sample));
        }
    }
    const double scale = (max_abs > 0.0) ? (0.92 / max_abs) : 0.0;

    std::vector<int16_t> pcm(kFrames * kChannels);
    for (std::size_t frame = 0; frame < kFrames; ++frame) {
        for (std::size_t ch = 0; ch < kChannels; ++ch) {
            const double value = lanes[ch][frame] * scale;
            const auto quantized = static_cast<long>(std::lround(value * 32767.0));
            pcm[kChannels * frame + ch] =
                static_cast<int16_t>(std::clamp<long>(quantized, -32768L, 32767L));
        }
    }

    std::ostringstream log;
    log << "# stage71-bus control log" << '\n';
    log << "frames=" << kFrames << " sample_rate_hz=" << static_cast<int>(kSampleRate)
        << " channels=" << kChannels << '\n';
    log << "channel_order=[L,R,C,LFE,Ls,Rs,Lrs,Rrs]" << '\n';
    log << "channel_comments:" << '\n';
    log << "  L=front left main // base reference for panning math" << '\n';
    log << "  R=front right main // mirror of L so crossfeed drift shows up" << '\n';
    log << "  C=center bus // mid collapse proves downmix math" << '\n';
    log << "  LFE=sub energy // watches mid-side fold plus saturation" << '\n';
    log << "  Ls/Rs=side surrounds // ride front-side feeds" << '\n';
    log << "  Lrs/Rrs=rear surrounds // glue + tilt relative to sides" << '\n';
    log << "stem,index,name,freq_hz,amplitude,pan_speed,ms_rate,surround_rate,gains(L,R,C,LFE,Ls,Rs,Lrs,Rrs)" << '\n';
    log << std::fixed << std::setprecision(4);
    for (std::size_t idx = 0; idx < stems.size(); ++idx) {
        log << idx << ',' << stems[idx].name << ',' << stems[idx].freq_hz << ','
            << stems[idx].amplitude << ',' << stems[idx].pan_speed_hz << ','
            << stems[idx].mid_side_rate_hz << ',' << stems[idx].surround_rate_hz << ',';
        log << '[';
        for (std::size_t ch = 0; ch < kChannels; ++ch) {
            if (ch != 0) {
                log << ';';
            }
            log << stems[idx].base_gains[ch];
        }
        log << ']' << '\n';
    }
    log << "bus_fx: front_mid_to_center=0.55 front_mid_to_lfe=0.35 front_side_to_surround=0.22"
        << " rear_glue=0.18 crossfeed=0.12 saturate={LFE:1.20,rear:1.05}" << '\n';

    SpatialRender scene;
    scene.samples = std::move(pcm);
    scene.control_log = log.str();
    scene.sample_rate_hz = static_cast<std::uint32_t>(kSampleRate);
    scene.channels = static_cast<std::uint16_t>(kChannels);
    scene.frames = kFrames;
    return scene;
}

}  // namespace golden
