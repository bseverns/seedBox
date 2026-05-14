#include "app/PresetStorageService.h"

#include <string>
#include <vector>

// PresetStorageService keeps the boring but important I/O rules in one place:
// slot naming, serialization, and the difference between "capture what exists
// now" and "request that a stored scene take over later".

namespace {
constexpr std::string_view kDefaultPresetSlot = "default";
}

void PresetStorageService::setPage(AppState& app, AppState::Page page) const {
  if (app.currentPage_ == page) {
    return;
  }
  app.currentPage_ = page;
  app.displayDirty_ = true;
}

std::vector<std::string> PresetStorageService::storedPresets(const AppState& app) const {
  if (!app.store_) {
    return {};
  }
  return app.store_->list();
}

bool PresetStorageService::savePreset(AppState& app, std::string_view slot) const {
  if (!app.store_) {
    return false;
  }
  const std::string slotName = slot.empty() ? std::string(kDefaultPresetSlot) : std::string(slot);
  seedbox::Preset preset = app.snapshotPreset(slotName);
  const auto bytes = preset.serialize();
  if (bytes.empty()) {
    return false;
  }
  if (!app.store_->save(slotName, bytes)) {
    return false;
  }
  // Saving also updates the app's notion of the active slot so the UI reflects
  // the last successful write, not just the last requested name.
  app.presetController_.setActivePresetSlot(slotName);
  app.setSeedPreset(preset.masterSeed, preset.seeds);
  return true;
}

bool PresetStorageService::recallPreset(AppState& app, std::string_view slot, bool crossfade) const {
  if (!app.store_) {
    return false;
  }
  const std::string slotName = slot.empty() ? std::string(kDefaultPresetSlot) : std::string(slot);
  std::vector<std::uint8_t> bytes;
  if (!app.store_->load(slotName, bytes)) {
    return false;
  }
  seedbox::Preset preset{};
  if (!seedbox::Preset::deserialize(bytes, preset)) {
    return false;
  }
  if (preset.slot.empty()) {
    preset.slot = slotName;
  }
  // Recall goes through the same preset-transition path as host-triggered
  // changes so boundary and crossfade behavior stay consistent.
  app.requestPresetChange(preset, crossfade, AppState::PresetBoundary::Step);
  return true;
}
