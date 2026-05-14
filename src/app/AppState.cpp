//
// AppState.cpp
// -------------
// The operational heart of SeedBox.  Everything the performer can touch rolls
// through here eventually: seeds get primed, transport sources compete for
// authority, and the audio engines receive their marching orders.  The comments
// are intentionally loud so you can walk a classroom through the firmware
// without needing a separate slide deck.
#include "app/AppState.h"
#include "app/AppUiClockService.h"
#include "app/AudioRuntimeState.h"
#include "app/DisplayTelemetryService.h"
#include "app/DisplaySnapshotBuilder.h"
#include "app/GateQuantizeService.h"
#include "app/InputGestureRouter.h"
#include "app/HostControlService.h"
#include "app/Mn42ControlRouter.h"
#include "app/ModeEventRouter.h"
#include "app/PresetStorageService.h"
#include "app/PresetTransitionRunner.h"
#include "app/SeedLockService.h"
#include "app/SeedMutationService.h"
#include "app/SeedPrimeRuntimeService.h"
#include "app/SeedPrimeController.h"
#include "app/ClockTransportController.h"
#include "app/PresetController.h"
#include "app/StatusSnapshotBuilder.h"
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
constexpr uint32_t kStorageLongPressFrames = 60;
constexpr std::string_view kDefaultPresetSlot = "default";
constexpr std::size_t kSeedSlotCount = 4;

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

SeedPrimeController gSeedPrimeController{};

const char* pageLabel(AppState::Page page) {
  switch (page) {
    case AppState::Page::kSeeds: return "Seeds";
    case AppState::Page::kStorage: return "Storage";
    case AppState::Page::kClock: return "Clock";
    default: return "Unknown";
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

AppState::AppState(hal::Board& board) : board_(board), input_(board) {
  selectClockProvider(&internalClock_);
  applySwingPercent(swingPercent_);
  inputGate_.setFloor(hal::audio::kEnginePassthroughFloor);
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
  // JUCE hosts hand us live audio, so keep the granular engine willing to use
  // it even though we're not on physical hardware.
  engines_.granular().armLiveInput(true);
  midi.markAppReady();
}
#endif

void AppState::configureMidiRouting() {
  // MIDI routing is page-aware on purpose: performance pages admit clock and
  // transport, while edit pages only admit parameter gestures so a remote
  // editor cannot accidentally seize time while someone is programming seeds.
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
  // bootRuntime is the "common spine" for hardware, sim, and JUCE startup. The
  // caller decides the body; this routine makes that body musically coherent.
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
    // Bypass mode boots a scheduler and UI shell without filling the whole seed
    // table, which is useful for teaching isolated prime modes and debugging
    // empty-slot behavior.
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
  // Sample-clock wiring is repeated after reseed because the runtime may have
  // rebuilt the scheduler while deciding what kind of body it is booting into.
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

  inputGate_.refreshFromDryInput(buffer.frames);

  const float* dryLeftInput = inputGate_.dryLeft();
  const float* dryRightInput = inputGate_.dryRight(buffer.frames);
  Engine::RenderContext ctx{dryLeftInput, dryRightInput, buffer.left, buffer.right, buffer.frames};

  // The render contract is simple: every callback starts from silence, then the
  // engines, test tone, and optional passthrough layers earn their way in.
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
  if (inputGate_.hasDryInput() && !seeds_.empty()) {
    // Desktop builds let the focused seed treat host input as effect material,
    // which keeps the simulator and JUCE lanes faithful to the same engine API.
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

  if (!audioRuntime_.hostAudioMode() && inputGate_.hasDryInput()) {
    const std::size_t copyFrames = std::min<std::size_t>(buffer.frames, inputGate_.dryFrames());
    const float* dryLeft = inputGate_.dryLeft();
    const float* dryRight = inputGate_.dryRight(copyFrames);

    const bool dryAudible = hal::audio::bufferHasEngineEnergy(dryLeft, dryRight, copyFrames,
                                                             kPassthroughFloor);
    const bool enginesAudible = hal::audio::bufferHasEngineEnergy(buffer.left, buffer.right,
                                                                 buffer.frames, kPassthroughFloor);

    // Passthrough only wins when the engines truly left the buffer empty. That
    // keeps "input monitor" from masking subtle engine output.
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
      // The reseed button is dual-purpose: outside Storage it arms a reseed;
      // inside Storage the same hold duration becomes save vs recall.
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
      // Long press means "commit this scene to disk"; short press means
      // "audition what is already stored there".
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
    // Long press escalates from slot lock to table lock so the gesture surface
    // stays tiny without losing a global "freeze the world" command.
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
  // tick() is the civil service of the instrument. It polls physical state,
  // resolves pending gestures, advances whichever clock currently owns time,
  // and finally republishes the display snapshot.
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
    // Manual reseeds do not increment the current seed; they hash it. That
    // gives us a deterministic but visibly new "next world" every time.
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
      // Internal time owns the whole cadence: transport tick, gate reseed
      // checks, and preset-boundary commits all happen off the same edge.
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
  // Alt+Tap rotates the prime "story" itself: LFSR, tap lineage, preset, or
  // live input. Shift simply walks that ring in reverse.
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
    // Tap-tempo long press does not enter Swing immediately; it raises a flag
    // so the mode change happens in the main tick after input processing.
    swingPageRequested_ = true;
    displayDirty_ = true;
    return true;
  }
  if (evt.type == InputEvents::Type::ButtonPress) {
    if (seedPrimeMode_ == SeedPrimeMode::kTapTempo) {
      // In tap-prime mode the Tap button teaches two things at once: transport
      // ownership can flip, and tempo lineage can become the next seed source.
      if (tapTempo_.noteTap(evt.timestampUs).has_value()) {
        setTempoTarget(currentTapTempoBpm(), false);
      }
    } else {
      tapTempo_.resetPendingTap();
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
  static const ModeEventRouter router{};
  router.applyModeTransition(*this, evt);
}

void AppState::dispatchToPage(const InputEvents::Event& evt) {
  static const ModeEventRouter router{};
  router.dispatchToPage(*this, evt);
}

void AppState::handleReseedRequest() {
  static const SeedPrimeRuntimeService service{};
  service.handleReseedRequest(*this);
}

void AppState::triggerLiveCaptureReseed() {
  static const SeedPrimeRuntimeService service{};
  service.triggerLiveCaptureReseed(*this);
}

void AppState::requestPresetChange(const seedbox::Preset& preset, bool crossfade, PresetBoundary boundary) {
  // AppState never commits a preset directly from UI intent. Everything goes
  // through the runner so step/bar boundaries and crossfade policy stay shared.
  static const PresetTransitionRunner runner{};
  runner.requestChange(*this, preset, crossfade, static_cast<std::uint8_t>(boundary));
}

void AppState::maybeCommitPendingPreset(uint64_t currentTick) {
  // The scheduler decides when "later" has arrived; this helper just asks the
  // runner whether the queued preset has reached its chosen boundary yet.
  static const PresetTransitionRunner runner{};
  runner.maybeCommitPending(*this, currentTick);
}

void AppState::triggerPanic() {
  // Panic has to clear every queue that can still make sound or motion:
  // scheduled triggers, live voices, MIDI notes, pending gate edges, and any
  // deferred preset/transport holds.
  scheduler_.clearPendingTriggers();
  engines_.panic();
  midi.panic();
  input_.clear();
  inputGate_.cancelPendingGateEdge();
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
  static const AppUiClockService service{};
  service.setModeFromHost(*this, mode);
}

void AppState::enterSwingMode() {
  static const AppUiClockService service{};
  service.enterSwingMode(*this);
}

void AppState::exitSwingMode(Mode targetMode) {
  static const AppUiClockService service{};
  service.exitSwingMode(*this, targetMode);
}

void AppState::adjustSwing(float delta) {
  static const AppUiClockService service{};
  service.adjustSwing(*this, delta);
}

void AppState::applySwingPercent(float value) {
  static const AppUiClockService service{};
  service.applySwingPercent(*this, value);
}

void AppState::selectClockProvider(ClockProvider* provider) {
  static const AppUiClockService service{};
  service.selectClockProvider(*this, provider);
}

void AppState::toggleClockProvider() {
  static const AppUiClockService service{};
  service.toggleClockProvider(*this);
}

void AppState::toggleTransportLatchedRunning() {
  static const AppUiClockService service{};
  service.toggleTransportLatchedRunning(*this);
}

// primeSeeds is where the deterministic world-building happens. We treat
// masterSeed as the only source of entropy, then spin a tiny RNG to fill each
// Seed with musically interesting defaults. The idea: instructors can step
// through this function and show how density, jitter, and engine-specific
// parameters fall out of simple pseudo-random math.
void AppState::primeSeeds(uint32_t masterSeed) {
  static const SeedPrimeRuntimeService service{};
  service.primeSeeds(*this, masterSeed);
}

float AppState::currentTapTempoBpm() const {
  return tapTempo_.currentBpm();
}

void AppState::onExternalClockTick() {
  static const AppUiClockService service{};
  service.onExternalClockTick(*this);
}

void AppState::onExternalTransportStart() {
  static const AppUiClockService service{};
  service.onExternalTransportStart(*this);
}

void AppState::onExternalTransportStop() {
  static const AppUiClockService service{};
  service.onExternalTransportStop(*this);
}

void AppState::onExternalControlChange(uint8_t ch, uint8_t cc, uint8_t val) {
  static const Mn42ControlRouter router{};
  router.route(*this, ch, cc, val);
}

void AppState::setSwingPercentFromHost(float value) {
  static const HostControlService service{};
  service.setSwingPercent(*this, value);
}

void AppState::applyQuantizeControlFromHost(uint8_t value) {
  static const HostControlService service{};
  service.applyQuantizeControl(*this, value);
}

void AppState::setDebugMetersEnabledFromHost(bool enabled) {
  static const HostControlService service{};
  service.setDebugMetersEnabled(*this, enabled);
}

void AppState::setTransportLatchFromHost(bool enabled) {
  static const HostControlService service{};
  service.setTransportLatch(*this, enabled);
}

void AppState::setFollowExternalClockFromHost(bool enabled) {
  static const HostControlService service{};
  service.setFollowExternalClock(*this, enabled);
}

void AppState::setClockSourceExternalFromHost(bool external) {
  static const HostControlService service{};
  service.setClockSourceExternal(*this, external);
}

void AppState::setInternalBpmFromHost(float bpm) {
  static const HostControlService service{};
  service.setInternalBpm(*this, bpm);
}

void AppState::setTempoTarget(float bpm, bool immediate) {
  // Tempo smoothing is applied by default so host and UI jumps do not zipper.
  // "immediate" is reserved for places like boot/reseed where the new world
  // should start at its target BPM on the next tick.
  targetBpm_ = bpm;
  scheduler_.setDiagnosticsEnabled(diagnosticsEnabled_);
  if (immediate) {
    bpmSmoother_.reset(bpm);
    scheduler_.setBpm(bpm);
  }
}

void AppState::updateTempoSmoothing() {
  const float smoothed = bpmSmoother_.process(targetBpm_);
  // The scheduler only gets touched when the smoothed value meaningfully moved,
  // which keeps high-frequency tick code from thrashing on tiny float noise.
  if (std::fabs(smoothed - scheduler_.bpm()) > 1e-4f) {
    scheduler_.setBpm(smoothed);
  }
}

void AppState::setDiagnosticsEnabledFromHost(bool enabled) {
  static const HostControlService service{};
  service.setDiagnosticsEnabled(*this, enabled);
}

AppState::DiagnosticsSnapshot AppState::diagnosticsSnapshot() const {
  static const HostControlService service{};
  return service.diagnosticsSnapshot(*this);
}

void AppState::setSeedPrimeBypassFromHost(bool enabled) {
  static const HostControlService service{};
  service.setSeedPrimeBypass(*this, enabled);
}

bool AppState::applySeedEditFromHost(uint8_t seedIndex, const std::function<void(Seed&)>& edit) {
  static const SeedMutationService service{};
  return service.applySeedEditFromHost(*this, seedIndex, edit);
}

void AppState::setLiveCaptureVariation(uint8_t variationSteps) {
  static const HostControlService service{};
  service.setLiveCaptureVariation(*this, variationSteps);
}

void AppState::setInputGateDivisionFromHost(GateDivision division) {
  static const HostControlService service{};
  service.setInputGateDivision(*this, static_cast<std::uint8_t>(division));
}

void AppState::setInputGateFloorFromHost(float floor) {
  static const HostControlService service{};
  service.setInputGateFloor(*this, floor);
}

void AppState::setDryInputFromHost(const float* left, const float* right, std::size_t frames) {
  static const HostControlService service{};
  service.setDryInput(*this, left, right, frames);
}

void AppState::updateClockDominance() {
  static const AppUiClockService service{};
  service.updateClockDominance(*this);
}

void AppState::handleGateTick() {
  static const GateQuantizeService service{};
  service.handleGateTick(*this);
}

void AppState::stepGateDivision(int delta) {
  static const GateQuantizeService service{};
  service.stepGateDivision(*this, delta);
}

void AppState::updateExternalClockWatchdog() {
  static const AppUiClockService service{};
  service.updateExternalClockWatchdog(*this);
}

void AppState::reseed(uint32_t masterSeed) {
  static const SeedPrimeRuntimeService service{};
  service.reseed(*this, masterSeed);
}

void AppState::seedPageReseed(uint32_t masterSeed, SeedPrimeMode mode) {
  static const SeedPrimeRuntimeService service{};
  service.seedPageReseed(*this, masterSeed, mode);
}

void AppState::setSeedPrimeMode(SeedPrimeMode mode) {
  static const SeedPrimeRuntimeService service{};
  service.setSeedPrimeMode(*this, mode);
}

void AppState::seedPageToggleLock(uint8_t index) {
  static const SeedLockService service{};
  service.toggleSeedLock(*this, index);
}

void AppState::seedPageToggleGlobalLock() {
  static const SeedLockService service{};
  service.toggleGlobalLock(*this);
}

bool AppState::isSeedLocked(uint8_t index) const {
  static const SeedLockService service{};
  return service.isSeedLocked(*this, index);
}

void AppState::seedPageNudge(uint8_t index, const SeedNudge& nudge) {
  static const SeedMutationService service{};
  service.seedPageNudge(*this, index, nudge);
}

void AppState::seedPageCycleGranularSource(uint8_t index, int32_t steps) {
  static const SeedMutationService service{};
  service.seedPageCycleGranularSource(*this, index, steps);
}

void AppState::recordTapTempoInterval(uint32_t intervalMs) {
  tapTempo_.recordInterval(intervalMs);
}

void AppState::setSeedPreset(uint32_t presetId, const std::vector<Seed>& seeds) {
  static const SeedPrimeRuntimeService service{};
  service.setSeedPreset(*this, presetId, seeds);
}

void AppState::applyQuantizeControl(uint8_t value) {
  static const GateQuantizeService service{};
  service.applyQuantizeControl(*this, value);
}

void AppState::setFocusSeed(uint8_t index) {
  static const SeedMutationService service{};
  service.setFocusSeed(*this, index);
}

void AppState::setSeedEngine(uint8_t seedIndex, uint8_t engineId) {
  static const SeedMutationService service{};
  service.setSeedEngine(*this, seedIndex, engineId);
}

void AppState::setPage(Page page) {
  static const PresetStorageService service{};
  service.setPage(*this, page);
}

std::vector<std::string> AppState::storedPresets() const {
  static const PresetStorageService service{};
  return service.storedPresets(*this);
}

bool AppState::savePreset(std::string_view slot) {
  static const PresetStorageService service{};
  return service.savePreset(*this, slot);
}

bool AppState::recallPreset(std::string_view slot, bool crossfade) {
  static const PresetStorageService service{};
  return service.recallPreset(*this, slot, crossfade);
}

void AppState::captureDisplaySnapshot(DisplaySnapshot& out) const {
  captureDisplaySnapshot(out, nullptr);
}

void AppState::captureDisplaySnapshot(DisplaySnapshot& out, UiState* ui) const {
  static const DisplayTelemetryService service{};
  service.captureDisplaySnapshot(*this, out, ui);
}

void AppState::captureLearnFrame(LearnFrame& out) const {
  static const DisplayTelemetryService service{};
  service.captureLearnFrame(*this, out);
}

void AppState::captureStatusSnapshot(StatusSnapshot& out) const {
  // StatusSnapshot is the lightweight machine-readable witness: enough truth
  // for status endpoints and tests, without dragging in the full display frame.
  StatusSnapshotBuilder::Input input{};
  input.mode = modeLabel(mode_);
  input.page = pageLabel(currentPage_);
  input.masterSeed = masterSeed_;
  input.activePresetId = activePresetId();
  input.activePresetSlot =
      presetController_.activePresetSlot().empty() ? kDefaultPresetSlot : std::string_view{presetController_.activePresetSlot()};
  input.bpm = scheduler_.bpm();
  input.schedulerTick = scheduler_.ticks();
  input.externalClockDominant = externalClockDominant();
  input.followExternalClockEnabled = followExternalClockEnabled();
  input.waitingForExternalClock = waitingForExternalClock();
  input.quietMode = SeedBoxConfig::kQuietMode;
  input.focusSeed = focusSeed_;
  input.seeds = &seeds_;
  input.engines = &engines_;
  input.seedLock = &seedLock_;

  StatusSnapshotBuilder builder;
  builder.build(out, input);
}

std::string AppState::captureStatusJson() const {
  StatusSnapshot status{};
  captureStatusSnapshot(status);
  // JSON formatting lives in the builder so the status endpoint and tests share
  // the same escaping and field-order rules.
  StatusSnapshotBuilder builder;
  return builder.toJson(status);
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
  static const PresetTransitionRunner runner{};
  return runner.snapshot(*this, slot);
}

void AppState::applyPreset(const seedbox::Preset& preset, bool crossfade) {
  static const PresetTransitionRunner runner{};
  runner.apply(*this, preset, crossfade);
}

void AppState::stepPresetCrossfade() {
  static const PresetTransitionRunner runner{};
  runner.stepCrossfade(*this);
}

void AppState::clearPresetCrossfade() {
  static const PresetTransitionRunner runner{};
  runner.clearCrossfade(*this);
}
