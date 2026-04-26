#pragma once

#include "app/InputEvents.h"

class AppState;

class InputGestureRouter {
public:
  void process(AppState& app) const;
};
