#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "app/AppState.h"

class PresetStorageService {
public:
  void setPage(AppState& app, AppState::Page page) const;
  std::vector<std::string> storedPresets(const AppState& app) const;
  bool savePreset(AppState& app, std::string_view slot) const;
  bool recallPreset(AppState& app, std::string_view slot, bool crossfade) const;
};

