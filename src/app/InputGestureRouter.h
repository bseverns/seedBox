#pragma once

#include "app/InputEvents.h"

class AppState;

class InputGestureRouter {
public:
  // Drain the debounced input queue and hand each gesture to AppState's mode
  // and page routers. This keeps polling separate from musical intent.
  void process(AppState& app) const;
};
