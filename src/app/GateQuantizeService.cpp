#include "app/GateQuantizeService.h"

#include <algorithm>

#include "util/RNG.h"
#include "util/ScaleQuantizer.h"

// GateQuantizeService groups the "small but sharp" edit rules: when input-gate
// edges are allowed to cause reseeds, and how quantize maps a coarse control
// value onto a focused seed's pitch.

std::uint32_t GateQuantizeService::gateDivisionTicksFor(AppState::GateDivision div) {
  constexpr std::uint32_t kTicksPerBeat = 24u;
  switch (div) {
    case AppState::GateDivision::kOneOverTwo:
      return kTicksPerBeat / 2u;
    case AppState::GateDivision::kOneOverFour:
      return kTicksPerBeat / 4u;
    case AppState::GateDivision::kBars:
      return kTicksPerBeat * 4u;
    case AppState::GateDivision::kOneOverOne:
    default:
      return kTicksPerBeat;
  }
}

void GateQuantizeService::handleGateTick(AppState& app) const {
  const std::uint64_t tick = app.scheduler_.ticks();
  const std::uint32_t divisionTicks = gateDivisionTicksFor(app.gateDivision_);
  if (!app.inputGate_.gateReady(tick, divisionTicks)) {
    return;
  }

  // A hot input edge is only allowed to reseed when the current slot geometry
  // and lock state say the focused seed is actually writable.
  if (app.seeds_.empty() || app.seedLock_.globalLocked()) {
    app.inputGate_.syncGateTick(tick);
    return;
  }

  const std::size_t focusIndex = std::min<std::size_t>(app.focusSeed_, app.seeds_.size() - 1);
  if (app.seedLock_.seedLocked(focusIndex)) {
    app.inputGate_.syncGateTick(tick);
    return;
  }

  const std::uint32_t reseedValue = RNG::xorshift(app.masterSeed_);
  // Gate reseed derives from the current master seed so repeated edges march
  // through a deterministic lineage instead of inventing a second RNG stream.
  app.seedPageReseed(reseedValue, app.seedPrimeMode_);
  app.inputGate_.syncGateTick(app.scheduler_.ticks());
}

void GateQuantizeService::stepGateDivision(AppState& app, int delta) const {
  // Gate division is a tiny circular menu. Wrapping keeps encoder turns feeling
  // continuous rather than bounded.
  constexpr int kDivCount = 4;
  int next = static_cast<int>(app.gateDivision_) + delta;
  next %= kDivCount;
  if (next < 0) {
    next += kDivCount;
  }
  const AppState::GateDivision target = static_cast<AppState::GateDivision>(next);
  if (app.gateDivision_ != target) {
    app.gateDivision_ = target;
    app.inputGate_.syncGateTick(app.scheduler_.ticks());
    app.displayDirty_ = true;
  }
}

void GateQuantizeService::applyQuantizeControl(AppState& app, std::uint8_t value) const {
  if (app.seeds_.empty()) {
    return;
  }

  const std::uint8_t rawScaleIndex = static_cast<std::uint8_t>(value / 32);
  const std::uint8_t rawRoot = static_cast<std::uint8_t>(value % 12);
  const std::uint8_t sanitizedScaleIndex = std::min<std::uint8_t>(rawScaleIndex, static_cast<std::uint8_t>(4));
  const std::uint8_t sanitizedRoot = static_cast<std::uint8_t>(rawRoot % 12);
  app.quantizeScaleIndex_ = sanitizedScaleIndex;
  app.quantizeRoot_ = sanitizedRoot;

  // Quantize always targets the focused seed so both front-panel and remote
  // surfaces keep one clear "who am I editing?" story.
  const std::size_t idx = static_cast<std::size_t>(app.focusSeed_) % app.seeds_.size();
  if (app.seedLock_.seedLocked(idx)) {
    return;
  }

  util::ScaleQuantizer::Scale scale = util::ScaleQuantizer::Scale::kChromatic;
  switch (sanitizedScaleIndex) {
    case 1:
      scale = util::ScaleQuantizer::Scale::kMajor;
      break;
    case 2:
      scale = util::ScaleQuantizer::Scale::kMinor;
      break;
    case 3:
      scale = util::ScaleQuantizer::Scale::kPentatonicMajor;
      break;
    case 4:
      scale = util::ScaleQuantizer::Scale::kPentatonicMinor;
      break;
    case 0:
    default:
      scale = util::ScaleQuantizer::Scale::kChromatic;
      break;
  }

  Seed& seed = app.seeds_[idx];
  const float quantized = util::ScaleQuantizer::SnapToScale(seed.pitch, sanitizedRoot, scale);
  if (quantized != seed.pitch) {
    // Quantize is a real seed mutation, so the scheduler and engine cache have
    // to be refreshed immediately after the pitch snap.
    seed.pitch = quantized;
    app.scheduler_.updateSeed(idx, seed);
    app.engines_.onSeed(seed);
    app.displayDirty_ = true;
  }
}
