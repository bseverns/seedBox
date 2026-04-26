//
// AppState.cpp
// -------------
// The operational heart of SeedBox.  Everything the performer can touch rolls
// through here eventually: seeds get primed, transport sources compete for
// authority, and the audio engines receive their marching orders.  The comments
// are intentionally loud so you can walk a classroom through the firmware
// without needing a separate slide deck.
#include "app/AppState.h"
#include "app/AudioRuntimeState.h"
#include "app/DisplaySnapshotBuilder.h"
#include "app/InputGestureRouter.h"
#include "app/SeedPrimeController.h"
#include "app/ClockTransportController.h"
#include "app/PresetController.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <initializer_list>
#include "SeedBoxConfig.h"
#include "interop/mn42_map.h"
#include "io/Storage.h"
#include "interop/mn42_param_map.h"
#include "util/RNG.h"
#include "util/ScaleQuantizer.h"
#include "util/Units.h"
#include "engine/Granular.h"
#include "engine/EuclidEngine.h"
#include "engine/BurstEngine.h"
#include "engine/Sampler.h"
#include "hal/hal_audio.h"
#include "hal/hal_io.h"
#if SEEDBOX_HW
  #include "HardwarePrelude.h"
  #include "AudioMemoryBudget.h"
#else
  #include <filesystem>
  #include <fstream>
#endif

#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  #include <Arduino.h>
#endif

namespace {
constexpr uint8_t kEngineCycleCc = seedbox::interop::mn42::param::kEngineCycle;
constexpr uint32_t kStorageLongPressFrames = 60;
constexpr std::string_view kDefaultPresetSlot = "default";
constexpr uint8_t kShortCaptureSlots = 4;
constexpr int kEuclidStepNudge = 1;
constexpr int kBurstSpacingStepSamples = 240;  // ~5ms at 48k.
constexpr std::size_t kSeedSlotCount = 4;
constexpr uint32_t kExternalClockTimeoutMs = 2000;

constexpr hal::io::PinNumber kReseedButtonPin = 2;
constexpr hal::io::PinNumber kLockButtonPin = 3;
constexpr hal::io::PinNumber kStatusLedPin = 13;

const std::array<hal::io::DigitalConfig, 3> kFrontPanelPins{{
    {kReseedButtonPin, true, true},
    {kLockButtonPin, true, true},
    {kStatusLedPin, false, false},
}};

constexpr uint32_t kLockLongPressUs = 600000;  // ~0.6s long press threshold.

constexpr std::array<const char*, 4> kDemoSdClips{{"wash", "dust", "vox", "pads"}};
#if !SEEDBOX_HW
constexpr std::string_view kTeachingPresetPath = "presets/teaching/01_clock_subdivision.json";

void configureDesktopBootSeeds(std::vector<Seed>& seeds) {
  if (seeds.empty()) {
    return;
  }

  for (std::size_t i = 0; i < seeds.size(); ++i) {
    Seed& seed = seeds[i];
    seed.probability = std::max(seed.probability, 0.82f);
    seed.density = std::max(seed.density, 1.35f);
    seed.tone = 0.58f;
    seed.spread = 0.7f;
    seed.mutateAmt = 0.42f;
  }

  seeds[0].engine = EngineRouter::kGranularId;
  seeds[0].granular.grainSizeMs = 170.0f;
  seeds[0].granular.sprayMs = 42.0f;
  seeds[0].granular.transpose = 7.0f;
  seeds[0].granular.windowSkew = 0.35f;
  seeds[0].granular.stereoSpread = 0.9f;

  if (seeds.size() > 1) {
    seeds[1].engine = EngineRouter::kResonatorId;
    seeds[1].resonator.feedback = 0.86f;
    seeds[1].resonator.brightness = 0.75f;
    seeds[1].resonator.damping = 0.42f;
    seeds[1].pitch = 7.0f;
  }

  if (seeds.size() > 2) {
    seeds[2].engine = EngineRouter::kBurstId;
    seeds[2].density = 2.5f;
    seeds[2].probability = 0.92f;
  }

  if (seeds.size() > 3) {
    seeds[3].engine = EngineRouter::kEuclidId;
    seeds[3].density = 1.75f;
    seeds[3].probability = 0.78f;
  }
}
#endif

void populateSdClips(GranularEngine& engine) {
  for (uint8_t i = 0; i < kDemoSdClips.size(); ++i) {
    engine.registerSdClip(static_cast<uint8_t>(i + 1), kDemoSdClips[i]);
  }
}

#if !SEEDBOX_HW
std::optional<seedbox::Preset> loadTeachingPresetForSim() {
  std::filesystem::path root;
#ifdef SEEDBOX_PROJECT_ROOT_HINT
  root = std::filesystem::path(SEEDBOX_PROJECT_ROOT_HINT);
#else
  root = std::filesystem::current_path();
#endif
  const std::filesystem::path presetPath = root / kTeachingPresetPath;
  std::ifstream in(presetPath, std::ios::binary);
  if (!in.good()) {
    return std::nullopt;
  }
  const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
  seedbox::Preset preset{};
  if (!seedbox::Preset::deserialize(bytes, preset)) {
    return std::nullopt;
  }
  if (preset.slot.empty()) {
    preset.slot = "teach-01-clock";
  }
  return preset;
}
#endif

SeedPrimeController::Mode toPrimeControllerMode(AppState::SeedPrimeMode mode) {
  switch (mode) {
    case AppState::SeedPrimeMode::kTapTempo: return SeedPrimeController::Mode::kTapTempo;
    case AppState::SeedPrimeMode::kPreset: return SeedPrimeController::Mode::kPreset;
    case AppState::SeedPrimeMode::kLiveInput: return SeedPrimeController::Mode::kLiveInput;
    case AppState::SeedPrimeMode::kLfsr:
    default: return SeedPrimeController::Mode::kLfsr;
  }
}

AppState::SeedPrimeMode toAppSeedPrimeMode(SeedPrimeController::Mode mode) {
  switch (mode) {
    case SeedPrimeController::Mode::kTapTempo: return AppState::SeedPrimeMode::kTapTempo;
    case SeedPrimeController::Mode::kPreset: return AppState::SeedPrimeMode::kPreset;
    case SeedPrimeController::Mode::kLiveInput: return AppState::SeedPrimeMode::kLiveInput;
    case SeedPrimeController::Mode::kLfsr:
    default: return AppState::SeedPrimeMode::kLfsr;
  }
}

PresetController::Boundary toPresetControllerBoundary(AppState::PresetBoundary boundary) {
  switch (boundary) {
    case AppState::PresetBoundary::Bar: return PresetController::Boundary::kBar;
    case AppState::PresetBoundary::Step:
    default: return PresetController::Boundary::kStep;
  }
}

SeedPrimeController gSeedPrimeController{};

const char* pageLabel(AppState::Page page) {
  switch (page) {
    case AppState::Page::kSeeds: return "Seeds";
    case AppState::Page::kStorage: return "Storage";
    case AppState::Page::kClock: return "Clock";
    default: return "Unknown";
  }
}

std::size_t boundedCStringLength(const char* value, std::size_t maxLen) {
  if (!value) {
    return 0;
  }
  std::size_t len = 0;
  while (len < maxLen && value[len] != '\0') {
    ++len;
  }
  return len;
}

void appendJsonEscaped(std::string& out, std::string_view value) {
  constexpr char kHex[] = "0123456789ABCDEF";
  for (const char ch : value) {
    switch (ch) {
      case '\"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default: {
        const auto code = static_cast<unsigned char>(ch);
        if (code < 0x20u) {
          out += "\\u00";
          out.push_back(kHex[(code >> 4u) & 0x0Fu]);
          out.push_back(kHex[code & 0x0Fu]);
        } else {
          out.push_back(ch);
        }
        break;
      }
    }
  }
}

void appendJsonStringField(std::string& out, std::string_view key, std::string_view value, bool trailingComma) {
  out.push_back('\"');
  out.append(key.data(), key.size());
  out += "\":\"";
  appendJsonEscaped(out, value);
  out.push_back('\"');
  if (trailingComma) {
    out.push_back(',');
  }
}

void appendJsonBoolField(std::string& out, std::string_view key, bool value, bool trailingComma) {
  out.push_back('\"');
  out.append(key.data(), key.size());
  out += "\":";
  out += value ? "true" : "false";
  if (trailingComma) {
    out.push_back(',');
  }
}

void appendJsonUIntField(std::string& out, std::string_view key, std::uint64_t value, bool trailingComma) {
  out.push_back('\"');
  out.append(key.data(), key.size());
  out += "\":";
  out += std::to_string(value);
  if (trailingComma) {
    out.push_back(',');
  }
}

void appendJsonFloatField(std::string& out, std::string_view key, float value, bool trailingComma) {
  std::array<char, 32> scratch{};
  std::snprintf(scratch.data(), scratch.size(), "%.3f", static_cast<double>(value));

  out.push_back('\"');
  out.append(key.data(), key.size());
  out += "\":";
  out += scratch.data();
  if (trailingComma) {
    out.push_back(',');
  }
}

uint32_t gateDivisionTicksFor(AppState::GateDivision div) {
  constexpr uint32_t kTicksPerBeat = 24u;
  switch (div) {
    case AppState::GateDivision::kOneOverTwo: return kTicksPerBeat / 2u;
    case AppState::GateDivision::kOneOverFour: return kTicksPerBeat / 4u;
    case AppState::GateDivision::kBars: return kTicksPerBeat * 4u;
    case AppState::GateDivision::kOneOverOne:
    default: return kTicksPerBeat;
  }
}

struct EnergyProbe {
  float rms{0.0f};
  float peak{0.0f};
};

EnergyProbe measureEnergy(const float* left, const float* right, std::size_t frames) {
  EnergyProbe probe{};
  if (!left || frames == 0) {
    return probe;
  }

  double sumSquares = 0.0;
  float peak = 0.0f;
  const double samples = static_cast<double>(frames) * (right ? 2.0 : 1.0);
  for (std::size_t i = 0; i < frames; ++i) {
    const float l = left[i];
    peak = std::max(peak, std::fabs(l));
    sumSquares += static_cast<double>(l) * static_cast<double>(l);
    if (right) {
      const float r = right[i];
      peak = std::max(peak, std::fabs(r));
      sumSquares += static_cast<double>(r) * static_cast<double>(r);
    }
  }

  probe.peak = peak;
  if (samples > 0.0) {
    probe.rms = static_cast<float>(std::sqrt(sumSquares / samples));
  }
  return probe;
}

bool bufferClipDetected(const float* left, const float* right, std::size_t frames) {
  if (!left || frames == 0) {
    return false;
  }
  constexpr float kClipThreshold = 0.999f;
  for (std::size_t i = 0; i < frames; ++i) {
    if (std::fabs(left[i]) >= kClipThreshold) {
      return true;
    }
    if (right && std::fabs(right[i]) >= kClipThreshold) {
      return true;
    }
  }
  return false;
}

template <size_t N>
void writeDisplayField(char (&dst)[N], std::string_view text) {
  static_assert(N > 0, "Display field must have space for a terminator");
  const size_t maxCopy = N - 1;
  const size_t copyLen = std::min(text.size(), maxCopy);
  std::memcpy(dst, text.data(), copyLen);
  dst[copyLen] = '\0';
  if (copyLen < maxCopy) {
    std::fill(dst + copyLen + 1, dst + N, '\0');
  }
}

template <std::size_t N, typename... Args>
std::string_view formatScratch(std::array<char, N>& scratch, const char* fmt, Args&&... args) {
  // Minimal printf wrapper that clamps its output so we never scribble beyond
  // the OLED text buffers.
  const int written = std::snprintf(scratch.data(), scratch.size(), fmt, std::forward<Args>(args)...);
  if (written <= 0) {
    scratch[0] = '\0';
    return std::string_view{};
  }
  const size_t len = std::min(static_cast<size_t>(written), scratch.size() - 1);
  return std::string_view(scratch.data(), len);
}

template <std::size_t N>
void writeUiField(std::array<char, N>& dst, std::string_view text) {
  static_assert(N > 0, "UI field must have space for a terminator");
  const size_t maxCopy = N - 1;
  const size_t copyLen = std::min(text.size(), maxCopy);
  if (copyLen > 0) {
    std::memcpy(dst.data(), text.data(), copyLen);
  }
  dst[copyLen] = '\0';
  if (copyLen + 1 < N) {
    std::fill(dst.begin() + static_cast<std::ptrdiff_t>(copyLen + 1), dst.end(), '\0');
  }
}

uint8_t sanitizeEngine(const EngineRouter& router, uint8_t engine) {
  // Engine IDs arrive from MIDI CCs and debug tools, so we modulo them into the
  // valid range to avoid out-of-bounds dispatches.
  const std::size_t count = router.engineCount();
  if (count == 0) {
    return 0;
  }
  return static_cast<uint8_t>(engine % count);
}

const char* engineLongName(uint8_t engine) {
  switch (engine) {
    case 0: return "Sampler";
    case 1: return "Granular";
    case 2: return "Resonator";
    case 3: return "Euclid";
    case 4: return "Burst";
    case 5: return "Toy";
    default: return "Unknown";
  }
}

float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

constexpr uint32_t kSaltRepeatBias = 0xD667788Au;

constexpr float kNormalizedRange = 1.0f / 16777216.0f;

float deterministicNormalizedValue(uint32_t masterSeed, std::size_t slot, uint32_t salt) {
  uint32_t state = masterSeed ? masterSeed : 0x5EEDB0B1u;
  state ^= static_cast<uint32_t>(slot) * 0x9E3779B9u;
  state ^= salt;
  const uint32_t hashed = RNG::xorshift(state);
  return (hashed >> 8) * kNormalizedRange;
}

seedbox::io::Store* ensureStore(seedbox::io::Store* current) {
  // Quiet mode still deserves a live store so reads work in lessons; the backend
  // itself is responsible for short-circuiting writes when QUIET_MODE stays on.
  if (current) {
    return current;
  }

#if SEEDBOX_HW
  static seedbox::io::StoreEeprom hwStore;
  return &hwStore;
#else
  static seedbox::io::StoreEeprom simStore(4096);
  return &simStore;
#endif
}
}

constexpr std::uint32_t buttonMask(hal::Board::ButtonID id) {
  return 1u << static_cast<std::uint32_t>(id);
}

constexpr std::uint32_t buttonMask(std::initializer_list<hal::Board::ButtonID> ids) {
  std::uint32_t mask = 0;
  for (auto id : ids) {
    mask |= buttonMask(id);
  }
  return mask;
}

struct ModeTransition {
  AppState::Mode from;
  InputEvents::Type trigger;
  std::uint32_t buttons;
  AppState::Mode to;
};

constexpr std::array<ModeTransition, 31> kModeTransitions{{
    {AppState::Mode::HOME, InputEvents::Type::ButtonPress, buttonMask(hal::Board::ButtonID::EncoderSeedBank),
     AppState::Mode::SEEDS},
    {AppState::Mode::HOME, InputEvents::Type::ButtonPress, buttonMask(hal::Board::ButtonID::EncoderDensity),
     AppState::Mode::ENGINE},
    {AppState::Mode::HOME, InputEvents::Type::ButtonPress, buttonMask(hal::Board::ButtonID::EncoderToneTilt),
     AppState::Mode::PERF},
    {AppState::Mode::HOME, InputEvents::Type::ButtonPress, buttonMask(hal::Board::ButtonID::EncoderFxMutate),
     AppState::Mode::UTIL},
    {AppState::Mode::SWING, InputEvents::Type::ButtonPress, buttonMask(hal::Board::ButtonID::EncoderSeedBank),
     AppState::Mode::SEEDS},
    {AppState::Mode::SWING, InputEvents::Type::ButtonPress, buttonMask(hal::Board::ButtonID::EncoderDensity),
     AppState::Mode::ENGINE},
    {AppState::Mode::SWING, InputEvents::Type::ButtonPress, buttonMask(hal::Board::ButtonID::EncoderToneTilt),
     AppState::Mode::PERF},
    {AppState::Mode::SWING, InputEvents::Type::ButtonPress, buttonMask(hal::Board::ButtonID::EncoderFxMutate),
     AppState::Mode::UTIL},
    {AppState::Mode::HOME, InputEvents::Type::ButtonDoublePress, buttonMask(hal::Board::ButtonID::TapTempo),
     AppState::Mode::SETTINGS},
    {AppState::Mode::SEEDS, InputEvents::Type::ButtonDoublePress, buttonMask(hal::Board::ButtonID::TapTempo),
     AppState::Mode::SETTINGS},
    {AppState::Mode::ENGINE, InputEvents::Type::ButtonDoublePress, buttonMask(hal::Board::ButtonID::TapTempo),
     AppState::Mode::SETTINGS},
    {AppState::Mode::PERF, InputEvents::Type::ButtonDoublePress, buttonMask(hal::Board::ButtonID::TapTempo),
     AppState::Mode::SETTINGS},
    {AppState::Mode::UTIL, InputEvents::Type::ButtonDoublePress, buttonMask(hal::Board::ButtonID::TapTempo),
     AppState::Mode::SETTINGS},
    {AppState::Mode::SWING, InputEvents::Type::ButtonDoublePress, buttonMask(hal::Board::ButtonID::TapTempo),
     AppState::Mode::SETTINGS},
    {AppState::Mode::SETTINGS, InputEvents::Type::ButtonDoublePress, buttonMask(hal::Board::ButtonID::TapTempo),
     AppState::Mode::HOME},
    {AppState::Mode::SEEDS, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::Shift),
     AppState::Mode::HOME},
    {AppState::Mode::ENGINE, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::Shift),
     AppState::Mode::HOME},
    {AppState::Mode::PERF, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::Shift),
     AppState::Mode::HOME},
    {AppState::Mode::UTIL, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::Shift),
     AppState::Mode::HOME},
    {AppState::Mode::SETTINGS, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::Shift),
     AppState::Mode::HOME},
    {AppState::Mode::HOME, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::Shift),
     AppState::Mode::HOME},
    {AppState::Mode::SWING, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::Shift),
     AppState::Mode::HOME},
    {AppState::Mode::HOME, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::AltSeed),
     AppState::Mode::HOME},
    {AppState::Mode::SEEDS, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::AltSeed),
     AppState::Mode::HOME},
    {AppState::Mode::ENGINE, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::AltSeed),
     AppState::Mode::HOME},
    {AppState::Mode::PERF, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::AltSeed),
     AppState::Mode::HOME},
    {AppState::Mode::UTIL, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::AltSeed),
     AppState::Mode::HOME},
    {AppState::Mode::SETTINGS, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::AltSeed),
     AppState::Mode::HOME},
    {AppState::Mode::SETTINGS, InputEvents::Type::ButtonChord,
     buttonMask({hal::Board::ButtonID::Shift, hal::Board::ButtonID::AltSeed}), AppState::Mode::PERF},
    {AppState::Mode::PERF, InputEvents::Type::ButtonChord,
     buttonMask({hal::Board::ButtonID::Shift, hal::Board::ButtonID::AltSeed}), AppState::Mode::SETTINGS},
    {AppState::Mode::SWING, InputEvents::Type::ButtonChord,
     buttonMask({hal::Board::ButtonID::Shift, hal::Board::ButtonID::AltSeed}), AppState::Mode::PERF},
}};

AppState::AppState(hal::Board& board) : board_(board), input_(board) {
  selectClockProvider(&internalClock_);
  applySwingPercent(swingPercent_);
  static bool ioInitialised = false;
  if (!ioInitialised) {
    hal::io::init(kFrontPanelPins.data(), kFrontPanelPins.size());
    ioInitialised = true;
  }
  hal::io::setDigitalCallback(&AppState::digitalCallbackThunk, this);
  Storage::registerApp(*this);
}

void AppState::audioCallbackTrampoline(const hal::audio::StereoBufferView& buffer, void* ctx) {
  if (auto* self = static_cast<AppState*>(ctx)) {
    self->handleAudio(buffer);
  }
}

AppState::~AppState() {
  Storage::unregisterApp(*this);
  hal::audio::stop();
  hal::audio::shutdown();
}

void AppState::digitalCallbackThunk(hal::io::PinNumber pin, bool level, std::uint32_t timestamp,
                                    void* ctx) {
  if (auto* self = static_cast<AppState*>(ctx)) {
    self->handleDigitalEdge(static_cast<uint8_t>(pin), level, timestamp);
  }
}

// initHardware wires up the physical instrument: MIDI ingress, the engine
// router in hardware mode, and finally the deterministic seed prime. The code
// mirrors initSim because we want both paths to be interchangeable in class —
// students can run the simulator on a laptop and trust that the Teensy build
// behaves the same.
void AppState::initHardware() {
  store_ = ensureStore(store_);
#if SEEDBOX_HW
  // Lock in the entire audio buffer pool before any engine spins up. We slam
  // all four line items from AudioMemoryBudget into one call so individual
  // init() routines can't accidentally stomp the global allocator mid-set.
  AudioMemory(AudioMemoryBudget::kTotalBlocks);
#endif
  configureMidiRouting();
  hal::audio::init(&AppState::audioCallbackTrampoline, this);
  audioRuntime_.resetHostState(false);
  hal::audio::start();
  hal::io::writeDigital(kStatusLedPin, false);
  bootRuntime(EngineRouter::Mode::kHardware, true);
#if SEEDBOX_HW
  midi.markAppReady();
#endif
}

// initSim is the lab-friendly twin of initHardware. No MIDI bootstrap, but the
// rest of the wiring is identical so deterministic behaviour survives unit
// tests and lecture demos.
void AppState::initSim() {
  store_ = ensureStore(store_);
  #if !SEEDBOX_HW
  const auto teachingPreset = loadTeachingPresetForSim();
#endif
  hal::audio::init(&AppState::audioCallbackTrampoline, this);
  audioRuntime_.resetHostState(false);
  hal::audio::stop();
  hal::io::writeDigital(kStatusLedPin, false);
  bootRuntime(EngineRouter::Mode::kSim, false);
#if !SEEDBOX_HW
  if (teachingPreset && !seedPrimeBypassEnabled_) {
    applyPreset(*teachingPreset, false);
  }
#endif
}

#if !SEEDBOX_HW
void AppState::initJuceHost(float sampleRate, std::size_t framesPerBlock) {
  store_ = ensureStore(store_);
  configureMidiRouting();
  hal::audio::init(&AppState::audioCallbackTrampoline, this);
  hal::audio::configureHostStream(sampleRate, framesPerBlock);
  audioRuntime_.resetHostState(true);
  // JUCE-only boot path: load a default preset before the host spins audio so
  // the very first render has a deterministic, teachable seed table. We try
  // a built-in preset snapshot generated from the current master seed. Desktop
  // plugin instances should boot into an audible effect-forward state rather
  // than inheriting a stale local store preset.
  seedbox::Preset bootPreset{};
  bootPreset.slot = std::string(kDefaultPresetSlot);
  bootPreset.masterSeed = masterSeed_;
  bootPreset.focusSeed = 0;
  bootPreset.clock.bpm = scheduler_.bpm();
  bootPreset.seeds = gSeedPrimeController.buildSeeds(
      SeedPrimeController::Mode::kLfsr, bootPreset.masterSeed, kSeedSlotCount, randomnessPanel_.entropy,
      randomnessPanel_.mutationRate, 120.f, liveCaptureLineage_, liveCaptureSlot_, presetBuffer_.seeds,
      presetBuffer_.id);
  if (bootPreset.seeds.empty()) {
    const uint32_t fallbackSeed = bootPreset.masterSeed ? bootPreset.masterSeed : masterSeed_;
    bootPreset.seeds = gSeedPrimeController.buildSeeds(
        SeedPrimeController::Mode::kLfsr, fallbackSeed, kSeedSlotCount, randomnessPanel_.entropy,
        randomnessPanel_.mutationRate, 120.f, liveCaptureLineage_, liveCaptureSlot_, presetBuffer_.seeds,
        presetBuffer_.id);
  }
  configureDesktopBootSeeds(bootPreset.seeds);
  if (!bootPreset.seeds.empty()) {
    masterSeed_ = bootPreset.masterSeed ? bootPreset.masterSeed : masterSeed_;
    focusSeed_ = bootPreset.focusSeed;
    setSeedPreset(masterSeed_, bootPreset.seeds);
    setSeedPrimeMode(SeedPrimeMode::kPreset);
  }
  hal::audio::start();
  hal::io::writeDigital(kStatusLedPin, false);
  bootRuntime(EngineRouter::Mode::kSim, false);
  midi.markAppReady();
}
#endif

void AppState::configureMidiRouting() {
  midi.begin();
  midi.setClockHandler([this]() { onExternalClockTick(); });
  midi.setStartHandler([this]() { onExternalTransportStart(); });
  midi.setStopHandler([this]() { onExternalTransportStop(); });
  midi.setControlChangeHandler(
      [this](uint8_t ch, uint8_t cc, uint8_t val) { onExternalControlChange(ch, cc, val); });

  MidiRouter::ChannelMap trsChannelMap;
  for (auto& ch : trsChannelMap.inbound) {
    ch = seedbox::interop::mn42::kDefaultChannel;
  }
  for (auto& ch : trsChannelMap.outbound) {
    ch = seedbox::interop::mn42::kDefaultChannel;
  }
  midi.setChannelMap(MidiRouter::Port::kTrsA, trsChannelMap);

  std::array<MidiRouter::RouteConfig, MidiRouter::kPortCount> perfRoutes{};
  const std::size_t usbIndex = static_cast<std::size_t>(MidiRouter::Port::kUsb);
  const std::size_t trsIndex = static_cast<std::size_t>(MidiRouter::Port::kTrsA);
  perfRoutes[usbIndex].acceptClock = true;
  perfRoutes[usbIndex].acceptTransport = true;
  perfRoutes[usbIndex].acceptControlChange = true;
  perfRoutes[usbIndex].mirrorClock = true;
  perfRoutes[usbIndex].mirrorTransport = true;
  perfRoutes[trsIndex] = perfRoutes[usbIndex];

  std::array<MidiRouter::RouteConfig, MidiRouter::kPortCount> editRoutes{};
  for (auto& cfg : editRoutes) {
    cfg.acceptControlChange = true;
    cfg.acceptClock = false;
    cfg.acceptTransport = false;
    cfg.mirrorClock = false;
    cfg.mirrorTransport = false;
  }

  midi.configurePageRouting(MidiRouter::Page::kPerf, perfRoutes);
  midi.configurePageRouting(MidiRouter::Page::kEdit, editRoutes);
  midi.configurePageRouting(MidiRouter::Page::kHack, editRoutes);
  midi.activatePage(MidiRouter::Page::kPerf);
}

void AppState::bootRuntime(EngineRouter::Mode mode, bool hardwareMode) {
  engines_.init(mode);
  enginesReady_ = true;
  engines_.granular().setMaxActiveVoices(hardwareMode ? 36 : 12);
  engines_.granular().armLiveInput(hardwareMode);
  populateSdClips(engines_.granular());
  granularStats_ = engines_.granular().stats();
  engines_.resonator().setMaxVoices(hardwareMode ? 10 : 4);
  engines_.resonator().setDampingRange(0.18f, 0.92f);
  ClockProvider* provider = clockTransport_.followExternalClockEnabled() ? static_cast<ClockProvider*>(&midiClockIn_)
                                                                         : static_cast<ClockProvider*>(&internalClock_);
  selectClockProvider(provider);
  applySwingPercent(swingPercent_);
  if (seedPrimeBypassEnabled_) {
    scheduler_ = PatternScheduler{};
    const float bpm = (seedPrimeMode_ == SeedPrimeMode::kTapTempo) ? currentTapTempoBpm() : 120.f;
    setTempoTarget(bpm, true);
    scheduler_.setTriggerCallback(&engines_, &EngineRouter::dispatchThunk);
    const bool hardwareModeFlag = (engines_.granular().mode() == GranularEngine::Mode::kHardware);
    scheduler_.setSampleClockFn(hardwareModeFlag ? &hal::audio::sampleClock : nullptr);
    seeds_.assign(kSeedSlotCount, Seed{});
    seedEngineSelections_.assign(kSeedSlotCount, 0);
    seedsPrimed_ = false;
    setFocusSeed(focusSeed_);
  } else {
    reseed(masterSeed_);
  }
  scheduler_.setSampleClockFn(hardwareMode ? &hal::audio::sampleClock : nullptr);
  captureDisplaySnapshot(displayCache_, uiStateCache_);
  displayDirty_ = true;
  audioRuntime_.resetAudioCallbackCount();
  clearPresetCrossfade();
  presetController_.setActivePresetSlot(std::string{});
  currentPage_ = Page::kSeeds;
  storageButtonHeld_ = false;
  storageLongPress_ = false;
  storageButtonPressFrame_ = frame_;
  quantizeScaleIndex_ = 0;
  quantizeRoot_ = 0;
  mode_ = Mode::HOME;
  input_.clear();
  swingPageRequested_ = false;
  swingEditing_ = false;
  previousModeBeforeSwing_ = Mode::HOME;
  quantizeScaleIndex_ = 0;
  quantizeRoot_ = 0;
}

void AppState::handleAudio(const hal::audio::StereoBufferView& buffer) {
  audioRuntime_.incrementAudioCallbackCount();
  if (!buffer.left || !buffer.right || buffer.frames == 0) {
    return;
  }

  if (!dryInputLeft_.empty()) {
    const std::size_t probeFrames = std::min<std::size_t>(buffer.frames, dryInputLeft_.size());
    const float* dryLeft = dryInputLeft_.data();
    const float* dryRight = (!dryInputRight_.empty() && dryInputRight_.size() >= probeFrames)
                                ? dryInputRight_.data()
                                : dryLeft;
    const EnergyProbe probe = measureEnergy(dryLeft, dryRight, probeFrames);
    updateInputGateState(probe.rms, probe.peak);
  } else {
    updateInputGateState(0.0f, 0.0f);
  }

  const float* dryLeftInput = dryInputLeft_.empty() ? nullptr : dryInputLeft_.data();
  const float* dryRightInput = (!dryInputRight_.empty() && dryInputRight_.size() >= buffer.frames)
                                   ? dryInputRight_.data()
                                   : dryLeftInput;
  Engine::RenderContext ctx{dryLeftInput, dryRightInput, buffer.left, buffer.right, buffer.frames};

  // Start from silence, then let the engines (or a test tone) paint over the
  // scratch pad. If they stay quiet we fall back to the cached host input so
  // the JUCE builds mirror the “engine/test tone beats passthrough” contract in
  // SeedboxAudioProcessor::processBlock.
  std::fill(buffer.left, buffer.left + buffer.frames, 0.0f);
  std::fill(buffer.right, buffer.right + buffer.frames, 0.0f);

  if (audioRuntime_.testToneEnabled()) {
    audioRuntime_.renderTestTone(buffer.left, buffer.right, buffer.frames, hal::audio::sampleRate());
  }

  engines_.sampler().renderAudio(ctx);
  engines_.granular().renderAudio(ctx);
  engines_.resonator().renderAudio(ctx);
  engines_.euclid().renderAudio(ctx);
  engines_.burst().renderAudio(ctx);

#if !SEEDBOX_HW
  if (!dryInputLeft_.empty() && !seeds_.empty()) {
    const std::size_t idx = static_cast<std::size_t>(focusSeed_) % seeds_.size();
    engines_.processInputAudio(seeds_[idx], ctx);
  }
#endif

  constexpr float kPassthroughFloor = hal::audio::kEnginePassthroughFloor;
  const bool enginesIdle [[maybe_unused]] =
      hal::audio::bufferEngineIdle(buffer.left, buffer.right, buffer.frames,
                                   hal::audio::kEngineIdleEpsilon, hal::audio::kEngineIdleRmsSlack);

#if !(SEEDBOX_HW && QUIET_MODE)
  (void)enginesIdle;
#endif

#if SEEDBOX_HW && QUIET_MODE
  // Classroom rigs built in quiet mode keep their codecs muted; we still tick
  // the callback counter above so timing-sensitive tests can probe the audio
  // heartbeat without blasting speakers.
  if (enginesIdle) {
    return;
  }
#endif

  if (!audioRuntime_.hostAudioMode() && !dryInputLeft_.empty()) {
    const std::size_t copyFrames = std::min<std::size_t>(buffer.frames, dryInputLeft_.size());
    const float* dryLeft = dryInputLeft_.data();
    const float* dryRight = (!dryInputRight_.empty() && dryInputRight_.size() >= copyFrames)
                                ? dryInputRight_.data()
                                : dryLeft;

    const bool dryAudible = hal::audio::bufferHasEngineEnergy(dryLeft, dryRight, copyFrames,
                                                             kPassthroughFloor);
    const bool enginesAudible = hal::audio::bufferHasEngineEnergy(buffer.left, buffer.right,
                                                                 buffer.frames, kPassthroughFloor);

    if (dryAudible && !enginesAudible) {
      std::copy(dryLeft, dryLeft + copyFrames, buffer.left);
      std::copy(dryRight, dryRight + copyFrames, buffer.right);
    }
  }

  if (audioRuntime_.hostAudioMode()) {
    audioRuntime_.applyHostOutputSafety(buffer.left, buffer.right, buffer.frames);
  }

  const auto leftEnergy = measureEnergy(buffer.left, nullptr, buffer.frames);
  const auto rightEnergy = measureEnergy(buffer.right, nullptr, buffer.frames);
  const auto combinedEnergy = measureEnergy(buffer.left, buffer.right, buffer.frames);
  latestAudioMetrics_.leftRms = leftEnergy.rms;
  latestAudioMetrics_.rightRms = rightEnergy.rms;
  latestAudioMetrics_.combinedRms = combinedEnergy.rms;
  latestAudioMetrics_.leftPeak = leftEnergy.peak;
  latestAudioMetrics_.rightPeak = rightEnergy.peak;
  latestAudioMetrics_.combinedPeak = combinedEnergy.peak;
  latestAudioMetrics_.clip = bufferClipDetected(buffer.left, buffer.right, buffer.frames);
  latestAudioMetrics_.limiter = latestAudioMetrics_.clip;
}

void AppState::handleDigitalEdge(uint8_t pin, bool level, uint32_t timestamp) {
  if (pin == kReseedButtonPin) {
    if (level) {
      storageButtonHeld_ = true;
      storageLongPress_ = false;
      storageButtonPressFrame_ = frame_;
      if (currentPage_ != Page::kStorage) {
        reseedRequested_ = true;
      }
      return;
    }

    if (!storageButtonHeld_) {
      return;
    }

    storageButtonHeld_ = false;
    const uint64_t heldFrames = (frame_ > storageButtonPressFrame_)
                                    ? (frame_ - storageButtonPressFrame_)
                                    : 0ULL;
    const bool longPress = heldFrames >= kStorageLongPressFrames;

    if (currentPage_ != Page::kStorage) {
      return;
    }

    const std::string slotName = presetController_.activePresetSlot().empty()
                                     ? std::string(kDefaultPresetSlot)
                                     : presetController_.activePresetSlot();
    if (longPress) {
      storageLongPress_ = true;
      savePreset(slotName);
    } else {
      storageLongPress_ = false;
      recallPreset(slotName, true);
    }
    return;
  }

  if (pin != kLockButtonPin) {
    return;
  }

  if (level) {
    lockButtonHeld_ = true;
    lockButtonPressTimestamp_ = timestamp;
    return;
  }

  if (!lockButtonHeld_) {
    return;
  }

  lockButtonHeld_ = false;
  const uint32_t heldUs = (timestamp >= lockButtonPressTimestamp_)
                              ? (timestamp - lockButtonPressTimestamp_)
                              : 0u;
  const bool longPress = heldUs >= kLockLongPressUs;
  if (longPress) {
    seedPageToggleGlobalLock();
  } else {
    seedPageToggleLock(focusSeed_);
  }
  displayDirty_ = true;
}

// tick is the heartbeat. Every call either primes seeds (first-run), lets the
// internal scheduler drive things when we own the transport, or just counts
// frames so the OLED can display a ticking counter.
void AppState::tick() {
  hal::io::poll();
  board_.poll();
  input_.update();
  InputGestureRouter{}.process(*this);
  updateTempoSmoothing();
  updateExternalClockWatchdog();
  if (swingPageRequested_) {
    swingPageRequested_ = false;
    enterSwingMode();
  }
  if (reseedRequested_) {
    uint32_t base = masterSeed_ ? masterSeed_ : 0x5EEDB0B1u;
    const uint32_t nextSeed = RNG::xorshift(base);
    reseed(nextSeed);
    reseedRequested_ = false;
  }
  if (!seedsPrimed_ && !seedPrimeBypassEnabled_) {
    reseed(masterSeed_);
  }
  if (!externalClockDominant()) {
    if (!clock()) {
      selectClockProvider(&internalClock_);
    }
    if (panicSkipNextTick_) {
      panicSkipNextTick_ = false;
    } else {
      clock()->onTick();
      handleGateTick();
      maybeCommitPendingPreset(scheduler_.ticks());
    }
  } else if (!externalTransportRunning()) {
    // If we're waiting on an external clock that hasn't started yet, apply any
    // pending preset step immediately so storage recalls don't stall.
    maybeCommitPendingPreset(scheduler_.ticks());
  }
  stepPresetCrossfade();
  ++frame_;
  granularStats_ = engines_.granular().stats();
  captureDisplaySnapshot(displayCache_, uiStateCache_);
  displayDirty_ = true;
}

bool AppState::handleSeedPrimeGesture(const InputEvents::Event& evt) {
  if (evt.type != InputEvents::Type::ButtonPress) {
    return false;
  }
  if (evt.primaryButton != hal::Board::ButtonID::TapTempo) {
    return false;
  }
  if (!input_.buttonDown(hal::Board::ButtonID::AltSeed)) {
    return false;
  }

  const bool reverse = input_.buttonDown(hal::Board::ButtonID::Shift);
  const int step = reverse ? -1 : 1;
  const SeedPrimeMode nextMode =
      toAppSeedPrimeMode(gSeedPrimeController.rotateMode(toPrimeControllerMode(seedPrimeMode_), step));
  setSeedPrimeMode(nextMode);
  seedPageReseed(masterSeed_, nextMode);
  return true;
}

bool AppState::handleClockButtonEvent(const InputEvents::Event& evt) {
  if (evt.primaryButton != hal::Board::ButtonID::TapTempo) {
    return false;
  }
  if (mode_ == Mode::SWING && evt.type == InputEvents::Type::ButtonPress) {
    return false;
  }
  if (evt.type == InputEvents::Type::ButtonLongPress) {
    if (mode_ == Mode::SWING) {
      return true;
    }
    swingPageRequested_ = true;
    displayDirty_ = true;
    return true;
  }
  if (evt.type == InputEvents::Type::ButtonPress) {
    if (seedPrimeMode_ == SeedPrimeMode::kTapTempo) {
      if (lastTapTempoTapUs_ != 0 && evt.timestampUs > lastTapTempoTapUs_) {
        const uint64_t deltaUs = evt.timestampUs - lastTapTempoTapUs_;
        const uint64_t intervalMs64 = deltaUs / 1000ULL;
        if (intervalMs64 > 0) {
          const uint64_t clamped =
              std::min<uint64_t>(intervalMs64, static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()));
          const uint32_t intervalMs = static_cast<uint32_t>(clamped);
          recordTapTempoInterval(intervalMs);
          setTempoTarget(currentTapTempoBpm(), false);
        }
      }
      lastTapTempoTapUs_ = evt.timestampUs;
    } else {
      lastTapTempoTapUs_ = 0;
    }
    toggleClockProvider();
    if (mode_ == Mode::PERF) {
      toggleTransportLatchedRunning();
    }
    displayDirty_ = true;
    return true;
  }
  return false;
}

void AppState::applyModeTransition(const InputEvents::Event& evt) {
  std::uint32_t mask = 0;
  if (!evt.buttons.empty()) {
    for (auto id : evt.buttons) {
      mask |= buttonMask(id);
    }
  } else {
    mask = buttonMask(evt.primaryButton);
  }

  for (const auto& transition : kModeTransitions) {
    if (transition.from == mode_ && transition.trigger == evt.type && transition.buttons == mask) {
      const Mode fromMode = mode_;
      if (transition.trigger == InputEvents::Type::ButtonLongPress &&
          transition.buttons == buttonMask(hal::Board::ButtonID::AltSeed)) {
        setPage(Page::kStorage);
        storageButtonHeld_ = false;
        storageLongPress_ = false;
      }
      if (mode_ != transition.to) {
        if (fromMode == Mode::SWING && transition.to != Mode::SWING) {
          swingEditing_ = false;
          previousModeBeforeSwing_ = transition.to;
        }
        mode_ = transition.to;
        displayDirty_ = true;
      }
      return;
    }
  }
}

void AppState::dispatchToPage(const InputEvents::Event& evt) {
  using ModeHandler = void (AppState::*)(const InputEvents::Event&);
  struct ModeDispatch {
    Mode mode;
    ModeHandler handler;
  };
  static constexpr std::array<ModeDispatch, 7> kModeHandlers{{
      {Mode::HOME, &AppState::handleHomeEvent},
      {Mode::SEEDS, &AppState::handleSeedsEvent},
      {Mode::ENGINE, &AppState::handleEngineEvent},
      {Mode::PERF, &AppState::handlePerfEvent},
      {Mode::SETTINGS, &AppState::handleSettingsEvent},
      {Mode::UTIL, &AppState::handleUtilEvent},
      {Mode::SWING, &AppState::handleSwingEvent},
  }};

  for (const auto& entry : kModeHandlers) {
    if (entry.mode == mode_) {
      (this->*entry.handler)(evt);
      return;
    }
  }
}

namespace {
bool eventHasButton(const InputEvents::Event& evt, hal::Board::ButtonID id) {
  return std::find(evt.buttons.begin(), evt.buttons.end(), id) != evt.buttons.end();
}
}  // namespace

void AppState::handleHomeEvent(const InputEvents::Event& evt) {
  if (evt.type == InputEvents::Type::EncoderHoldTurn &&
      evt.encoder == hal::Board::EncoderID::SeedBank &&
      eventHasButton(evt, hal::Board::ButtonID::Shift)) {
    if (evt.encoderDelta != 0 && !seeds_.empty()) {
      const int32_t next = static_cast<int32_t>(focusSeed_) + evt.encoderDelta;
      setFocusSeed(static_cast<uint8_t>(next));
      displayDirty_ = true;
    }
  }
}

void AppState::handleSeedsEvent(const InputEvents::Event& evt) {
  if (evt.type == InputEvents::Type::EncoderTurn &&
      evt.encoder == hal::Board::EncoderID::SeedBank) {
    if (evt.encoderDelta != 0 && !seeds_.empty()) {
      const int32_t next = static_cast<int32_t>(focusSeed_) + evt.encoderDelta;
      setFocusSeed(static_cast<uint8_t>(next));
      displayDirty_ = true;
    }
  }

  if (evt.type == InputEvents::Type::EncoderHoldTurn &&
      evt.encoder == hal::Board::EncoderID::Density &&
      eventHasButton(evt, hal::Board::ButtonID::Shift)) {
    if (evt.encoderDelta != 0) {
      stepGateDivision(static_cast<int>(evt.encoderDelta));
    }
    return;
  }

  if (evt.encoderDelta == 0) {
    return;
  }

  if (evt.type == InputEvents::Type::EncoderHoldTurn &&
      evt.encoder == hal::Board::EncoderID::ToneTilt) {
    // Tone/Tilt doubles as the "micro edit" knob on the Seeds page. We lean on
    // modifier buttons so the detents stay musical: Shift now flips the
    // granular engine between live input and SD clips, Alt still pushes density,
    // and holding both piggybacks the original pitch/density micro nudges.
    const bool shiftHeld = eventHasButton(evt, hal::Board::ButtonID::Shift);
    const bool altHeld = eventHasButton(evt, hal::Board::ButtonID::AltSeed);
    if (seeds_.empty()) {
      return;
    }
    const std::size_t focusIndex = std::min<std::size_t>(focusSeed_, seeds_.size() - 1);
    if (shiftHeld && !altHeld) {
      seedPageCycleGranularSource(static_cast<uint8_t>(focusIndex), evt.encoderDelta);
      return;
    }
    if (shiftHeld || altHeld) {
      SeedNudge nudge{};
      if (shiftHeld) {
        nudge.pitchSemitones = static_cast<float>(evt.encoderDelta);
      }
      if (altHeld) {
        constexpr float kDensityStep = 0.1f;
        nudge.densityDelta = static_cast<float>(evt.encoderDelta) * kDensityStep;
      }
      if (nudge.pitchSemitones != 0.f || nudge.densityDelta != 0.f) {
        seedPageNudge(static_cast<uint8_t>(focusIndex), nudge);
      }
      return;
    }
  }

  if (evt.type == InputEvents::Type::EncoderHoldTurn &&
      evt.encoder == hal::Board::EncoderID::FxMutate &&
      eventHasButton(evt, hal::Board::ButtonID::AltSeed)) {
    // Alt + FX becomes a quantize scale selector. Each detent marches through
    // the CC map locally so the hardware surface matches the MN-42 remote.
    constexpr int kScaleCount = 5;
    int next = static_cast<int>(quantizeScaleIndex_) + static_cast<int>(evt.encoderDelta);
    next %= kScaleCount;
    if (next < 0) {
      next += kScaleCount;
    }
    quantizeScaleIndex_ = static_cast<uint8_t>(next);
    const uint8_t controlValue = static_cast<uint8_t>((quantizeScaleIndex_ * 32u) + (quantizeRoot_ % 12u));
    applyQuantizeControl(controlValue);
    return;
  }
}

void AppState::handleEngineEvent(const InputEvents::Event& evt) {
  if (evt.type == InputEvents::Type::EncoderTurn &&
      evt.encoder == hal::Board::EncoderID::SeedBank && evt.encoderDelta != 0 && !seeds_.empty()) {
    const size_t focus = std::min<size_t>(focusSeed_, seeds_.size() - 1);
    const int32_t next = static_cast<int32_t>(focus) + evt.encoderDelta;
    setFocusSeed(static_cast<uint8_t>(next));
    displayDirty_ = true;
    return;
  }

  if (evt.type == InputEvents::Type::EncoderHoldTurn &&
      evt.encoder == hal::Board::EncoderID::Density && eventHasButton(evt, hal::Board::ButtonID::Shift)) {
    if (!seeds_.empty() && evt.encoderDelta != 0) {
      const size_t focus = std::min<size_t>(focusSeed_, seeds_.size() - 1);
      const uint8_t current = seeds_[focus].engine;
      const uint8_t next = static_cast<uint8_t>(static_cast<int>(current) + evt.encoderDelta);
      setSeedEngine(static_cast<uint8_t>(focus), next);
      displayDirty_ = true;
    }
    return;
  }

  if (seeds_.empty()) {
    return;
  }

  const size_t focus = std::min<size_t>(focusSeed_, seeds_.size() - 1);
  if (seedLock_.globalLocked() || seedLock_.seedLocked(focus)) {
    return;
  }

  const Seed& seed = seeds_[focus];
  const uint8_t engineId = seed.engine;

  if (evt.type == InputEvents::Type::EncoderTurn && evt.encoderDelta != 0 && engineId == EngineRouter::kEuclidId) {
    Engine::ParamChange change{};
    change.seedId = seed.id;
    switch (evt.encoder) {
      case hal::Board::EncoderID::Density: {
        change.id = static_cast<std::uint16_t>(EuclidEngine::Param::kSteps);
        change.value = static_cast<std::int32_t>(engines_.euclid().steps()) +
                       (evt.encoderDelta * kEuclidStepNudge);
        engines_.euclid().onParam(change);
        displayDirty_ = true;
        return;
      }
      case hal::Board::EncoderID::ToneTilt: {
        change.id = static_cast<std::uint16_t>(EuclidEngine::Param::kFills);
        change.value = static_cast<std::int32_t>(engines_.euclid().fills()) +
                       (evt.encoderDelta * kEuclidStepNudge);
        engines_.euclid().onParam(change);
        displayDirty_ = true;
        return;
      }
      case hal::Board::EncoderID::FxMutate: {
        change.id = static_cast<std::uint16_t>(EuclidEngine::Param::kRotate);
        change.value = static_cast<std::int32_t>(engines_.euclid().rotate()) +
                       (evt.encoderDelta * kEuclidStepNudge);
        engines_.euclid().onParam(change);
        displayDirty_ = true;
        return;
      }
      default:
        break;
    }
  }

  if (evt.type == InputEvents::Type::EncoderTurn && evt.encoderDelta != 0 && engineId == EngineRouter::kBurstId) {
    Engine::ParamChange change{};
    change.seedId = seed.id;
    switch (evt.encoder) {
      case hal::Board::EncoderID::Density: {
        change.id = static_cast<std::uint16_t>(BurstEngine::Param::kClusterCount);
        change.value = static_cast<std::int32_t>(engines_.burst().clusterCount()) + evt.encoderDelta;
        engines_.burst().onParam(change);
        displayDirty_ = true;
        return;
      }
      case hal::Board::EncoderID::ToneTilt: {
        change.id = static_cast<std::uint16_t>(BurstEngine::Param::kSpacingSamples);
        const int32_t delta = evt.encoderDelta * kBurstSpacingStepSamples;
        const int32_t current = static_cast<std::int32_t>(engines_.burst().spacingSamples());
        change.value = std::max<int32_t>(0, current + delta);
        engines_.burst().onParam(change);
        displayDirty_ = true;
        return;
      }
      default:
        break;
    }
  }
}

void AppState::handlePerfEvent(const InputEvents::Event& evt) {
  if (evt.type == InputEvents::Type::ButtonPress && evt.primaryButton == hal::Board::ButtonID::TapTempo) {
    toggleTransportLatchedRunning();
    displayDirty_ = true;
  }
}

void AppState::handleSettingsEvent(const InputEvents::Event& evt) {
  if (evt.type == InputEvents::Type::ButtonPress && evt.primaryButton == hal::Board::ButtonID::TapTempo) {
    setFollowExternalClockFromHost(!followExternalClockEnabled());
    displayDirty_ = true;
    return;
  }

  if (evt.type == InputEvents::Type::ButtonPress && evt.primaryButton == hal::Board::ButtonID::AltSeed) {
    setSeedPrimeBypassFromHost(!seedPrimeBypassEnabled_);
    return;
  }
}

void AppState::handleUtilEvent(const InputEvents::Event& evt) {
  if (evt.type == InputEvents::Type::EncoderTurn &&
      evt.encoder == hal::Board::EncoderID::FxMutate && evt.encoderDelta != 0) {
    debugMetersEnabled_ = evt.encoderDelta > 0 ? true : false;
    displayDirty_ = true;
  }
}

void AppState::handleSwingEvent(const InputEvents::Event& evt) {
  if (evt.type == InputEvents::Type::EncoderTurn || evt.type == InputEvents::Type::EncoderHoldTurn) {
    float step = 0.0f;
    switch (evt.encoder) {
      case hal::Board::EncoderID::SeedBank: step = 0.05f; break;
      case hal::Board::EncoderID::Density: step = 0.01f; break;
      default: break;
    }
    if (step != 0.0f && evt.encoderDelta != 0) {
      adjustSwing(step * static_cast<float>(evt.encoderDelta));
    }
  }

  if (evt.type == InputEvents::Type::ButtonPress && evt.primaryButton == hal::Board::ButtonID::TapTempo) {
    Mode target = previousModeBeforeSwing_;
    if (target == Mode::SWING) {
      target = Mode::HOME;
    }
    exitSwingMode(target);
  }
}

void AppState::handleReseedRequest() {
  reseedRequested_ = true;
  displayDirty_ = true;
}

void AppState::triggerLiveCaptureReseed() {
  // Momentary snapshot button: bump the capture counter, compute a deterministic
  // short-bank slot, and reseed in live-input mode so the granular lane keeps
  // hoovering the codec feed. The slot math is intentionally tiny so students
  // can replay it on paper.
  ++liveCaptureCounter_;
  liveCaptureSlot_ = static_cast<uint8_t>((liveCaptureCounter_ + liveCaptureVariation_) % kShortCaptureSlots);
  liveCaptureLineage_ = masterSeed_ ^ (liveCaptureCounter_ * 131u) ^ liveCaptureVariation_;
  if (liveCaptureLineage_ == 0) {
    liveCaptureLineage_ = masterSeed_ ? masterSeed_ : 0x5EEDB0B1u;
  }
  const uint32_t reseedValue = RNG::xorshift(liveCaptureLineage_);
  seedPageReseed(reseedValue, SeedPrimeMode::kLiveInput);
}

void AppState::requestPresetChange(const seedbox::Preset& preset, bool crossfade, PresetBoundary boundary) {
  presetController_.requestPresetChange(preset, crossfade, toPresetControllerBoundary(boundary), scheduler_.ticks());
  displayDirty_ = true;
}

void AppState::maybeCommitPendingPreset(uint64_t currentTick) {
  auto pending = presetController_.takePendingPreset(currentTick);
  if (!pending) {
    return;
  }
  applyPreset(pending->preset, pending->crossfade);
}

void AppState::triggerPanic() {
  scheduler_.clearPendingTriggers();
  engines_.panic();
  midi.panic();
  input_.clear();
  gateEdgePending_ = false;
  presetController_.clearPendingPresetRequest();
  clockTransport_.clearTransportGateHold();
  panicSkipNextTick_ = true;
  displayDirty_ = true;

#if SEEDBOX_HW
  Serial.println(F("PANIC: voices, queues, and transport cleared."));
#endif
}

const char* AppState::modeLabel(Mode mode) {
  switch (mode) {
    case Mode::HOME: return "HOME";
    case Mode::SEEDS: return "SEEDS";
    case Mode::ENGINE: return "ENGINE";
    case Mode::PERF: return "PERF";
    case Mode::SETTINGS: return "SET";
    case Mode::UTIL: return "UTIL";
    case Mode::SWING: return "SWING";
    default: return "?";
  }
}

void AppState::setModeFromHost(Mode mode) {
  if (mode == Mode::SWING) {
    enterSwingMode();
    return;
  }

  swingEditing_ = false;
  mode_ = mode;
  previousModeBeforeSwing_ = mode_;
  displayDirty_ = true;
}

void AppState::enterSwingMode() {
  if (mode_ != Mode::SWING) {
    previousModeBeforeSwing_ = mode_;
  }
  swingEditing_ = true;
  mode_ = Mode::SWING;
  applySwingPercent(swingPercent_);
  displayDirty_ = true;
}

void AppState::exitSwingMode(Mode targetMode) {
  swingEditing_ = false;
  previousModeBeforeSwing_ = targetMode;
  mode_ = targetMode;
  displayDirty_ = true;
}

void AppState::adjustSwing(float delta) {
  applySwingPercent(swingPercent_ + delta);
}

void AppState::applySwingPercent(float value) {
  const float clamped = std::clamp(value, 0.0f, 0.99f);
  const bool changed = std::fabs(clamped - swingPercent_) > 1e-5f;
  swingPercent_ = clamped;
  clockTransport_.applySwing(clamped);
  if (changed) {
    displayDirty_ = true;
  }
}

void AppState::selectClockProvider(ClockProvider* provider) {
  clockTransport_.selectClockProvider(provider);
}

void AppState::toggleClockProvider() {
  clockTransport_.toggleClockProvider();
  displayDirty_ = true;
}

void AppState::toggleTransportLatchedRunning() { clockTransport_.toggleTransportLatchedRunning(); }

// primeSeeds is where the deterministic world-building happens. We treat
// masterSeed as the only source of entropy, then spin a tiny RNG to fill each
// Seed with musically interesting defaults. The idea: instructors can step
// through this function and show how density, jitter, and engine-specific
// parameters fall out of simple pseudo-random math.
void AppState::primeSeeds(uint32_t masterSeed) {
  masterSeed_ = masterSeed ? masterSeed : 0x5EEDB0B1u;

  const std::vector<uint8_t> previousSelections = seedEngineSelections_;
  const std::vector<Seed> previousSeeds = seeds_;
  const uint8_t previousFocus = focusSeed_;
  const bool allowRepeatBias = (seedPrimeMode_ == SeedPrimeMode::kLfsr);
  const auto controllerMode = toPrimeControllerMode(seedPrimeMode_);
  const auto buildBatch = [&](std::size_t count) {
    return gSeedPrimeController.buildSeeds(controllerMode, masterSeed_, count, randomnessPanel_.entropy,
                                           randomnessPanel_.mutationRate, currentTapTempoBpm(),
                                           liveCaptureLineage_, liveCaptureSlot_, presetBuffer_.seeds,
                                           presetBuffer_.id);
  };

  seedLock_.resize(kSeedSlotCount);
  seedLock_.trim(kSeedSlotCount);

  std::vector<Seed> generated;
  if (seedPrimeBypassEnabled_) {
    generated.assign(kSeedSlotCount, Seed{});
    std::vector<Seed> focusOnly = buildBatch(1);
    if (focusOnly.empty()) {
      focusOnly = gSeedPrimeController.buildSeeds(SeedPrimeController::Mode::kLfsr, masterSeed_, 1,
                                                 randomnessPanel_.entropy, randomnessPanel_.mutationRate,
                                                 currentTapTempoBpm(), liveCaptureLineage_, liveCaptureSlot_,
                                                 presetBuffer_.seeds, presetBuffer_.id);
    }
    if (!focusOnly.empty()) {
      const std::size_t targetIndex = std::min<std::size_t>(focusSeed_, kSeedSlotCount - 1);
      Seed focused = focusOnly.front();
      focused.id = static_cast<uint32_t>(targetIndex);
      if (focused.prng == 0) {
        focused.prng = RNG::xorshift(masterSeed_);
      }
      generated[targetIndex] = focused;
    }
    if (allowRepeatBias) {
      applyRepeatBias(previousSeeds, generated);
    }
  } else if (!seedLock_.globalLocked() || previousSeeds.empty()) {
    generated = buildBatch(kSeedSlotCount);
    if (generated.empty()) {
      generated = gSeedPrimeController.buildSeeds(SeedPrimeController::Mode::kLfsr, masterSeed_,
                                                  kSeedSlotCount, randomnessPanel_.entropy,
                                                  randomnessPanel_.mutationRate, currentTapTempoBpm(),
                                                  liveCaptureLineage_, liveCaptureSlot_, presetBuffer_.seeds,
                                                  presetBuffer_.id);
    }
    if (generated.size() < kSeedSlotCount) {
      generated.resize(kSeedSlotCount);
    }
    if (generated.size() > kSeedSlotCount) {
      generated.resize(kSeedSlotCount);
    }
    if (allowRepeatBias) {
      applyRepeatBias(previousSeeds, generated);
    }
    for (std::size_t i = 0; i < generated.size(); ++i) {
      if (seedLock_.seedLocked(i) && i < previousSeeds.size()) {
        generated[i] = previousSeeds[i];
      } else {
        generated[i].id = static_cast<uint32_t>(i);
        if (generated[i].prng == 0) {
          generated[i].prng = RNG::xorshift(masterSeed_);
        }
      }
    }
  } else {
    generated = previousSeeds;
    if (generated.size() < kSeedSlotCount) {
      const auto topUp = gSeedPrimeController.buildSeeds(SeedPrimeController::Mode::kLfsr, masterSeed_,
                                                          kSeedSlotCount, randomnessPanel_.entropy,
                                                          randomnessPanel_.mutationRate, currentTapTempoBpm(),
                                                          liveCaptureLineage_, liveCaptureSlot_,
                                                          presetBuffer_.seeds, presetBuffer_.id);
      for (std::size_t i = generated.size(); i < kSeedSlotCount && i < topUp.size(); ++i) {
        generated.push_back(topUp[i]);
      }
    }
    if (generated.size() > kSeedSlotCount) {
      generated.resize(kSeedSlotCount);
    }
    for (std::size_t i = 0; i < generated.size(); ++i) {
      generated[i].id = static_cast<uint32_t>(i);
    }
  }

  seeds_ = generated;

  scheduler_ = PatternScheduler{};
  const float bpm = (seedPrimeMode_ == SeedPrimeMode::kTapTempo) ? currentTapTempoBpm() : 120.f;
  setTempoTarget(bpm, true);
  scheduler_.setTriggerCallback(&engines_, &EngineRouter::dispatchThunk);

  for (const Seed& seed : seeds_) {
    if (seed.prng != 0) {
      scheduler_.addSeed(seed);
    }
  }

  lastGateTick_ = scheduler_.ticks();
  gateEdgePending_ = false;

  seedEngineSelections_.assign(seeds_.size(), 0);
  for (std::size_t i = 0; i < seeds_.size(); ++i) {
    seeds_[i].id = static_cast<uint32_t>(i);
    if (seeds_[i].prng == 0) {
      seedEngineSelections_[i] = seeds_[i].engine;
      continue;
    }
    const uint8_t desired = (i < previousSelections.size()) ? previousSelections[i] : seeds_[i].engine;
    setSeedEngine(static_cast<uint8_t>(i), desired);
  }

  const bool hasSeedContent = std::any_of(seeds_.begin(), seeds_.end(), [](const Seed& s) { return s.prng != 0; });
  if (!seeds_.empty()) {
    const std::size_t maxIndex = seeds_.size() - 1;
    const uint8_t targetFocus = previousSeeds.empty()
                                    ? 0
                                    : static_cast<uint8_t>(std::min<std::size_t>(previousFocus, maxIndex));
    setFocusSeed(targetFocus);
  } else {
    focusSeed_ = 0;
  }

  seedsPrimed_ = hasSeedContent;
  clockTransport_.resetTransportState();
  const bool ledOn = !SeedBoxConfig::kQuietMode;
  hal::io::writeDigital(kStatusLedPin, ledOn);
  displayDirty_ = true;
}

void AppState::applyRepeatBias(const std::vector<Seed>& previousSeeds, std::vector<Seed>& generated) {
  if (randomnessPanel_.resetBehavior == RandomnessPanel::ResetBehavior::Hard) {
    return;
  }
  if (previousSeeds.empty() || generated.empty()) {
    return;
  }
  const float bias = std::clamp(randomnessPanel_.repeatBias, 0.0f, 1.0f);
  if (bias <= 0.0f) {
    return;
  }
  const std::size_t limit = std::min(previousSeeds.size(), generated.size());
  for (std::size_t i = 0; i < limit; ++i) {
    if (seedLock_.seedLocked(i)) {
      continue;
    }
    const float roll = deterministicNormalizedValue(masterSeed_, i, kSaltRepeatBias);
    if (roll >= bias) {
      continue;
    }
    if (randomnessPanel_.resetBehavior == RandomnessPanel::ResetBehavior::Drift) {
      generated[i] = blendSeeds(previousSeeds[i], generated[i], randomnessPanel_.entropy);
    } else {
      generated[i] = previousSeeds[i];
    }
  }
}

float AppState::currentTapTempoBpm() const {
  if (tapTempoHistory_.empty()) {
    return 120.f;
  }
  double total = 0.0;
  std::size_t count = 0;
  for (uint32_t interval : tapTempoHistory_) {
    if (interval == 0) {
      continue;
    }
    total += static_cast<double>(interval);
    ++count;
  }
  if (count == 0) {
    return 120.f;
  }
  const double averageMs = total / static_cast<double>(count);
  if (averageMs <= 0.0) {
    return 120.f;
  }
  return static_cast<float>(60000.0 / averageMs);
}

void AppState::onExternalClockTick() {
  if (panicSkipNextTick_) {
    panicSkipNextTick_ = false;
    return;
  }

  const uint32_t now = board_.nowMillis();

  if (!seedsPrimed_ && !seedPrimeBypassEnabled_) {
    primeSeeds(masterSeed_);
  }
#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  const bool wasDominant = externalClockDominant();
#endif
  clockTransport_.onExternalClockTick(now);
#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  if (!wasDominant && externalClockDominant()) {
    Serial.println(F("external clock: TRS/USB seized transport"));
  }
#endif
  if (clock() == &midiClockIn_ || externalClockDominant()) {
    midiClockIn_.onTick();
    handleGateTick();
    maybeCommitPendingPreset(scheduler_.ticks());
  }
}

void AppState::onExternalTransportStart() {
#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  const bool wasDominant = externalClockDominant();
#endif
  clockTransport_.onExternalTransportStart();
#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  if (!wasDominant && externalClockDominant()) {
    Serial.println(F("external clock: transport START"));
  }
#endif
}

void AppState::onExternalTransportStop() {
#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  const bool wasDominant = externalClockDominant();
#endif
  clockTransport_.onExternalTransportStop();
#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  if (wasDominant && !externalClockDominant()) {
    Serial.println(F("external clock: transport STOP"));
  }
#endif
}

void AppState::onExternalControlChange(uint8_t ch, uint8_t cc, uint8_t val) {
  using namespace seedbox::interop::mn42;
  if (ch == kDefaultChannel) {
    if (cc == cc::kHandshake) {
      if (val == handshake::kHello) {
        mn42HelloSeen_ = true;
      }
      return;
    }
    if (cc == cc::kMode) {
      applyMn42ModeBits(val);
      return;
    }
    if (cc == cc::kTransportGate) {
      handleTransportGate(val);
      return;
    }
  }
  if (cc == kEngineCycleCc) {
    const std::size_t engineCount = engines_.engineCount();
    if (engineCount == 0) {
      return;
    }
    if (seeds_.empty()) {
      return;
    }
    const size_t count = seeds_.size();
    const uint8_t focus = static_cast<uint8_t>(std::min<size_t>(focusSeed_, count - 1));
    const std::size_t current = static_cast<std::size_t>(seeds_[focus].engine % static_cast<uint8_t>(engineCount));
    // Our rule-of-thumb: CC values >= 64 spin the encoder clockwise (advance to
    // the next engine) while lower values back up one slot. The math is tiny
    // but worth spelling out for clarity.
    const std::size_t next = (val >= 64) ? (current + 1) % engineCount : (current + engineCount - 1) % engineCount;
    const uint8_t target = engines_.sanitizeEngineId(static_cast<uint8_t>(next));
    setSeedEngine(focus, target);
    return;
  }
  if (cc == cc::kQuantize) {
    applyQuantizeControl(val);
    return;
  }

  if (applyMn42ParamControl(cc, val)) {
    return;
  }

  // Future CC maps will route through here once the macro table lands.
}

void AppState::setSwingPercentFromHost(float value) { applySwingPercent(value); }

void AppState::applyQuantizeControlFromHost(uint8_t value) { applyQuantizeControl(value); }

void AppState::setDebugMetersEnabledFromHost(bool enabled) {
  if (debugMetersEnabled_ == enabled) {
    return;
  }
  debugMetersEnabled_ = enabled;
  displayDirty_ = true;
}

void AppState::setTransportLatchFromHost(bool enabled) {
  if (transportLatchEnabled() == enabled) {
    return;
  }
  clockTransport_.setTransportLatchEnabled(enabled);
  displayDirty_ = true;
}

void AppState::setFollowExternalClockFromHost(bool enabled) {
  if (followExternalClockEnabled() == enabled) {
    return;
  }
  clockTransport_.setFollowExternalClockEnabled(enabled, board_.nowMillis());
  displayDirty_ = true;
}

void AppState::setClockSourceExternalFromHost(bool external) {
  clockTransport_.setClockSourceExternal(external, board_.nowMillis());
  displayDirty_ = true;
}

void AppState::setInternalBpmFromHost(float bpm) {
  const float sanitized = std::clamp(bpm, 20.0f, 999.0f);
  setTempoTarget(sanitized, false);
  displayDirty_ = true;
}

void AppState::setTempoTarget(float bpm, bool immediate) {
  targetBpm_ = bpm;
  scheduler_.setDiagnosticsEnabled(diagnosticsEnabled_);
  if (immediate) {
    bpmSmoother_.reset(bpm);
    scheduler_.setBpm(bpm);
  }
}

void AppState::updateTempoSmoothing() {
  const float smoothed = bpmSmoother_.process(targetBpm_);
  if (std::fabs(smoothed - scheduler_.bpm()) > 1e-4f) {
    scheduler_.setBpm(smoothed);
  }
}

void AppState::setDiagnosticsEnabledFromHost(bool enabled) {
  diagnosticsEnabled_ = enabled;
  scheduler_.setDiagnosticsEnabled(enabled);
  if (enabled) {
    scheduler_.resetDiagnostics();
  }
}

AppState::DiagnosticsSnapshot AppState::diagnosticsSnapshot() const {
  DiagnosticsSnapshot snap{};
  snap.scheduler = scheduler_.diagnostics();
  snap.audioCallbackCount = audioRuntime_.audioCallbackCount();
  return snap;
}

void AppState::setSeedPrimeBypassFromHost(bool enabled) {
  if (seedPrimeBypassEnabled_ == enabled) {
    return;
  }
  seedPrimeBypassEnabled_ = enabled;
  if (seedPrimeBypassEnabled_) {
    scheduler_ = PatternScheduler{};
    const float bpm = (seedPrimeMode_ == SeedPrimeMode::kTapTempo) ? currentTapTempoBpm() : 120.f;
    setTempoTarget(bpm, true);
    scheduler_.setTriggerCallback(&engines_, &EngineRouter::dispatchThunk);
    const bool hardwareModeFlag = (enginesReady_ && engines_.granular().mode() == GranularEngine::Mode::kHardware);
    scheduler_.setSampleClockFn(hardwareModeFlag ? &hal::audio::sampleClock : nullptr);
    seeds_.assign(kSeedSlotCount, Seed{});
    seedEngineSelections_.assign(kSeedSlotCount, 0);
    seedsPrimed_ = false;
    setFocusSeed(focusSeed_);
  } else {
    reseed(masterSeed_);
  }
  displayDirty_ = true;
}

bool AppState::applySeedEditFromHost(uint8_t seedIndex, const std::function<void(Seed&)>& edit) {
  if (seeds_.empty()) {
    return false;
  }

  const std::size_t idx = static_cast<std::size_t>(seedIndex) % seeds_.size();
  if (seedLock_.globalLocked() || seedLock_.seedLocked(idx)) {
    return false;
  }

  Seed before = seeds_[idx];
  edit(seeds_[idx]);

  if (std::memcmp(&before, &seeds_[idx], sizeof(Seed)) == 0) {
    return false;
  }

  scheduler_.updateSeed(idx, seeds_[idx]);
  engines_.onSeed(seeds_[idx]);
  displayDirty_ = true;
  return true;
}

void AppState::setLiveCaptureVariation(uint8_t variationSteps) {
  // Variation is a user-tweakable offset into the short capture bank. Keeping it
  // small and explicit makes the reseed maths traceable in class instead of
  // hiding behind a giant PRNG spray.
  liveCaptureVariation_ = static_cast<uint8_t>(variationSteps % kShortCaptureSlots);
}

void AppState::setInputGateDivisionFromHost(GateDivision division) {
  stepGateDivision(static_cast<int>(division) - static_cast<int>(gateDivision_));
}

void AppState::setInputGateFloorFromHost(float floor) { inputGateFloor_ = std::max(1e-6f, floor); }

void AppState::setDryInputFromHost(const float* left, const float* right, std::size_t frames) {
  if (!left || frames == 0) {
    dryInputLeft_.clear();
    dryInputRight_.clear();
    updateInputGateState(0.0f, 0.0f);
    return;
  }

  dryInputLeft_.assign(left, left + frames);
  if (right) {
    dryInputRight_.assign(right, right + frames);
  } else {
    dryInputRight_.assign(dryInputLeft_.begin(), dryInputLeft_.end());
  }

  const float* probeRight = dryInputRight_.empty() ? nullptr : dryInputRight_.data();
  const EnergyProbe probe = measureEnergy(dryInputLeft_.data(), probeRight, frames);
  updateInputGateState(probe.rms, probe.peak);
}

void AppState::updateClockDominance() {
  // External clock wins if either the follow bit is set or the transport is
  // actively running.  This line is the truth table behind the entire sync
  // story, so yeah, spell it out.
  clockTransport_.updateClockDominance();
}

void AppState::handleGateTick() {
  const uint64_t tick = scheduler_.ticks();
  const uint64_t division = static_cast<uint64_t>(gateDivisionTicks());
  if (!gateEdgePending_ || division == 0u) {
    return;
  }

  const uint64_t elapsed = (tick >= lastGateTick_) ? (tick - lastGateTick_) : 0u;
  if (elapsed < division) {
    return;
  }

  if (seeds_.empty() || seedLock_.globalLocked()) {
    gateEdgePending_ = false;
    lastGateTick_ = tick;
    return;
  }

  const std::size_t focusIndex = std::min<std::size_t>(focusSeed_, seeds_.size() - 1);
  if (seedLock_.seedLocked(focusIndex)) {
    gateEdgePending_ = false;
    lastGateTick_ = tick;
    return;
  }

  const uint32_t reseedValue = RNG::xorshift(masterSeed_);
  seedPageReseed(reseedValue, seedPrimeMode_);
  gateEdgePending_ = false;
  lastGateTick_ = scheduler_.ticks();
}

void AppState::stepGateDivision(int delta) {
  constexpr int kDivCount = 4;
  int next = static_cast<int>(gateDivision_) + delta;
  next %= kDivCount;
  if (next < 0) {
    next += kDivCount;
  }
  const GateDivision target = static_cast<GateDivision>(next);
  if (gateDivision_ != target) {
    gateDivision_ = target;
    lastGateTick_ = scheduler_.ticks();
    gateEdgePending_ = false;
    displayDirty_ = true;
  }
}

uint32_t AppState::gateDivisionTicks() const { return gateDivisionTicksFor(gateDivision_); }

void AppState::updateInputGateState(float rms, float peak) {
  inputGateLevel_ = rms;
  inputGatePeak_ = peak;
  const bool hot = (rms >= inputGateFloor_) || (peak >= inputGateFloor_);
  if (hot && !inputGateHot_) {
    gateEdgePending_ = true;
  }
  inputGateHot_ = hot;
}

void AppState::updateExternalClockWatchdog() {
  const bool wasFollowing = followExternalClockEnabled();
  clockTransport_.updateExternalClockWatchdog(board_.nowMillis(), kExternalClockTimeoutMs);
  if (!clockTransport_.followExternalClockEnabled() && wasFollowing) {
    displayDirty_ = true;
  }
}

void AppState::applyMn42ModeBits(uint8_t value) {
  // MN42 packs several toggle bits into one CC.  Unpack them and update the
  // flags that control our sync + debug behaviours.
  const bool follow = (value & seedbox::interop::mn42::mode::kFollowExternalClock) != 0;
  if (followExternalClockEnabled() != follow) {
    clockTransport_.setFollowExternalClockEnabled(follow, board_.nowMillis());
  }

  debugMetersEnabled_ = (value & seedbox::interop::mn42::mode::kExposeDebugMeters) != 0;

  const bool latch = (value & seedbox::interop::mn42::mode::kLatchTransport) != 0;
  if (transportLatchEnabled() != latch) {
    clockTransport_.setTransportLatchEnabled(latch);
  } else if (transportLatchEnabled()) {
    clockTransport_.refreshTransportLatchState();
  }
}

bool AppState::applyMn42ParamControl(uint8_t controller, uint8_t value) {
  using namespace seedbox::interop::mn42;
  if (!LookupParam(controller)) {
    return false;
  }

  if (controller == param::kFocusSeed) {
    if (seeds_.empty()) {
      setFocusSeed(0);
      displayDirty_ = true;
      return true;
    }
    const std::size_t count = seeds_.size();
    const std::uint32_t scaled = static_cast<std::uint32_t>(value) * static_cast<std::uint32_t>(count);
    const std::uint8_t target = static_cast<std::uint8_t>(std::min<std::size_t>(scaled / 128u, count - 1));
    if (focusSeed_ != target) {
      setFocusSeed(target);
      displayDirty_ = true;
    }
    return true;
  }

  if (seeds_.empty()) {
    return true;
  }
  const std::size_t count = seeds_.size();
  const std::size_t idx = static_cast<std::size_t>(focusSeed_) % count;
  if (seedLock_.seedLocked(idx)) {
    return true;
  }

  Seed& seed = seeds_[idx];
  bool changed = false;
  const float normalized = static_cast<float>(value) / 127.f;

  auto assignIfDifferent = [&](float& field, float target) {
    if (std::fabs(field - target) > 1e-4f) {
      field = target;
      changed = true;
    }
  };

  switch (controller) {
    case param::kSeedPitch: {
      constexpr float kRange = 48.f;   // ±24 semitones total span.
      constexpr float kFloor = -24.f;
      const float target = kFloor + (normalized * kRange);
      assignIfDifferent(seed.pitch, target);
      break;
    }
    case param::kSeedDensity: {
      constexpr float kMaxDensity = 8.f;
      const float target = std::clamp(normalized * kMaxDensity, 0.f, kMaxDensity);
      assignIfDifferent(seed.density, target);
      break;
    }
    case param::kSeedProbability: {
      const float target = std::clamp(normalized, 0.f, 1.f);
      assignIfDifferent(seed.probability, target);
      break;
    }
    case param::kSeedJitter: {
      constexpr float kMaxJitterMs = 30.f;
      const float target = std::clamp(normalized * kMaxJitterMs, 0.f, kMaxJitterMs);
      assignIfDifferent(seed.jitterMs, target);
      break;
    }
    case param::kSeedTone: {
      const float target = std::clamp(normalized, 0.f, 1.f);
      assignIfDifferent(seed.tone, target);
      break;
    }
    case param::kSeedSpread: {
      const float target = std::clamp(normalized, 0.f, 1.f);
      assignIfDifferent(seed.spread, target);
      break;
    }
    case param::kSeedMutate: {
      const float target = std::clamp(normalized, 0.f, 1.f);
      assignIfDifferent(seed.mutateAmt, target);
      break;
    }
    default:
      return true;
  }

  if (changed) {
    scheduler_.updateSeed(idx, seed);
    engines_.onSeed(seed);
    displayDirty_ = true;
  }
  return true;
}

void AppState::handleTransportGate(uint8_t value) {
  // Mode bit decides whether the gate is momentary (direct transport control)
  // or latched (each press toggles state).  Both flows live here so it's easy
  // to demo the difference.
  clockTransport_.handleTransportGate(value);
}

void AppState::reseed(uint32_t masterSeed) {
  primeSeeds(masterSeed);
  clearPresetCrossfade();
  presetController_.setActivePresetSlot(std::string{});
  engines_.reseed(masterSeed_);
  const bool hardwareMode = (engines_.granular().mode() == GranularEngine::Mode::kHardware);
  scheduler_.setSampleClockFn(hardwareMode ? &hal::audio::sampleClock : nullptr);
}

void AppState::seedPageReseed(uint32_t masterSeed, SeedPrimeMode mode) {
  setSeedPrimeMode(mode);
  reseed(masterSeed);
}

void AppState::setSeedPrimeMode(SeedPrimeMode mode) {
  if (seedPrimeMode_ != mode) {
    seedPrimeMode_ = mode;
    displayDirty_ = true;
  } else {
    seedPrimeMode_ = mode;
  }
  lastTapTempoTapUs_ = 0;
}

void AppState::seedPageToggleLock(uint8_t index) {
  if (seeds_.empty()) {
    return;
  }
  const std::size_t idx = static_cast<std::size_t>(index) % seeds_.size();
  seedLock_.toggleSeedLock(idx);
  displayDirty_ = true;
}

void AppState::seedPageToggleGlobalLock() {
  seedLock_.toggleGlobalLock();
  displayDirty_ = true;
}

bool AppState::isSeedLocked(uint8_t index) const {
  if (seedLock_.globalLocked()) {
    return true;
  }
  if (seeds_.empty()) {
    return false;
  }
  const std::size_t idx = static_cast<std::size_t>(index) % seeds_.size();
  return seedLock_.seedLocked(idx);
}

void AppState::seedPageNudge(uint8_t index, const SeedNudge& nudge) {
  if (seeds_.empty()) {
    return;
  }
  const std::size_t idx = static_cast<std::size_t>(index) % seeds_.size();
  if (seedLock_.seedLocked(idx)) {
    return;
  }
  Seed& seed = seeds_[idx];
  if (nudge.pitchSemitones != 0.f) {
    seed.pitch += nudge.pitchSemitones;
  }
  if (nudge.densityDelta != 0.f) {
    seed.density = std::max(0.f, seed.density + nudge.densityDelta);
  }
  if (nudge.probabilityDelta != 0.f) {
    seed.probability = std::clamp(seed.probability + nudge.probabilityDelta, 0.f, 1.f);
  }
  if (nudge.jitterDeltaMs != 0.f) {
    seed.jitterMs = std::max(0.f, seed.jitterMs + nudge.jitterDeltaMs);
  }
  if (nudge.toneDelta != 0.f) {
    seed.tone = std::clamp(seed.tone + nudge.toneDelta, 0.f, 1.f);
  }
  if (nudge.spreadDelta != 0.f) {
    seed.spread = std::clamp(seed.spread + nudge.spreadDelta, 0.f, 1.f);
  }
  scheduler_.updateSeed(idx, seed);
  engines_.onSeed(seed);
  displayDirty_ = true;
}

void AppState::seedPageCycleGranularSource(uint8_t index, int32_t steps) {
  if (seeds_.empty() || steps == 0) {
    return;
  }
  const std::size_t idx = static_cast<std::size_t>(index) % seeds_.size();
  if (seedLock_.seedLocked(idx)) {
    return;
  }

  Seed& seed = seeds_[idx];
  const uint8_t originalSource = seed.granular.source;
  const uint8_t originalSlot = seed.granular.sdSlot;

  constexpr uint8_t kClipSlots = GranularEngine::kSdClipSlots;
  const bool sdClipsAvailable = kClipSlots > 1;

  if (!sdClipsAvailable) {
    const uint8_t liveEncoded = static_cast<uint8_t>(GranularEngine::Source::kLiveInput);
    seed.granular.source = liveEncoded;
    seed.granular.sdSlot = 0;
    if (seed.granular.source == originalSource && seed.granular.sdSlot == originalSlot) {
      return;
    }
    scheduler_.updateSeed(idx, seed);
    engines_.onSeed(seed);
    displayDirty_ = true;
    return;
  }

  seed.granular.sdSlot = static_cast<uint8_t>(seed.granular.sdSlot % kClipSlots);

  GranularEngine::Source source = static_cast<GranularEngine::Source>(seed.granular.source);
  if (source != GranularEngine::Source::kSdClip) {
    source = GranularEngine::Source::kLiveInput;
  }

  const int direction = (steps > 0) ? 1 : -1;
  const int stepCount = static_cast<int>((steps > 0) ? steps : -steps);

  const auto cycleSlot = [&](uint8_t current) {
    int slot = static_cast<int>(current % kClipSlots);
    if (slot <= 0) {
      slot = (direction > 0) ? 1 : (kClipSlots - 1);
    } else {
      slot += direction;
      if (slot >= kClipSlots) {
        slot = 1;
      } else if (slot <= 0) {
        slot = kClipSlots - 1;
      }
    }
    return static_cast<uint8_t>(slot);
  };

  for (int i = 0; i < stepCount; ++i) {
    if (source == GranularEngine::Source::kLiveInput) {
      source = GranularEngine::Source::kSdClip;
      seed.granular.sdSlot = cycleSlot(seed.granular.sdSlot);
    } else {
      source = GranularEngine::Source::kLiveInput;
    }
  }

  if (source == GranularEngine::Source::kSdClip && seed.granular.sdSlot == 0) {
    seed.granular.sdSlot = 1;
  }

  const uint8_t encodedSource = static_cast<uint8_t>(source);
  if (encodedSource == originalSource && seed.granular.sdSlot == originalSlot) {
    return;
  }

  seed.granular.source = encodedSource;
  scheduler_.updateSeed(idx, seed);
  engines_.onSeed(seed);
  displayDirty_ = true;
}

void AppState::recordTapTempoInterval(uint32_t intervalMs) {
  if (intervalMs == 0) {
    return;
  }
  tapTempoHistory_.push_back(intervalMs);
  constexpr std::size_t kMaxHistory = 8;
  if (tapTempoHistory_.size() > kMaxHistory) {
    const std::size_t drop = tapTempoHistory_.size() - kMaxHistory;
    tapTempoHistory_.erase(tapTempoHistory_.begin(), tapTempoHistory_.begin() + drop);
  }
}

void AppState::setSeedPreset(uint32_t presetId, const std::vector<Seed>& seeds) {
  presetBuffer_.id = presetId;
  presetBuffer_.seeds = seeds;
}

void AppState::applyQuantizeControl(uint8_t value) {
  if (seeds_.empty()) {
    return;
  }
  const uint8_t rawScaleIndex = static_cast<uint8_t>(value / 32);
  const uint8_t rawRoot = static_cast<uint8_t>(value % 12);
  const uint8_t sanitizedScaleIndex = std::min<uint8_t>(rawScaleIndex, static_cast<uint8_t>(4));
  const uint8_t sanitizedRoot = static_cast<uint8_t>(rawRoot % 12);
  quantizeScaleIndex_ = sanitizedScaleIndex;
  quantizeRoot_ = sanitizedRoot;
  const std::size_t idx = static_cast<std::size_t>(focusSeed_) % seeds_.size();
  if (seedLock_.seedLocked(idx)) {
    return;
  }
  util::ScaleQuantizer::Scale scale = util::ScaleQuantizer::Scale::kChromatic;
  switch (sanitizedScaleIndex) {
    case 0:
      scale = util::ScaleQuantizer::Scale::kChromatic;
      break;
    case 1:
      scale = util::ScaleQuantizer::Scale::kMajor;
      break;
    case 2:
      scale = util::ScaleQuantizer::Scale::kMinor;
      break;
    case 3:
      scale = util::ScaleQuantizer::Scale::kPentatonicMajor;
      break;
    case 4:
      scale = util::ScaleQuantizer::Scale::kPentatonicMinor;
      break;
    default:
      scale = util::ScaleQuantizer::Scale::kChromatic;
      break;
  }

  Seed& seed = seeds_[idx];
  const float quantized = util::ScaleQuantizer::SnapToScale(seed.pitch, sanitizedRoot, scale);
  if (quantized != seed.pitch) {
    seed.pitch = quantized;
    scheduler_.updateSeed(idx, seed);
    engines_.onSeed(seed);
    displayDirty_ = true;
  }
}

void AppState::setFocusSeed(uint8_t index) {
  if (seeds_.empty()) {
    focusSeed_ = 0;
    return;
  }
  // Modulo arithmetic means you can spin the encoder forever without throwing
  // the app off a cliff. It also makes for a nice classroom demo about wrap-
  // around indexing.
  const uint8_t count = static_cast<uint8_t>(seeds_.size());
  focusSeed_ = static_cast<uint8_t>(index % count);
}

void AppState::setSeedEngine(uint8_t seedIndex, uint8_t engineId) {
  if (seeds_.empty()) {
    return;
  }
  const size_t count = seeds_.size();
  const size_t idx = static_cast<size_t>(seedIndex) % count;
  const uint8_t sanitized = sanitizeEngine(engines_, engineId);

  if (seedEngineSelections_.size() < count) {
    seedEngineSelections_.resize(count, 0);
  }

  Seed& seed = seeds_[idx];
  if (seed.prng == 0) {
    seed.engine = sanitized;
    seedEngineSelections_[idx] = sanitized;
    displayDirty_ = true;
    return;
  }
  seed.engine = sanitized;
  seedEngineSelections_[idx] = sanitized;
  engines_.assignSeed(idx, sanitized);
  // Re-register the seed with the scheduler so downstream engines fire the
  // right trigger callback. This keeps PatternScheduler as the single source of
  // truth for playback order.
  scheduler_.updateSeed(idx, seed);
  engines_.onSeed(seed);
  displayDirty_ = true;
}

void AppState::setPage(Page page) {
  if (currentPage_ == page) {
    return;
  }
  currentPage_ = page;
  displayDirty_ = true;
}

std::vector<std::string> AppState::storedPresets() const {
  if (!store_) {
    return {};
  }
  return store_->list();
}

bool AppState::savePreset(std::string_view slot) {
  if (!store_) {
    return false;
  }
  const std::string slotName = slot.empty() ? std::string(kDefaultPresetSlot) : std::string(slot);
  seedbox::Preset preset = snapshotPreset(slotName);
  const auto bytes = preset.serialize();
  if (bytes.empty()) {
    return false;
  }
  if (!store_->save(slotName, bytes)) {
    return false;
  }
  presetController_.setActivePresetSlot(slotName);
  setSeedPreset(preset.masterSeed, preset.seeds);
  return true;
}

bool AppState::recallPreset(std::string_view slot, bool crossfade) {
  if (!store_) {
    return false;
  }
  const std::string slotName = slot.empty() ? std::string(kDefaultPresetSlot) : std::string(slot);
  std::vector<std::uint8_t> bytes;
  if (!store_->load(slotName, bytes)) {
    return false;
  }
  seedbox::Preset preset{};
  if (!seedbox::Preset::deserialize(bytes, preset)) {
    return false;
  }
  if (preset.slot.empty()) {
    preset.slot = slotName;
  }
  requestPresetChange(preset, crossfade, PresetBoundary::Step);
  return true;
}

void AppState::captureDisplaySnapshot(DisplaySnapshot& out) const {
  captureDisplaySnapshot(out, nullptr);
}

void AppState::captureDisplaySnapshot(DisplaySnapshot& out, UiState* ui) const {
  UiState localUi{};
  UiState* uiOut = ui ? ui : &localUi;
  DisplaySnapshotBuilder builder;
  DisplaySnapshotBuilder::Input input{};
  input.masterSeed = masterSeed_;
  input.sampleRate = hal::audio::sampleRate();
  input.framesPerBlock = hal::audio::framesPerBlock();
  input.ledOn = hal::io::readDigital(kStatusLedPin);
  input.audioCallbackCount = audioRuntime_.audioCallbackCount();
  input.frame = frame_;
  input.mode = static_cast<std::uint8_t>(mode_);
  input.currentPage = static_cast<std::uint8_t>(currentPage_);
  input.seedPrimeMode = static_cast<std::uint8_t>(seedPrimeMode_);
  input.gateDivision = static_cast<std::uint8_t>(gateDivision_);
  input.focusSeed = focusSeed_;
  input.bpm = scheduler_.bpm();
  input.swing = swingPercent_;
  input.externalClockDominant = externalClockDominant();
  input.waitingForExternalClock = waitingForExternalClock();
  input.debugMetersEnabled = debugMetersEnabled_;
  input.seedPrimeBypassEnabled = seedPrimeBypassEnabled_;
  input.quietMode = SeedBoxConfig::kQuietMode;
  input.followExternalClockEnabled = followExternalClockEnabled();
  input.inputGateHot = inputGateHot_;
  input.gateEdgePending = gateEdgePending_;
  input.seeds = &seeds_;
  input.engines = &engines_;
  input.seedLock = &seedLock_;
  input.schedulerSeed = debugScheduledSeed(focusSeed_);
  input.granularStats = &granularStats_;
  builder.build(out, *uiOut, input);
}

void AppState::captureLearnFrame(LearnFrame& out) const {
  out = LearnFrame{};
  out.audio = latestAudioMetrics_;

  out.generator.bpm = scheduler_.bpm();
  out.generator.clock =
      externalClockDominant() ? UiState::ClockSource::kExternal : UiState::ClockSource::kInternal;
  out.generator.tick = scheduler_.ticks();
  constexpr uint32_t kTicksPerBeat = 24u;
  const uint32_t ticksPerBar = kPresetBoundaryTicksPerBar;
  out.generator.step = static_cast<uint32_t>(out.generator.tick % kTicksPerBeat);
  out.generator.bar = static_cast<uint32_t>(out.generator.tick / ticksPerBar);
  out.generator.events = scheduler_.lastTickTriggerCount();

  const bool hasSeeds = !seeds_.empty();
  if (hasSeeds) {
    const std::size_t index = std::min<std::size_t>(focusSeed_, seeds_.size() - 1);
    const Seed& s = seeds_[index];
    out.generator.focusSeedId = s.id;
    out.generator.focusMutateAmt = s.mutateAmt;
    out.generator.density = s.density;
    out.generator.probability = s.probability;
  } else {
    out.generator.focusSeedId = focusSeed_;
  }

  out.generator.mutationCount =
      static_cast<uint32_t>(std::count_if(seeds_.begin(), seeds_.end(), [](const Seed& seed) {
        return seed.mutateAmt > 0.0f;
      }));
  out.generator.primeMode = seedPrimeMode_;
  out.generator.tapTempoBpm = currentTapTempoBpm();
  out.generator.lastTapIntervalMs = tapTempoHistory_.empty() ? 0 : tapTempoHistory_.back();
  out.generator.mutationRate = randomnessPanel_.mutationRate;
}

void AppState::captureStatusSnapshot(StatusSnapshot& out) const {
  out = StatusSnapshot{};
  writeDisplayField(out.mode, modeLabel(mode_));
  writeDisplayField(out.page, pageLabel(currentPage_));
  out.masterSeed = masterSeed_;
  out.activePresetId = activePresetId();
  writeDisplayField(out.activePresetSlot,
                    presetController_.activePresetSlot().empty() ? kDefaultPresetSlot
                                                                 : presetController_.activePresetSlot());
  out.bpm = scheduler_.bpm();
  out.schedulerTick = scheduler_.ticks();
  out.externalClockDominant = externalClockDominant();
  out.followExternalClockEnabled = followExternalClockEnabled();
  out.waitingForExternalClock = waitingForExternalClock();
  out.quietMode = SeedBoxConfig::kQuietMode;
  out.globalSeedLocked = seedLock_.globalLocked();

  if (seeds_.empty()) {
    out.hasFocusedSeed = false;
    out.focusSeedIndex = focusSeed_;
    out.focusSeedId = 0;
    out.focusSeedEngineId = EngineRouter::kSamplerId;
    writeDisplayField(out.focusSeedEngineName, "None");
    out.focusSeedLocked = false;
    return;
  }

  const std::size_t focusIndex = std::min<std::size_t>(focusSeed_, seeds_.size() - 1);
  const Seed& focus = seeds_[focusIndex];
  out.hasFocusedSeed = true;
  out.focusSeedIndex = static_cast<std::uint8_t>(focusIndex);
  out.focusSeedId = focus.id;
  out.focusSeedEngineId = engines_.sanitizeEngineId(focus.engine);
  writeDisplayField(out.focusSeedEngineName, engineLongName(out.focusSeedEngineId));
  out.focusSeedLocked = seedLock_.seedLocked(focusIndex);
}

std::string AppState::captureStatusJson() const {
  StatusSnapshot status{};
  captureStatusSnapshot(status);

  const auto mode = std::string_view(status.mode, boundedCStringLength(status.mode, sizeof(status.mode)));
  const auto page = std::string_view(status.page, boundedCStringLength(status.page, sizeof(status.page)));
  const auto slot =
      std::string_view(status.activePresetSlot, boundedCStringLength(status.activePresetSlot, sizeof(status.activePresetSlot)));
  const auto engineName =
      std::string_view(status.focusSeedEngineName,
                       boundedCStringLength(status.focusSeedEngineName, sizeof(status.focusSeedEngineName)));

  std::string out;
  out.reserve(512);
  out.push_back('{');
  appendJsonStringField(out, "mode", mode, true);
  appendJsonStringField(out, "page", page, true);
  appendJsonUIntField(out, "masterSeed", status.masterSeed, true);
  appendJsonUIntField(out, "activePresetId", status.activePresetId, true);
  appendJsonStringField(out, "activePresetSlot", slot, true);
  appendJsonFloatField(out, "bpm", status.bpm, true);
  appendJsonUIntField(out, "schedulerTick", status.schedulerTick, true);
  appendJsonBoolField(out, "externalClockDominant", status.externalClockDominant, true);
  appendJsonBoolField(out, "followExternalClockEnabled", status.followExternalClockEnabled, true);
  appendJsonBoolField(out, "waitingForExternalClock", status.waitingForExternalClock, true);
  appendJsonBoolField(out, "quietMode", status.quietMode, true);
  appendJsonBoolField(out, "globalSeedLocked", status.globalSeedLocked, true);
  appendJsonBoolField(out, "focusSeedLocked", status.focusSeedLocked, true);
  out += "\"focusSeed\":{";
  appendJsonBoolField(out, "present", status.hasFocusedSeed, true);
  appendJsonUIntField(out, "index", status.focusSeedIndex, true);
  appendJsonUIntField(out, "id", status.focusSeedId, true);
  appendJsonUIntField(out, "engineId", status.focusSeedEngineId, true);
  appendJsonStringField(out, "engineName", engineName, false);
  out += "}}";
  return out;
}

const Seed* AppState::debugScheduledSeed(uint8_t index) const {
  // Straight-through view into the scheduler's copy of a seed.  Gives us a
  // stable reference for debugging displays + tests.
  return scheduler_.seedForDebug(static_cast<size_t>(index));
}

void AppState::armGranularLiveInput(bool enabled) {
  engines_.granular().armLiveInput(enabled);
}

GranularEngine::GrainVoice AppState::debugGranularVoice(uint8_t index) const {
  return engines_.granular().voice(index);
}

#if !SEEDBOX_HW
GranularEngine::SimHardwareVoice AppState::debugGranularSimVoice(uint8_t index) const {
  return engines_.granular().simHardwareVoice(index);
}
#endif

seedbox::Preset AppState::snapshotPreset(std::string_view slot) const {
  PresetController::SnapshotInput input{};
  input.slot = slot.empty() ? std::string_view(kDefaultPresetSlot) : slot;
  input.masterSeed = masterSeed_;
  input.focusSeed = focusSeed_;
  input.bpm = scheduler_.bpm();
  input.followExternal = followExternalClockEnabled();
  input.debugMeters = debugMetersEnabled_;
  input.transportLatch = transportLatchEnabled();
  input.page = static_cast<seedbox::PageId>(currentPage_);
  input.seeds = &seeds_;
  input.engineSelections = &seedEngineSelections_;
  return presetController_.snapshotPreset(input);
}

Seed AppState::blendSeeds(const Seed& from, const Seed& to, float t) const {
  const float mix = std::clamp(t, 0.0f, 1.0f);
  Seed blended = from;
  blended.id = to.id;
  blended.prng = to.prng;
  blended.pitch = lerp(from.pitch, to.pitch, mix);
  blended.envA = lerp(from.envA, to.envA, mix);
  blended.envD = lerp(from.envD, to.envD, mix);
  blended.envS = lerp(from.envS, to.envS, mix);
  blended.envR = lerp(from.envR, to.envR, mix);
  blended.density = lerp(from.density, to.density, mix);
  blended.probability = lerp(from.probability, to.probability, mix);
  blended.jitterMs = lerp(from.jitterMs, to.jitterMs, mix);
  blended.tone = lerp(from.tone, to.tone, mix);
  blended.spread = lerp(from.spread, to.spread, mix);
  blended.engine = (mix < 0.5f) ? from.engine : to.engine;
  blended.sampleIdx = (mix < 0.5f) ? from.sampleIdx : to.sampleIdx;
  blended.mutateAmt = lerp(from.mutateAmt, to.mutateAmt, mix);

  blended.granular.grainSizeMs = lerp(from.granular.grainSizeMs, to.granular.grainSizeMs, mix);
  blended.granular.sprayMs = lerp(from.granular.sprayMs, to.granular.sprayMs, mix);
  blended.granular.transpose = lerp(from.granular.transpose, to.granular.transpose, mix);
  blended.granular.windowSkew = lerp(from.granular.windowSkew, to.granular.windowSkew, mix);
  blended.granular.stereoSpread = lerp(from.granular.stereoSpread, to.granular.stereoSpread, mix);
  blended.granular.source = (mix < 0.5f) ? from.granular.source : to.granular.source;
  blended.granular.sdSlot = (mix < 0.5f) ? from.granular.sdSlot : to.granular.sdSlot;

  blended.resonator.exciteMs = lerp(from.resonator.exciteMs, to.resonator.exciteMs, mix);
  blended.resonator.damping = lerp(from.resonator.damping, to.resonator.damping, mix);
  blended.resonator.brightness = lerp(from.resonator.brightness, to.resonator.brightness, mix);
  blended.resonator.feedback = lerp(from.resonator.feedback, to.resonator.feedback, mix);
  blended.resonator.mode = (mix < 0.5f) ? from.resonator.mode : to.resonator.mode;
  blended.resonator.bank = (mix < 0.5f) ? from.resonator.bank : to.resonator.bank;
  return blended;
}

void AppState::applyPreset(const seedbox::Preset& preset, bool crossfade) {
  presetController_.setActivePresetSlot(preset.slot.empty() ? std::string(kDefaultPresetSlot) : preset.slot);
  masterSeed_ = preset.masterSeed;
  clockTransport_.applyPresetClockState(preset.clock.followExternal, preset.clock.transportLatch,
                                        clockTransport_.externalTransportRunning());
  debugMetersEnabled_ = preset.clock.debugMeters;
  setTempoTarget(preset.clock.bpm, true);
  currentPage_ = static_cast<Page>(preset.page);
  storageButtonHeld_ = false;
  storageLongPress_ = false;

  if (!preset.engineSelections.empty()) {
    seedEngineSelections_ = preset.engineSelections;
  }
  if (seedEngineSelections_.size() < preset.seeds.size()) {
    seedEngineSelections_.resize(preset.seeds.size(), 0);
    for (std::size_t i = 0; i < preset.seeds.size(); ++i) {
      seedEngineSelections_[i] = preset.seeds[i].engine;
    }
  }

  const bool haveSeeds = !preset.seeds.empty();
  bool doCrossfade = crossfade && haveSeeds && !seeds_.empty() && preset.seeds.size() == seeds_.size();
  if (doCrossfade) {
    presetController_.beginCrossfade(seeds_, preset.seeds, kPresetCrossfadeTicks);
  } else {
    seeds_ = preset.seeds;
    presetController_.clearCrossfade();
    scheduler_ = PatternScheduler{};
    setTempoTarget(preset.clock.bpm, true);
    scheduler_.setTriggerCallback(&engines_, &EngineRouter::dispatchThunk);
    const bool hardwareMode = (engines_.granular().mode() == GranularEngine::Mode::kHardware);
    scheduler_.setSampleClockFn(hardwareMode ? &hal::audio::sampleClock : nullptr);
    for (const Seed& s : seeds_) {
      scheduler_.addSeed(s);
    }
  }

  setFocusSeed(preset.focusSeed);
  seedsPrimed_ = haveSeeds;
  setSeedPreset(preset.masterSeed, preset.seeds);
  lastGateTick_ = scheduler_.ticks();
  gateEdgePending_ = false;
  displayDirty_ = true;
}

void AppState::stepPresetCrossfade() {
  if (!presetController_.crossfadeActive()) {
    return;
  }
  const auto& crossfade = presetController_.crossfade();
  if (crossfade.from.size() != crossfade.to.size() || crossfade.to.size() != seeds_.size()) {
    seeds_ = crossfade.to;
    presetController_.clearCrossfade();
    const float currentBpm = scheduler_.bpm();
    scheduler_ = PatternScheduler{};
    setTempoTarget(currentBpm, true);
    scheduler_.setTriggerCallback(&engines_, &EngineRouter::dispatchThunk);
    const bool hardwareMode = (engines_.granular().mode() == GranularEngine::Mode::kHardware);
    scheduler_.setSampleClockFn(hardwareMode ? &hal::audio::sampleClock : nullptr);
    for (const Seed& s : seeds_) {
      scheduler_.addSeed(s);
    }
    return;
  }

  const float total = static_cast<float>(crossfade.total);
  const float remaining = static_cast<float>(crossfade.remaining);
  const float mix = (total <= 0.f) ? 1.0f : (1.0f - (remaining / total));
  for (std::size_t i = 0; i < seeds_.size(); ++i) {
    seeds_[i] = blendSeeds(crossfade.from[i], crossfade.to[i], mix);
    scheduler_.updateSeed(i, seeds_[i]);
  }

  presetController_.decrementCrossfade();
  if (!presetController_.crossfadeActive()) {
    seeds_ = crossfade.to;
    for (std::size_t i = 0; i < seeds_.size(); ++i) {
      scheduler_.updateSeed(i, seeds_[i]);
    }
    presetController_.clearCrossfade();
  }
}

void AppState::clearPresetCrossfade() {
  presetController_.clearCrossfade();
}
