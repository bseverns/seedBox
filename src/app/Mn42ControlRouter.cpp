#include "app/Mn42ControlRouter.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "app/AppState.h"
#include "interop/mn42_map.h"
#include "interop/mn42_param_map.h"

namespace {
constexpr std::uint8_t kEngineCycleCc = seedbox::interop::mn42::param::kEngineCycle;
}

// MN-42 is the dense external-control surface: compact CCs, mode bits, and
// host macros that all need to land on the same SeedBox runtime without
// bypassing lock semantics or scheduler refresh rules.
void Mn42ControlRouter::route(AppState& app, std::uint8_t channel, std::uint8_t controller, std::uint8_t value) const {
  using namespace seedbox::interop::mn42;

  if (channel == kDefaultChannel) {
    if (controller == cc::kHandshake) {
      if (value == handshake::kHello) {
        app.mn42HelloSeen_ = true;
      }
      return;
    }
    if (controller == cc::kMode) {
      applyModeBits(app, value);
      return;
    }
    if (controller == cc::kTransportGate) {
      handleTransportGate(app, value);
      return;
    }
  }

  if (controller == kEngineCycleCc) {
    cycleFocusedSeedEngine(app, value);
    return;
  }

  if (controller == cc::kQuantize) {
    app.applyQuantizeControl(value);
    return;
  }

  if (applyParamControl(app, controller, value)) {
    return;
  }
}

void Mn42ControlRouter::applyModeBits(AppState& app, std::uint8_t value) const {
  // These mode bits are coarse global policy toggles, so they change transport
  // ownership and debug visibility rather than touching any single seed.
  const bool follow = (value & seedbox::interop::mn42::mode::kFollowExternalClock) != 0;
  if (app.followExternalClockEnabled() != follow) {
    app.clockTransport_.setFollowExternalClockEnabled(follow, app.board_.nowMillis());
  }

  app.debugMetersEnabled_ = (value & seedbox::interop::mn42::mode::kExposeDebugMeters) != 0;

  const bool latch = (value & seedbox::interop::mn42::mode::kLatchTransport) != 0;
  if (app.transportLatchEnabled() != latch) {
    app.clockTransport_.setTransportLatchEnabled(latch);
  } else if (app.transportLatchEnabled()) {
    app.clockTransport_.refreshTransportLatchState();
  }
}

bool Mn42ControlRouter::applyParamControl(AppState& app, std::uint8_t controller, std::uint8_t value) const {
  using namespace seedbox::interop::mn42;
  if (!LookupParam(controller)) {
    return false;
  }

  if (controller == param::kFocusSeed) {
    // Focus selection scales the 7-bit CC range across the live seed table so
    // external controllers can scrub the current locus without knowing count.
    if (app.seeds_.empty()) {
      app.setFocusSeed(0);
      app.displayDirty_ = true;
      return true;
    }
    const std::size_t count = app.seeds_.size();
    const std::uint32_t scaled = static_cast<std::uint32_t>(value) * static_cast<std::uint32_t>(count);
    const std::uint8_t target = static_cast<std::uint8_t>(std::min<std::size_t>(scaled / 128u, count - 1));
    if (app.focusSeed_ != target) {
      app.setFocusSeed(target);
      app.displayDirty_ = true;
    }
    return true;
  }

  if (app.seeds_.empty()) {
    return true;
  }
  const std::size_t count = app.seeds_.size();
  const std::size_t idx = static_cast<std::size_t>(app.focusSeed_) % count;
  if (app.seedLock_.seedLocked(idx)) {
    // External macros still obey local lock policy; a locked seed should feel
    // immutable no matter whether the gesture came from panel or host.
    return true;
  }

  Seed& seed = app.seeds_[idx];
  bool changed = false;
  const float normalized = static_cast<float>(value) / 127.f;

  // The CC map speaks in normalized 0..127 values. Each branch reprojects that
  // into the musical range the local UI would have offered.
  const auto assignIfDifferent = [&](float& field, float target) {
    if (std::fabs(field - target) > 1e-4f) {
      field = target;
      changed = true;
    }
  };

  switch (controller) {
    case param::kSeedPitch: {
      constexpr float kRange = 48.f;
      constexpr float kFloor = -24.f;
      assignIfDifferent(seed.pitch, kFloor + (normalized * kRange));
      break;
    }
    case param::kSeedDensity: {
      constexpr float kMaxDensity = 8.f;
      assignIfDifferent(seed.density, std::clamp(normalized * kMaxDensity, 0.f, kMaxDensity));
      break;
    }
    case param::kSeedProbability:
      assignIfDifferent(seed.probability, std::clamp(normalized, 0.f, 1.f));
      break;
    case param::kSeedJitter: {
      constexpr float kMaxJitterMs = 30.f;
      assignIfDifferent(seed.jitterMs, std::clamp(normalized * kMaxJitterMs, 0.f, kMaxJitterMs));
      break;
    }
    case param::kSeedTone:
      assignIfDifferent(seed.tone, std::clamp(normalized, 0.f, 1.f));
      break;
    case param::kSeedSpread:
      assignIfDifferent(seed.spread, std::clamp(normalized, 0.f, 1.f));
      break;
    case param::kSeedMutate:
      assignIfDifferent(seed.mutateAmt, std::clamp(normalized, 0.f, 1.f));
      break;
    default:
      return true;
  }

  if (changed) {
    // Host edits must update both the scheduler's canonical seed copy and the
    // engine focus cache so the next audio block reflects the new gesture.
    app.scheduler_.updateSeed(idx, seed);
    app.engines_.onSeed(seed);
    app.displayDirty_ = true;
  }
  return true;
}

void Mn42ControlRouter::handleTransportGate(AppState& app, std::uint8_t value) const {
  // Transport gate stays delegated to the shared clock controller so hardware,
  // simulator, and host automation all interpret latching the same way.
  app.clockTransport_.handleTransportGate(value);
}

void Mn42ControlRouter::cycleFocusedSeedEngine(AppState& app, std::uint8_t value) const {
  const std::size_t engineCount = app.engines_.engineCount();
  if (engineCount == 0 || app.seeds_.empty()) {
    return;
  }

  const std::size_t count = app.seeds_.size();
  const std::uint8_t focus = static_cast<std::uint8_t>(std::min<std::size_t>(app.focusSeed_, count - 1));
  const std::size_t current = static_cast<std::size_t>(app.seeds_[focus].engine % static_cast<std::uint8_t>(engineCount));
  // High CC values mean "next", low values mean "previous", which gives
  // endless encoders and button pairs the same mental model.
  const std::size_t next = (value >= 64) ? (current + 1) % engineCount : (current + engineCount - 1) % engineCount;
  const std::uint8_t target = app.engines_.sanitizeEngineId(static_cast<std::uint8_t>(next));
  app.setSeedEngine(focus, target);
}
