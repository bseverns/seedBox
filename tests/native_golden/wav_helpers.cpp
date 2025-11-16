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
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>
#include <cmath>

#include "SeedBoxConfig.h"
#include "engine/BurstEngine.h"
#include "engine/EuclidEngine.h"
#include "Seed.h"

#if ENABLE_GOLDEN
extern bool emit_control_log(const char* fixture_name, const std::string& body);
#endif

namespace golden {

namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr double kHalfPi = 1.5707963267948966192313216916398;

#if ENABLE_GOLDEN

struct LayeredEvent {
    std::uint32_t whenSamples = 0;
    std::uint32_t euclidStep = 0;
    std::uint32_t burstIndex = 0;
    int engine = 0;  // 0=sampler,1=resonator,2=granular
    double panSeed = 0.5;
};

inline double clamp01(double value) {
    return std::max(0.0, std::min(1.0, value));
}

inline std::array<double, 2> make_pan(double pan) {
    const double clamped = clamp01(pan);
    const double angle = clamped * kHalfPi;
    return {std::cos(angle), std::sin(angle)};
}

inline double sample_lane(const std::vector<double>& lane, std::size_t frame) {
    if (lane.empty()) {
        return 0.0;
    }
    if (frame >= lane.size()) {
        return lane.back();
    }
    return lane[frame];
}
#endif  // ENABLE_GOLDEN

}  // namespace

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

SpatialRender render_engine_hybrid_fixture() {
#if !ENABLE_GOLDEN
    return {};
#else
    constexpr double kSampleRate = 48000.0;
    constexpr double kBpm = 128.0;
    constexpr int kBeats = 28;
    constexpr double kStepsPerBeat = 4.0;
    constexpr std::uint8_t kEuclidSteps = 11;
    constexpr std::uint8_t kEuclidFills = 6;
    constexpr std::uint8_t kEuclidRotate = 2;
    constexpr std::uint8_t kBurstCluster = 4;
    constexpr std::uint32_t kBurstSpacing = 360;
    constexpr std::size_t kAutomationStride = 96;
    constexpr double kNormalizeTarget = 0.91;

    const std::size_t totalSteps = static_cast<std::size_t>(kBeats * kStepsPerBeat);
    const double framesPerBeat = kSampleRate * (60.0 / static_cast<double>(kBpm));
    const double framesPerStep = framesPerBeat / kStepsPerBeat;
    const std::size_t frames = static_cast<std::size_t>(
        std::lround(totalSteps * framesPerStep + kSampleRate * 2.0));
    if (frames == 0) {
        return {};
    }

    SpatialRender render;
    render.sample_rate_hz = static_cast<std::uint32_t>(kSampleRate);
    render.channels = 2u;
    render.frames = frames;

    std::vector<double> samplerBrightness(frames);
    std::vector<double> samplerDrive(frames);
    std::vector<double> resonatorBloom(frames);
    std::vector<double> resonatorFeedback(frames);
    std::vector<double> granularDensity(frames);
    std::vector<double> macroPan(frames);

    const double normalization = (frames > 1) ? static_cast<double>(frames - 1u) : 1.0;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const double progress = static_cast<double>(frame) / normalization;
        samplerBrightness[frame] = 0.30 + 0.70 * 0.5 *
                                   (1.0 + std::sin(kTwoPi * (progress * 1.8 + 0.07)));
        samplerDrive[frame] = 0.20 + 0.80 * 0.5 *
                              (1.0 + std::cos(kTwoPi * (progress * 0.6 + 0.25)));
        resonatorBloom[frame] = 0.18 + 0.82 * 0.5 *
                                (1.0 + std::sin(kTwoPi * (progress * 0.9 + 0.41)));
        resonatorFeedback[frame] = 0.35 + 0.65 * 0.5 *
                                   (1.0 + std::cos(kTwoPi * (progress * 1.15 + samplerDrive[frame] * 0.2)));
        granularDensity[frame] = 0.12 + 0.88 * 0.5 *
                                 (1.0 + std::sin(kTwoPi * (progress * 0.55 + resonatorBloom[frame] * 0.3)));
        macroPan[frame] = clamp01(0.5 + 0.45 * std::sin(kTwoPi * (progress * 0.33 + 0.15)));
    }

    EuclidEngine euclid;
    Engine::PrepareContext euclidPrep{};
    euclidPrep.masterSeed = 0xC0FFEEu;
    euclid.prepare(euclidPrep);
    euclid.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kSteps), kEuclidSteps});
    euclid.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kFills), kEuclidFills});
    euclid.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kRotate), kEuclidRotate});

    BurstEngine burst;
    Engine::PrepareContext burstPrep{};
    burstPrep.masterSeed = 0xB17EB5Eu;
    burst.prepare(burstPrep);
    burst.onParam({0, static_cast<std::uint16_t>(BurstEngine::Param::kClusterCount), kBurstCluster});
    burst.onParam({0, static_cast<std::uint16_t>(BurstEngine::Param::kSpacingSamples),
                   static_cast<std::int32_t>(kBurstSpacing)});

    std::vector<LayeredEvent> events;
    events.reserve(totalSteps * kBurstCluster);
    Engine::TickContext tick{};
    Seed burstSeed{};
    std::size_t eventCounter = 0;
    for (std::uint32_t step = 0; step < totalSteps; ++step) {
        tick.tick = step;
        euclid.onTick(tick);
        if (!euclid.lastGate()) {
            continue;
        }
        const std::uint32_t when = static_cast<std::uint32_t>(
            std::lround(static_cast<double>(step) * framesPerStep));
        burstSeed.id = static_cast<std::uint32_t>(0x400 + step);
        burst.onSeed({burstSeed, when});
        const auto& cluster = burst.pendingTriggers();
        for (std::size_t clusterIdx = 0; clusterIdx < cluster.size(); ++clusterIdx) {
            LayeredEvent event{};
            event.whenSamples = std::min(cluster[clusterIdx], static_cast<std::uint32_t>(frames - 1));
            event.euclidStep = step;
            event.burstIndex = static_cast<std::uint32_t>(clusterIdx);
            event.engine = static_cast<int>((eventCounter + clusterIdx) % 3u);
            const double panSeed = 0.18 + 0.7 *
                                   (static_cast<double>((step * 17 + clusterIdx * 13) % 101) / 100.0);
            event.panSeed = panSeed;
            events.push_back(event);
        }
        eventCounter += cluster.size();
    }

    std::vector<double> left(frames, 0.0);
    std::vector<double> right(frames, 0.0);

    const std::array<double, 6> samplerBase = {110.0, 147.0, 196.0, 220.0, 246.94, 329.63};
    const std::array<double, 5> resonatorModes = {196.0, 233.08, 261.63, 311.13, 392.0};

    for (const auto& event : events) {
        const std::size_t start = std::min<std::size_t>(event.whenSamples, frames - 1);
        const double macro = sample_lane(macroPan, start);
        const double blendedPan = clamp01(0.2 + 0.5 * event.panSeed + 0.3 * (macro - 0.5));
        const auto pan = make_pan(blendedPan);
        switch (event.engine) {
            case 0: {
                const double brightness = sample_lane(samplerBrightness, start);
                const double drive = sample_lane(samplerDrive, start);
                const double freq = samplerBase[(event.euclidStep + event.burstIndex) % samplerBase.size()] *
                                   (1.0 + 0.35 * (brightness - 0.5));
                const std::size_t duration = std::min<std::size_t>(
                    frames - start, static_cast<std::size_t>(kSampleRate * (0.12 + 0.5 * drive)));
                for (std::size_t i = 0; i < duration; ++i) {
                    const double t = static_cast<double>(i) / kSampleRate;
                    double env = std::exp(-t * (2.4 - 1.2 * drive));
                    env *= (1.0 - std::exp(-t * (18.0 + 40.0 * drive)));
                    double sample = std::sin(kTwoPi * freq * t + drive * 1.2);
                    sample += 0.35 * brightness * std::sin(kTwoPi * freq * 2.01 * t + brightness);
                    sample += 0.18 * std::sin(kTwoPi * freq * 0.5 * t + event.burstIndex);
                    sample *= env * (0.42 + 0.48 * drive);
                    left[start + i] += sample * pan[0];
                    right[start + i] += sample * pan[1];
                }
                break;
            }
            case 1: {
                const double bloom = sample_lane(resonatorBloom, start);
                const double feedback = sample_lane(resonatorFeedback, start);
                const double mode = resonatorModes[(event.euclidStep + event.burstIndex) % resonatorModes.size()];
                const double freq = mode * (1.0 + 0.25 * (bloom - 0.5));
                const std::size_t duration = std::min<std::size_t>(
                    frames - start, static_cast<std::size_t>(kSampleRate * (0.35 + 0.9 * feedback)));
                for (std::size_t i = 0; i < duration; ++i) {
                    const double t = static_cast<double>(i) / kSampleRate;
                    double env = std::exp(-t * (0.7 - 0.3 * bloom));
                    env += 0.1 * std::sin(kTwoPi * (0.15 + bloom * 0.1) * t);
                    const double shimmer = std::sin(kTwoPi * freq * t + 0.3 * event.euclidStep);
                    const double golden = std::sin(kTwoPi * freq * 1.618 * t + bloom);
                    const double undertone = std::sin(kTwoPi * freq * 0.5 * t + feedback);
                    const double regen = feedback * std::sin(kTwoPi * freq * 0.25 * t + event.burstIndex);
                    const double sample = (shimmer * (0.6 + 0.2 * bloom) + golden * 0.3 + undertone * 0.2 + regen) *
                                          env * (0.4 + 0.4 * bloom);
                    left[start + i] += sample * pan[0];
                    right[start + i] += sample * pan[1];
                }
                break;
            }
            default: {
                const double density = sample_lane(granularDensity, start);
                const std::size_t grains = std::min<std::size_t>(
                    frames - start, static_cast<std::size_t>(kSampleRate * (0.18 + 0.6 * density)));
                for (std::size_t i = 0; i < grains; ++i) {
                    const double t = static_cast<double>(i) / kSampleRate;
                    const double window = std::sin(std::min(1.0, t / (0.02 + density * 0.08)) * kHalfPi);
                    const double env = window * std::exp(-t * (1.4 - 0.6 * density));
                    const double swirl = std::sin(kTwoPi * (0.3 + 0.25 * density) * t + macro);
                    const double wobble = std::cos(kTwoPi * (80.0 + 140.0 * density) * t + density);
                    const double shimmer = std::sin(kTwoPi * (120.0 + 200.0 * density) * t + event.burstIndex);
                    double sample = (wobble * (0.65 + 0.25 * swirl) + shimmer * 0.35) * env * 0.5;
                    left[start + i] += sample * (pan[0] + 0.15 * swirl);
                    right[start + i] += sample * (pan[1] - 0.15 * swirl);
                }
                break;
            }
        }
    }

    double maxAbs = 0.0;
    for (std::size_t i = 0; i < frames; ++i) {
        maxAbs = std::max(maxAbs, std::max(std::abs(left[i]), std::abs(right[i])));
    }
    const double scale = (maxAbs > 0.0) ? (kNormalizeTarget / maxAbs) : 0.0;

    std::vector<int16_t> samples(frames * 2u);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const double l = left[frame] * scale;
        const double r = right[frame] * scale;
        samples[2u * frame] = static_cast<int16_t>(
            std::clamp<long>(static_cast<long>(std::lround(l * 32767.0)), -32768L, 32767L));
        samples[2u * frame + 1u] = static_cast<int16_t>(
            std::clamp<long>(static_cast<long>(std::lround(r * 32767.0)), -32768L, 32767L));
    }

    std::ostringstream log;
    log << "# engine-hybrid-stack control log" << '\n';
    log << "sample_rate_hz=" << static_cast<int>(kSampleRate)
        << " frames=" << frames << " bpm=" << kBpm << " beats=" << kBeats << '\n';
    log << "euclid_steps=" << static_cast<int>(kEuclidSteps)
        << " fills=" << static_cast<int>(kEuclidFills)
        << " rotate=" << static_cast<int>(kEuclidRotate)
        << " steps_per_beat=" << kStepsPerBeat << '\n';
    log << "burst_cluster=" << static_cast<int>(kBurstCluster)
        << " spacing_samples=" << kBurstSpacing
        << " frames_per_step=" << framesPerStep << '\n';
    log << "euclid_seed=0x" << std::hex << std::nouppercase << euclid.generationSeed()
        << " burst_seed=0x" << burst.generationSeed() << std::dec << '\n';
    log << "events=" << events.size() << '\n';
    log << "index,engine,when_samples,euclid_step,burst_idx,pan,sampler_brightness,sampler_drive,"
           "resonator_bloom,resonator_feedback,granular_density,macro_pan"
        << '\n';
    log << std::fixed << std::setprecision(6);
    for (std::size_t idx = 0; idx < events.size(); ++idx) {
        const auto& event = events[idx];
        const std::size_t frame = std::min<std::size_t>(event.whenSamples, frames - 1);
        const char* engineLabel = (event.engine == 0) ? "sampler"
                                   : (event.engine == 1) ? "resonator"
                                                         : "granular";
        log << idx << ',' << engineLabel << ',' << event.whenSamples << ',' << event.euclidStep << ','
            << event.burstIndex << ',' << clamp01(event.panSeed) << ','
            << sample_lane(samplerBrightness, frame) << ',' << sample_lane(samplerDrive, frame) << ','
            << sample_lane(resonatorBloom, frame) << ',' << sample_lane(resonatorFeedback, frame) << ','
            << sample_lane(granularDensity, frame) << ',' << sample_lane(macroPan, frame) << '\n';
    }
    log << "automation_stride_frames=" << kAutomationStride << '\n';
    log << "frame,sampler_brightness,sampler_drive,resonator_bloom,resonator_feedback,granular_density,macro_pan"
        << '\n';
    for (std::size_t frame = 0; frame < frames; frame += kAutomationStride) {
        log << frame << ',' << samplerBrightness[frame] << ',' << samplerDrive[frame] << ','
            << resonatorBloom[frame] << ',' << resonatorFeedback[frame] << ','
            << granularDensity[frame] << ',' << macroPan[frame] << '\n';
    }
    if ((frames - 1u) % kAutomationStride != 0u) {
        const std::size_t tail = frames - 1u;
        log << tail << ',' << samplerBrightness[tail] << ',' << samplerDrive[tail] << ','
            << resonatorBloom[tail] << ',' << resonatorFeedback[tail] << ','
            << granularDensity[tail] << ',' << macroPan[tail] << '\n';
    }
    log << "normalize=" << kNormalizeTarget << '\n';

    render.samples = std::move(samples);
    render.control_log = log.str();
    return render;
#endif
}

std::vector<int16_t> render_layered_euclid_burst_fixture() {
#if !ENABLE_GOLDEN
    return {};
#else
    constexpr double kSampleRate = 48000.0;
    constexpr int kBpm = 110;
    constexpr int kBeats = 24;
    constexpr double kStepsPerBeat = 4.0;
    constexpr std::uint8_t kEuclidSteps = 13;
    constexpr std::uint8_t kEuclidFills = 7;
    constexpr std::uint8_t kEuclidRotate = 4;
    constexpr std::uint8_t kBurstCluster = 3;
    constexpr std::uint32_t kBurstSpacing = 420;
    constexpr std::size_t kAutomationStride = 192;
    constexpr double kNormalizeTarget = 0.93;

    const std::size_t totalSteps = static_cast<std::size_t>(kBeats * kStepsPerBeat);
    const double framesPerBeat = kSampleRate * (60.0 / static_cast<double>(kBpm));
    const double framesPerStep = framesPerBeat / kStepsPerBeat;
    const std::size_t frames = static_cast<std::size_t>(
        std::lround(totalSteps * framesPerStep + kSampleRate * 3.0));
    if (frames == 0) {
        return {};
    }

    std::vector<double> samplerTone(frames);
    std::vector<double> resonatorColor(frames);
    std::vector<double> granularSpray(frames);
    const double normalization = (frames > 1) ? static_cast<double>(frames - 1u) : 1.0;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const double progress = static_cast<double>(frame) / normalization;
        samplerTone[frame] = 0.22 + 0.78 * 0.5 * (1.0 + std::sin(kTwoPi * (progress * 2.4 + 0.1)));
        resonatorColor[frame] = 0.4 + 0.55 * 0.5 * (1.0 + std::cos(kTwoPi * (progress * 1.3 - 0.25)));
        granularSpray[frame] = 0.18 + 0.82 * 0.5 *
                               (1.0 + std::sin(kTwoPi * (progress * 0.9 + samplerTone[frame] * 0.35)));
    }

    EuclidEngine euclid;
    Engine::PrepareContext euclidPrep{};
    euclidPrep.masterSeed = 0xE0C11Du;
    euclid.prepare(euclidPrep);
    euclid.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kSteps), kEuclidSteps});
    euclid.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kFills), kEuclidFills});
    euclid.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kRotate), kEuclidRotate});

    BurstEngine burst;
    Engine::PrepareContext burstPrep{};
    burstPrep.masterSeed = 0xB0057Du;
    burst.prepare(burstPrep);
    burst.onParam({0, static_cast<std::uint16_t>(BurstEngine::Param::kClusterCount), kBurstCluster});
    burst.onParam({0, static_cast<std::uint16_t>(BurstEngine::Param::kSpacingSamples),
                   static_cast<std::int32_t>(kBurstSpacing)});

    std::vector<LayeredEvent> events;
    events.reserve(totalSteps * kBurstCluster);
    Engine::TickContext tick{};
    Seed burstSeed{};
    std::size_t eventCounter = 0;
    for (std::uint32_t step = 0; step < totalSteps; ++step) {
        tick.tick = step;
        euclid.onTick(tick);
        if (!euclid.lastGate()) {
            continue;
        }
        const std::uint32_t when = static_cast<std::uint32_t>(
            std::lround(static_cast<double>(step) * framesPerStep));
        burstSeed.id = static_cast<std::uint32_t>(0x100 + step);
        burst.onSeed({burstSeed, when});
        const auto& cluster = burst.pendingTriggers();
        for (std::size_t clusterIdx = 0; clusterIdx < cluster.size(); ++clusterIdx) {
            LayeredEvent event{};
            event.whenSamples = std::min(cluster[clusterIdx], static_cast<std::uint32_t>(frames - 1));
            event.euclidStep = step;
            event.burstIndex = static_cast<std::uint32_t>(clusterIdx);
            event.engine = static_cast<int>(eventCounter % 3u);
            const double panSeed =
                0.12 + 0.76 * (static_cast<double>((step * 31 + clusterIdx * 19) % 100) / 100.0);
            event.panSeed = panSeed;
            events.push_back(event);
            ++eventCounter;
        }
    }

    std::vector<double> left(frames, 0.0);
    std::vector<double> right(frames, 0.0);

    const std::array<double, 5> samplerBase = {110.0, 164.81, 196.0, 246.94, 329.63};
    const std::array<double, 5> resonatorModes = {196.0, 246.94, 329.63, 392.0, 523.25};

    for (const auto& event : events) {
        const std::size_t start = std::min<std::size_t>(event.whenSamples, frames - 1);
        const auto pan = make_pan(event.panSeed + 0.15 * (granularSpray[start] - 0.5));
        switch (event.engine) {
            case 0: {
                const double tone = sample_lane(samplerTone, start);
                const double freq = samplerBase[event.euclidStep % samplerBase.size()] *
                                   (1.0 + 0.35 * (tone - 0.5));
                const std::size_t duration = std::min<std::size_t>(
                    frames - start, static_cast<std::size_t>(kSampleRate * (0.08 + tone * 0.35)));
                for (std::size_t i = 0; i < duration; ++i) {
                    const double t = static_cast<double>(i) / kSampleRate;
                    double env = std::exp(-t * (3.5 - 1.2 * tone));
                    env *= (1.0 - std::exp(-t * 32.0));
                    double sample = std::sin(kTwoPi * freq * t + 0.25 * event.burstIndex);
                    sample += 0.25 * tone * std::sin(kTwoPi * freq * 1.97 * t + tone);
                    sample *= env * (0.5 + 0.45 * tone);
                    left[start + i] += sample * pan[0];
                    right[start + i] += sample * pan[1];
                }
                break;
            }
            case 1: {
                const double color = sample_lane(resonatorColor, start);
                const double mode = resonatorModes[(event.euclidStep + event.burstIndex) % resonatorModes.size()];
                const double freq = mode * (1.0 + 0.25 * (color - 0.5));
                const std::size_t duration = std::min<std::size_t>(
                    frames - start, static_cast<std::size_t>(kSampleRate * (0.35 + 1.2 * color)));
                for (std::size_t i = 0; i < duration; ++i) {
                    const double t = static_cast<double>(i) / kSampleRate;
                    double env = std::exp(-t * (0.6 + 0.25 * (1.0 - color)));
                    double modal = std::sin(kTwoPi * freq * t + 0.1 * event.euclidStep);
                    modal += 0.22 * std::sin(kTwoPi * freq * 1.618 * t + color);
                    modal += 0.18 * std::sin(kTwoPi * freq * 0.5 * t + event.burstIndex);
                    const double sample = modal * env * (0.45 + 0.35 * color);
                    left[start + i] += sample * pan[0];
                    right[start + i] += sample * pan[1];
                }
                break;
            }
            default: {
                const double spray = sample_lane(granularSpray, start);
                const double base = 80.0 + 180.0 * spray + 12.0 * static_cast<double>(event.euclidStep % 4);
                const std::size_t grains = std::min<std::size_t>(
                    frames - start, static_cast<std::size_t>(kSampleRate * (0.12 + 0.45 * spray)));
                for (std::size_t i = 0; i < grains; ++i) {
                    const double t = static_cast<double>(i) / kSampleRate;
                    const double window = std::sin(std::min(1.0, t / (0.04 + spray * 0.08)) * kHalfPi);
                    const double env = window * std::exp(-t * (1.5 - 0.4 * spray));
                    const double swirl = std::sin(kTwoPi * (0.35 + 0.4 * spray) * t + event.burstIndex);
                    const double wobble = std::sin(kTwoPi * (base + 25.0 * spray) * t + spray * 4.0);
                    const double shimmer = std::cos(kTwoPi * base * 0.5 * t + 0.5 * swirl);
                    const double leftSample = (wobble * (0.7 + 0.3 * swirl) + shimmer * 0.25) * env * 0.45;
                    const double rightSample = (shimmer * (0.7 - 0.3 * swirl) + wobble * 0.25) * env * 0.45;
                    left[start + i] += leftSample;
                    right[start + i] += rightSample;
                }
                break;
            }
        }
    }

    double maxAbs = 0.0;
    for (std::size_t i = 0; i < frames; ++i) {
        maxAbs = std::max(maxAbs, std::max(std::abs(left[i]), std::abs(right[i])));
    }
    const double scale = (maxAbs > 0.0) ? (kNormalizeTarget / maxAbs) : 0.0;

    std::vector<int16_t> samples(frames * 2u);
    for (std::size_t i = 0; i < frames; ++i) {
        const double l = left[i] * scale;
        const double r = right[i] * scale;
        samples[2u * i] = static_cast<int16_t>(
            std::clamp<long>(static_cast<long>(std::lround(l * 32767.0)), -32768L, 32767L));
        samples[2u * i + 1u] = static_cast<int16_t>(
            std::clamp<long>(static_cast<long>(std::lround(r * 32767.0)), -32768L, 32767L));
    }

    std::ostringstream control;
    control << "# layered-euclid-burst control log" << '\n';
    control << "sample_rate_hz=" << static_cast<int>(kSampleRate)
            << " frames=" << frames << " bpm=" << kBpm << " beats=" << kBeats << '\n';
    control << "euclid_steps=" << static_cast<int>(kEuclidSteps)
            << " fills=" << static_cast<int>(kEuclidFills)
            << " rotate=" << static_cast<int>(kEuclidRotate) << '\n';
    control << "burst_cluster=" << static_cast<int>(kBurstCluster)
            << " spacing_samples=" << kBurstSpacing << '\n';
    control << "euclid_seed=0x" << std::hex << std::nouppercase << euclid.generationSeed()
            << " burst_seed=0x" << burst.generationSeed() << std::dec << '\n';
    control << "events=" << events.size() << '\n';
    control << "index,engine,when_samples,euclid_step,burst_idx,pan,sampler_tone,resonator_color,granular_spray"
            << '\n';
    control << std::fixed << std::setprecision(6);
    for (std::size_t idx = 0; idx < events.size(); ++idx) {
        const auto& event = events[idx];
        const std::size_t frame = std::min<std::size_t>(event.whenSamples, frames - 1);
        const char* engineLabel = (event.engine == 0) ? "sampler"
                                   : (event.engine == 1) ? "resonator"
                                                         : "granular";
        control << idx << ',' << engineLabel << ',' << event.whenSamples << ',' << event.euclidStep
                << ',' << event.burstIndex << ',' << clamp01(event.panSeed) << ','
                << sample_lane(samplerTone, frame) << ',' << sample_lane(resonatorColor, frame) << ','
                << sample_lane(granularSpray, frame) << '\n';
    }
    control << "automation_stride_frames=" << kAutomationStride << '\n';
    control << "frame,sampler_tone,resonator_color,granular_spray" << '\n';
    for (std::size_t frame = 0; frame < frames; frame += kAutomationStride) {
        control << frame << ',' << samplerTone[frame] << ',' << resonatorColor[frame] << ','
                << granularSpray[frame] << '\n';
    }
    if ((frames - 1u) % kAutomationStride != 0u) {
        const std::size_t tail = frames - 1u;
        control << tail << ',' << samplerTone[tail] << ',' << resonatorColor[tail] << ','
                << granularSpray[tail] << '\n';
    }
    control << "normalize=" << kNormalizeTarget << '\n';
    (void)emit_control_log("layered-euclid-burst", control.str());

    return samples;
#endif
}

}  // namespace golden
