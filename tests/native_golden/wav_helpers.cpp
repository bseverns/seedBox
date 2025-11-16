#include "wav_helpers.hpp"

#include <algorithm>
#include <array>
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

#if ENABLE_GOLDEN
constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr double kHalfPi = 1.5707963267948966192313216916398;

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
