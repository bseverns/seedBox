#pragma once

//
// AppState.h
// -----------
// Everything publicly visible about the firmware's state machine lives here.
// Treat it like a field guide for the rest of the codebase: the UI, tests, and
// documentation all poke through this interface.  That means generous comments
// are fair game — we're not hiding cleverness, we're teaching it.
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "SeedLock.h"
#include "Seed.h"
#include "app/Preset.h"
#include "engine/Patterns.h"
#include "engine/EngineRouter.h"
#include "util/Annotations.h"
#include "io/Store.h"
#include "app/UiState.h"
#include "hal/Board.h"
#include "hal/hal_io.h"
#include "app/InputEvents.h"
#include "app/Clock.h"
#include "io/MidiRouter.h"

namespace hal {
namespace audio {
struct StereoBufferView;
}  // namespace audio
}  // namespace hal

namespace Storage {
bool saveScene(const char* path);
}

// AppState is the mothership for everything the performer can poke at run time.
// It owns the seed table, orchestrates scheduling, and provides a place for the
// UI layer (physical or simulated) to read back human-friendly snapshots. The
// teaching intent: students should be able to glance at this header and see
// what control hooks exist without spelunking the implementation.
class AppState {
public:
  enum class Page : std::uint8_t {
    kSeeds = static_cast<std::uint8_t>(seedbox::PageId::kSeeds),
    kStorage = static_cast<std::uint8_t>(seedbox::PageId::kStorage),
    kClock = static_cast<std::uint8_t>(seedbox::PageId::kClock),
  };

  static constexpr std::uint32_t kPresetCrossfadeTicks = 48;

  enum class SeedPrimeMode : uint8_t { kLfsr = 0, kTapTempo, kPreset, kLiveInput };
  struct SeedNudge {
    float pitchSemitones{0.f};
    float densityDelta{0.f};
    float probabilityDelta{0.f};
    float jitterDeltaMs{0.f};
    float toneDelta{0.f};
    float spreadDelta{0.f};
  };

  enum class Mode : std::uint8_t {
    HOME,
    SEEDS,
    ENGINE,
    PERF,
    SETTINGS,
    UTIL,
    SWING,
  };

  explicit AppState(hal::Board& board = hal::board());
  ~AppState();
  // Boot the full hardware stack. We spin up MIDI routing, initialise the
  // engine router in hardware mode, and prime the deterministic seed table so
  // the Teensy build behaves exactly like the simulator.
  void initHardware();

  // Boot the simulator variant. Same story as initHardware, just without the
  // hardware drivers so unit tests (and classroom demos) run fast and
  // repeatably on a laptop.
  void initSim();

#if !SEEDBOX_HW
  // Desktop/host bootstrap for JUCE builds. We lean on the native audio/MIDI
  // driver for buffers + clocking but keep the core engine wiring identical to
  // the simulator path.
  void initJuceHost(float sampleRate, std::size_t framesPerBlock);
#endif

  // Pump one control-tick. In sim builds tests call this manually; on hardware
  // the main loop does the honors. Either way, the scheduler decides whether a
  // seed should trigger on this frame.
  void tick();

  struct DisplaySnapshot {
    char title[17];
    char status[17];
    char metrics[17];
    char nuance[17];
  };

  // Populate the snapshot struct with text destined for the OLED / debug
  // display. Think of this as the "mixing console" view for teaching labs.
  void captureDisplaySnapshot(DisplaySnapshot& out) const;
  void captureDisplaySnapshot(DisplaySnapshot& out, UiState& ui) const {
    captureDisplaySnapshot(out, &ui);
  }

  const UiState& uiStateCache() const { return uiStateCache_; }

  const DisplaySnapshot& displayCache() const { return displayCache_; }
  bool displayDirty() const { return displayDirty_; }
  void clearDisplayDirtyFlag() { displayDirty_ = false; }

  // Host/editor helper: jump straight to a mode without playing the button
  // chord guessing game.  Useful for DAW-hosted inspectors.
  void setModeFromHost(Mode mode);

  // Re-roll the procedural seeds using a supplied master seed. Students can use
  // this hook to explore how the scheduler reacts to different pseudo-random
  // genomes without rebooting the whole device.
  void reseed(uint32_t masterSeed);
  void seedPageReseed(uint32_t masterSeed, SeedPrimeMode mode);
  void setSeedPrimeMode(SeedPrimeMode mode);
  SeedPrimeMode seedPrimeMode() const { return seedPrimeMode_; }
  bool seedPrimeBypassEnabled() const { return seedPrimeBypassEnabled_; }
  void armGranularLiveInput(bool enabled);
  GranularEngine::GrainVoice debugGranularVoice(uint8_t index) const;
#if !SEEDBOX_HW
  GranularEngine::SimHardwareVoice debugGranularSimVoice(uint8_t index) const;
#endif
  const GranularEngine::Stats& granularStats() const { return granularStats_; }

  void seedPageToggleLock(uint8_t index);
  void seedPageToggleGlobalLock();
  bool isSeedLocked(uint8_t index) const;
  bool isGlobalSeedLocked() const { return seedLock_.globalLocked(); }
  void seedPageNudge(uint8_t index, const SeedNudge& nudge);
  void seedPageCycleGranularSource(uint8_t index, int32_t steps);
  void recordTapTempoInterval(uint32_t intervalMs);
  void setSeedPreset(uint32_t presetId, const std::vector<Seed>& seeds);
  uint32_t activePresetId() const { return presetBuffer_.id; }
  EngineRouter& engineRouterForDebug() { return engines_; }
  const EngineRouter& engineRouterForDebug() const { return engines_; }

  // Set which seed the UI is focused on. The focus influences which engine gets
  // cycled when the performer mashes the CC encoder.
  void setFocusSeed(uint8_t index);

  // Core teaching hook: explicitly assign a playback engine (sampler, granular,
  // resonator) to a seed. We expose it so integration tests — and students — can
  // reason about deterministic engine swaps.
  void setSeedEngine(uint8_t seedIndex, uint8_t engineId);
  const std::vector<Seed>& seeds() const { return seeds_; }
  uint32_t masterSeed() const { return masterSeed_; }
  uint8_t focusSeed() const { return focusSeed_; }
  uint64_t schedulerTicks() const { return scheduler_.ticks(); }

  const Seed* debugScheduledSeed(uint8_t index) const;

  void attachStore(seedbox::io::Store* store) { store_ = store; }
  seedbox::io::Store* store() const { return store_; }

  void setPage(Page page);
  Page page() const { return currentPage_; }

  bool savePreset(std::string_view slot);
  bool recallPreset(std::string_view slot, bool crossfade = true);
  std::vector<std::string> storedPresets() const;
  const std::string& activePresetSlot() const { return activePresetSlot_; }

  // JUCE host helpers. Intentionally thin wrappers around the private preset
  // plumbing so the plugin can persist/restore state without opening up the
  // whole API surface.
  seedbox::Preset snapshotPresetForHost(std::string_view slot) const { return snapshotPreset(slot); }
  void applyPresetFromHost(const seedbox::Preset& preset, bool crossfade) { applyPreset(preset, crossfade); }

  // MIDI ingress points. Each handler maps 1:1 with incoming transport/clock
  // events so lessons about external sync can point here directly.
  void onExternalClockTick();
  void onExternalTransportStart();
  void onExternalTransportStop();
  void onExternalControlChange(uint8_t ch, uint8_t cc, uint8_t val);

  bool externalClockDominant() const { return externalClockDominant_; }
  bool followExternalClockEnabled() const { return followExternalClockEnabled_; }
  bool debugMetersEnabled() const { return debugMetersEnabled_; }
  bool transportLatchEnabled() const { return transportLatchEnabled_; }
  bool transportLatchedRunning() const { return transportLatchedRunning_; }
  bool externalTransportRunning() const { return externalTransportRunning_; }
  bool mn42HelloSeen() const { return mn42HelloSeen_; }
  Mode mode() const { return mode_; }
  bool swingPageRequested() const { return swingPageRequested_; }
  float swingPercent() const { return swingPercent_; }
  uint8_t quantizeScaleIndex() const { return quantizeScaleIndex_; }
  uint8_t quantizeRoot() const { return quantizeRoot_; }

  // Host/automation hooks. These mirror the hardware gestures so DAWs can hit
  // the same code paths the front panel pokes.
  void setSwingPercentFromHost(float value);
  void applyQuantizeControlFromHost(uint8_t value);
  void setDebugMetersEnabledFromHost(bool enabled);
  void setTransportLatchFromHost(bool enabled);
  void setFollowExternalClockFromHost(bool enabled);
  void setClockSourceExternalFromHost(bool external);
  void setInternalBpmFromHost(float bpm);
  void setSeedPrimeBypassFromHost(bool enabled);
  void setLiveCaptureVariation(uint8_t variationSteps);
  void setDryInputFromHost(const float* left, const float* right, std::size_t frames);
  bool applySeedEditFromHost(uint8_t seedIndex, const std::function<void(Seed&)>& edit);

  MidiRouter midi;

private:
  static void audioCallbackTrampoline(const hal::audio::StereoBufferView& buffer, void* ctx);

  // Helper that hydrates the seeds_ array deterministically from masterSeed_.
  // The implementation leans heavily on comments so students can watch the RNG
  // state machine do its thing.

  void primeSeeds(uint32_t masterSeed);
  void updateClockDominance();
  void applyMn42ModeBits(uint8_t value);
  bool applyMn42ParamControl(uint8_t controller, uint8_t value);
  void handleTransportGate(uint8_t value);
  void handleDigitalEdge(uint8_t pin, bool level, uint32_t timestamp);
  void handleAudio(const hal::audio::StereoBufferView& buffer);
  void configureMidiRouting();
  void bootRuntime(EngineRouter::Mode mode, bool hardwareMode);
  void stepPresetCrossfade();
  void clearPresetCrossfade();
  seedbox::Preset snapshotPreset(std::string_view slot) const;
  void applyPreset(const seedbox::Preset& preset, bool crossfade);
  Seed blendSeeds(const Seed& from, const Seed& to, float t) const;
  std::vector<Seed> buildLfsrSeeds(uint32_t masterSeed, std::size_t count);
  std::vector<Seed> buildTapTempoSeeds(uint32_t masterSeed, std::size_t count, float bpm);
  std::vector<Seed> buildPresetSeeds(std::size_t count);
  std::vector<Seed> buildLiveInputSeeds(uint32_t masterSeed, std::size_t count);
  float currentTapTempoBpm() const;
  void applyQuantizeControl(uint8_t value);
  void captureDisplaySnapshot(DisplaySnapshot& out, UiState* ui) const;
  void processInputEvents();
  bool handleClockButtonEvent(const InputEvents::Event& evt);
  void applyModeTransition(const InputEvents::Event& evt);
  bool handleSeedPrimeGesture(const InputEvents::Event& evt);
  void dispatchToPage(const InputEvents::Event& evt);
  void handleHomeEvent(const InputEvents::Event& evt);
  void handleSeedsEvent(const InputEvents::Event& evt);
  void handleEngineEvent(const InputEvents::Event& evt);
  void handlePerfEvent(const InputEvents::Event& evt);
  void handleSettingsEvent(const InputEvents::Event& evt);
  void handleUtilEvent(const InputEvents::Event& evt);
  void handleSwingEvent(const InputEvents::Event& evt);
  void handleReseedRequest();
  void triggerLiveCaptureReseed();
  static const char* modeLabel(Mode mode);
  void selectClockProvider(ClockProvider* provider);
  void toggleClockProvider();
  void enterSwingMode();
  void exitSwingMode(Mode targetMode);
  void adjustSwing(float delta);
  void applySwingPercent(float value);
  static void digitalCallbackThunk(hal::io::PinNumber pin, bool level, std::uint32_t timestamp,
                                   void* ctx);

  // Runtime guts.  Nothing fancy here, just all the levers AppState pulls while
  // the performance is running.
  friend bool Storage::saveScene(const char* path);
  hal::Board& board_;
  InputEvents input_;
  Mode mode_{Mode::HOME};
  Mode previousModeBeforeSwing_{Mode::HOME};
  uint32_t frame_{0};
  std::vector<Seed> seeds_{};
  InternalClock internalClock_{};
  MidiClockIn midiClockIn_{};
  MidiClockOut midiClockOut_{};
  PatternScheduler scheduler_{};
  ClockProvider* clock_{nullptr};
  EngineRouter engines_{};
  bool enginesReady_{false};
  std::vector<uint8_t> seedEngineSelections_{};
  SeedLock seedLock_{};
  SeedPrimeMode seedPrimeMode_{SeedPrimeMode::kLfsr};
  std::vector<uint32_t> tapTempoHistory_{};
  std::uint64_t lastTapTempoTapUs_{0};
  uint32_t liveCaptureCounter_{0};
  uint8_t liveCaptureSlot_{0};
  uint8_t liveCaptureVariation_{0};
  uint32_t liveCaptureLineage_{0};
  struct PresetBuffer {
    uint32_t id{0};
    std::vector<Seed> seeds{};
  } presetBuffer_{};
  uint32_t masterSeed_{0x5EEDB0B1u};
  uint8_t focusSeed_{0};
  bool seedsPrimed_{false};
  bool seedPrimeBypassEnabled_{SeedBoxConfig::kSeedPrimeBypass};
  bool externalClockDominant_{false};
  bool followExternalClockEnabled_{false};
  bool debugMetersEnabled_{false};
  bool transportLatchEnabled_{false};
  bool transportLatchedRunning_{false};
  bool externalTransportRunning_{false};
  bool transportGateHeld_{false};
  bool mn42HelloSeen_{false};
  bool swingPageRequested_{false};
  bool swingEditing_{false};
  DisplaySnapshot displayCache_{};
  GranularEngine::Stats granularStats_{};
  UiState uiStateCache_{};
  bool displayDirty_{false};
  uint64_t audioCallbackCount_{0};
  bool reseedRequested_{false};
  seedbox::io::Store* store_{nullptr};
  std::string activePresetSlot_{};
  Page currentPage_{Page::kSeeds};
  uint8_t quantizeScaleIndex_{0};
  uint8_t quantizeRoot_{0};
  struct PresetCrossfade {
    std::vector<Seed> from;
    std::vector<Seed> to;
    std::uint32_t remaining{0};
    std::uint32_t total{0};
  } presetCrossfade_{};
  bool storageButtonHeld_{false};
  bool storageLongPress_{false};
  uint64_t storageButtonPressFrame_{0};
  bool lockButtonHeld_{false};
  uint32_t lockButtonPressTimestamp_{0};
  float swingPercent_{0.0f};
  std::vector<float> dryInputLeft_{};
  std::vector<float> dryInputRight_{};
};

