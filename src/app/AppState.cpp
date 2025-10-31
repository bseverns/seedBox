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
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <utility>
#include <initializer_list>
#include "SeedBoxConfig.h"
#include "interop/mn42_map.h"
#include "util/RNG.h"
#include "engine/Sampler.h"
#include "hal/hal_audio.h"
#include "hal/hal_io.h"
#ifdef SEEDBOX_HW
  #include "HardwarePrelude.h"
  #include "AudioMemoryBudget.h"
  #include "io/Storage.h"
#endif

#if defined(SEEDBOX_HW) && defined(SEEDBOX_DEBUG_CLOCK_SOURCE)
  #include <Arduino.h>
#endif

namespace {
constexpr uint8_t kEngineCycleCc = 20;

constexpr hal::io::PinNumber kStatusLedPin = 13;

constexpr std::array<const char*, 4> kDemoSdClips{{"wash", "dust", "vox", "pads"}};

void populateSdClips(GranularEngine& engine) {
  for (uint8_t i = 0; i < kDemoSdClips.size(); ++i) {
    engine.registerSdClip(static_cast<uint8_t>(i + 1), kDemoSdClips[i]);
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

uint8_t sanitizeEngine(const EngineRouter& router, uint8_t engine) {
  return router.sanitizeEngineId(engine);
}

std::string_view engineLabel(const EngineRouter& router, uint8_t engine) {
  const uint8_t sanitized = router.sanitizeEngineId(engine);
  const std::string_view label = router.engineShortName(sanitized);
  if (label.empty()) {
    return std::string_view{"UNK"};
  }
  return label;
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

constexpr std::array<ModeTransition, 18> kModeTransitions{{
    {AppState::Mode::HOME, InputEvents::Type::ButtonPress, buttonMask(hal::Board::ButtonID::EncoderSeedBank),
     AppState::Mode::SEEDS},
    {AppState::Mode::HOME, InputEvents::Type::ButtonPress, buttonMask(hal::Board::ButtonID::EncoderDensity),
     AppState::Mode::ENGINE},
    {AppState::Mode::HOME, InputEvents::Type::ButtonPress, buttonMask(hal::Board::ButtonID::EncoderToneTilt),
     AppState::Mode::PERF},
    {AppState::Mode::HOME, InputEvents::Type::ButtonPress, buttonMask(hal::Board::ButtonID::EncoderFxMutate),
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
    {AppState::Mode::SETTINGS, InputEvents::Type::ButtonChord,
     buttonMask({hal::Board::ButtonID::Shift, hal::Board::ButtonID::AltSeed}), AppState::Mode::PERF},
    {AppState::Mode::PERF, InputEvents::Type::ButtonChord,
     buttonMask({hal::Board::ButtonID::Shift, hal::Board::ButtonID::AltSeed}), AppState::Mode::SETTINGS},
}};

}

AppState::AppState(hal::Board& board) : board_(board), input_(board) {
  internalClock_.attachScheduler(&scheduler_);
  midiClockIn_.attachScheduler(&scheduler_);
  midiClockOut_.attachScheduler(&scheduler_);
  selectClockProvider(&internalClock_);
}

void AppState::audioCallbackTrampoline(const hal::audio::StereoBufferView& buffer, void* ctx) {
  if (auto* self = static_cast<AppState*>(ctx)) {
    self->handleAudio(buffer);
  }
}

AppState::~AppState() {
  hal::audio::stop();
  hal::audio::shutdown();
}

// initHardware wires up the physical instrument: MIDI ingress, the engine
// router in hardware mode, and finally the deterministic seed prime. The code
// mirrors initSim because we want both paths to be interchangeable in class —
// students can run the simulator on a laptop and trust that the Teensy build
// behaves the same.
void AppState::initHardware() {
#ifdef SEEDBOX_HW
  // Lock in the entire audio buffer pool before any engine spins up. We slam
  // all four line items from AudioMemoryBudget into one call so individual
  // init() routines can't accidentally stomp the global allocator mid-set.
  AudioMemory(AudioMemoryBudget::kTotalBlocks);

  if constexpr (!SeedBoxConfig::kQuietMode) {
    midi.begin();
    midi.setClockHandler([this]() { onExternalClockTick(); });
    midi.setStartHandler([this]() { onExternalTransportStart(); });
    midi.setStopHandler([this]() { onExternalTransportStop(); });
    midi.setControlChangeHandler(
        [this](uint8_t ch, uint8_t cc, uint8_t val) {
          onExternalControlChange(ch, cc, val);
        });
  } else {
    midi.begin();
    midi.setClockHandler(nullptr);
    midi.setStartHandler(nullptr);
    midi.setStopHandler(nullptr);
    midi.setControlChangeHandler(nullptr);
  }
#endif
  hal::audio::init(&AppState::audioCallbackTrampoline, this);
  hal::audio::start();
  hal::io::writeDigital(kStatusLedPin, false);
  bootRuntime(EngineRouter::Mode::kHardware, true);
#ifdef SEEDBOX_HW
  midi.markAppReady();
#endif
}

// initSim is the lab-friendly twin of initHardware. No MIDI bootstrap, but the
// rest of the wiring is identical so deterministic behaviour survives unit
// tests and lecture demos.
void AppState::initSim() {
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
  reseed(masterSeed_);
  scheduler_.setSampleClockFn(hardwareMode ? &hal::audio::sampleClock : nullptr);
  captureDisplaySnapshot(displayCache_);
  displayDirty_ = true;
  audioCallbackCount_ = 0;
  mode_ = Mode::HOME;
  input_.clear();
  swingPageRequested_ = false;
}

void AppState::handleAudio(const hal::audio::StereoBufferView& buffer) {
  ++audioCallbackCount_;
  if (!buffer.left || !buffer.right) {
    return;
  }
  std::fill(buffer.left, buffer.left + buffer.frames, 0.0f);
  std::fill(buffer.right, buffer.right + buffer.frames, 0.0f);
}

// tick is the heartbeat. Every call either primes seeds (first-run), lets the
// internal scheduler drive things when we own the transport, or just counts
// frames so the OLED can display a ticking counter.
void AppState::tick() {
  board_.poll();
  input_.update();
  processInputEvents();
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
  ++frame_;
  captureDisplaySnapshot(displayCache_);
  displayDirty_ = true;
}

void AppState::processInputEvents() {
  const auto& evts = input_.events();
  for (const auto& evt : evts) {
    if (evt.type == InputEvents::Type::ButtonLongPress &&
        evt.primaryButton == hal::Board::ButtonID::EncoderSeedBank) {
      handleReseedRequest();
    }
    if (handleClockButtonEvent(evt)) {
      continue;
    }
    applyModeTransition(evt);
    dispatchToPage(evt);
  }
}

bool AppState::handleClockButtonEvent(const InputEvents::Event& evt) {
  if (evt.primaryButton != hal::Board::ButtonID::TapTempo) {
    return false;
  }
  if (evt.type == InputEvents::Type::ButtonLongPress) {
    swingPageRequested_ = true;
    displayDirty_ = true;
    return true;
  }
  if (evt.type == InputEvents::Type::ButtonPress) {
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
      if (mode_ != transition.to) {
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
  static constexpr std::array<ModeDispatch, 6> kModeHandlers{{
      {Mode::HOME, &AppState::handleHomeEvent},
      {Mode::SEEDS, &AppState::handleSeedsEvent},
      {Mode::ENGINE, &AppState::handleEngineEvent},
      {Mode::PERF, &AppState::handlePerfEvent},
      {Mode::SETTINGS, &AppState::handleSettingsEvent},
      {Mode::UTIL, &AppState::handleUtilEvent},
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
    default: return "?";
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

  if constexpr (SeedBoxConfig::kQuietMode) {
    seeds_.clear();
    ClockProvider* provider = clock_ ? clock_ : &internalClock_;
    scheduler_ = PatternScheduler(provider);
    selectClockProvider(provider);
    scheduler_.setBpm(0.f);
    scheduler_.setTriggerCallback(&engines_, &EngineRouter::dispatchThunk);
    seedEngineSelections_.clear();
    setFocusSeed(0);
    // Quiet mode is a one-shot reset: once the mute scaffold is built we want
    // tick() to stop cycling back through primeSeeds().  Mark the table as
    // primed so followExternalClockEnabled() and friends keep whatever state
    // the tests (or a teacher) dial in afterwards.
    seedsPrimed_ = true;
    externalTransportRunning_ = false;
    transportLatchedRunning_ = false;
    transportGateHeld_ = false;
    followExternalClockEnabled_ = false;
    debugMetersEnabled_ = false;
    transportLatchEnabled_ = false;
    mn42HelloSeen_ = false;
    alignProviderRunning(clock_, internalClock_, midiClockIn_, midiClockOut_, externalTransportRunning_);
    updateClockDominance();
    hal::io::writeDigital(kStatusLedPin, false);
    displayDirty_ = true;
    return;
  }

  const std::vector<uint8_t> previousSelections = seedEngineSelections_;
  seeds_.clear();
  ClockProvider* provider = clock_ ? clock_ : &internalClock_;
  scheduler_ = PatternScheduler(provider);
  selectClockProvider(provider);
  scheduler_.setBpm(120.f);
  scheduler_.setTriggerCallback(&engines_, &EngineRouter::dispatchThunk);

  uint32_t state = masterSeed_;
  constexpr size_t kSeedCount = 4;
  seedEngineSelections_.assign(kSeedCount, 0);
  engines_.setSeedCount(kSeedCount);
  for (size_t i = 0; i < kSeedCount; ++i) {
    // Every loop iteration births a new Seed and walks RNG::xorshift / uniform
    // helpers forward. Because the RNG is deterministic we can rerun
    // primeSeeds with the same masterSeed and always land on the same genome.
    Seed seed{};
    seed.id = static_cast<uint32_t>(i);
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

    // --- Granular controls ---
    // Grain size controls the duration of a single voice. We skew the random
    // walk toward shorter grains so the scheduler can demonstrate density
    // stacking without smearing the entire texture.
    seed.granular.grainSizeMs = 35.f + 120.f * RNG::uniform01(state);
    seed.granular.sprayMs = 4.f + 24.f * RNG::uniform01(state);
    seed.granular.transpose = static_cast<float>(static_cast<int32_t>(RNG::xorshift(state) % 13) - 6);
    seed.granular.windowSkew = (RNG::uniform01(state) * 2.f) - 1.f;
    seed.granular.stereoSpread = 0.2f + 0.7f * RNG::uniform01(state);
    seed.granular.source = (RNG::uniform01(state) > 0.4f)
                               ? static_cast<uint8_t>(GranularEngine::Source::kSdClip)
                               : static_cast<uint8_t>(GranularEngine::Source::kLiveInput);
    seed.granular.sdSlot = static_cast<uint8_t>(RNG::xorshift(state) % GranularEngine::kSdClipSlots);

    // --- Resonator controls ---
    seed.resonator.exciteMs = 2.0f + 10.0f * RNG::uniform01(state);
    seed.resonator.damping = RNG::uniform01(state);
    seed.resonator.brightness = RNG::uniform01(state);
    seed.resonator.feedback = 0.55f + 0.4f * RNG::uniform01(state);
    if (seed.resonator.feedback > 0.99f) seed.resonator.feedback = 0.99f;
    seed.resonator.mode = static_cast<uint8_t>(i % 2);
    seed.resonator.bank = static_cast<uint8_t>(RNG::xorshift(state) % 6);

    seeds_.push_back(seed);
    scheduler_.addSeed(seeds_.back());
  }

  for (size_t i = 0; i < kSeedCount; ++i) {
    // Restore any explicit engine selections we captured before reseeding.
    // That keeps lesson plans deterministic: once a class sets seed 2 to the
    // resonator they can reseed the parameters without losing the engine
    // choice.
    const uint8_t desired = (i < previousSelections.size())
                                ? previousSelections[i]
                                : seeds_[i].engine;
    setSeedEngine(static_cast<uint8_t>(i), desired);
  }

  setFocusSeed(0);
  seedsPrimed_ = true;
  externalTransportRunning_ = false;
  transportLatchedRunning_ = false;
  transportGateHeld_ = false;
  alignProviderRunning(clock_, internalClock_, midiClockIn_, midiClockOut_, externalTransportRunning_);
  updateClockDominance();
  hal::io::writeDigital(kStatusLedPin, true);
  displayDirty_ = true;
}

void AppState::onExternalClockTick() {
  if constexpr (SeedBoxConfig::kQuietMode) {
    externalTransportRunning_ = true;
    updateClockDominance();
    return;
  }
  if (!seedsPrimed_) {
    primeSeeds(masterSeed_);
  }
#if defined(SEEDBOX_HW) && defined(SEEDBOX_DEBUG_CLOCK_SOURCE)
  const bool wasDominant = externalClockDominant_;
#endif
  externalTransportRunning_ = true;
  alignProviderRunning(clock_, internalClock_, midiClockIn_, midiClockOut_, externalTransportRunning_);
  updateClockDominance();
#if defined(SEEDBOX_HW) && defined(SEEDBOX_DEBUG_CLOCK_SOURCE)
  if (!wasDominant && externalClockDominant_) {
    Serial.println(F("external clock: TRS/USB seized transport"));
  }
#endif
  if (clock_ == &midiClockIn_ || externalClockDominant_) {
    midiClockIn_.onTick();
  }
}

void AppState::onExternalTransportStart() {
#if defined(SEEDBOX_HW) && defined(SEEDBOX_DEBUG_CLOCK_SOURCE)
  const bool wasDominant = externalClockDominant_;
#endif
  externalTransportRunning_ = true;
  selectClockProvider(&midiClockIn_);
  updateClockDominance();
  if (transportLatchEnabled_) {
    transportLatchedRunning_ = true;
  }
#if defined(SEEDBOX_HW) && defined(SEEDBOX_DEBUG_CLOCK_SOURCE)
  if (!wasDominant && externalClockDominant_) {
    Serial.println(F("external clock: transport START"));
  }
#endif
}

void AppState::onExternalTransportStop() {
#if defined(SEEDBOX_HW) && defined(SEEDBOX_DEBUG_CLOCK_SOURCE)
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
#if defined(SEEDBOX_HW) && defined(SEEDBOX_DEBUG_CLOCK_SOURCE)
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
  engines_.reseed(masterSeed_);
  const bool hardwareMode = (engines_.granular().mode() == GranularEngine::Mode::kHardware);
  scheduler_.setSampleClockFn(hardwareMode ? &hal::audio::sampleClock : nullptr);
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
}

void AppState::captureDisplaySnapshot(DisplaySnapshot& out) const {
  // OLED real estate is tiny, so we jam the master seed into the title and then
  // use status/metrics/nuance rows to narrate what's happening with the focus
  // seed. Think of it as a glorified logcat for the front panel.
  std::array<char, 64> scratch{};
  std::array<char, 64> statusScratch{};
  writeDisplayField(out.title, formatScratch(scratch, "SeedBox %06X", masterSeed_ & 0xFFFFFFu));

  const float sampleRate = hal::audio::sampleRate();
  const std::size_t block = hal::audio::framesPerBlock();
  const bool ledOn = hal::io::readDigital(kStatusLedPin);
  const uint32_t nowSamples = scheduler_.nowSamples();

  if (seeds_.empty()) {
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

  const size_t focusIndex = std::min<size_t>(focusSeed_, seeds_.size() - 1);
  const Seed& s = seeds_[focusIndex];
  const std::string_view shortName = engineLabel(engines_, s.engine);
  writeDisplayField(out.status,
                    formatScratch(scratch, "#%02u%.*s%+0.1fst%c", s.id, static_cast<int>(shortName.size()),
                                   shortName.data(), s.pitch, ledOn ? '*' : '-'));
  const float density = std::clamp(s.density, 0.0f, 99.99f);
  const float probability = std::clamp(s.probability, 0.0f, 1.0f);
  const Seed* schedulerSeed = debugScheduledSeed(static_cast<uint8_t>(focusIndex));
  const unsigned prngByte = schedulerSeed ? static_cast<unsigned>(schedulerSeed->prng & 0xFFu) : 0u;

#if defined(SEEDBOX_HW) && !QUIET_MODE
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
      const char sourceTag = (voice.source == GranularEngine::Source::kLiveInput) ? 'L' : 'C';
      std::snprintf(engineToken, sizeof(engineToken), "%c%c%02u", voice.active ? 'G' : 'g', sourceTag, voice.sdSlot);
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
