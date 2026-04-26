#include "app/InputGestureRouter.h"

#include "app/AppState.h"

void InputGestureRouter::process(AppState& app) const {
  const auto& evts = app.input_.events();
  for (const auto& evt : evts) {
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
