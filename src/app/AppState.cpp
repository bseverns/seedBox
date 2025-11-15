//
// AppState.cpp
// -------------
// The operational heart of SeedBox.  Everything the performer can touch rolls
// through here eventually: seeds get primed, transport sources compete for
// authority, and the audio engines receive their marching orders.  The comments
// are intentionally loud so you can walk a classroom through the firmware
// without needing a separate slide deck.
#include "app/AppState.h"
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
#include "engine/Granular.h"
#include "engine/Sampler.h"
#include "hal/hal_audio.h"
#include "hal/hal_io.h"
#if SEEDBOX_HW
  #include "HardwarePrelude.h"
  #include "AudioMemoryBudget.h"
#endif

#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  #include <Arduino.h>
#endif

namespace {
constexpr uint8_t kEngineCycleCc = seedbox::interop::mn42::param::kEngineCycle;
constexpr uint32_t kStorageLongPressFrames = 60;
constexpr std::string_view kDefaultPresetSlot = "default";

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

void populateSdClips(GranularEngine& engine) {
  for (uint8_t i = 0; i < kDemoSdClips.size(); ++i) {
    engine.registerSdClip(static_cast<uint8_t>(i + 1), kDemoSdClips[i]);
  }
}

constexpr std::array<AppState::SeedPrimeMode, 4> kPrimeModes{{
    AppState::SeedPrimeMode::kLfsr,
    AppState::SeedPrimeMode::kTapTempo,
    AppState::SeedPrimeMode::kPreset,
    AppState::SeedPrimeMode::kLiveInput,
}};

AppState::SeedPrimeMode rotatePrimeMode(AppState::SeedPrimeMode current, int step) {
  const int count = static_cast<int>(kPrimeModes.size());
  auto it = std::find(kPrimeModes.begin(), kPrimeModes.end(), current);
  const int index = (it == kPrimeModes.end())
                        ? 0
                        : static_cast<int>(std::distance(kPrimeModes.begin(), it));
  int next = index + step;
  if (next < 0) {
    next = (next % count) + count;
  }
  next %= count;
  return kPrimeModes[static_cast<std::size_t>(next)];
}

const char* primeModeLabel(AppState::SeedPrimeMode mode) {
  switch (mode) {
    case AppState::SeedPrimeMode::kTapTempo: return "Tap";
    case AppState::SeedPrimeMode::kPreset: return "Preset";
    case AppState::SeedPrimeMode::kLiveInput: return "Live";
    case AppState::SeedPrimeMode::kLfsr:
    default: return "LFSR";
  }
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

std::string_view engineLabel(const EngineRouter& router, uint8_t engine) {
  const uint8_t sanitized = router.sanitizeEngineId(engine);
  const std::string_view label = router.engineShortName(sanitized);
  if (label.empty()) {
    return std::string_view{"UNK"};
  }
  return label;
}

const char* engineLongName(uint8_t engine) {
  switch (engine) {
    case 0: return "Sampler";
    case 1: return "Granular";
    case 2: return "Resonator";
    default: return "Unknown";
  }
}

float lerp(float a, float b, float t) {
  return a + (b - a) * t;
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
  internalClock_.attachScheduler(&scheduler_);
  midiClockIn_.attachScheduler(&scheduler_);
  midiClockOut_.attachScheduler(&scheduler_);
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
#if SEEDBOX_HW
  store_ = ensureStore(store_);
  // Lock in the entire audio buffer pool before any engine spins up. We slam
  // all four line items from AudioMemoryBudget into one call so individual
  // init() routines can't accidentally stomp the global allocator mid-set.
  AudioMemory(AudioMemoryBudget::kTotalBlocks);

  midi.begin();
  midi.setClockHandler([this]() { onExternalClockTick(); });
  midi.setStartHandler([this]() { onExternalTransportStart(); });
  midi.setStopHandler([this]() { onExternalTransportStop(); });
  midi.setControlChangeHandler(
      [this](uint8_t ch, uint8_t cc, uint8_t val) {
        onExternalControlChange(ch, cc, val);
      });

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
#endif
  hal::audio::init(&AppState::audioCallbackTrampoline, this);
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
  hal::audio::init(&AppState::audioCallbackTrampoline, this);
  hal::audio::stop();
  hal::io::writeDigital(kStatusLedPin, false);
  bootRuntime(EngineRouter::Mode::kSim, false);
}

void AppState::bootRuntime(EngineRouter::Mode mode, bool hardwareMode) {
  engines_.init(mode);
  engines_.granular().setMaxActiveVoices(hardwareMode ? 36 : 12);
  engines_.granular().armLiveInput(hardwareMode);
  populateSdClips(engines_.granular());
  engines_.resonator().setMaxVoices(hardwareMode ? 10 : 4);
  engines_.resonator().setDampingRange(0.18f, 0.92f);
  ClockProvider* provider = followExternalClockEnabled_ ? static_cast<ClockProvider*>(&midiClockIn_)
                                                        : static_cast<ClockProvider*>(&internalClock_);
  selectClockProvider(provider);
  applySwingPercent(swingPercent_);
  reseed(masterSeed_);
  scheduler_.setSampleClockFn(hardwareMode ? &hal::audio::sampleClock : nullptr);
  captureDisplaySnapshot(displayCache_, uiStateCache_);
  displayDirty_ = true;
  audioCallbackCount_ = 0;
  clearPresetCrossfade();
  activePresetSlot_.clear();
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
  ++audioCallbackCount_;
  if (!buffer.left || !buffer.right) {
    return;
  }
  std::fill(buffer.left, buffer.left + buffer.frames, 0.0f);
  std::fill(buffer.right, buffer.right + buffer.frames, 0.0f);
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

    const std::string slotName = activePresetSlot_.empty() ? std::string(kDefaultPresetSlot)
                                                           : activePresetSlot_;
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
  processInputEvents();
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
  if (!seedsPrimed_) {
    reseed(masterSeed_);
  }
  if (!externalClockDominant_) {
    if (!clock_) {
      selectClockProvider(&internalClock_);
    }
    clock_->onTick();
  }
  stepPresetCrossfade();
  ++frame_;
  captureDisplaySnapshot(displayCache_, uiStateCache_);
  displayDirty_ = true;
}

void AppState::processInputEvents() {
  const auto& evts = input_.events();
  for (const auto& evt : evts) {
    if (evt.type == InputEvents::Type::ButtonLongPress &&
        evt.primaryButton == hal::Board::ButtonID::EncoderSeedBank) {
      handleReseedRequest();
    }
    if (handleSeedPrimeGesture(evt)) {
      continue;
    }
    if (handleClockButtonEvent(evt)) {
      continue;
    }
    applyModeTransition(evt);
    dispatchToPage(evt);
  }
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
  const SeedPrimeMode nextMode = rotatePrimeMode(seedPrimeMode_, step);
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
          scheduler_.setBpm(currentTapTempoBpm());
        }
      }
      lastTapTempoTapUs_ = evt.timestampUs;
    } else {
      lastTapTempoTapUs_ = 0;
    }
    toggleClockProvider();
    if (mode_ == Mode::PERF) {
      transportLatchedRunning_ = !transportLatchedRunning_;
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
  if (evt.type == InputEvents::Type::EncoderHoldTurn &&
      evt.encoder == hal::Board::EncoderID::Density && eventHasButton(evt, hal::Board::ButtonID::Shift)) {
    if (!seeds_.empty() && evt.encoderDelta != 0) {
      const size_t focus = std::min<size_t>(focusSeed_, seeds_.size() - 1);
      const uint8_t current = seeds_[focus].engine;
      const uint8_t next = static_cast<uint8_t>(static_cast<int>(current) + evt.encoderDelta);
      setSeedEngine(static_cast<uint8_t>(focus), next);
      displayDirty_ = true;
    }
  }
}

void AppState::handlePerfEvent(const InputEvents::Event& evt) {
  if (evt.type == InputEvents::Type::ButtonPress && evt.primaryButton == hal::Board::ButtonID::TapTempo) {
    transportLatchedRunning_ = !transportLatchedRunning_;
    displayDirty_ = true;
  }
}

void AppState::handleSettingsEvent(const InputEvents::Event& evt) {
  if (evt.type == InputEvents::Type::ButtonPress && evt.primaryButton == hal::Board::ButtonID::TapTempo) {
    followExternalClockEnabled_ = !followExternalClockEnabled_;
    updateClockDominance();
    displayDirty_ = true;
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
  internalClock_.setSwing(clamped);
  midiClockOut_.setSwing(clamped);
  if (clock_) {
    clock_->setSwing(clamped);
  }
  if (changed) {
    displayDirty_ = true;
  }
}

namespace {
void alignProviderRunning(ClockProvider* clock, InternalClock& internal, MidiClockIn& midiIn,
                          MidiClockOut& midiOut, bool externalRunning) {
  if (!clock) {
    internal.stopTransport();
    midiIn.stopTransport();
    midiOut.stopTransport();
    return;
  }
  if (clock == &internal) {
    internal.startTransport();
    midiIn.stopTransport();
  } else {
    internal.stopTransport();
    if (externalRunning) {
      midiIn.startTransport();
    } else {
      midiIn.stopTransport();
    }
  }
  // Outbound MIDI stays in lockstep with whichever source is active.
  if (clock == &midiOut) {
    midiOut.startTransport();
  } else {
    if (clock == &internal || externalRunning) {
      midiOut.startTransport();
    } else {
      midiOut.stopTransport();
    }
  }
}
}  // namespace

void AppState::selectClockProvider(ClockProvider* provider) {
  ClockProvider* target = provider ? provider : &internalClock_;
  ClockProvider* previous = clock_;

  if (previous && previous != target) {
    previous->stopTransport();
  }

  clock_ = target;

  internalClock_.attachScheduler(&scheduler_);
  midiClockIn_.attachScheduler(&scheduler_);
  midiClockOut_.attachScheduler(&scheduler_);
  scheduler_.setClockProvider(clock_);

  if (clock_) {
    clock_->setBpm(scheduler_.bpm());
  }

  alignProviderRunning(clock_, internalClock_, midiClockIn_, midiClockOut_, externalTransportRunning_);
  applySwingPercent(swingPercent_);
}

void AppState::toggleClockProvider() {
  const bool useExternal = (clock_ != &midiClockIn_);
  if (useExternal) {
    selectClockProvider(&midiClockIn_);
    followExternalClockEnabled_ = true;
  } else {
    selectClockProvider(&internalClock_);
    followExternalClockEnabled_ = false;
  }
  updateClockDominance();
  displayDirty_ = true;
}

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

  constexpr std::size_t kSeedCount = 4;
  seedLock_.resize(kSeedCount);
  seedLock_.trim(kSeedCount);

  std::vector<Seed> generated;
  if (!seedLock_.globalLocked() || previousSeeds.empty()) {
    switch (seedPrimeMode_) {
      case SeedPrimeMode::kTapTempo:
        generated = buildTapTempoSeeds(masterSeed_, kSeedCount, currentTapTempoBpm());
        break;
      case SeedPrimeMode::kPreset:
        generated = buildPresetSeeds(kSeedCount);
        break;
      case SeedPrimeMode::kLiveInput:
        generated = buildLiveInputSeeds(masterSeed_, kSeedCount);
        break;
      case SeedPrimeMode::kLfsr:
      default:
        generated = buildLfsrSeeds(masterSeed_, kSeedCount);
        break;
    }
    if (generated.empty()) {
      generated = buildLfsrSeeds(masterSeed_, kSeedCount);
    }
    if (generated.size() < kSeedCount) {
      generated.resize(kSeedCount);
    }
    if (generated.size() > kSeedCount) {
      generated.resize(kSeedCount);
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
    if (generated.size() < kSeedCount) {
      const auto topUp = buildLfsrSeeds(masterSeed_, kSeedCount);
      for (std::size_t i = generated.size(); i < kSeedCount && i < topUp.size(); ++i) {
        generated.push_back(topUp[i]);
      }
    }
    if (generated.size() > kSeedCount) {
      generated.resize(kSeedCount);
    }
    for (std::size_t i = 0; i < generated.size(); ++i) {
      generated[i].id = static_cast<uint32_t>(i);
    }
  }

  seeds_ = generated;

  scheduler_ = PatternScheduler{};
  const float bpm = (seedPrimeMode_ == SeedPrimeMode::kTapTempo) ? currentTapTempoBpm() : 120.f;
  scheduler_.setBpm(bpm);
  scheduler_.setTriggerCallback(&engines_, &EngineRouter::dispatchThunk);

  for (const Seed& seed : seeds_) {
    scheduler_.addSeed(seed);
  }

  seedEngineSelections_.assign(seeds_.size(), 0);
  for (std::size_t i = 0; i < seeds_.size(); ++i) {
    const uint8_t desired = (i < previousSelections.size()) ? previousSelections[i] : seeds_[i].engine;
    setSeedEngine(static_cast<uint8_t>(i), desired);
  }

  if (!seeds_.empty()) {
    const std::size_t maxIndex = seeds_.size() - 1;
    const uint8_t targetFocus = previousSeeds.empty()
                                    ? 0
                                    : static_cast<uint8_t>(std::min<std::size_t>(previousFocus, maxIndex));
    setFocusSeed(targetFocus);
  } else {
    focusSeed_ = 0;
  }

  seedsPrimed_ = true;
  externalTransportRunning_ = false;
  transportLatchedRunning_ = false;
  transportGateHeld_ = false;
  updateClockDominance();
  const bool ledOn = !SeedBoxConfig::kQuietMode;
  hal::io::writeDigital(kStatusLedPin, ledOn);
  displayDirty_ = true;
}

std::vector<Seed> AppState::buildLfsrSeeds(uint32_t masterSeed, std::size_t count) {
  std::vector<Seed> seeds;
  seeds.reserve(count);
  uint32_t state = masterSeed ? masterSeed : 0x5EEDB0B1u;
  for (std::size_t i = 0; i < count; ++i) {
    Seed seed{};
    seed.id = static_cast<uint32_t>(i);
    seed.source = Seed::Source::kLfsr;
    seed.lineage = masterSeed;
    seed.prng = RNG::xorshift(state);
    seed.engine = 0;
    seed.sampleIdx = static_cast<uint8_t>(i % 16);
    seed.pitch = static_cast<float>(static_cast<int32_t>(RNG::xorshift(state) % 25) - 12);
    seed.density = 0.5f + 0.75f * RNG::uniform01(state);
    seed.probability = 0.6f + 0.4f * RNG::uniform01(state);
    seed.jitterMs = 2.0f + 12.0f * RNG::uniform01(state);
    seed.tone = RNG::uniform01(state);
    seed.spread = 0.1f + 0.8f * RNG::uniform01(state);
    seed.mutateAmt = 0.05f + 0.15f * RNG::uniform01(state);

    seed.granular.grainSizeMs = 35.f + 120.f * RNG::uniform01(state);
    seed.granular.sprayMs = 4.f + 24.f * RNG::uniform01(state);
    seed.granular.transpose = static_cast<float>(static_cast<int32_t>(RNG::xorshift(state) % 13) - 6);
    seed.granular.windowSkew = (RNG::uniform01(state) * 2.f) - 1.f;
    seed.granular.stereoSpread = 0.2f + 0.7f * RNG::uniform01(state);
    seed.granular.source = (RNG::uniform01(state) > 0.4f)
                               ? static_cast<uint8_t>(GranularEngine::Source::kSdClip)
                               : static_cast<uint8_t>(GranularEngine::Source::kLiveInput);
    seed.granular.sdSlot = static_cast<uint8_t>(RNG::xorshift(state) % GranularEngine::kSdClipSlots);

    seed.resonator.exciteMs = 2.0f + 10.0f * RNG::uniform01(state);
    seed.resonator.damping = RNG::uniform01(state);
    seed.resonator.brightness = RNG::uniform01(state);
    seed.resonator.feedback = 0.55f + 0.4f * RNG::uniform01(state);
    if (seed.resonator.feedback > 0.99f) seed.resonator.feedback = 0.99f;
    seed.resonator.mode = static_cast<uint8_t>(i % 2);
    seed.resonator.bank = static_cast<uint8_t>(RNG::xorshift(state) % 6);

    seeds.push_back(seed);
  }
  return seeds;
}

std::vector<Seed> AppState::buildTapTempoSeeds(uint32_t masterSeed, std::size_t count, float bpm) {
  auto seeds = buildLfsrSeeds(masterSeed, count);
  const float safeBpm = bpm > 1.f ? bpm : 120.f;
  const float densityScale = safeBpm / 120.f;
  const uint32_t lineageTag = static_cast<uint32_t>(std::max(0.f, safeBpm * 100.f));
  for (auto& seed : seeds) {
    seed.source = Seed::Source::kTapTempo;
    seed.lineage = lineageTag;
    seed.density = std::clamp(seed.density * densityScale, 0.25f, 6.0f);
    seed.jitterMs = std::max(0.5f, seed.jitterMs * 0.5f);
  }
  return seeds;
}

std::vector<Seed> AppState::buildLiveInputSeeds(uint32_t masterSeed, std::size_t count) {
  auto seeds = buildLfsrSeeds(masterSeed, count);
  for (auto& seed : seeds) {
    seed.source = Seed::Source::kLiveInput;
    seed.lineage = masterSeed;
    seed.granular.source = static_cast<uint8_t>(GranularEngine::Source::kLiveInput);
    seed.granular.sdSlot = 0;
  }
  return seeds;
}

std::vector<Seed> AppState::buildPresetSeeds(std::size_t count) {
  std::vector<Seed> seeds;
  if (presetBuffer_.seeds.empty()) {
    return buildLfsrSeeds(masterSeed_, count);
  }
  seeds.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    const Seed& templateSeed = presetBuffer_.seeds[i % presetBuffer_.seeds.size()];
    Seed seed = templateSeed;
    seed.id = static_cast<uint32_t>(i);
    seed.source = Seed::Source::kPreset;
    seed.lineage = presetBuffer_.id;
    if (seed.prng == 0) {
      uint32_t lineageSeed = masterSeed_ ^ (presetBuffer_.id + static_cast<uint32_t>(i * 97));
      uint32_t rngState = lineageSeed;
      if (rngState == 0) {
        rngState = masterSeed_;
      }
      seed.prng = RNG::xorshift(rngState);
    }
    seeds.push_back(seed);
  }
  return seeds;
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
  if (!seedsPrimed_) {
    primeSeeds(masterSeed_);
  }
#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  const bool wasDominant = externalClockDominant_;
#endif
  externalTransportRunning_ = true;
  alignProviderRunning(clock_, internalClock_, midiClockIn_, midiClockOut_, externalTransportRunning_);
  updateClockDominance();
#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  if (!wasDominant && externalClockDominant_) {
    Serial.println(F("external clock: TRS/USB seized transport"));
  }
#endif
  if (clock_ == &midiClockIn_ || externalClockDominant_) {
    midiClockIn_.onTick();
  }
}

void AppState::onExternalTransportStart() {
#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  const bool wasDominant = externalClockDominant_;
#endif
  externalTransportRunning_ = true;
  selectClockProvider(&midiClockIn_);
  updateClockDominance();
  if (transportLatchEnabled_) {
    transportLatchedRunning_ = true;
  }
#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  if (!wasDominant && externalClockDominant_) {
    Serial.println(F("external clock: transport START"));
  }
#endif
}

void AppState::onExternalTransportStop() {
#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  const bool wasDominant = externalClockDominant_;
#endif
  externalTransportRunning_ = false;
  if (!followExternalClockEnabled_) {
    selectClockProvider(&internalClock_);
  } else {
    alignProviderRunning(clock_, internalClock_, midiClockIn_, midiClockOut_, externalTransportRunning_);
  }
  updateClockDominance();
  if (transportLatchEnabled_) {
    transportLatchedRunning_ = false;
  }
#if SEEDBOX_HW && SEEDBOX_DEBUG_CLOCK_SOURCE
  if (wasDominant && !externalClockDominant_) {
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

void AppState::updateClockDominance() {
  // External clock wins if either the follow bit is set or the transport is
  // actively running.  This line is the truth table behind the entire sync
  // story, so yeah, spell it out.
  externalClockDominant_ = followExternalClockEnabled_ || externalTransportRunning_;
}

void AppState::applyMn42ModeBits(uint8_t value) {
  // MN42 packs several toggle bits into one CC.  Unpack them and update the
  // flags that control our sync + debug behaviours.
  const bool follow = (value & seedbox::interop::mn42::mode::kFollowExternalClock) != 0;
  if (followExternalClockEnabled_ != follow) {
    followExternalClockEnabled_ = follow;
    selectClockProvider(follow ? static_cast<ClockProvider*>(&midiClockIn_)
                               : static_cast<ClockProvider*>(&internalClock_));
    updateClockDominance();
  }

  debugMetersEnabled_ = (value & seedbox::interop::mn42::mode::kExposeDebugMeters) != 0;

  const bool latch = (value & seedbox::interop::mn42::mode::kLatchTransport) != 0;
  if (transportLatchEnabled_ != latch) {
    transportLatchEnabled_ = latch;
    if (transportLatchEnabled_) {
      transportLatchedRunning_ = externalTransportRunning_;
    } else {
      transportLatchedRunning_ = false;
      transportGateHeld_ = false;
    }
  } else if (transportLatchEnabled_) {
    transportLatchedRunning_ = externalTransportRunning_;
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
  const bool gateHigh = value > 0;
  if (transportLatchEnabled_) {
    if (gateHigh && !transportGateHeld_) {
      transportGateHeld_ = true;
      if (transportLatchedRunning_) {
        onExternalTransportStop();
      } else {
        onExternalTransportStart();
      }
    } else if (!gateHigh) {
      transportGateHeld_ = false;
    }
    return;
  }

  if (gateHigh) {
    onExternalTransportStart();
  } else {
    onExternalTransportStop();
  }
}

void AppState::reseed(uint32_t masterSeed) {
  primeSeeds(masterSeed);
  clearPresetCrossfade();
  activePresetSlot_.clear();
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
  activePresetSlot_ = slotName;
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
  applyPreset(preset, crossfade);
  return true;
}

void AppState::captureDisplaySnapshot(DisplaySnapshot& out) const {
  captureDisplaySnapshot(out, nullptr);
}

void AppState::captureDisplaySnapshot(DisplaySnapshot& out, UiState* ui) const {
  // OLED real estate is tiny, so we jam the master seed into the title and then
  // use status/metrics/nuance rows to narrate what's happening with the focus
  // seed. Think of it as a glorified logcat for the front panel.
  std::array<char, 64> scratch{};
  writeDisplayField(out.title, formatScratch(scratch, "SeedBox %06X", masterSeed_ & 0xFFFFFFu));

  const float sampleRate = hal::audio::sampleRate();
  const std::size_t block = hal::audio::framesPerBlock();
  const bool ledOn = hal::io::readDigital(kStatusLedPin);
  const uint32_t nowSamples = scheduler_.nowSamples();

  UiState localUi{};
  UiState* uiOut = ui ? ui : &localUi;

  const bool hasSeeds = !seeds_.empty();
  std::size_t focusIndex = 0;
  if (hasSeeds) {
    focusIndex = std::min<std::size_t>(focusSeed_, seeds_.size() - 1);
  }
  const bool globalLocked = seedLock_.globalLocked();
  const bool focusLocked = hasSeeds ? seedLock_.seedLocked(focusIndex) : false;
  const bool anyLockActive = globalLocked || focusLocked;

  uiOut->mode = UiState::Mode::kPerformance;
  if (mode_ == Mode::SWING) {
    uiOut->mode = UiState::Mode::kEdit;
  } else if (anyLockActive) {
    uiOut->mode = UiState::Mode::kEdit;
  }
  if (debugMetersEnabled_) {
    uiOut->mode = UiState::Mode::kSystem;
  }
  uiOut->bpm = scheduler_.bpm();
  uiOut->swing = swingPercent_;
  uiOut->clock = externalClockDominant_ ? UiState::ClockSource::kExternal : UiState::ClockSource::kInternal;
  uiOut->seedLocked = anyLockActive;

  if (hasSeeds) {
    const Seed& s = seeds_[focusIndex];
    writeUiField(uiOut->engineName, engineLongName(s.engine));
  } else {
    writeUiField(uiOut->engineName, "Idle");
  }

  if (mode_ == Mode::SWING) {
    writeUiField(uiOut->pageHints[0], "Tap: exit swing");
    writeUiField(uiOut->pageHints[1], "Seed:5% Den:1%");
  } else if (currentPage_ == Page::kStorage) {
    writeUiField(uiOut->pageHints[0], "GPIO: recall");
    writeUiField(uiOut->pageHints[1], "Hold GPIO: save");
  } else if (globalLocked) {
    writeUiField(uiOut->pageHints[0], "Pg seeds locked");
    writeUiField(uiOut->pageHints[1], "Pg+Md: unlock all");
  } else if (focusLocked) {
    writeUiField(uiOut->pageHints[0], "Pg focus locked");
    writeUiField(uiOut->pageHints[1], "Pg+Md: unlock");
  } else {
    writeUiField(uiOut->pageHints[0], "Tone S:src ALT:d");
    const auto primeHint = formatScratch(scratch, "S+A:p Tap:%s", primeModeLabel(seedPrimeMode_));
    writeUiField(uiOut->pageHints[1], primeHint);
  }

  if (!hasSeeds) {
    if constexpr (SeedBoxConfig::kQuietMode) {
      writeDisplayField(out.status, formatScratch(scratch, "%s quiet", modeLabel(mode_)));
    } else {
      writeDisplayField(out.status, formatScratch(scratch, "%s empty", modeLabel(mode_)));
    }
    writeDisplayField(out.metrics, formatScratch(scratch, "SR%.1fkB%02zu", sampleRate / 1000.f, block));
    writeDisplayField(out.nuance,
                      formatScratch(scratch, "AC%05lluF%05lu",
                                     static_cast<unsigned long long>(audioCallbackCount_ % 100000ULL),
                                     static_cast<unsigned long>(frame_ % 100000UL)));
    return;
  }

  const Seed& s = seeds_[focusIndex];
  const std::string_view shortName = engineLabel(engines_, s.engine);
  writeDisplayField(out.status,
                    formatScratch(scratch, "#%02u%.*s%+0.1fst%c", s.id, static_cast<int>(shortName.size()),
                                   shortName.data(), s.pitch, ledOn ? '*' : '-'));
  const float density = std::clamp(s.density, 0.0f, 99.99f);
  const float probability = std::clamp(s.probability, 0.0f, 1.0f);
  const Seed* schedulerSeed = debugScheduledSeed(static_cast<uint8_t>(focusIndex));
  const unsigned prngByte = schedulerSeed ? static_cast<unsigned>(schedulerSeed->prng & 0xFFu) : 0u;

#if SEEDBOX_HW && !QUIET_MODE
  if (debugMetersEnabled_ && s.engine == 2) {
    const float fanout = engines_.resonator().fanoutProbeLevel();
    writeDisplayField(out.metrics, formatScratch(scratch, "D%.2fP%.2fF%.2f", density, probability, fanout));
  } else {
    writeDisplayField(out.metrics,
                      formatScratch(scratch, "D%.2fP%.2fN%03u", density, probability,
                                     static_cast<unsigned>(nowSamples % 1000u)));
  }
#else
  writeDisplayField(out.metrics,
                    formatScratch(scratch, "D%.2fP%.2fN%03u", density, probability,
                                   static_cast<unsigned>(nowSamples % 1000u)));
#endif

  const float mutate = std::clamp(s.mutateAmt, 0.0f, 1.0f);
  const float jitterMs = std::clamp(s.jitterMs, 0.0f, 999.9f);
  const unsigned jitterInt = static_cast<unsigned>(std::min(99.0f, std::round(jitterMs)));
  char engineToken[8] = {'-', '-', '-', '-', '\0'};

  switch (s.engine) {
    case 0: {
      const auto voice = engines_.sampler().voice(static_cast<uint8_t>(focusIndex % Sampler::kMaxVoices));
      std::snprintf(engineToken, sizeof(engineToken), "%c%c%02u", voice.active ? 'S' : 's',
                    voice.usesSdStreaming ? 'D' : 'M', voice.sampleIndex);
      break;
    }
    case 1: {
      const auto voice = engines_.granular().voice(static_cast<uint8_t>(focusIndex % GranularEngine::kVoicePoolSize));
      GranularEngine::Source seedSource = static_cast<GranularEngine::Source>(s.granular.source);
      if (seedSource != GranularEngine::Source::kSdClip) {
        seedSource = GranularEngine::Source::kLiveInput;
      }
      uint8_t sdSlot = s.granular.sdSlot;
      if (GranularEngine::kSdClipSlots > 0) {
        sdSlot = static_cast<uint8_t>(sdSlot % GranularEngine::kSdClipSlots);
      }
      if (seedSource == GranularEngine::Source::kSdClip && GranularEngine::kSdClipSlots > 1 && sdSlot == 0) {
        sdSlot = 1;
      }
      const bool voiceActive = voice.active && voice.seedId == s.id;
      char sourceTag = (seedSource == GranularEngine::Source::kLiveInput) ? 'L' : 'C';
      if (voiceActive && voice.source == seedSource) {
        if (seedSource == GranularEngine::Source::kSdClip && GranularEngine::kSdClipSlots > 0) {
          sdSlot = static_cast<uint8_t>(voice.sdSlot % GranularEngine::kSdClipSlots);
        }
      }
      std::snprintf(engineToken, sizeof(engineToken), "%c%c%02u", voiceActive ? 'G' : 'g', sourceTag, sdSlot);
      break;
    }
    case 2: {
      const auto voice = engines_.resonator().voice(static_cast<uint8_t>(focusIndex % ResonatorBank::kMaxVoices));
      const char* preset = engines_.resonator().presetName(voice.bank);
      char presetA = '-';
      char presetB = '-';
      if (preset && preset[0] != '\0') {
        presetA = preset[0];
        if (preset[1] != '\0') {
          presetB = preset[1];
        }
      }
      const uint8_t modeDigit = static_cast<uint8_t>(std::min<uint8_t>(voice.mode, 9));
      std::snprintf(engineToken, sizeof(engineToken), "%c%c%c%c", voice.active ? 'R' : 'r', presetA, presetB,
                    static_cast<char>('0' + modeDigit));
      break;
    }
    default:
      std::snprintf(engineToken, sizeof(engineToken), "?%03u", static_cast<unsigned>(s.engine % 1000));
      break;
  }

  writeDisplayField(out.nuance,
                    formatScratch(scratch, "Mu%.2f%sR%02XJ%02u", mutate, engineToken, prngByte, jitterInt));
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
  seedbox::Preset preset;
  preset.slot = slot.empty() ? std::string(kDefaultPresetSlot) : std::string(slot);
  preset.masterSeed = masterSeed_;
  preset.focusSeed = focusSeed_;
  preset.clock.bpm = scheduler_.bpm();
  preset.clock.followExternal = followExternalClockEnabled_;
  preset.clock.debugMeters = debugMetersEnabled_;
  preset.clock.transportLatch = transportLatchEnabled_;
  preset.page = static_cast<seedbox::PageId>(currentPage_);
  preset.seeds = seeds_;
  preset.engineSelections = seedEngineSelections_;
  if (preset.engineSelections.size() < preset.seeds.size()) {
    preset.engineSelections.resize(preset.seeds.size(), 0);
    for (std::size_t i = 0; i < preset.seeds.size(); ++i) {
      preset.engineSelections[i] = preset.seeds[i].engine;
    }
  }
  return preset;
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
  activePresetSlot_ = preset.slot.empty() ? std::string(kDefaultPresetSlot) : preset.slot;
  masterSeed_ = preset.masterSeed;
  followExternalClockEnabled_ = preset.clock.followExternal;
  debugMetersEnabled_ = preset.clock.debugMeters;
  const bool previousLatch = transportLatchEnabled_;
  transportLatchEnabled_ = preset.clock.transportLatch;
  if (!transportLatchEnabled_) {
    transportLatchedRunning_ = false;
    transportGateHeld_ = false;
  } else if (!previousLatch) {
    transportLatchedRunning_ = externalTransportRunning_;
  }
  scheduler_.setBpm(preset.clock.bpm);
  updateClockDominance();
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
    presetCrossfade_.from = seeds_;
    presetCrossfade_.to = preset.seeds;
    presetCrossfade_.total = kPresetCrossfadeTicks;
    presetCrossfade_.remaining = kPresetCrossfadeTicks;
  } else {
    seeds_ = preset.seeds;
    clearPresetCrossfade();
    scheduler_ = PatternScheduler{};
    scheduler_.setBpm(preset.clock.bpm);
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
  displayDirty_ = true;
}

void AppState::stepPresetCrossfade() {
  if (presetCrossfade_.remaining == 0 || presetCrossfade_.total == 0) {
    return;
  }
  if (presetCrossfade_.from.size() != presetCrossfade_.to.size() ||
      presetCrossfade_.to.size() != seeds_.size()) {
    seeds_ = presetCrossfade_.to;
    clearPresetCrossfade();
    const float currentBpm = scheduler_.bpm();
    scheduler_ = PatternScheduler{};
    scheduler_.setBpm(currentBpm);
    scheduler_.setTriggerCallback(&engines_, &EngineRouter::dispatchThunk);
    const bool hardwareMode = (engines_.granular().mode() == GranularEngine::Mode::kHardware);
    scheduler_.setSampleClockFn(hardwareMode ? &hal::audio::sampleClock : nullptr);
    for (const Seed& s : seeds_) {
      scheduler_.addSeed(s);
    }
    return;
  }

  const float total = static_cast<float>(presetCrossfade_.total);
  const float remaining = static_cast<float>(presetCrossfade_.remaining);
  const float mix = (total <= 0.f) ? 1.0f : (1.0f - (remaining / total));
  for (std::size_t i = 0; i < seeds_.size(); ++i) {
    seeds_[i] = blendSeeds(presetCrossfade_.from[i], presetCrossfade_.to[i], mix);
    scheduler_.updateSeed(i, seeds_[i]);
  }

  if (presetCrossfade_.remaining > 0) {
    --presetCrossfade_.remaining;
  }
  if (presetCrossfade_.remaining == 0) {
    seeds_ = presetCrossfade_.to;
    for (std::size_t i = 0; i < seeds_.size(); ++i) {
      scheduler_.updateSeed(i, seeds_[i]);
    }
    clearPresetCrossfade();
  }
}

void AppState::clearPresetCrossfade() {
  presetCrossfade_ = {};
}
