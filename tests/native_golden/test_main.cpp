#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include <unity.h>

#include "Seed.h"
#include "SeedBoxConfig.h"
#include "engine/BurstEngine.h"
#include "engine/EuclidEngine.h"
#include "engine/Resonator.h"
#include "engine/Sampler.h"
#include "io/MidiRouter.h"
#include "wav_helpers.hpp"

#ifndef ENABLE_GOLDEN
#define ENABLE_GOLDEN 0
#endif

namespace {

constexpr std::size_t kDroneFrames = 48000;
constexpr double kSampleRate = 48000.0;
constexpr double kDroneFreqHz = 110.0;
constexpr double kDroneAmplitude = 0.5;
constexpr char kManifestPath[] = "tests/native_golden/golden.json";

struct GoldenFixture {
    const char* name;
    const char* path;
    const char* expected_hash;
};

constexpr GoldenFixture kAudioFixtures[] = {
    {"drone-intro", "build/fixtures/drone-intro.wav", "f53315eb7db89d33"},
    {"sampler-grains", "build/fixtures/sampler-grains.wav", "630fbfadca574688"},
    {"resonator-tail", "build/fixtures/resonator-tail.wav", "e329aa6faffb39f4"},
};

constexpr GoldenFixture kLogFixtures[] = {
    {"euclid-mask", "build/fixtures/euclid-mask.txt", "2431091b3af7d347"},
    {"burst-cluster", "build/fixtures/burst-cluster.txt", "082de9ac9a3cb359"},
};

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

    for (const auto& spec : voices) {
        sampler.trigger(spec.seed, spec.when);
    }

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

    for (const auto& spec : specs) {
        bank.trigger(spec.seed, spec.when);
    }

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

std::string render_euclid_log() {
    EuclidEngine engine;
    Engine::PrepareContext prep{};
    prep.masterSeed = 0xE0C10BADu;
    engine.prepare(prep);

    engine.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kSteps), 16});
    engine.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kFills), 5});
    engine.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kRotate), 3});

    Seed seed{};
    seed.id = 24;
    engine.onSeed({seed, 0u});

    std::ostringstream log;
    log << "# Euclid engine mask" << '\n';
    log << "steps=" << static_cast<int>(engine.mask().size())
        << " fills=5 rotate=3 master_seed=0x" << std::hex << std::nouppercase << engine.generationSeed()
        << std::dec << '\n';
    log << "mask:";
    for (std::size_t i = 0; i < engine.mask().size(); ++i) {
        log << (engine.mask()[i] ? 'X' : '.');
    }
    log << '\n';

    log << "gates:";
    Engine::TickContext tick{};
    const std::size_t sample_span = engine.mask().empty() ? 0 : engine.mask().size() * 2;
    for (std::size_t step = 0; step < sample_span; ++step) {
        tick.tick = step;
        engine.onTick(tick);
        log << (engine.lastGate() ? '1' : '0');
    }
    log << '\n';
    return log.str();
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

bool write_text_file(const std::string& path, const std::string& body) {
#if ENABLE_GOLDEN
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    if (ec) {
        return false;
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out.write(body.data(), static_cast<std::streamsize>(body.size()));
    return out.good();
#else
    (void)path;
    (void)body;
    return false;
#endif
}

std::string load_manifest() {
    std::ifstream manifest_stream{kManifestPath};
    TEST_ASSERT_TRUE_MESSAGE(manifest_stream.good(),
                             "Golden manifest missing — run scripts/compute_golden_hashes.py --write");
    return std::string{std::istreambuf_iterator<char>(manifest_stream), std::istreambuf_iterator<char>()};
}

void assert_manifest_contains(const std::string& manifest_body, const GoldenFixture& fixture) {
    const std::string expected_fixture = std::string{"\"name\": \""} + fixture.name + "\"";
    TEST_ASSERT_NOT_EQUAL_MESSAGE(std::string::npos, manifest_body.find(expected_fixture),
                                  "Manifest missing fixture entry");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(std::string::npos, manifest_body.find(fixture.expected_hash),
                                  "Manifest missing updated hash");
}

}  // namespace

void test_emit_flag_matrix() {
    std::cout << "[seedbox-config] active flag matrix" << std::endl;
    for (const auto& flag : SeedBoxConfig::kFlagMatrix) {
        std::cout << "  " << flag.name << "=" << (flag.enabled ? "1" : "0")
                  << " // " << flag.story << std::endl;
    }
    std::cout << std::flush;
    TEST_MESSAGE("Flag matrix dumped for golden log capture.");
}

void test_render_and_compare_golden() {
    golden::WavWriteRequest request{};
    request.path = kAudioFixtures[0].path;
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.samples = make_drone();

    const bool write_ok = golden::write_wav_16(request);
    const std::string hash = golden::hash_pcm16(request.samples);

    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write golden WAV fixture");
    TEST_ASSERT_EQUAL_STRING(kAudioFixtures[0].expected_hash, hash.c_str());

    const std::filesystem::path wav_path(request.path);
    TEST_ASSERT_TRUE_MESSAGE(std::filesystem::exists(wav_path),
                             "Golden WAV missing on disk");

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, kAudioFixtures[0]);
}

void test_render_sampler_golden() {
    golden::WavWriteRequest request{};
    request.path = kAudioFixtures[1].path;
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.samples = render_sampler_fixture();

    const bool write_ok = golden::write_wav_16(request);
    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write sampler golden WAV");

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(kAudioFixtures[1].expected_hash, hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, kAudioFixtures[1]);
}

void test_render_resonator_golden() {
    golden::WavWriteRequest request{};
    request.path = kAudioFixtures[2].path;
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.samples = render_resonator_fixture();

    const bool write_ok = golden::write_wav_16(request);
    TEST_ASSERT_TRUE_MESSAGE(write_ok, "Failed to write resonator golden WAV");

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(kAudioFixtures[2].expected_hash, hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, kAudioFixtures[2]);
}

void test_log_euclid_burst_golden() {
    const std::string euclid_log = render_euclid_log();
    const std::string burst_log = render_burst_log();

    const bool euclid_ok = write_text_file(kLogFixtures[0].path, euclid_log);
    const bool burst_ok = write_text_file(kLogFixtures[1].path, burst_log);
    TEST_ASSERT_TRUE_MESSAGE(euclid_ok, "Failed to write Euclid golden log");
    TEST_ASSERT_TRUE_MESSAGE(burst_ok, "Failed to write Burst golden log");

    const std::string euclid_hash = fnv1a_bytes(euclid_log);
    const std::string burst_hash = fnv1a_bytes(burst_log);
    TEST_ASSERT_EQUAL_STRING(kLogFixtures[0].expected_hash, euclid_hash.c_str());
    TEST_ASSERT_EQUAL_STRING(kLogFixtures[1].expected_hash, burst_hash.c_str());

    const std::string manifest_body = load_manifest();
    assert_manifest_contains(manifest_body, kLogFixtures[0]);
    assert_manifest_contains(manifest_body, kLogFixtures[1]);
}

#else

void test_golden_mode_disabled() {
    golden::WavWriteRequest request{};
    request.sample_rate_hz = static_cast<uint32_t>(kSampleRate);
    request.samples = make_drone();

    const std::string hash = golden::hash_pcm16(request.samples);
    TEST_ASSERT_EQUAL_STRING(kAudioFixtures[0].expected_hash, hash.c_str());
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
    RUN_TEST(test_render_resonator_golden);
    RUN_TEST(test_log_euclid_burst_golden);
#else
    RUN_TEST(test_golden_mode_disabled);
#endif
    return UNITY_END();
}
