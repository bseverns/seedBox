#include "app/InputGestureRouter.h"

#include "app/AppState.h"

void InputGestureRouter::process(AppState& app) const {
  const auto& evts = app.input_.events();
  for (const auto& evt : evts) {
    // Storage owns the Seed switch while open. Decide on release so a hold
    // saves without first recalling or reseeding on the initial press.
    if (app.page() == AppState::Page::kStorage &&
        evt.primaryButton == hal::Board::ButtonID::EncoderSeedBank &&
        (evt.type == InputEvents::Type::ButtonPress ||
         evt.type == InputEvents::Type::ButtonLongPress ||
         evt.type == InputEvents::Type::ButtonDoublePress ||
         evt.type == InputEvents::Type::ButtonRelease)) {
      if (evt.type == InputEvents::Type::ButtonRelease) {
        const auto slot = app.activePresetSlot().empty() ? std::string("default") : app.activePresetSlot();
        if (evt.heldUs >= 450000) {
          app.savePreset(slot);
        } else {
          app.recallPreset(slot, true);
        }
      }
      continue;
    }
    if (evt.type == InputEvents::Type::ButtonRelease) {
      continue;
    }

    if (evt.type == InputEvents::Type::ButtonLongPress &&
        evt.primaryButton == hal::Board::ButtonID::LiveCapture) {
      app.triggerPanic();
      continue;
    }

    if (evt.type == InputEvents::Type::ButtonPress && evt.primaryButton == hal::Board::ButtonID::LiveCapture) {
      app.triggerLiveCaptureReseed();
      continue;
    }

    if (evt.type == InputEvents::Type::ButtonLongPress &&
        evt.primaryButton == hal::Board::ButtonID::EncoderSeedBank) {
      app.handleReseedRequest();
    }
    if (app.handleSeedPrimeGesture(evt)) {
      continue;
    }
    if (app.handleClockButtonEvent(evt)) {
      continue;
    }
    app.applyModeTransition(evt);
    app.dispatchToPage(evt);
  }
}
