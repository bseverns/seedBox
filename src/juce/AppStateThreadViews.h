#pragma once

#if SEEDBOX_JUCE

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

#include "app/AppState.h"
#include "app/Preset.h"

namespace seedbox::juce_bridge {

// Narrow JUCE-side views over AppState so audio-thread and host-thread code do
// not both reach for the same broad public surface by habit.
class HostAudioThreadAccess {
 public:
  explicit HostAudioThreadAccess(AppState& app) : app_(app) {}

  void setDryInput(const float* left, const float* right, std::size_t frames) {
    app_.setDryInputFromHost(left, right, frames);
  }

  void setTestToneEnabled(bool enabled) { app_.setTestToneEnabledFromHost(enabled); }
  void syncTransportBpm(float bpm) { app_.syncInternalBpmFromHostTransport(bpm); }
  void transportStart() { app_.onExternalTransportStart(); }
  void transportStop() { app_.onExternalTransportStop(); }
  void tick() { app_.tickHostAudio(); }

 private:
  AppState& app_;
};

class HostReadThreadAccess {
 public:
  explicit HostReadThreadAccess(const AppState& app) : app_(app) {}

  const AppState::UiState& uiStateCache() const { return app_.uiStateCache(); }
  const AppState::DisplaySnapshot& displayCache() const { return app_.displayCache(); }
  const std::vector<Seed>& seeds() const { return app_.seeds(); }
  std::uint8_t focusSeed() const { return app_.focusSeed(); }
  std::string_view engineName(std::uint8_t engineId) const { return app_.engineRouterForDebug().engineName(engineId); }
  std::string_view engineShortName(std::uint8_t engineId) const {
    return app_.engineRouterForDebug().engineShortName(engineId);
  }
  AppState::Mode mode() const { return app_.mode(); }
  std::uint64_t schedulerTicks() const { return app_.schedulerTicks(); }
  std::uint8_t quantizeScaleIndex() const { return app_.quantizeScaleIndex(); }
  std::uint8_t quantizeRoot() const { return app_.quantizeRoot(); }
  bool waitingForExternalClock() const { return app_.waitingForExternalClock(); }
  bool followExternalClockEnabled() const { return app_.followExternalClockEnabled(); }
  bool transportLatchEnabled() const { return app_.transportLatchEnabled(); }
  bool displayDirty() const { return app_.displayDirty(); }
  bool isSeedLocked(std::uint8_t index) const { return app_.isSeedLocked(index); }
  void captureDisplaySnapshot(AppState::DisplaySnapshot& out) const { app_.captureDisplaySnapshot(out); }
  void captureLearnFrame(AppState::LearnFrame& out) const { app_.captureLearnFrame(out); }

 private:
  const AppState& app_;
};

class HostControlThreadAccess {
 public:
  explicit HostControlThreadAccess(AppState& app) : app_(app) {}

  void serviceMaintenance() { app_.serviceHostMaintenance(); }
  std::uint32_t masterSeed() const { return app_.masterSeed(); }
  void reseed(std::uint32_t masterSeed) { app_.reseed(masterSeed); }
  void setMode(AppState::Mode mode) { app_.setModeFromHost(mode); }
  void setFocusSeed(std::uint8_t index) { app_.setFocusSeed(index); }
  std::uint8_t focusSeed() const { return app_.focusSeed(); }
  void setSeedEngine(std::uint8_t seedIndex, std::uint8_t engineId) { app_.setSeedEngine(seedIndex, engineId); }
  bool applySeedEdit(std::uint8_t seedIndex, const std::function<void(Seed&)>& edit) {
    return app_.applySeedEditFromHost(seedIndex, edit);
  }
  void seedPageCycleGranularSource(std::uint8_t seedIndex, int steps) {
    app_.seedPageCycleGranularSource(seedIndex, steps);
  }
  void setInternalBpm(float bpm) { app_.setInternalBpmFromHost(bpm); }
  void setSwingPercent(float value) { app_.setSwingPercentFromHost(value); }
  void applyQuantizeControl(std::uint8_t value) { app_.applyQuantizeControlFromHost(value); }
  void setDebugMetersEnabled(bool enabled) { app_.setDebugMetersEnabledFromHost(enabled); }
  void setTransportLatch(bool enabled) { app_.setTransportLatchFromHost(enabled); }
  void setFollowExternalClock(bool enabled) { app_.setFollowExternalClockFromHost(enabled); }
  void setClockSourceExternal(bool external) { app_.setClockSourceExternalFromHost(external); }
  void setInputGateDivision(AppState::GateDivision division) { app_.setInputGateDivisionFromHost(division); }
  void setInputGateFloor(float floor) { app_.setInputGateFloorFromHost(floor); }
  void setTestToneEnabled(bool enabled) { app_.setTestToneEnabledFromHost(enabled); }
  void panicMidi() { app_.midi.panic(); }
  void clearDisplayDirtyFlag() { app_.clearDisplayDirtyFlag(); }
  void seedPageToggleLock(std::uint8_t index) { app_.seedPageToggleLock(index); }
  void seedPageReseed(std::uint32_t masterSeed, AppState::SeedPrimeMode mode) {
    app_.seedPageReseed(masterSeed, mode);
  }
  void recordTapTempoInterval(std::uint32_t intervalMs) { app_.recordTapTempoInterval(intervalMs); }
  bool isSeedLocked(std::uint8_t index) const { return app_.isSeedLocked(index); }
  seedbox::Preset snapshotPreset(std::string_view slot) const { return app_.snapshotPresetForHost(slot); }
  void applyPreset(const seedbox::Preset& preset, bool crossfade) { app_.applyPresetFromHost(preset, crossfade); }

 private:
  AppState& app_;
};

}  // namespace seedbox::juce_bridge

#endif  // SEEDBOX_JUCE
