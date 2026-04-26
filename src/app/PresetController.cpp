#include "app/PresetController.h"

#include <algorithm>
#include <utility>

namespace {
constexpr std::uint64_t kPresetBoundaryTicksPerBar = 24u * 4u;
}

void PresetController::setActivePresetSlot(std::string slot) {
  activePresetSlot_ = std::move(slot);
}

seedbox::Preset PresetController::snapshotPreset(const SnapshotInput& input) const {
  seedbox::Preset preset;
  preset.slot = input.slot.empty() ? std::string("default") : std::string(input.slot);
  preset.masterSeed = input.masterSeed;
  preset.focusSeed = input.focusSeed;
  preset.clock.bpm = input.bpm;
  preset.clock.followExternal = input.followExternal;
  preset.clock.debugMeters = input.debugMeters;
  preset.clock.transportLatch = input.transportLatch;
  preset.page = input.page;
  if (input.seeds) {
    preset.seeds = *input.seeds;
  }
  if (input.engineSelections) {
    preset.engineSelections = *input.engineSelections;
  }
  if (preset.engineSelections.size() < preset.seeds.size()) {
    preset.engineSelections.resize(preset.seeds.size(), 0);
    for (std::size_t i = 0; i < preset.seeds.size(); ++i) {
      preset.engineSelections[i] = preset.seeds[i].engine;
    }
  }
  return preset;
}

void PresetController::requestPresetChange(const seedbox::Preset& preset, bool crossfade, Boundary boundary,
                                           std::uint64_t currentTick) {
  PendingPresetRequest request;
  request.preset = preset;
  request.crossfade = crossfade;
  request.boundary = boundary;
  request.targetTick = computeNextPresetTickForBoundary(boundary, currentTick);
  pendingPresetRequest_ = std::move(request);
}

std::optional<PresetController::PendingPresetRequest> PresetController::takePendingPreset(std::uint64_t currentTick) {
  if (!pendingPresetRequest_) {
    return std::nullopt;
  }
  if (currentTick < pendingPresetRequest_->targetTick) {
    return std::nullopt;
  }
  PendingPresetRequest request = *pendingPresetRequest_;
  pendingPresetRequest_.reset();
  return request;
}

std::uint64_t PresetController::computeNextPresetTickForBoundary(Boundary boundary, std::uint64_t currentTick) const {
  if (boundary == Boundary::kBar) {
    const std::uint64_t base = (currentTick / kPresetBoundaryTicksPerBar) * kPresetBoundaryTicksPerBar;
    std::uint64_t target = base + kPresetBoundaryTicksPerBar;
    if (target <= currentTick) {
      target += kPresetBoundaryTicksPerBar;
    }
    return target;
  }
  return currentTick;
}

void PresetController::clearPendingPresetRequest() { pendingPresetRequest_.reset(); }

void PresetController::beginCrossfade(std::vector<Seed> from, std::vector<Seed> to, std::uint32_t totalTicks) {
  crossfade_.from = std::move(from);
  crossfade_.to = std::move(to);
  crossfade_.total = totalTicks;
  crossfade_.remaining = totalTicks;
}

void PresetController::clearCrossfade() { crossfade_ = {}; }

bool PresetController::crossfadeActive() const {
  return crossfade_.remaining != 0u && crossfade_.total != 0u;
}

void PresetController::decrementCrossfade() {
  if (crossfade_.remaining > 0u) {
    --crossfade_.remaining;
  }
}
