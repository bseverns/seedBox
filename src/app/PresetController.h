#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Seed.h"
#include "app/Preset.h"

class AppState;

// PresetController owns the small state machine around preset snapshots,
// queued recalls, and crossfade bookkeeping. AppState still applies the preset
// to the live engines, but the preset lifecycle itself now has a home.
class PresetController {
public:
  enum class Boundary : std::uint8_t {
    kStep = 0,
    kBar,
  };

  struct SnapshotInput {
    std::string_view slot{};
    std::uint32_t masterSeed{0};
    std::uint8_t focusSeed{0};
    float bpm{0.0f};
    bool followExternal{false};
    bool debugMeters{false};
    bool transportLatch{false};
    seedbox::PageId page{seedbox::PageId::kSeeds};
    const std::vector<Seed>* seeds{nullptr};
    const std::vector<std::uint8_t>* engineSelections{nullptr};
  };

  struct PendingPresetRequest {
    seedbox::Preset preset{};
    bool crossfade{false};
    Boundary boundary{Boundary::kStep};
    std::uint64_t targetTick{0};
  };

  struct CrossfadeState {
    std::vector<Seed> from{};
    std::vector<Seed> to{};
    std::uint32_t remaining{0};
    std::uint32_t total{0};
  };

  const std::string& activePresetSlot() const { return activePresetSlot_; }
  void setActivePresetSlot(std::string slot);

  seedbox::Preset snapshotPreset(const SnapshotInput& input) const;

  void requestPresetChange(const seedbox::Preset& preset, bool crossfade, Boundary boundary, std::uint64_t currentTick);
  std::optional<PendingPresetRequest> takePendingPreset(std::uint64_t currentTick);
  std::uint64_t computeNextPresetTickForBoundary(Boundary boundary, std::uint64_t currentTick) const;
  void clearPendingPresetRequest();

  void beginCrossfade(std::vector<Seed> from, std::vector<Seed> to, std::uint32_t totalTicks);
  void clearCrossfade();
  bool crossfadeActive() const;
  const CrossfadeState& crossfade() const { return crossfade_; }
  CrossfadeState& crossfade() { return crossfade_; }
  void decrementCrossfade();

private:
  std::string activePresetSlot_{};
  std::optional<PendingPresetRequest> pendingPresetRequest_{};
  CrossfadeState crossfade_{};
};
