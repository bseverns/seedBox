#include "app/ModeEventRouter.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>

#include "app/AppState.h"
#include "engine/BurstEngine.h"
#include "engine/EuclidEngine.h"

// ModeEventRouter is the front-panel grammar. Its job is not to make musical
// decisions; it translates button/encoder gestures into the higher-level verbs
// AppState already understands.

namespace {
constexpr std::uint32_t buttonMask(hal::Board::ButtonID id) {
  return 1u << static_cast<std::uint32_t>(id);
}

constexpr std::uint32_t buttonMask(std::initializer_list<hal::Board::ButtonID> ids) {
  std::uint32_t mask = 0;
  for (auto id : ids) {
    mask |= buttonMask(id);
  }
  return mask;
}

struct ModeTransition {
  AppState::Mode from;
  InputEvents::Type trigger;
  std::uint32_t buttons;
  AppState::Mode to;
};

constexpr std::array<ModeTransition, 31> kModeTransitions{{
    // These rows are the "chord chart" for the instrument body: which gesture
    // moves between the named pages, independent of whatever that page later
    // does with encoders or taps.
    {AppState::Mode::HOME, InputEvents::Type::ButtonPress, buttonMask(hal::Board::ButtonID::EncoderSeedBank),
     AppState::Mode::SEEDS},
    {AppState::Mode::HOME, InputEvents::Type::ButtonPress, buttonMask(hal::Board::ButtonID::EncoderDensity),
     AppState::Mode::ENGINE},
    {AppState::Mode::HOME, InputEvents::Type::ButtonPress, buttonMask(hal::Board::ButtonID::EncoderToneTilt),
     AppState::Mode::PERF},
    {AppState::Mode::HOME, InputEvents::Type::ButtonPress, buttonMask(hal::Board::ButtonID::EncoderFxMutate),
     AppState::Mode::UTIL},
    {AppState::Mode::SWING, InputEvents::Type::ButtonPress, buttonMask(hal::Board::ButtonID::EncoderSeedBank),
     AppState::Mode::SEEDS},
    {AppState::Mode::SWING, InputEvents::Type::ButtonPress, buttonMask(hal::Board::ButtonID::EncoderDensity),
     AppState::Mode::ENGINE},
    {AppState::Mode::SWING, InputEvents::Type::ButtonPress, buttonMask(hal::Board::ButtonID::EncoderToneTilt),
     AppState::Mode::PERF},
    {AppState::Mode::SWING, InputEvents::Type::ButtonPress, buttonMask(hal::Board::ButtonID::EncoderFxMutate),
     AppState::Mode::UTIL},
    {AppState::Mode::HOME, InputEvents::Type::ButtonDoublePress, buttonMask(hal::Board::ButtonID::TapTempo),
     AppState::Mode::SETTINGS},
    {AppState::Mode::SEEDS, InputEvents::Type::ButtonDoublePress, buttonMask(hal::Board::ButtonID::TapTempo),
     AppState::Mode::SETTINGS},
    {AppState::Mode::ENGINE, InputEvents::Type::ButtonDoublePress, buttonMask(hal::Board::ButtonID::TapTempo),
     AppState::Mode::SETTINGS},
    {AppState::Mode::PERF, InputEvents::Type::ButtonDoublePress, buttonMask(hal::Board::ButtonID::TapTempo),
     AppState::Mode::SETTINGS},
    {AppState::Mode::UTIL, InputEvents::Type::ButtonDoublePress, buttonMask(hal::Board::ButtonID::TapTempo),
     AppState::Mode::SETTINGS},
    {AppState::Mode::SWING, InputEvents::Type::ButtonDoublePress, buttonMask(hal::Board::ButtonID::TapTempo),
     AppState::Mode::SETTINGS},
    {AppState::Mode::SETTINGS, InputEvents::Type::ButtonDoublePress, buttonMask(hal::Board::ButtonID::TapTempo),
     AppState::Mode::HOME},
    {AppState::Mode::SEEDS, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::Shift),
     AppState::Mode::HOME},
    {AppState::Mode::ENGINE, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::Shift),
     AppState::Mode::HOME},
    {AppState::Mode::PERF, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::Shift),
     AppState::Mode::HOME},
    {AppState::Mode::UTIL, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::Shift),
     AppState::Mode::HOME},
    {AppState::Mode::SETTINGS, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::Shift),
     AppState::Mode::HOME},
    {AppState::Mode::HOME, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::Shift),
     AppState::Mode::HOME},
    {AppState::Mode::SWING, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::Shift),
     AppState::Mode::HOME},
    {AppState::Mode::HOME, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::AltSeed),
     AppState::Mode::HOME},
    {AppState::Mode::SEEDS, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::AltSeed),
     AppState::Mode::HOME},
    {AppState::Mode::ENGINE, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::AltSeed),
     AppState::Mode::HOME},
    {AppState::Mode::PERF, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::AltSeed),
     AppState::Mode::HOME},
    {AppState::Mode::UTIL, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::AltSeed),
     AppState::Mode::HOME},
    {AppState::Mode::SETTINGS, InputEvents::Type::ButtonLongPress, buttonMask(hal::Board::ButtonID::AltSeed),
     AppState::Mode::HOME},
    {AppState::Mode::SETTINGS, InputEvents::Type::ButtonChord,
     buttonMask({hal::Board::ButtonID::Shift, hal::Board::ButtonID::AltSeed}), AppState::Mode::PERF},
    {AppState::Mode::PERF, InputEvents::Type::ButtonChord,
     buttonMask({hal::Board::ButtonID::Shift, hal::Board::ButtonID::AltSeed}), AppState::Mode::SETTINGS},
    {AppState::Mode::SWING, InputEvents::Type::ButtonChord,
     buttonMask({hal::Board::ButtonID::Shift, hal::Board::ButtonID::AltSeed}), AppState::Mode::PERF},
}};

bool eventHasButton(const InputEvents::Event& evt, hal::Board::ButtonID id) {
  return std::find(evt.buttons.begin(), evt.buttons.end(), id) != evt.buttons.end();
}

}  // namespace

void ModeEventRouter::applyModeTransition(AppState& app, const InputEvents::Event& evt) const {
  std::uint32_t mask = 0;
  if (!evt.buttons.empty()) {
    for (auto id : evt.buttons) {
      mask |= buttonMask(id);
    }
  } else {
    mask = buttonMask(evt.primaryButton);
  }

  for (const auto& transition : kModeTransitions) {
    if (transition.from == app.mode_ && transition.trigger == evt.type && transition.buttons == mask) {
      const AppState::Mode fromMode = app.mode_;
      if (transition.trigger == InputEvents::Type::ButtonLongPress &&
          transition.buttons == buttonMask(hal::Board::ButtonID::AltSeed)) {
        // Alt long-press is the one routing gesture that also changes page
        // state, because Storage lives "under" the current mode system.
        app.setPage(AppState::Page::kStorage);
        app.storageButtonHeld_ = false;
        app.storageLongPress_ = false;
      }
      if (app.mode_ != transition.to) {
        if (fromMode == AppState::Mode::SWING && transition.to != AppState::Mode::SWING) {
          app.swingEditing_ = false;
          app.previousModeBeforeSwing_ = transition.to;
        }
        app.mode_ = transition.to;
        app.displayDirty_ = true;
      }
      return;
    }
  }
}

void ModeEventRouter::dispatchToPage(AppState& app, const InputEvents::Event& evt) const {
  // Once mode has been chosen, dispatch becomes a simple per-page decoder.
  switch (app.mode_) {
    case AppState::Mode::HOME: handleHomeEvent(app, evt); break;
    case AppState::Mode::SEEDS: handleSeedsEvent(app, evt); break;
    case AppState::Mode::ENGINE: handleEngineEvent(app, evt); break;
    case AppState::Mode::PERF: handlePerfEvent(app, evt); break;
    case AppState::Mode::SETTINGS: handleSettingsEvent(app, evt); break;
    case AppState::Mode::UTIL: handleUtilEvent(app, evt); break;
    case AppState::Mode::SWING: handleSwingEvent(app, evt); break;
    default: break;
  }
}

void ModeEventRouter::handleHomeEvent(AppState& app, const InputEvents::Event& evt) const {
  if (evt.type == InputEvents::Type::EncoderHoldTurn &&
      evt.encoder == hal::Board::EncoderID::SeedBank &&
      eventHasButton(evt, hal::Board::ButtonID::Shift)) {
    if (evt.encoderDelta != 0 && !app.seeds_.empty()) {
      const int32_t next = static_cast<int32_t>(app.focusSeed_) + evt.encoderDelta;
      app.setFocusSeed(static_cast<uint8_t>(next));
      app.displayDirty_ = true;
    }
  }
}

void ModeEventRouter::handleSeedsEvent(AppState& app, const InputEvents::Event& evt) const {
  if (evt.type == InputEvents::Type::EncoderTurn &&
      evt.encoder == hal::Board::EncoderID::SeedBank) {
    if (evt.encoderDelta != 0 && !app.seeds_.empty()) {
      const int32_t next = static_cast<int32_t>(app.focusSeed_) + evt.encoderDelta;
      app.setFocusSeed(static_cast<uint8_t>(next));
      app.displayDirty_ = true;
    }
  }

  if (evt.type == InputEvents::Type::EncoderHoldTurn &&
      evt.encoder == hal::Board::EncoderID::Density &&
      eventHasButton(evt, hal::Board::ButtonID::Shift)) {
    if (evt.encoderDelta != 0) {
      // Shift+Density on Seeds edits the input-gate cadence rather than seed
      // content, so rhythmic reseed timing lives on the same page as seed DNA.
      app.stepGateDivision(static_cast<int>(evt.encoderDelta));
    }
    return;
  }

  if (evt.encoderDelta == 0) {
    return;
  }

  if (evt.type == InputEvents::Type::EncoderHoldTurn &&
      evt.encoder == hal::Board::EncoderID::ToneTilt) {
    // Tone/Tilt is the "micro-edit" lane on Seeds: Shift leans pitch/source,
    // Alt leans density, and the button combination decides the semantic layer.
    const bool shiftHeld = eventHasButton(evt, hal::Board::ButtonID::Shift);
    const bool altHeld = eventHasButton(evt, hal::Board::ButtonID::AltSeed);
    if (app.seeds_.empty()) {
      return;
    }
    const std::size_t focusIndex = std::min<std::size_t>(app.focusSeed_, app.seeds_.size() - 1);
    if (shiftHeld && !altHeld) {
      app.seedPageCycleGranularSource(static_cast<uint8_t>(focusIndex), evt.encoderDelta);
      return;
    }
    if (shiftHeld || altHeld) {
      AppState::SeedNudge nudge{};
      if (shiftHeld) {
        nudge.pitchSemitones = static_cast<float>(evt.encoderDelta);
      }
      if (altHeld) {
        constexpr float kDensityStep = 0.1f;
        nudge.densityDelta = static_cast<float>(evt.encoderDelta) * kDensityStep;
      }
      if (nudge.pitchSemitones != 0.f || nudge.densityDelta != 0.f) {
        app.seedPageNudge(static_cast<uint8_t>(focusIndex), nudge);
      }
      return;
    }
  }

  if (evt.type == InputEvents::Type::EncoderHoldTurn &&
      evt.encoder == hal::Board::EncoderID::FxMutate &&
      eventHasButton(evt, hal::Board::ButtonID::AltSeed)) {
    // Quantize scale selection lives here so the physical surface and the MN-42
    // remote surface both speak the same scale/root control language.
    constexpr int kScaleCount = 5;
    int next = static_cast<int>(app.quantizeScaleIndex_) + static_cast<int>(evt.encoderDelta);
    next %= kScaleCount;
    if (next < 0) {
      next += kScaleCount;
    }
    app.quantizeScaleIndex_ = static_cast<uint8_t>(next);
    const uint8_t controlValue = static_cast<uint8_t>((app.quantizeScaleIndex_ * 32u) + (app.quantizeRoot_ % 12u));
    app.applyQuantizeControl(controlValue);
    return;
  }
}

void ModeEventRouter::handleEngineEvent(AppState& app, const InputEvents::Event& evt) const {
  if (evt.type == InputEvents::Type::EncoderTurn &&
      evt.encoder == hal::Board::EncoderID::SeedBank && evt.encoderDelta != 0 && !app.seeds_.empty()) {
    const size_t focus = std::min<size_t>(app.focusSeed_, app.seeds_.size() - 1);
    const int32_t next = static_cast<int32_t>(focus) + evt.encoderDelta;
    app.setFocusSeed(static_cast<uint8_t>(next));
    app.displayDirty_ = true;
    return;
  }

  if (evt.type == InputEvents::Type::EncoderHoldTurn &&
      evt.encoder == hal::Board::EncoderID::Density && eventHasButton(evt, hal::Board::ButtonID::Shift)) {
    if (!app.seeds_.empty() && evt.encoderDelta != 0) {
      const size_t focus = std::min<size_t>(app.focusSeed_, app.seeds_.size() - 1);
      const uint8_t current = app.seeds_[focus].engine;
      const uint8_t next = static_cast<uint8_t>(static_cast<int>(current) + evt.encoderDelta);
      app.setSeedEngine(static_cast<uint8_t>(focus), next);
      app.displayDirty_ = true;
    }
    return;
  }

  if (app.seeds_.empty()) {
    return;
  }

  // Engine-page edits are blocked by both global and per-seed locks because
  // they rewrite the focused seed's engine-local behavior in place.
  const size_t focus = std::min<size_t>(app.focusSeed_, app.seeds_.size() - 1);
  if (app.seedLock_.globalLocked() || app.seedLock_.seedLocked(focus)) {
    return;
  }

  const Seed& seed = app.seeds_[focus];
  const uint8_t engineId = seed.engine;

  if (evt.type == InputEvents::Type::EncoderTurn && evt.encoderDelta != 0 && engineId == EngineRouter::kEuclidId) {
    // Euclid gets a page-local mini control surface: steps, fills, and rotate.
    Engine::ParamChange change{};
    change.seedId = seed.id;
    switch (evt.encoder) {
      case hal::Board::EncoderID::Density: {
        change.id = static_cast<std::uint16_t>(EuclidEngine::Param::kSteps);
        change.value = static_cast<std::int32_t>(app.engines_.euclid().steps()) +
                       (evt.encoderDelta * 1);
        app.engines_.euclid().onParam(change);
        app.displayDirty_ = true;
        return;
      }
      case hal::Board::EncoderID::ToneTilt: {
        change.id = static_cast<std::uint16_t>(EuclidEngine::Param::kFills);
        change.value = static_cast<std::int32_t>(app.engines_.euclid().fills()) +
                       (evt.encoderDelta * 1);
        app.engines_.euclid().onParam(change);
        app.displayDirty_ = true;
        return;
      }
      case hal::Board::EncoderID::FxMutate: {
        change.id = static_cast<std::uint16_t>(EuclidEngine::Param::kRotate);
        change.value = static_cast<std::int32_t>(app.engines_.euclid().rotate()) +
                       (evt.encoderDelta * 1);
        app.engines_.euclid().onParam(change);
        app.displayDirty_ = true;
        return;
      }
      default:
        break;
    }
  }

  if (evt.type == InputEvents::Type::EncoderTurn && evt.encoderDelta != 0 && engineId == EngineRouter::kBurstId) {
    // Burst exposes the same idea, but with cluster count and spacing in
    // samples so students can feel the difference between count and time.
    Engine::ParamChange change{};
    change.seedId = seed.id;
    switch (evt.encoder) {
      case hal::Board::EncoderID::Density: {
        change.id = static_cast<std::uint16_t>(BurstEngine::Param::kClusterCount);
        change.value = static_cast<std::int32_t>(app.engines_.burst().clusterCount()) + evt.encoderDelta;
        app.engines_.burst().onParam(change);
        app.displayDirty_ = true;
        return;
      }
      case hal::Board::EncoderID::ToneTilt: {
        change.id = static_cast<std::uint16_t>(BurstEngine::Param::kSpacingSamples);
        const int32_t delta = evt.encoderDelta * 240;
        const int32_t current = static_cast<std::int32_t>(app.engines_.burst().spacingSamples());
        change.value = std::max<int32_t>(0, current + delta);
        app.engines_.burst().onParam(change);
        app.displayDirty_ = true;
        return;
      }
      default:
        break;
    }
  }
}

void ModeEventRouter::handlePerfEvent(AppState& app, const InputEvents::Event& evt) const {
  if (evt.type == InputEvents::Type::ButtonPress && evt.primaryButton == hal::Board::ButtonID::TapTempo) {
    // Performance mode gives Tap a transport meaning instead of a seed-prime
    // meaning so the live page can start/stop without mode-switching.
    app.toggleTransportLatchedRunning();
    app.displayDirty_ = true;
  }
}

void ModeEventRouter::handleSettingsEvent(AppState& app, const InputEvents::Event& evt) const {
  if (evt.type == InputEvents::Type::ButtonPress && evt.primaryButton == hal::Board::ButtonID::TapTempo) {
    // Settings turns Tap into "follow external clock?" because this page is
    // where transport allegiance is supposed to be configured explicitly.
    app.setFollowExternalClockFromHost(!app.followExternalClockEnabled());
    app.displayDirty_ = true;
    return;
  }

  if (evt.type == InputEvents::Type::ButtonPress && evt.primaryButton == hal::Board::ButtonID::AltSeed) {
    // Alt on Settings is the seed-prime bypass toggle: a runtime topology
    // change, not a per-seed edit.
    app.setSeedPrimeBypassFromHost(!app.seedPrimeBypassEnabled_);
    return;
  }
}

void ModeEventRouter::handleUtilEvent(AppState& app, const InputEvents::Event& evt) const {
  if (evt.type == InputEvents::Type::EncoderTurn &&
      evt.encoder == hal::Board::EncoderID::FxMutate && evt.encoderDelta != 0) {
    // Utility mode treats the FX encoder as a simple debug-meter on/off switch
    // so diagnostics stay deliberately out of the musical pages.
    app.debugMetersEnabled_ = evt.encoderDelta > 0 ? true : false;
    app.displayDirty_ = true;
  }
}

void ModeEventRouter::handleSwingEvent(AppState& app, const InputEvents::Event& evt) const {
  if (evt.type == InputEvents::Type::EncoderTurn || evt.type == InputEvents::Type::EncoderHoldTurn) {
    // Swing mode intentionally makes multiple encoders mean "nudge the groove",
    // but with coarse and fine step sizes depending on which knob was grabbed.
    float step = 0.0f;
    switch (evt.encoder) {
      case hal::Board::EncoderID::SeedBank: step = 0.05f; break;
      case hal::Board::EncoderID::Density: step = 0.01f; break;
      default: break;
    }
    if (step != 0.0f && evt.encoderDelta != 0) {
      app.adjustSwing(step * static_cast<float>(evt.encoderDelta));
    }
  }

  if (evt.type == InputEvents::Type::ButtonPress && evt.primaryButton == hal::Board::ButtonID::TapTempo) {
    AppState::Mode target = app.previousModeBeforeSwing_;
    if (target == AppState::Mode::SWING) {
      target = AppState::Mode::HOME;
    }
    app.exitSwingMode(target);
  }
}
