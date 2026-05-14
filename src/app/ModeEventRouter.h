#pragma once

#include "app/InputEvents.h"

class AppState;

class ModeEventRouter {
public:
  // Update the high-level page/mode state machine before page-local handlers
  // interpret a gesture.
  void applyModeTransition(AppState& app, const InputEvents::Event& evt) const;
  // Dispatch a gesture into the currently selected page grammar.
  void dispatchToPage(AppState& app, const InputEvents::Event& evt) const;

private:
  void handleHomeEvent(AppState& app, const InputEvents::Event& evt) const;
  void handleSeedsEvent(AppState& app, const InputEvents::Event& evt) const;
  void handleEngineEvent(AppState& app, const InputEvents::Event& evt) const;
  void handlePerfEvent(AppState& app, const InputEvents::Event& evt) const;
  void handleSettingsEvent(AppState& app, const InputEvents::Event& evt) const;
  void handleUtilEvent(AppState& app, const InputEvents::Event& evt) const;
  void handleSwingEvent(AppState& app, const InputEvents::Event& evt) const;
};
