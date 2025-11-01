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
#include <string>
#include <string_view>
#include <utility>
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
constexpr uint8_t kEngineCount = 3;
constexpr uint8_t kEngineCycleCc = 20;
constexpr uint32_t kStorageLongPressFrames = 60;
constexpr std::string_view kDefaultPresetSlot = "default";

constexpr hal::io::PinNumber kReseedButtonPin = 2;
constexpr hal::io::PinNumber kStatusLedPin = 13;

const std::array<hal::io::DigitalConfig, 2> kFrontPanelPins{{
    {kReseedButtonPin, true, true},
    {kStatusLedPin, false, false},
}};

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

template <typename... Args>
std::string_view formatScratch(std::array<char, 64>& scratch, const char* fmt, Args&&... args) {
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

uint8_t sanitizeEngine(uint8_t engine) {
  // Engine IDs arrive from MIDI CCs and debug tools, so we modulo them into the
  // valid range to avoid out-of-bounds dispatches.
  if (kEngineCount == 0) {
    return 0;
  }
  return static_cast<uint8_t>(engine % kEngineCount);
}

const char* engineLabel(uint8_t engine) {
  // Compact three-letter labels fit the OLED.  Handy when teaching folks how to
  // read the status line without squinting.
  switch (engine) {
    case 0: return "SMP";
    case 1: return "GRA";
    case 2: return "PING";
    default: return "UNK";
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

#ifdef SEEDBOX_HW
  static seedbox::io::StoreEeprom hwStore;
  return &hwStore;
#else
  static seedbox::io::StoreEeprom simStore(4096);
  return &simStore;
#endif
}
}

void AppState::audioCallbackTrampoline(const hal::audio::StereoBufferView& buffer, void* ctx) {
  if (auto* self = static_cast<AppState*>(ctx)) {
    self->handleAudio(buffer);
  }
}

void AppState::digitalCallbackTrampoline(hal::io::PinNumber pin, bool level, uint32_t timestamp,
                                         void* ctx) {
  if (auto* self = static_cast<AppState*>(ctx)) {
    self->handleDigitalEdge(static_cast<uint8_t>(pin), level, timestamp);
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
  store_ = ensureStore(store_);
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
  hal::io::init(kFrontPanelPins.data(), kFrontPanelPins.size());
  hal::io::setDigitalCallback(&AppState::digitalCallbackTrampoline, this);
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
  store_ = ensureStore(store_);
  hal::audio::init(&AppState::audioCallbackTrampoline, this);
  hal::audio::stop();
  hal::io::init(kFrontPanelPins.data(), kFrontPanelPins.size());
  hal::io::setDigitalCallback(&AppState::digitalCallbackTrampoline, this);
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
  reseed(masterSeed_);
  scheduler_.setSampleClockFn(hardwareMode ? &hal::audio::sampleClock : nullptr);
  captureDisplaySnapshot(displayCache_);
  displayDirty_ = true;
  audioCallbackCount_ = 0;
  clearPresetCrossfade();
  activePresetSlot_.clear();
  currentPage_ = Page::kSeeds;
  storageButtonHeld_ = false;
  storageLongPress_ = false;
  storageButtonPressFrame_ = frame_;
}

void AppState::handleAudio(const hal::audio::StereoBufferView& buffer) {
  ++audioCallbackCount_;
  if (!buffer.left || !buffer.right) {
    return;
  }
  std::fill(buffer.left, buffer.left + buffer.frames, 0.0f);
  std::fill(buffer.right, buffer.right + buffer.frames, 0.0f);
}

void AppState::handleDigitalEdge(uint8_t pin, bool level, uint32_t) {
  if (pin != kReseedButtonPin) {
    return;
  }

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
}

// tick is the heartbeat. Every call either primes seeds (first-run), lets the
// internal scheduler drive things when we own the transport, or just counts
// frames so the OLED can display a ticking counter.
void AppState::tick() {
  hal::io::poll();
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
    scheduler_.onTick();
  }
  stepPresetCrossfade();
  ++frame_;
  captureDisplaySnapshot(displayCache_);
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
    scheduler_ = PatternScheduler{};
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
    updateClockDominance();
    hal::io::writeDigital(kStatusLedPin, false);
    displayDirty_ = true;
    return;
  }

  const std::vector<uint8_t> previousSelections = seedEngineSelections_;
  seeds_.clear();
  scheduler_ = PatternScheduler{};
  scheduler_.setBpm(120.f);
  scheduler_.setTriggerCallback(&engines_, &EngineRouter::dispatchThunk);

  uint32_t state = masterSeed_;
  constexpr size_t kSeedCount = 4;
  seedEngineSelections_.assign(kSeedCount, 0);
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
  updateClockDominance();
#if defined(SEEDBOX_HW) && defined(SEEDBOX_DEBUG_CLOCK_SOURCE)
  if (!wasDominant && externalClockDominant_) {
    Serial.println(F("external clock: TRS/USB seized transport"));
  }
#endif
  scheduler_.onTick();
}

void AppState::onExternalTransportStart() {
#if defined(SEEDBOX_HW) && defined(SEEDBOX_DEBUG_CLOCK_SOURCE)
  const bool wasDominant = externalClockDominant_;
#endif
  externalTransportRunning_ = true;
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
    if (kEngineCount == 0) {
      return;
    }
    if (seeds_.empty()) {
      return;
    }
    const size_t count = seeds_.size();
    const uint8_t focus = static_cast<uint8_t>(std::min<size_t>(focusSeed_, count - 1));
    const uint8_t current = seeds_[focus].engine;
    // Our rule-of-thumb: CC values >= 64 spin the encoder clockwise (advance to
    // the next engine) while lower values back up one slot. The math is tiny
    // but worth spelling out for clarity.
    const uint8_t target = (val >= 64)
                               ? static_cast<uint8_t>((current + 1) % kEngineCount)
                               : static_cast<uint8_t>((current + kEngineCount - 1) % kEngineCount);
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
  clearPresetCrossfade();
  activePresetSlot_.clear();
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
  const uint8_t sanitized = sanitizeEngine(engineId);

  if (seedEngineSelections_.size() < count) {
    seedEngineSelections_.resize(count, 0);
  }

  Seed& seed = seeds_[idx];
  seed.engine = sanitized;
  seedEngineSelections_[idx] = sanitized;
  // Re-register the seed with the scheduler so downstream engines fire the
  // right trigger callback. This keeps PatternScheduler as the single source of
  // truth for playback order.
  scheduler_.updateSeed(idx, seed);
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
  // OLED real estate is tiny, so we jam the master seed into the title and then
  // use status/metrics/nuance rows to narrate what's happening with the focus
  // seed. Think of it as a glorified logcat for the front panel.
  std::array<char, 64> scratch{};
  writeDisplayField(out.title, formatScratch(scratch, "SeedBox %06X", masterSeed_ & 0xFFFFFFu));

  const float sampleRate = hal::audio::sampleRate();
  const std::size_t block = hal::audio::framesPerBlock();
  const bool ledOn = hal::io::readDigital(kStatusLedPin);
  const uint32_t nowSamples = scheduler_.nowSamples();

  if (seeds_.empty()) {
    if constexpr (SeedBoxConfig::kQuietMode) {
      writeDisplayField(out.status, "quiet mode zzz");
    } else {
      writeDisplayField(out.status, "no seeds loaded");
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
  writeDisplayField(out.status, formatScratch(scratch, "#%02u%s%+0.1fst%c", s.id, engineLabel(s.engine), s.pitch,
                                              ledOn ? '*' : '-'));
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
