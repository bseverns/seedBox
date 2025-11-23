#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <numeric>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>
#include <cstring>

#include <unity.h>

#include "Seed.h"
#include "SeedBoxConfig.h"
#include "engine/BurstEngine.h"
#include "engine/EuclidEngine.h"
#include "engine/Granular.h"
#include "engine/Resonator.h"
#include "engine/Sampler.h"
#include "io/MidiRouter.h"
#include "wav_helpers.hpp"

#include "../../examples/shared/offline_renderer.hpp"
#include "../../examples/shared/reseed_playbook.hpp"
#include "fixtures_autogen.hpp"

#ifndef ENABLE_GOLDEN
#define ENABLE_GOLDEN 0
#endif

namespace {

constexpr std::size_t kDroneFrames = 48000;
constexpr double kSampleRate = 48000.0;
constexpr double kDroneFreqHz = 110.0;
constexpr double kDroneAmplitude = 0.5;
constexpr char kManifestPath[] = "tests/native_golden/golden.json";

using native_golden::FixtureInfo;
using native_golden::kAudioFixtures;
using native_golden::kLogFixtures;

std::filesystem::path find_project_root() {
    if (const char* override = std::getenv("SEEDBOX_PROJECT_ROOT")) {
        if (*override != '\0') {
            return std::filesystem::path(override);
        }
    }

#if defined(SEEDBOX_PROJECT_ROOT_HINT)
    if (SEEDBOX_PROJECT_ROOT_HINT[0] != '\0') {
        std::filesystem::path hinted(SEEDBOX_PROJECT_ROOT_HINT);
        std::error_code ec;
        const auto normalized = std::filesystem::weakly_canonical(hinted, ec);
        const auto& candidate = ec ? hinted : normalized;
        if (std::filesystem::exists(candidate / "platformio.ini")) {
            return candidate;
        }
    }
#endif

    auto cursor = std::filesystem::current_path();
    for (int depth = 0; depth < 10; ++depth) {
        if (std::filesystem::exists(cursor / "platformio.ini")) {
            return cursor;
        }
        if (!cursor.has_parent_path()) {
            break;
        }
        const auto parent = cursor.parent_path();
        if (parent == cursor) {
            break;
        }
        cursor = parent;
    }
    return std::filesystem::current_path();
}

std::filesystem::path fixture_root() {
#if ENABLE_GOLDEN
    if (const char* override = std::getenv("SEEDBOX_FIXTURE_ROOT")) {
        if (*override != '\0') {
            return std::filesystem::path(override);
        }
    }
#endif
    return find_project_root() / "build/fixtures";
}

const FixtureInfo* find_audio_fixture(const char* name) {
    for (const auto& fixture : kAudioFixtures) {
        if (std::strcmp(fixture.name, name) == 0) {
            return &fixture;
        }
    }
    return nullptr;
}

const FixtureInfo* find_log_fixture(const char* name) {
    for (const auto& fixture : kLogFixtures) {
        if (std::strcmp(fixture.name, name) == 0) {
            return &fixture;
        }
    }
    return nullptr;
}

std::filesystem::path fixture_disk_path(const std::string& manifest_path) {
    std::filesystem::path path(manifest_path);
    if (path.is_absolute()) {
        return path;
    }

    const std::string normalized = path.generic_string();
    if (normalized == "build/fixtures") {
        return fixture_root();
    }

    const std::string prefix = "build/fixtures/";
    if (normalized.rfind(prefix, 0) == 0) {
        return fixture_root() / normalized.substr(prefix.size());
    }

    return fixture_root() / path;
}

std::vector<int16_t> make_drone() {
    std::vector<int16_t> samples(kDroneFrames);
    constexpr double kTwoPi = 6.283185307179586476925286766559;
    for (std::size_t i = 0; i < kDroneFrames; ++i) {
        const double t = static_cast<double>(i) / kSampleRate;
        const double cycle = std::sin(kTwoPi * kDroneFreqHz * t);
        const double scaled = std::clamp(cycle * kDroneAmplitude, -1.0, 1.0);
        const auto quantized = static_cast<long>(std::lround(scaled * 32767.0));
        samples[i] = static_cast<int16_t>(std::clamp<long>(quantized, -32768L, 32767L));
    }
    std::ostringstream control;
    control << "# drone-intro control log" << '\n';
    control << "frames=" << kDroneFrames << " sample_rate_hz=" << static_cast<int>(kSampleRate)
            << " freq_hz=" << kDroneFreqHz << " amplitude=" << kDroneAmplitude << '\n';
    (void)emit_control_log_impl("drone-intro", control.str());
    return samples;
}

void setUp(void) {}
void tearDown(void) {}

#if !defined(SEEDBOX_HW)
void test_cli_backend_clock_routing() {
    MidiRouter router;
    router.begin();

    std::array<MidiRouter::RouteConfig, MidiRouter::kPortCount> perf{};
    for (auto& cfg : perf) {
        cfg.acceptControlChange = true;
    }
    const std::size_t usbIndex = static_cast<std::size_t>(MidiRouter::Port::kUsb);
    perf[usbIndex].acceptClock = true;
    router.configurePageRouting(MidiRouter::Page::kPerf, perf);

    bool clockSeen = false;
    router.setClockHandler([&]() { clockSeen = true; });

    auto* usbCli = router.cliBackend(MidiRouter::Port::kUsb);
    usbCli->pushClock();
    router.poll();

    TEST_ASSERT_TRUE(clockSeen);
}

void test_cli_channel_map_and_panic() {
    MidiRouter router;
    router.begin();

    std::array<MidiRouter::RouteConfig, MidiRouter::kPortCount> perf{};
    for (auto& cfg : perf) {
        cfg.acceptControlChange = true;
    }
    router.configurePageRouting(MidiRouter::Page::kPerf, perf);

    MidiRouter::ChannelMap usbMap;
    for (auto& entry : usbMap.inbound) {
        entry = static_cast<std::uint8_t>((entry + 1u) % 16u);
    }
    for (auto& entry : usbMap.outbound) {
        entry = static_cast<std::uint8_t>((entry + 2u) % 16u);
    }
    router.setChannelMap(MidiRouter::Port::kUsb, usbMap);

    std::uint8_t observedChannel = 0xFFu;
    router.setControlChangeHandler(
        [&](std::uint8_t channel, std::uint8_t, std::uint8_t) { observedChannel = channel; });

    auto* usbCli = router.cliBackend(MidiRouter::Port::kUsb);
    usbCli->pushControlChange(0u, 10u, 64u);
    router.poll();
    TEST_ASSERT_EQUAL_UINT8(1u, observedChannel);

    usbCli->clearSent();
    router.sendNoteOn(MidiRouter::Port::kUsb, 0u, 60u, 100u);
    router.sendNoteOff(MidiRouter::Port::kUsb, 0u, 60u, 0u);
    router.sendNoteOff(MidiRouter::Port::kUsb, 0u, 60u, 0u);  // Guarded duplicate.
    router.sendNoteOn(MidiRouter::Port::kUsb, 3u, 67u, 120u);
    router.panic();

    const auto& sent = usbCli->sentMessages();
    TEST_ASSERT_EQUAL_size_t(4u, sent.size());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(MidiRouter::CliBackend::SentMessage::Type::kNoteOn),
                            static_cast<std::uint8_t>(sent[0].type));
    TEST_ASSERT_EQUAL_UINT8(2u, sent[0].channel);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(MidiRouter::CliBackend::SentMessage::Type::kNoteOff),
                            static_cast<std::uint8_t>(sent[1].type));
    TEST_ASSERT_EQUAL_UINT8(2u, sent[1].channel);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(MidiRouter::CliBackend::SentMessage::Type::kNoteOn),
                            static_cast<std::uint8_t>(sent[2].type));
    TEST_ASSERT_EQUAL_UINT8(5u, sent[2].channel);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(MidiRouter::CliBackend::SentMessage::Type::kAllNotesOff),
                            static_cast<std::uint8_t>(sent[3].type));
    TEST_ASSERT_EQUAL_UINT8(5u, sent[3].channel);
}

void test_cli_trs_transport_mirror() {
    MidiRouter router;
    router.begin();

    std::array<MidiRouter::RouteConfig, MidiRouter::kPortCount> perf{};
    const std::size_t usbIndex = static_cast<std::size_t>(MidiRouter::Port::kUsb);
    const std::size_t trsIndex = static_cast<std::size_t>(MidiRouter::Port::kTrsA);
    perf[usbIndex].acceptControlChange = true;
    perf[usbIndex].mirrorTransport = true;
    perf[trsIndex].acceptTransport = true;
    router.configurePageRouting(MidiRouter::Page::kPerf, perf);

    bool startSeen = false;
    router.setStartHandler([&]() { startSeen = true; });

    auto* usbCli = router.cliBackend(MidiRouter::Port::kUsb);
    auto* trsCli = router.cliBackend(MidiRouter::Port::kTrsA);
    usbCli->clearSent();
    trsCli->pushStart();
    router.poll();

    TEST_ASSERT_TRUE(startSeen);
    const auto& sent = usbCli->sentMessages();
    TEST_ASSERT_EQUAL_size_t(1u, sent.size());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(MidiRouter::CliBackend::SentMessage::Type::kStart),
                            static_cast<std::uint8_t>(sent[0].type));
}
#endif

#if ENABLE_GOLDEN
namespace {

struct SamplerVoiceSpec {
    Seed seed;
    std::uint32_t when;
};

std::vector<int16_t> render_reseed_variant(
    std::uint32_t master_seed,
    const char* control_fixture_name = nullptr,
    int bpm = 124,
    int passes = 3,
    const std::vector<reseed::StemDefinition>* stems_override = nullptr);
std::vector<int16_t> render_granular_fixture();
std::vector<int16_t> render_long_random_take_fixture();
bool write_text_file_impl(const std::string& manifest_path, const std::string& body);
bool emit_control_log_impl(const char* fixture_name, const std::string& body);

Seed make_sampler_seed(std::uint32_t id,
                       std::uint8_t sample_idx,
                       float pitch,
                       float attack,
                       float decay,
                       float sustain,
                       float release,
                       float tone,
                       float spread) {
    Seed seed{};
    seed.id = id;
    seed.sampleIdx = sample_idx;
    seed.pitch = pitch;
    seed.envA = attack;
    seed.envD = decay;
    seed.envS = sustain;
    seed.envR = release;
    seed.tone = tone;
    seed.spread = spread;
    seed.engine = static_cast<std::uint8_t>(Engine::Type::kSampler);
    return seed;
}

double sampler_adsr(double t,
                    double attack,
                    double decay,
                    double sustain,
                    double release,
                    double sustain_hold) {
    if (t < 0.0) {
        return 0.0;
    }

    const double kMinStage = 1e-4;
    attack = std::max(attack, kMinStage);
    decay = std::max(decay, kMinStage);
    release = std::max(release, kMinStage);

    if (t < attack) {
        return t / attack;
    }
    t -= attack;
    if (t < decay) {
        const double progress = t / decay;
        return 1.0 + (sustain - 1.0) * progress;
    }
    t -= decay;
    if (t < sustain_hold) {
        return sustain;
    }
    t -= sustain_hold;
    if (t < release) {
        const double progress = t / release;
        return sustain * (1.0 - progress);
    }
    return 0.0;
}

std::vector<int16_t> render_sampler_fixture() {
    Sampler sampler;
    sampler.init();

    const SamplerVoiceSpec voices[] = {
        {make_sampler_seed(1, 0, -5.0f, 0.01f, 0.08f, 0.6f, 0.2f, 0.35f, 0.1f), 0u},
        {make_sampler_seed(2, 3, 2.0f, 0.005f, 0.12f, 0.5f, 0.35f, 0.65f, 0.45f), 8000u},
        {make_sampler_seed(3, 6, 9.0f, 0.02f, 0.18f, 0.4f, 0.5f, 0.8f, 0.85f), 16000u},
    };

    std::ostringstream control;
    control << "# sampler-grains control log" << '\n';
    control << "frames=" << kDroneFrames << " sample_rate_hz=" << static_cast<int>(kSampleRate)
            << " voices=" << std::size(voices) << '\n';
    control << "voice,when_samples,id,sample,pitch,envA,envD,envS,envR,tone,spread" << '\n';
    control << std::fixed << std::setprecision(4);

    for (std::size_t idx = 0; idx < std::size(voices); ++idx) {
        const auto& spec = voices[idx];
        sampler.trigger(spec.seed, spec.when);
        control << idx << ',' << spec.when << ',' << spec.seed.id << ','
                << static_cast<int>(spec.seed.sampleIdx) << ',' << spec.seed.pitch << ','
                << spec.seed.envA << ',' << spec.seed.envD << ',' << spec.seed.envS << ','
                << spec.seed.envR << ',' << spec.seed.tone << ',' << spec.seed.spread << '\n';
    }
    (void)emit_control_log_impl("sampler-grains", control.str());

    std::vector<double> mix(kDroneFrames, 0.0);
    const double sustain_hold = 0.25;
    const double baseFrequencies[] = {110.0, 164.81, 220.0, 261.63, 329.63, 392.0, 523.25};

    for (std::uint8_t i = 0; i < Sampler::kMaxVoices; ++i) {
        const auto voice = sampler.voice(i);
        if (!voice.active) {
            continue;
        }
        const double freq_base = baseFrequencies[voice.sampleIndex % std::size(baseFrequencies)];
        const double freq = freq_base * static_cast<double>(voice.playbackRate);
        const double pan_gain = 0.5 * (static_cast<double>(voice.leftGain) + static_cast<double>(voice.rightGain));
        for (std::size_t frame = 0; frame < mix.size(); ++frame) {
            if (frame < voice.startSample) {
                continue;
            }
            const double t = (static_cast<double>(frame - voice.startSample)) / kSampleRate;
            const double env = sampler_adsr(t,
                                            static_cast<double>(voice.envelope.attack),
                                            static_cast<double>(voice.envelope.decay),
                                            static_cast<double>(voice.envelope.sustain),
                                            static_cast<double>(voice.envelope.release),
                                            sustain_hold);
            if (env <= 0.0) {
                continue;
            }
            const double tone_blend = static_cast<double>(voice.tone);
            const double fundamental = std::sin(2.0 * M_PI * freq * t);
            const double harmonic = std::sin(2.0 * M_PI * freq * 2.03 * t);
            double sample = (1.0 - tone_blend) * fundamental + tone_blend * harmonic;
            if (voice.usesSdStreaming) {
                const double grit = std::sin(2.0 * M_PI * freq * 0.125 * t);
                sample = (sample * 0.9) + (grit * 0.1);
            }
            mix[frame] += sample * env * pan_gain;
        }
    }

    double max_abs = 0.0;
    for (double v : mix) {
        max_abs = std::max(max_abs, std::abs(v));
    }
    const double scale = (max_abs > 0.0) ? (0.92 / max_abs) : 0.0;

    std::vector<int16_t> samples(mix.size());
    for (std::size_t i = 0; i < mix.size(); ++i) {
        const double scaled = mix[i] * scale;
        samples[i] = static_cast<int16_t>(std::clamp<std::int32_t>(
            static_cast<std::int32_t>(std::lround(scaled * 32767.0)), -32768, 32767));
    }
    return samples;
}

std::vector<int16_t> render_modulated_sampler_fixture() {
    const auto sampler = render_sampler_fixture();
    const auto granular = render_granular_fixture();

    const std::size_t sampler_frames = sampler.size();
    const std::size_t granular_frames = granular.size() / 2u;
    const std::size_t frames = std::min({sampler_frames, granular_frames, kDroneFrames});

    if (frames == 0) {
        return {};
    }

    constexpr double sampler_scale = 1.0 / 32768.0;
    constexpr double granular_scale = 1.0 / 32768.0;
    constexpr std::size_t kAutomationStride = 128;

    std::vector<double> tone(frames);
    std::vector<double> spread(frames);
    std::vector<double> grain_lfo(frames);
    std::vector<double> left(frames);
    std::vector<double> right(frames);

    double last_sampler = 0.0;
    double max_abs = 0.0;

    const double normalization = (frames > 1) ? static_cast<double>(frames - 1u) : 1.0;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const double normalized = static_cast<double>(frame) / normalization;
        const double tone_phase = normalized * 3.0;
        const double spread_phase = normalized * 2.0 + 0.25;
        const double lfo_phase = normalized * 0.5;

        tone[frame] = 0.15 + 0.85 * 0.5 * (1.0 + std::sin(2.0 * M_PI * tone_phase));
        spread[frame] = 0.05 + 0.95 * 0.5 * (1.0 + std::cos(2.0 * M_PI * spread_phase));
        grain_lfo[frame] = 0.25 + 0.75 * 0.5 *
                           (1.0 + std::sin(2.0 * M_PI * (lfo_phase + spread[frame] * 0.35)));

        const double sampler_sample = static_cast<double>(sampler[frame]) * sampler_scale;
        const double bright = sampler_sample - last_sampler;
        last_sampler = sampler_sample;
        const double shaped = (1.0 - tone[frame]) * sampler_sample + tone[frame] * bright;

        const double pan_angle = spread[frame] * (M_PI * 0.5);
        const double pan_l = std::cos(pan_angle);
        const double pan_r = std::sin(pan_angle);

        const double grain_l = (frame < granular_frames)
                                   ? static_cast<double>(granular[2u * frame]) * granular_scale
                                   : 0.0;
        const double grain_r = (frame < granular_frames)
                                   ? static_cast<double>(granular[2u * frame + 1u]) * granular_scale
                                   : 0.0;
        const double swirl = grain_lfo[frame];
        const double swirl_l = grain_l * swirl;
        const double swirl_r = grain_r * (1.0 - 0.5 * swirl);

        left[frame] = shaped * pan_l + swirl_l;
        right[frame] = shaped * pan_r + swirl_r;

        max_abs = std::max({max_abs, std::abs(left[frame]), std::abs(right[frame])});
    }

    const double scale = (max_abs > 0.0) ? (0.92 / max_abs) : 0.0;
    std::vector<int16_t> samples(frames * 2u);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const double l = left[frame] * scale;
        const double r = right[frame] * scale;
        samples[2u * frame] = static_cast<int16_t>(
            std::clamp<long>(static_cast<long>(std::lround(l * 32767.0)), -32768L, 32767L));
        samples[2u * frame + 1u] = static_cast<int16_t>(
            std::clamp<long>(static_cast<long>(std::lround(r * 32767.0)), -32768L, 32767L));
    }

    std::ostringstream control;
    control << "# modulated-sampler control log" << '\n';
    control << "frames=" << frames << " sample_rate_hz=" << static_cast<int>(kSampleRate) << '\n';
    control << "automation_stride_frames=" << kAutomationStride << '\n';
    control << "frame,tone,spread,grain_lfo" << '\n';
    control << std::fixed << std::setprecision(6);
    for (std::size_t frame = 0; frame < frames; frame += kAutomationStride) {
        control << frame << ',' << tone[frame] << ',' << spread[frame] << ',' << grain_lfo[frame]
                << '\n';
    }
    if ((frames - 1u) % kAutomationStride != 0u) {
        const std::size_t tail = frames - 1u;
        control << tail << ',' << tone[tail] << ',' << spread[tail] << ',' << grain_lfo[tail]
                << '\n';
    }
    control << "normalize=0.92" << '\n';
    (void)emit_control_log_impl("modulated-sampler", control.str());

    return samples;
}

struct ResonatorVoiceSpec {
    Seed seed;
    std::uint32_t when;
};

Seed make_resonator_seed(std::uint8_t id,
                         float pitch,
                         float excite_ms,
                         float damping,
                         float brightness,
                         float feedback,
                         std::uint8_t mode,
                         std::uint8_t bank) {
    Seed seed{};
    seed.id = id;
    seed.pitch = pitch;
    seed.resonator.exciteMs = excite_ms;
    seed.resonator.damping = damping;
    seed.resonator.brightness = brightness;
    seed.resonator.feedback = feedback;
    seed.resonator.mode = mode;
    seed.resonator.bank = bank;
    seed.engine = static_cast<std::uint8_t>(Engine::Type::kResonator);
    return seed;
}

double modal_envelope(double t, double damping, double feedback) {
    const double sustain = std::clamp(0.35 + 0.45 * feedback, 0.1, 0.95);
    const double decay_hz = 0.75 + (1.5 - damping) * 1.75;
    return sustain * std::exp(-t * decay_hz);
}

std::vector<int16_t> render_resonator_fixture() {
    ResonatorBank bank;
    bank.init(ResonatorBank::Mode::kSim);
    bank.setMaxVoices(4);

    const ResonatorVoiceSpec specs[] = {
        {make_resonator_seed(10, -12.0f, 12.0f, 0.35f, 0.55f, 0.72f, 1, 0), 0u},
        {make_resonator_seed(11, -2.0f, 8.0f, 0.62f, 0.8f, 0.85f, 1, 2), 6000u},
        {make_resonator_seed(12, 5.0f, 5.0f, 0.48f, 0.45f, 0.6f, 0, 4), 12000u},
    };

    std::ostringstream control;
    control << "# resonator-tail control log" << '\n';
    control << "frames=" << kDroneFrames << " sample_rate_hz=" << static_cast<int>(kSampleRate)
            << " voices=" << std::size(specs) << '\n';
    control << "voice,when_samples,id,pitch,excite_ms,damping,brightness,feedback,mode,bank" << '\n';
    control << std::fixed << std::setprecision(4);

    for (std::size_t idx = 0; idx < std::size(specs); ++idx) {
        const auto& spec = specs[idx];
        bank.trigger(spec.seed, spec.when);
        control << idx << ',' << spec.when << ',' << spec.seed.id << ',' << spec.seed.pitch << ','
                << spec.seed.resonator.exciteMs << ',' << spec.seed.resonator.damping << ','
                << spec.seed.resonator.brightness << ',' << spec.seed.resonator.feedback << ','
                << static_cast<int>(spec.seed.resonator.mode) << ','
                << static_cast<int>(spec.seed.resonator.bank) << '\n';
    }
    (void)emit_control_log_impl("resonator-tail", control.str());

    std::vector<double> mix(kDroneFrames, 0.0);
    for (std::uint8_t i = 0; i < ResonatorBank::kMaxVoices; ++i) {
        const auto voice = bank.voice(i);
        if (!voice.active) {
            continue;
        }
        const double burst = static_cast<double>(voice.burstGain);
        const double damping = static_cast<double>(voice.damping);
        const double feedback = static_cast<double>(voice.feedback);
        for (std::size_t frame = 0; frame < mix.size(); ++frame) {
            if (frame < voice.startSample) {
                continue;
            }
            const double t = (static_cast<double>(frame - voice.startSample)) / kSampleRate;
            const double envelope = modal_envelope(t, damping, feedback);
            if (envelope < 1e-6) {
                continue;
            }
            const double excite = std::exp(-std::max(0.0, t - static_cast<double>(voice.burstMs) / 1000.0) * 6.5);
            double sample = 0.0;
            for (std::size_t mode = 0; mode < voice.modalFrequencies.size(); ++mode) {
                const double freq = static_cast<double>(voice.modalFrequencies[mode]);
                const double gain = static_cast<double>(voice.modalGains[mode]);
                if (gain <= 0.0 || freq <= 0.0) {
                    continue;
                }
                sample += gain * std::sin(2.0 * M_PI * freq * t);
            }
            sample += 0.35 * std::sin(2.0 * M_PI * static_cast<double>(voice.frequency) * t);
            mix[frame] += sample * burst * envelope * (0.5 + 0.5 * excite);
        }
    }

    double max_abs = 0.0;
    for (double v : mix) {
        max_abs = std::max(max_abs, std::abs(v));
    }
    const double scale = (max_abs > 0.0) ? (0.9 / max_abs) : 0.0;

    std::vector<int16_t> samples(mix.size());
    for (std::size_t i = 0; i < mix.size(); ++i) {
        const double scaled = mix[i] * scale;
        samples[i] = static_cast<int16_t>(std::clamp<std::int32_t>(
            static_cast<std::int32_t>(std::lround(scaled * 32767.0)), -32768, 32767));
    }
    return samples;
}

struct GranularVoiceSpec {
    Seed seed;
    std::uint32_t when;
    double carrier_hz;
    double drift_amount;
};

Seed make_granular_seed(std::uint32_t id,
                        std::uint32_t prng,
                        float pitch,
                        float transpose,
                        float grain_ms,
                        float spray_ms,
                        float window_skew,
                        float spread,
                        GranularEngine::Source source,
                        std::uint8_t sd_slot) {
    Seed seed{};
    seed.id = id;
    seed.prng = prng;
    seed.pitch = pitch;
    seed.granular.transpose = transpose;
    seed.granular.grainSizeMs = grain_ms;
    seed.granular.sprayMs = spray_ms;
    seed.granular.windowSkew = window_skew;
    seed.granular.stereoSpread = spread;
    seed.granular.source = static_cast<std::uint8_t>(source);
    seed.granular.sdSlot = sd_slot;
    seed.engine = static_cast<std::uint8_t>(Engine::Type::kGranular);
    return seed;
}

double granular_window_shape(double normalized, double skew) {
    normalized = std::clamp(normalized, 0.0, 1.0);
    const double base = 0.5 - 0.5 * std::cos(normalized * M_PI);
    const double bias = std::clamp(skew, -1.0, 1.0);
    const double exponent = 1.0 + bias * 1.5;
    return std::pow(base, exponent);
}

double random_signed(uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    const double scaled = static_cast<double>((state >> 8u) & 0x00FFFFFFu) / static_cast<double>(0x01000000u);
    return (scaled * 2.0) - 1.0;
}

std::vector<int16_t> render_granular_fixture() {
    GranularEngine engine;
    engine.init(GranularEngine::Mode::kSim);
    engine.setMaxActiveVoices(6);
    engine.armLiveInput(true);
    engine.registerSdClip(1, "clip-a.wav");
    engine.registerSdClip(2, "clip-b.wav");

    const GranularVoiceSpec specs[] = {
        {make_granular_seed(40, 0x1a2b3c4du, -5.0f, 0.0f, 95.0f, 24.0f, -0.45f, 0.35f,
                             GranularEngine::Source::kLiveInput, 0u),
         4000u, 146.83, 0.12},
        {make_granular_seed(41, 0x77773311u, 0.0f, 7.0f, 130.0f, 18.0f, 0.25f, 0.8f,
                             GranularEngine::Source::kSdClip, 1u),
         12000u, 196.0, 0.2},
        {make_granular_seed(42, 0x31337fffu, 7.0f, -5.0f, 80.0f, 12.0f, 0.65f, 0.6f,
                             GranularEngine::Source::kSdClip, 2u),
         20000u, 246.94, 0.08},
    };

    auto granular_source_label = [](GranularEngine::Source source) {
        switch (source) {
            case GranularEngine::Source::kLiveInput:
                return "live";
            case GranularEngine::Source::kSdClip:
                return "sd";
            default:
                return "unknown";
        }
    };

    std::ostringstream control;
    control << "# granular-haze control log" << '\n';
    control << "frames=" << kDroneFrames << " sample_rate_hz=" << static_cast<int>(kSampleRate)
            << " voices=" << std::size(specs) << '\n';
    control << "voice,when_samples,id,pitch,transpose,grain_ms,spray_ms,window_skew,spread,source,sd_slot,carrier_hz,drift" << '\n';
    control << std::fixed << std::setprecision(4);

    for (std::size_t idx = 0; idx < std::size(specs); ++idx) {
        const auto& spec = specs[idx];
        engine.trigger(spec.seed, spec.when);
        control << idx << ',' << spec.when << ',' << spec.seed.id << ',' << spec.seed.pitch << ','
                << spec.seed.granular.transpose << ',' << spec.seed.granular.grainSizeMs << ','
                << spec.seed.granular.sprayMs << ',' << spec.seed.granular.windowSkew << ','
                << spec.seed.granular.stereoSpread << ','
                << granular_source_label(static_cast<GranularEngine::Source>(spec.seed.granular.source))
                << ',' << static_cast<int>(spec.seed.granular.sdSlot) << ',' << specs[idx].carrier_hz
                << ',' << specs[idx].drift_amount << '\n';
    }
    (void)emit_control_log_impl("granular-haze", control.str());

    std::vector<double> left(kDroneFrames, 0.0);
    std::vector<double> right(kDroneFrames, 0.0);

    for (std::size_t i = 0; i < std::size(specs); ++i) {
        const auto voice = engine.voice(static_cast<std::uint8_t>(i));
        if (!voice.active) {
            continue;
        }
        uint32_t rng = voice.seedPrng;
        const double base_freq = specs[i].carrier_hz * static_cast<double>(voice.playbackRate);
        const double grain_length = std::max(0.04, static_cast<double>(voice.sizeMs) / 1000.0);
        const double sustain = grain_length * 2.5;
        const double spread = static_cast<double>(voice.stereoSpread);
        for (std::size_t frame = voice.startSample;
             frame < left.size() && frame < voice.startSample + static_cast<std::size_t>((sustain + grain_length) * kSampleRate);
             ++frame) {
            const double t = (static_cast<double>(frame) - static_cast<double>(voice.startSample)) / kSampleRate;
            const double normalized = t / grain_length;
            if (normalized > 4.0) {
                break;
            }
            double envelope = granular_window_shape(normalized, static_cast<double>(voice.windowSkew));
            const double tail = std::exp(-t * (1.2 + 0.6 * (1.0 - spread)));
            envelope *= tail;
            if (envelope < 1e-5) {
                continue;
            }

            const double lfo = std::sin(2.0 * M_PI * (0.35 + 0.1 * spread) * t);
            const double wobble = 1.0 + 0.015 * lfo;
            const double jitter = specs[i].drift_amount * random_signed(rng);
            const double freq = base_freq * wobble * (1.0 + 0.01 * jitter);
            const double phase = 2.0 * M_PI * freq * t;
            double sample = std::sin(phase);
            sample += 0.45 * std::sin(phase * 1.62 + 0.3 * jitter);
            if (voice.source == GranularEngine::Source::kSdClip) {
                sample += 0.25 * std::sin(phase * 0.5 + random_signed(rng) * 0.4);
            } else {
                sample += 0.2 * std::sin(phase * 2.0 + lfo);
            }
            sample *= envelope * (0.75 + 0.25 * spread);
            left[frame] += sample * static_cast<double>(voice.leftGain);
            right[frame] += sample * static_cast<double>(voice.rightGain);
        }
    }

    double max_abs = 0.0;
    for (std::size_t i = 0; i < left.size(); ++i) {
        max_abs = std::max(max_abs, std::max(std::abs(left[i]), std::abs(right[i])));
    }
    const double scale = (max_abs > 0.0) ? (0.9 / max_abs) : 0.0;

    std::vector<int16_t> samples(left.size() * 2u);
    for (std::size_t i = 0; i < left.size(); ++i) {
        const double l = left[i] * scale;
        const double r = right[i] * scale;
        samples[2u * i] = static_cast<int16_t>(std::clamp<long>(static_cast<long>(std::lround(l * 32767.0)), -32768, 32767));
        samples[2u * i + 1u] = static_cast<int16_t>(
            std::clamp<long>(static_cast<long>(std::lround(r * 32767.0)), -32768, 32767));
    }
    return samples;
}

std::vector<int16_t> render_mixer_fixture() {
    const auto drone = make_drone();
    const auto sampler = render_sampler_fixture();
    const auto resonator = render_resonator_fixture();
    const auto granular = render_granular_fixture();

    const std::size_t frames = kDroneFrames;
    std::vector<double> left(frames, 0.0);
    std::vector<double> right(frames, 0.0);

    auto accumulate_mono = [&](const std::vector<int16_t>& mono, double left_gain, double right_gain) {
        const double scale = 1.0 / 32768.0;
        for (std::size_t i = 0; i < frames && i < mono.size(); ++i) {
            const double sample = static_cast<double>(mono[i]) * scale;
            left[i] += sample * left_gain;
            right[i] += sample * right_gain;
        }
    };

    std::ostringstream control;
    control << "# mixer-console control log" << '\n';
    control << "frames=" << frames << " sample_rate_hz=" << static_cast<int>(kSampleRate) << '\n';
    control << "mono_sources:" << '\n';

    accumulate_mono(drone, 0.5, 0.5);
    control << "  drone-intro -> L=0.50 R=0.50" << '\n';
    accumulate_mono(sampler, 0.35, 0.55);
    control << "  sampler-grains -> L=0.35 R=0.55" << '\n';
    accumulate_mono(resonator, 0.4, 0.3);
    control << "  resonator-tail -> L=0.40 R=0.30" << '\n';

    const double granular_scale = 1.0 / 32768.0;
    for (std::size_t frame = 0; frame < frames && (frame * 2u + 1u) < granular.size(); ++frame) {
        const double l = static_cast<double>(granular[2u * frame]) * granular_scale;
        const double r = static_cast<double>(granular[2u * frame + 1u]) * granular_scale;
        left[frame] += l * 0.65;
        right[frame] += r * 0.65;
    }
    control << "stereo_sources:" << '\n';
    control << "  granular-haze -> gain=0.65/0.65" << '\n';

    for (std::size_t i = 0; i < frames; ++i) {
        const double cross = 0.12 * (left[i] - right[i]);
        left[i] -= cross;
        right[i] += cross;
        left[i] = std::tanh(left[i] * 1.1);
        right[i] = std::tanh(right[i] * 1.1);
    }
    control << "bus_fx: crossfeed=0.12 saturate=1.10" << '\n';

    double max_abs = 0.0;
    for (std::size_t i = 0; i < frames; ++i) {
        max_abs = std::max(max_abs, std::max(std::abs(left[i]), std::abs(right[i])));
    }
    const double scale = (max_abs > 0.0) ? (0.92 / max_abs) : 0.0;

    std::vector<int16_t> samples(frames * 2u);
    for (std::size_t i = 0; i < frames; ++i) {
        const double l = left[i] * scale;
        const double r = right[i] * scale;
        samples[2u * i] = static_cast<int16_t>(std::clamp<long>(static_cast<long>(std::lround(l * 32767.0)), -32768, 32767));
        samples[2u * i + 1u] = static_cast<int16_t>(
            std::clamp<long>(static_cast<long>(std::lround(r * 32767.0)), -32768, 32767));
    }
    control << "normalize=0.92" << '\n';
    (void)emit_control_log_impl("mixer-console", control.str());
    return samples;
}

std::vector<int16_t> render_quadraphonic_fixture() {
    const auto mixer = render_mixer_fixture();
    const auto granular = render_granular_fixture();
    const auto sampler = render_sampler_fixture();
    const auto resonator = render_resonator_fixture();
    const auto drone = make_drone();
    const auto reseed_a = render_reseed_variant(0xCAFEu);
    const auto reseed_b = render_reseed_variant(0xBEEFu);

    constexpr std::size_t kChannels = 4u;
    const std::size_t frames = kDroneFrames;
    std::array<std::vector<double>, kChannels> bus{};
    for (auto& lane : bus) {
        lane.assign(frames, 0.0);
    }

    auto accumulate_mono = [&](const std::vector<int16_t>& mono, const std::array<double, kChannels>& gains) {
        const double scale = 1.0 / 32768.0;
        const std::size_t limit = std::min(frames, mono.size());
        for (std::size_t i = 0; i < limit; ++i) {
            const double sample = static_cast<double>(mono[i]) * scale;
            for (std::size_t ch = 0; ch < kChannels; ++ch) {
                bus[ch][i] += sample * gains[ch];
            }
        }
    };

    auto accumulate_stereo = [&](const std::vector<int16_t>& stereo,
                                 const std::array<double, kChannels>& left_gains,
                                 const std::array<double, kChannels>& right_gains) {
        const double scale = 1.0 / 32768.0;
        const std::size_t sample_count = stereo.size() / 2u;
        const std::size_t limit = std::min(frames, sample_count);
        for (std::size_t i = 0; i < limit; ++i) {
            const double left = static_cast<double>(stereo[2u * i]) * scale;
            const double right = static_cast<double>(stereo[2u * i + 1u]) * scale;
            for (std::size_t ch = 0; ch < kChannels; ++ch) {
                bus[ch][i] += left * left_gains[ch];
                bus[ch][i] += right * right_gains[ch];
            }
        }
    };

    std::ostringstream control;
    control << "# quad-bus control log" << '\n';
    control << "frames=" << frames << " sample_rate_hz=" << static_cast<int>(kSampleRate) << '\n';
    control << "channel_order=[frontL,frontR,rearL,rearR]" << '\n';
    control << "mono routing:" << '\n';
    control << "  drone-intro -> [0.25,0.25,0.10,0.10]" << '\n';
    control << "  sampler-grains -> [0.20,0.35,0.30,0.18]" << '\n';
    control << "  resonator-tail -> [0.32,0.22,0.14,0.36]" << '\n';
    control << "  reseed-A -> [0.30,0.18,0.42,0.24]" << '\n';
    control << "  reseed-B -> [0.18,0.30,0.24,0.46]" << '\n';
    control << "stereo routing:" << '\n';
    control << "  granular-haze -> L[0.28,0.10,0.26,-0.18] R[0.10,0.28,-0.18,0.26]" << '\n';
    control << "  mixer-console -> L[0.48,0.16,0.20,0.20] R[0.16,0.48,0.20,0.20]" << '\n';

    accumulate_mono(drone, {0.25, 0.25, 0.10, 0.10});
    accumulate_mono(sampler, {0.20, 0.35, 0.30, 0.18});
    accumulate_mono(resonator, {0.32, 0.22, 0.14, 0.36});
    accumulate_stereo(granular, {0.28, 0.10, 0.26, -0.18}, {0.10, 0.28, -0.18, 0.26});
    accumulate_stereo(mixer, {0.48, 0.16, 0.20, 0.20}, {0.16, 0.48, 0.20, 0.20});
    accumulate_mono(reseed_a, {0.30, 0.18, 0.42, 0.24});
    accumulate_mono(reseed_b, {0.18, 0.30, 0.24, 0.46});

    for (std::size_t i = 0; i < frames; ++i) {
        const double front_mid = 0.5 * (bus[0][i] + bus[1][i]);
        const double front_side = bus[0][i] - bus[1][i];
        const double rear_mid = 0.5 * (bus[2][i] + bus[3][i]);

        bus[2][i] += front_side * 0.55;
        bus[3][i] -= front_side * 0.55;
        const double tilt = 0.12 * (rear_mid - front_mid);
        bus[0][i] -= tilt;
        bus[1][i] += tilt;

        for (std::size_t ch = 0; ch < kChannels; ++ch) {
            bus[ch][i] = std::tanh(bus[ch][i] * 1.08);
        }
    }

    double max_abs = 0.0;
    for (std::size_t ch = 0; ch < kChannels; ++ch) {
        for (double sample : bus[ch]) {
            max_abs = std::max(max_abs, std::abs(sample));
        }
    }
    const double scale = (max_abs > 0.0) ? (0.92 / max_abs) : 0.0;

    std::vector<int16_t> samples(frames * kChannels);
    for (std::size_t i = 0; i < frames; ++i) {
        for (std::size_t ch = 0; ch < kChannels; ++ch) {
            const double value = bus[ch][i] * scale;
            const auto quantized = static_cast<long>(std::lround(value * 32767.0));
            samples[kChannels * i + ch] = static_cast<int16_t>(
                std::clamp<long>(quantized, -32768L, 32767L));
        }
    }
    control << "bus_fx:" << '\n';
    control << "  front_side_feed=0.55" << '\n';
    control << "  tilt=0.12" << '\n';
    control << "  saturate=1.08" << '\n';
    control << "normalize=0.92" << '\n';
    (void)emit_control_log_impl("quad-bus", control.str());
    return samples;
}

std::vector<int16_t> render_surround_fixture() {
    const auto mixer = render_mixer_fixture();
    const auto granular = render_granular_fixture();
    const auto sampler = render_sampler_fixture();
    const auto resonator = render_resonator_fixture();
    const auto drone = make_drone();
    const auto reseed_a = render_reseed_variant(0xCAFEu);
    const auto reseed_b = render_reseed_variant(0xBEEFu);

    constexpr std::size_t kChannels = 6u;
    const std::size_t frames = kDroneFrames;
    std::array<std::vector<double>, kChannels> bus{};
    for (auto& lane : bus) {
        lane.assign(frames, 0.0);
    }

    auto accumulate_mono = [&](const std::vector<int16_t>& mono, const std::array<double, kChannels>& gains) {
        const double scale = 1.0 / 32768.0;
        const std::size_t limit = std::min(frames, mono.size());
        for (std::size_t i = 0; i < limit; ++i) {
            const double sample = static_cast<double>(mono[i]) * scale;
            for (std::size_t ch = 0; ch < kChannels; ++ch) {
                bus[ch][i] += sample * gains[ch];
            }
        }
    };

    auto accumulate_stereo = [&](const std::vector<int16_t>& stereo,
                                 const std::array<double, kChannels>& left_gains,
                                 const std::array<double, kChannels>& right_gains) {
        const double scale = 1.0 / 32768.0;
        const std::size_t sample_count = stereo.size() / 2u;
        const std::size_t limit = std::min(frames, sample_count);
        for (std::size_t i = 0; i < limit; ++i) {
            const double left = static_cast<double>(stereo[2u * i]) * scale;
            const double right = static_cast<double>(stereo[2u * i + 1u]) * scale;
            for (std::size_t ch = 0; ch < kChannels; ++ch) {
                bus[ch][i] += left * left_gains[ch];
                bus[ch][i] += right * right_gains[ch];
            }
        }
    };

    std::ostringstream control;
    control << "# surround-bus control log" << '\n';
    control << "frames=" << frames << " sample_rate_hz=" << static_cast<int>(kSampleRate) << '\n';
    control << "channel_order=[L,R,C,LFE,SL,SR]" << '\n';
    control << "mono routing:" << '\n';
    control << "  drone-intro -> [0.20,0.20,0.35,0.40,0.05,0.05]" << '\n';
    control << "  sampler-grains -> [0.32,0.42,0.28,0.10,0.18,0.24]" << '\n';
    control << "  resonator-tail -> [0.24,0.18,0.15,0.08,0.32,0.28]" << '\n';
    control << "  reseed-A -> [0.36,0.22,0.12,0.05,0.34,0.26]" << '\n';
    control << "  reseed-B -> [0.22,0.36,0.12,0.05,0.26,0.34]" << '\n';
    control << "stereo routing:" << '\n';
    control << "  granular-haze -> L[0.40,0.10,0.18,0.08,0.30,-0.08] R[0.10,0.40,0.18,0.08,-0.08,0.30]" << '\n';
    control << "  mixer-console -> L[0.55,0.20,0.12,0.05,0.16,0.06] R[0.20,0.55,0.12,0.05,0.06,0.16]" << '\n';

    accumulate_mono(drone, {0.20, 0.20, 0.35, 0.40, 0.05, 0.05});
    accumulate_mono(sampler, {0.32, 0.42, 0.28, 0.10, 0.18, 0.24});
    accumulate_mono(resonator, {0.24, 0.18, 0.15, 0.08, 0.32, 0.28});
    accumulate_stereo(granular, {0.40, 0.10, 0.18, 0.08, 0.30, -0.08}, {0.10, 0.40, 0.18, 0.08, -0.08, 0.30});
    accumulate_stereo(mixer, {0.55, 0.20, 0.12, 0.05, 0.16, 0.06}, {0.20, 0.55, 0.12, 0.05, 0.06, 0.16});
    accumulate_mono(reseed_a, {0.36, 0.22, 0.12, 0.05, 0.34, 0.26});
    accumulate_mono(reseed_b, {0.22, 0.36, 0.12, 0.05, 0.26, 0.34});

    control << "mid_side_fx: front->C+LFE ms=[0.65,0.35] surrounds_sidefeed=0.30" << '\n';
    control << "rear_glue: crossfeed=0.18 saturate=1.06" << '\n';

    for (std::size_t i = 0; i < frames; ++i) {
        const double front_mid = 0.5 * (bus[0][i] + bus[1][i]);
        const double front_side = 0.5 * (bus[0][i] - bus[1][i]);
        const double surround_mid = 0.5 * (bus[4][i] + bus[5][i]);
        const double surround_side = 0.5 * (bus[4][i] - bus[5][i]);

        bus[2][i] += front_mid * 0.65;  // center eats the mid component
        bus[3][i] += 0.35 * front_mid + 0.25 * std::abs(front_side);  // sub hugs mid w/ side energy

        bus[4][i] = surround_mid + surround_side + front_side * 0.30;
        bus[5][i] = surround_mid - surround_side - front_side * 0.30;

        const double rear_cross = 0.18 * (bus[4][i] - bus[5][i]);
        bus[4][i] -= rear_cross;
        bus[5][i] += rear_cross;

        for (std::size_t ch = 0; ch < kChannels; ++ch) {
            bus[ch][i] = std::tanh(bus[ch][i] * 1.06);
        }
    }

    double max_abs = 0.0;
    for (std::size_t ch = 0; ch < kChannels; ++ch) {
        for (double sample : bus[ch]) {
            max_abs = std::max(max_abs, std::abs(sample));
        }
    }
    const double scale = (max_abs > 0.0) ? (0.92 / max_abs) : 0.0;

    std::vector<int16_t> samples(frames * kChannels);
    for (std::size_t i = 0; i < frames; ++i) {
        for (std::size_t ch = 0; ch < kChannels; ++ch) {
            const double value = bus[ch][i] * scale;
            const auto quantized = static_cast<long>(std::lround(value * 32767.0));
            samples[kChannels * i + ch] = static_cast<int16_t>(
                std::clamp<long>(quantized, -32768L, 32767L));
        }
    }
    control << "normalize=0.92" << '\n';
    (void)emit_control_log_impl("surround-bus", control.str());
    return samples;
}

std::vector<int16_t> render_reseed_variant(
    std::uint32_t master_seed,
    const char* control_fixture_name,
    int bpm,
    int passes,
    const std::vector<reseed::StemDefinition>* stems_override) {
    const std::vector<reseed::StemDefinition>* stems = stems_override;
    if (stems == nullptr) {
        stems = &reseed::defaultStems();
    }
    const auto plan = reseed::makeBouncePlan(*stems, master_seed, kSampleRate, bpm, passes);
    offline::OfflineRenderer renderer({kSampleRate, plan.framesHint});
    renderer.mixSamplerEvents(plan.samplerEvents);
    renderer.mixResonatorEvents(plan.resonatorEvents);
    const auto& pcm = renderer.finalize();

    if (control_fixture_name != nullptr) {
        std::ostringstream control;
        control << "# " << control_fixture_name << " control log" << '\n';
        control << "master_seed=0x" << std::hex << std::nouppercase << master_seed << std::dec
                << " bpm=" << bpm << " passes=" << passes << '\n';
        control << "event,name,lane,when_samples,seed_id,prng,engine" << '\n';
        for (std::size_t idx = 0; idx < plan.logEntries.size(); ++idx) {
            const auto& entry = plan.logEntries[idx];
            control << idx << ',' << entry.name << ',' << entry.lane << ',' << entry.whenSamples
                    << ',' << entry.seedId << ',' << "0x" << std::hex << std::nouppercase
                    << entry.prng << std::dec << ','
                    << static_cast<int>(entry.engine == reseed::EngineKind::kResonator ? 2 : 0)
                    << '\n';
        }
        (void)emit_control_log_impl(control_fixture_name, control.str());
    }
    return std::vector<int16_t>(pcm.begin(), pcm.end());
}


std::vector<int16_t> render_long_random_take_fixture() {
    constexpr double kDurationSeconds = 30.0;
    constexpr std::uint32_t kMasterSeed = 0x30F00Du;
    constexpr int kBpm = 120;
    constexpr int kPasses = 10;

    std::vector<reseed::StemDefinition> stems = reseed::defaultStems();
    stems.push_back({"tape rattle", 4, reseed::EngineKind::kSampler});
    stems.push_back({"clank shimmer", 5, reseed::EngineKind::kResonator});

    const auto plan = reseed::makeBouncePlan(stems, kMasterSeed, kSampleRate, kBpm, kPasses);

    offline::RenderSettings settings;
    settings.sampleRate = kSampleRate;
    settings.frames = static_cast<std::size_t>(kDurationSeconds * kSampleRate);
    settings.samplerSustainHold = 0.35;
    settings.normalizeTarget = 0.9;

    offline::OfflineRenderer renderer(settings);
    renderer.mixSamplerEvents(plan.samplerEvents);
    renderer.mixResonatorEvents(plan.resonatorEvents);
    const auto& pcm = renderer.finalize();

    const std::size_t frames = settings.frames;
    std::ostringstream control;
    control << "# long-random-take control log" << '\n';
    control << "master_seed=0x" << std::hex << std::nouppercase << kMasterSeed << std::dec
            << " bpm=" << kBpm << " passes=" << kPasses << '\n';
    control << "event,name,lane,when_samples,seed_id,prng,engine" << '\n';
    for (std::size_t idx = 0; idx < plan.logEntries.size(); ++idx) {
        const auto& entry = plan.logEntries[idx];
        control << idx << ',' << entry.name << ',' << entry.lane << ',' << entry.whenSamples
                << ',' << entry.seedId << ',' << "0x" << std::hex << std::nouppercase << entry.prng
                << std::dec << ','
                << static_cast<int>(entry.engine == reseed::EngineKind::kResonator ? 2 : 0) << '\n';
    }
    (void)emit_control_log_impl("long-random-take", control.str());

    std::vector<int16_t> trimmed(frames * 2u, 0);
    const std::size_t available_frames = std::min(frames, pcm.size());
    for (std::size_t i = 0; i < available_frames; ++i) {
        const int16_t sample = pcm[i];
        trimmed[2u * i] = sample;
        trimmed[2u * i + 1u] = sample;
    }
    return trimmed;
}

std::string render_reseed_log_fixture() {
    const auto& stems = reseed::defaultStems();
    const auto plan_a = reseed::makeBouncePlan(stems, 0xCAFEu, kSampleRate, 124, 3);
    const auto plan_b = reseed::makeBouncePlan(stems, 0xBEEFu, kSampleRate, 124, 3);

    std::vector<reseed::BounceLogBlock> blocks;
    blocks.push_back({"A", 0xCAFEu, "out/reseed-A.wav", plan_a.logEntries});
    blocks.push_back({"B", 0xBEEFu, "out/reseed-B.wav", plan_b.logEntries});
    return reseed::serializeEventLog(stems, blocks, kSampleRate, 124, 3);
}

std::string fnv1a_bytes(const std::string& bytes) {
    constexpr std::uint64_t kOffset = 1469598103934665603ull;
    constexpr std::uint64_t kPrime = 1099511628211ull;
    std::uint64_t state = kOffset;
    for (unsigned char c : bytes) {
        state ^= static_cast<std::uint64_t>(c);
        state *= kPrime;
    }
    std::ostringstream oss;
    oss << std::hex << std::nouppercase << std::setfill('0') << std::setw(16) << state;
    return oss.str();
}

std::string render_burst_log() {
    BurstEngine engine;
    Engine::PrepareContext prep{};
    prep.masterSeed = 0xB0057BADu;
    engine.prepare(prep);

    engine.onParam({0, static_cast<std::uint16_t>(BurstEngine::Param::kClusterCount), 6});
    engine.onParam({0, static_cast<std::uint16_t>(BurstEngine::Param::kSpacingSamples), 720});

    Seed seed{};
    seed.id = 77;
    engine.onSeed({seed, 2048u});

    std::ostringstream log;
    log << "# Burst engine trigger cluster" << '\n';
    log << "cluster_count=6 spacing_samples=720 master_seed=0x" << std::hex << std::nouppercase
        << engine.generationSeed() << std::dec << '\n';
    log << "pending:";
    const auto& pending = engine.pendingTriggers();
    for (std::size_t i = 0; i < pending.size(); ++i) {
        if (i != 0) {
            log << ',';
        }
        log << pending[i];
    }
    log << '\n';
    return log.str();
}

bool write_text_file_impl(const std::string& manifest_path, const std::string& body) {
#if ENABLE_GOLDEN
    const auto disk_path = fixture_disk_path(manifest_path);
    std::error_code ec;
    std::filesystem::create_directories(disk_path.parent_path(), ec);
    if (ec) {
        return false;
    }
    std::ofstream out(disk_path, std::ios::binary);
    if (!out) {
        return false;
    }
    out.write(body.data(), static_cast<std::streamsize>(body.size()));
    return out.good();
#else
    (void)manifest_path;
    (void)body;
    return false;
#endif
}

bool emit_control_log_impl(const char* fixture_name, const std::string& body) {
    if (fixture_name == nullptr || *fixture_name == '\0') {
        return false;
    }
    const std::string log_name = std::string(fixture_name) + "-control";
    const FixtureInfo* spec = find_log_fixture(log_name.c_str());
    std::string manifest_path;
    if (spec != nullptr && spec->path != nullptr && spec->path[0] != '\0') {
        manifest_path.assign(spec->path);
    } else {
        manifest_path = std::string("build/fixtures/") + log_name + ".txt";
#if ENABLE_GOLDEN
        std::cout << "[note] missing manifest entry for log fixture '" << log_name
                  << "'; writing to " << manifest_path << std::endl;
#endif
    }

    return write_text_file_impl(manifest_path, body);
}

std::string load_manifest() {
    std::ifstream manifest_stream{kManifestPath};
    TEST_ASSERT_TRUE_MESSAGE(manifest_stream.good(),
                             "Golden manifest missing — run scripts/compute_golden_hashes.py --write");
    return std::string{std::istreambuf_iterator<char>(manifest_stream), std::istreambuf_iterator<char>()};
}

void assert_manifest_contains(const std::string& manifest_body, const FixtureInfo& fixture) {
    const std::string expected_fixture = std::string{"\"name\": \""} + fixture.name + "\"";
    TEST_ASSERT_NOT_EQUAL_MESSAGE(std::string::npos, manifest_body.find(expected_fixture),
                                  "Manifest missing fixture entry");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(std::string::npos, manifest_body.find(fixture.expected_hash),
                                  "Manifest missing updated hash");
}

}  // namespace

bool write_text_file(const std::string& manifest_path, const std::string& body) {
    return write_text_file_impl(manifest_path, body);
}

bool emit_control_log(const char* fixture_name, const std::string& body) {
    return emit_control_log_impl(fixture_name, body);
}

void test_emit_flag_matrix() {
#if ENABLE_GOLDEN
    const auto root = fixture_root();
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    TEST_ASSERT_FALSE_MESSAGE(static_cast<bool>(ec), "Failed to create golden fixture root");
    TEST_ASSERT_TRUE_MESSAGE(std::filesystem::exists(root), "Golden fixture root missing on disk");
#endif
    std::cout << "[seedbox-config] active flag matrix" << std::endl;
    for (const auto& flag : SeedBoxConfig::kFlagMatrix) {
        std::cout << "  " << flag.name << "=" << (flag.enabled ? "1" : "0")
                  << " // " << flag.story << std::endl;
    }
    std::cout << std::flush;
    TEST_MESSAGE("Flag matrix dumped for golden log capture.");
}

void test_render_and_compare_golden() {
    const auto* fixture = find_audio_fixture("drone-intro");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "drone-intro fixture metadata missing");

    golden::WavWriteRequest request{};
    const auto disk_path = fixture_disk_path(fixture->path);
    request.path = disk_path.string();
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.samples = make_drone();

    const bool write_ok = golden::write_wav_16(request);
    const std::string hash = golden::hash_pcm16(request.samples);

    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write golden WAV fixture");
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, hash.c_str());

    TEST_ASSERT_TRUE_MESSAGE(std::filesystem::exists(disk_path),
                             "Golden WAV missing on disk");

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *fixture);
}

void test_render_sampler_golden() {
    const auto* fixture = find_audio_fixture("sampler-grains");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "sampler-grains fixture metadata missing");

    golden::WavWriteRequest request{};
    request.path = fixture_disk_path(fixture->path).string();
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.samples = render_sampler_fixture();

    const bool write_ok = golden::write_wav_16(request);
    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write sampler golden WAV");

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *fixture);
}

void test_render_modulated_sampler_golden() {
    const auto* fixture = find_audio_fixture("modulated-sampler");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "modulated-sampler fixture metadata missing");

    golden::WavWriteRequest request{};
    request.path = fixture_disk_path(fixture->path).string();
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.channels = 2;
    request.samples = render_modulated_sampler_fixture();

    const bool write_ok = golden::write_wav_16(request);
    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write modulated sampler golden WAV");

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *fixture);
}

void test_render_layered_euclid_burst_golden() {
    const auto* fixture = find_audio_fixture("layered-euclid-burst");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "layered-euclid-burst fixture metadata missing");

    golden::WavWriteRequest request{};
    request.path = fixture_disk_path(fixture->path).string();
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.channels = 2;
    request.samples = golden::render_layered_euclid_burst_fixture();

    const bool write_ok = golden::write_wav_16(request);
    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write layered-euclid-burst golden WAV");

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *fixture);
}

void test_render_burst_cluster_golden() {
    const auto* fixture = find_audio_fixture("burst-cluster");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "burst-cluster fixture metadata missing");

    golden::WavWriteRequest request{};
    request.path = fixture_disk_path(fixture->path).string();
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.samples = golden::render_burst_cluster_fixture();

    TEST_ASSERT_FALSE_MESSAGE(request.samples.empty(), "Burst cluster render returned zero samples");

    const bool write_ok = golden::write_wav_16(request);
    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write burst-cluster golden WAV");

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *fixture);
}

void test_render_resonator_golden() {
    const auto* fixture = find_audio_fixture("resonator-tail");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "resonator-tail fixture metadata missing");

    golden::WavWriteRequest request{};
    request.path = fixture_disk_path(fixture->path).string();
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.samples = render_resonator_fixture();

    const bool write_ok = golden::write_wav_16(request);
    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write resonator golden WAV");

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *fixture);
}

void test_render_granular_golden() {
    const auto* fixture = find_audio_fixture("granular-haze");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "granular-haze fixture metadata missing");

    golden::WavWriteRequest request{};
    request.path = fixture_disk_path(fixture->path).string();
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.channels = 2;
    request.samples = render_granular_fixture();

    const bool write_ok = golden::write_wav_16(request);
    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write granular golden WAV");

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *fixture);
}

void test_render_mixer_golden() {
    const auto* fixture = find_audio_fixture("mixer-console");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "mixer-console fixture metadata missing");

    golden::WavWriteRequest request{};
    request.path = fixture_disk_path(fixture->path).string();
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.channels = 2;
    request.samples = render_mixer_fixture();

    const bool write_ok = golden::write_wav_16(request);
    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write mixer golden WAV");

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *fixture);
}

void test_render_quadraphonic_golden() {
    const auto* fixture = find_audio_fixture("quad-bus");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "quad-bus fixture metadata missing");

    golden::WavWriteRequest request{};
    request.path = fixture_disk_path(fixture->path).string();
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.channels = 4;
    request.samples = render_quadraphonic_fixture();

    const bool write_ok = golden::write_wav_16(request);
    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write quadraphonic golden WAV");

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *fixture);
}

void test_render_surround_golden() {
    const auto* fixture = find_audio_fixture("surround-bus");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "surround-bus fixture metadata missing");

    golden::WavWriteRequest request{};
    request.path = fixture_disk_path(fixture->path).string();
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.channels = 6;
    request.samples = render_surround_fixture();

    const bool write_ok = golden::write_wav_16(request);
    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write surround golden WAV");

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *fixture);
}

void test_render_engine_hybrid_stack_golden() {
    const auto* fixture = find_audio_fixture("engine-hybrid-stack");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "engine-hybrid-stack fixture metadata missing");
    const auto* log_fixture = find_log_fixture("engine-hybrid-stack-control");
    TEST_ASSERT_NOT_NULL_MESSAGE(log_fixture, "engine-hybrid-stack control metadata missing");

    const auto capture = golden::render_engine_hybrid_fixture();
    golden::WavWriteRequest request{};
    request.path = fixture_disk_path(fixture->path).string();
    request.sample_rate_hz = capture.sample_rate_hz;
    request.channels = capture.channels;
    request.samples = capture.samples;

    const bool write_ok = golden::write_wav_16(request);
    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write engine-hybrid-stack golden WAV");

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, hash.c_str());

    const bool log_ok = emit_control_log("engine-hybrid-stack", capture.control_log);
    TEST_ASSERT_TRUE_MESSAGE(log_ok, "Failed to write engine-hybrid-stack control log");
    const std::string log_hash = golden::hash_bytes(capture.control_log);
    TEST_ASSERT_EQUAL_STRING(log_fixture->expected_hash, log_hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *fixture);
    assert_manifest_contains(manifest_body, *log_fixture);
}

void test_render_engine_macro_orbits_golden() {
    const auto* fixture = find_audio_fixture("engine-macro-orbits");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "engine-macro-orbits fixture metadata missing");
    const auto* log_fixture = find_log_fixture("engine-macro-orbits-control");
    TEST_ASSERT_NOT_NULL_MESSAGE(log_fixture, "engine-macro-orbits control metadata missing");

    const auto capture = golden::render_engine_macro_orbits_fixture();
    golden::WavWriteRequest request{};
    request.path = fixture_disk_path(fixture->path).string();
    request.sample_rate_hz = capture.sample_rate_hz;
    request.channels = capture.channels;
    request.samples = capture.samples;

    const bool write_ok = golden::write_wav_16(request);
    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write engine-macro-orbits golden WAV");

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, hash.c_str());

    const bool log_ok = emit_control_log("engine-macro-orbits", capture.control_log);
    TEST_ASSERT_TRUE_MESSAGE(log_ok, "Failed to write engine-macro-orbits control log");
    const std::string log_hash = golden::hash_bytes(capture.control_log);
    TEST_ASSERT_EQUAL_STRING(log_fixture->expected_hash, log_hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *fixture);
    assert_manifest_contains(manifest_body, *log_fixture);
}

void test_render_stage71_golden() {
    const auto* fixture = find_audio_fixture("stage71-bus");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "stage71-bus fixture metadata missing");
    const auto* log_fixture = find_log_fixture("stage71-bus-control");
    TEST_ASSERT_NOT_NULL_MESSAGE(log_fixture, "stage71-bus control metadata missing");

    const auto stage = golden::render_stage71_scene();
    golden::WavWriteRequest request{};
    request.path = fixture_disk_path(fixture->path).string();
    request.sample_rate_hz = stage.sample_rate_hz;
    request.channels = stage.channels;
    request.samples = stage.samples;

    const bool write_ok = golden::write_wav_16(request);
    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write stage71 golden WAV");

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, hash.c_str());

    const bool log_ok = emit_control_log("stage71-bus", stage.control_log);
    TEST_ASSERT_TRUE_MESSAGE(log_ok, "Failed to write stage71 control log");
    const std::string log_hash = golden::hash_bytes(stage.control_log);
    TEST_ASSERT_EQUAL_STRING(log_fixture->expected_hash, log_hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *fixture);
    assert_manifest_contains(manifest_body, *log_fixture);
}

void test_render_reseed_a_golden() {
    const auto* fixture = find_audio_fixture("reseed-A");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "reseed-A fixture metadata missing");

    golden::WavWriteRequest request{};
    request.path = fixture_disk_path(fixture->path).string();
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.samples = render_reseed_variant(0xCAFEu, "reseed-A");

    const bool write_ok = golden::write_wav_16(request);
    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write reseed-A golden WAV");

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *fixture);
}

void test_render_reseed_b_golden() {
    const auto* fixture = find_audio_fixture("reseed-B");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "reseed-B fixture metadata missing");

    golden::WavWriteRequest request{};
    request.path = fixture_disk_path(fixture->path).string();
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.samples = render_reseed_variant(0xBEEFu, "reseed-B");

    const bool write_ok = golden::write_wav_16(request);
    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write reseed-B golden WAV");

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *fixture);
}

void test_render_reseed_C_golden() {
    const auto* fixture = find_audio_fixture("reseed-C");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "reseed-C fixture metadata missing");

    constexpr int kBpm = 132;
    constexpr int kPasses = 4;

    golden::WavWriteRequest request{};
    request.path = fixture_disk_path(fixture->path).string();
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.samples = render_reseed_variant(0xC0FFEEu, "reseed-C", kBpm, kPasses);

    const bool write_ok = golden::write_wav_16(request);
    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write reseed-C golden WAV");

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *fixture);
}

void test_render_reseed_poly_golden() {
    const auto* fixture = find_audio_fixture("reseed-poly");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "reseed-poly fixture metadata missing");

    auto stems = reseed::defaultStems();
    stems.push_back({"tape rattle", 4, reseed::EngineKind::kSampler});
    stems.push_back({"clank shimmer", 5, reseed::EngineKind::kResonator});

    constexpr int kBpm = 118;
    constexpr int kPasses = 5;

    golden::WavWriteRequest request{};
    request.path = fixture_disk_path(fixture->path).string();
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.samples = render_reseed_variant(0xC001CAFEu, "reseed-poly", kBpm, kPasses, &stems);

    const bool write_ok = golden::write_wav_16(request);
    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write reseed-poly golden WAV");

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *fixture);
}

void test_render_long_take_golden() {
    const auto* fixture = find_audio_fixture("long-random-take");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "long-random-take fixture metadata missing");

    golden::WavWriteRequest request{};
    request.path = fixture_disk_path(fixture->path).string();
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.samples = render_long_random_take_fixture();

    const bool write_ok = golden::write_wav_16(request);
    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write long random take WAV");

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *fixture);
}

void test_euclid_mask_pair_golden() {
    const auto* audio_fixture = find_audio_fixture("euclid-mask");
    const auto* log_fixture = find_log_fixture("euclid-mask-control");
    TEST_ASSERT_NOT_NULL_MESSAGE(audio_fixture, "euclid-mask fixture metadata missing");
    TEST_ASSERT_NOT_NULL_MESSAGE(log_fixture, "euclid-mask-control fixture metadata missing");

    const auto render = golden::render_euclid_mask_fixture();
    TEST_ASSERT_FALSE_MESSAGE(render.samples.empty(), "Euclid mask render returned zero samples");
    TEST_ASSERT_FALSE_MESSAGE(render.control_log.empty(), "Euclid mask control log is empty");

    golden::WavWriteRequest request{};
    request.path = fixture_disk_path(audio_fixture->path).string();
    request.sample_rate_hz = render.sample_rate_hz;
    request.channels = render.channels;
    request.samples = render.samples;

    const bool wav_ok = golden::write_wav_16(request);
    TEST_ASSERT_TRUE_MESSAGE(wav_ok, "Failed to write Euclid mask golden WAV");

    const bool log_ok = write_text_file(log_fixture->path, render.control_log);
    TEST_ASSERT_TRUE_MESSAGE(log_ok, "Failed to write Euclid mask control log");

    const std::string audio_hash = golden::hash_pcm16(request.samples);
    const std::string log_hash = fnv1a_bytes(render.control_log);
    TEST_ASSERT_EQUAL_STRING(audio_fixture->expected_hash, audio_hash.c_str());
    TEST_ASSERT_EQUAL_STRING(log_fixture->expected_hash, log_hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *audio_fixture);
    assert_manifest_contains(manifest_body, *log_fixture);
}

void test_log_burst_golden() {
    const auto* fixture = find_log_fixture("burst-cluster-control");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "burst-cluster-control fixture metadata missing");

    const std::string burst_log = render_burst_log();

    const bool burst_ok = write_text_file(fixture->path, burst_log);
    TEST_ASSERT_TRUE_MESSAGE(burst_ok, "Failed to write Burst golden control log");

    const std::string burst_hash = fnv1a_bytes(burst_log);
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, burst_hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *fixture);
}

void test_log_reseed_golden() {
    const auto* fixture = find_log_fixture("reseed-log");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "reseed-log fixture metadata missing");

    const std::string reseed_log = render_reseed_log_fixture();
    const bool log_ok = write_text_file(fixture->path, reseed_log);
    TEST_ASSERT_TRUE_MESSAGE(log_ok, "Failed to write reseed event log");

    const std::string hash = fnv1a_bytes(reseed_log);
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, *fixture);
}

#else

void test_golden_mode_disabled() {
    const auto* fixture = find_audio_fixture("drone-intro");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, "drone-intro fixture metadata missing");

    golden::WavWriteRequest request{};
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.samples = make_drone();

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(fixture->expected_hash, hash.c_str());
}

#endif  // ENABLE_GOLDEN

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
#if !defined(SEEDBOX_HW)
    RUN_TEST(test_cli_backend_clock_routing);
    RUN_TEST(test_cli_channel_map_and_panic);
    RUN_TEST(test_cli_trs_transport_mirror);
#endif
#if ENABLE_GOLDEN
    RUN_TEST(test_emit_flag_matrix);
    RUN_TEST(test_render_and_compare_golden);
    RUN_TEST(test_render_sampler_golden);
    RUN_TEST(test_render_modulated_sampler_golden);
    RUN_TEST(test_render_layered_euclid_burst_golden);
    RUN_TEST(test_render_burst_cluster_golden);
    RUN_TEST(test_render_resonator_golden);
    RUN_TEST(test_render_granular_golden);
    RUN_TEST(test_render_mixer_golden);
    RUN_TEST(test_render_quadraphonic_golden);
    RUN_TEST(test_render_surround_golden);
    RUN_TEST(test_render_engine_hybrid_stack_golden);
    RUN_TEST(test_render_engine_macro_orbits_golden);
    RUN_TEST(test_render_stage71_golden);
    RUN_TEST(test_render_reseed_a_golden);
    RUN_TEST(test_render_reseed_b_golden);
    RUN_TEST(test_render_reseed_C_golden);
    RUN_TEST(test_render_reseed_poly_golden);
    RUN_TEST(test_render_long_take_golden);
    RUN_TEST(test_euclid_mask_pair_golden);
    RUN_TEST(test_log_burst_golden);
    RUN_TEST(test_log_reseed_golden);
#else
    RUN_TEST(test_golden_mode_disabled);
#endif
    return UNITY_END();
}
