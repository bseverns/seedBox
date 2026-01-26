#include "engine/EuclidEngine.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "Seed.h"

Engine::Type EuclidEngine::type() const noexcept { return Engine::Type::kEuclid; }

namespace {
constexpr std::uint8_t clampSteps(std::int32_t value) {
  if (value < 1) return 1;
  if (value > 32) return 32;
  return static_cast<std::uint8_t>(value);
}

constexpr std::uint8_t clampFills(std::uint8_t steps, std::int32_t value) {
  if (value < 0) return 0;
  if (value > steps) return steps;
  return static_cast<std::uint8_t>(value);
}

constexpr std::uint8_t wrapRotate(std::uint8_t steps, std::int32_t value) {
  if (steps == 0) return 0;
  std::int32_t mod = value % steps;
  if (mod < 0) mod += steps;
  return static_cast<std::uint8_t>(mod);
}

void appendU32(Engine::StateBuffer& out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
}

std::uint32_t readU32(const Engine::StateBuffer& in, std::size_t index) {
  if (index + 4 > in.size()) {
    return 0;
  }
  return static_cast<std::uint32_t>(in[index]) |
         (static_cast<std::uint32_t>(in[index + 1]) << 8u) |
         (static_cast<std::uint32_t>(in[index + 2]) << 16u) |
         (static_cast<std::uint32_t>(in[index + 3]) << 24u);
}
}  // namespace

void EuclidEngine::prepare(const Engine::PrepareContext& ctx) {
  generationSeed_ = ctx.masterSeed;
  cursor_ = 0;
  lastSeedId_ = 0;
  // The mask lives entirely in RAM and is rebuilt from the param trio so
  // students can reseed or tweak rotation without paying for heap churn or
  // risking a race with the audio thread.
  rebuildMask();
}

void EuclidEngine::onTick(const Engine::TickContext& ctx) {
  (void)ctx;
  if (mask_.empty()) {
    lastGate_ = false;
    return;
  }
  const std::size_t index = cursor_ % mask_.size();
  lastGate_ = (mask_[index] != 0);
  ++cursor_;
}

void EuclidEngine::onParam(const Engine::ParamChange& change) {
  const auto param = static_cast<Param>(change.id);
  switch (param) {
    case Param::kSteps:
      steps_ = clampSteps(change.value);
      fills_ = std::min<std::uint8_t>(fills_, steps_);
      rotate_ = wrapRotate(steps_, rotate_);
      rebuildMask();
      break;
    case Param::kFills:
      fills_ = clampFills(steps_, change.value);
      rebuildMask();
      break;
    case Param::kRotate:
      rotate_ = wrapRotate(steps_, change.value);
      rebuildMask();
      break;
    default:
      break;
  }
}

void EuclidEngine::onSeed(const Engine::SeedContext& ctx) {
  lastSeedId_ = ctx.seed.id;
  (void)ctx.whenSamples;
}

void EuclidEngine::renderAudio(const Engine::RenderContext& ctx) {
  (void)ctx;
}

Engine::StateBuffer EuclidEngine::serializeState() const {
  Engine::StateBuffer buffer;
  buffer.reserve(3 + 8);
  buffer.push_back(steps_);
  buffer.push_back(fills_);
  buffer.push_back(rotate_);
  appendU32(buffer, cursor_);
  appendU32(buffer, generationSeed_);
  appendU32(buffer, lastSeedId_);
  return buffer;
}

void EuclidEngine::deserializeState(const Engine::StateBuffer& state) {
  if (state.size() < 3) {
    return;
  }
  steps_ = state[0];
  if (steps_ == 0) steps_ = 1;
  fills_ = std::min<std::uint8_t>(state[1], steps_);
  rotate_ = wrapRotate(steps_, state[2]);
  cursor_ = readU32(state, 3);
  generationSeed_ = readU32(state, 7);
  lastSeedId_ = readU32(state, 11);
  rebuildMask();
}

void EuclidEngine::rebuildMask() {
  const std::uint8_t steps = std::max<std::uint8_t>(1, steps_);
  const std::uint8_t fills = std::min<std::uint8_t>(steps, fills_);
  mask_.assign(steps, 0u);
  if (fills == 0) {
    return;
  }

  for (std::uint8_t i = 0; i < steps; ++i) {
    const std::uint32_t start = static_cast<std::uint32_t>(i) * fills;
    const std::uint32_t end = static_cast<std::uint32_t>(i + 1) * fills;
    const bool gate = (end / steps) > (start / steps);
    const std::uint8_t rotated = static_cast<std::uint8_t>((i + steps - (rotate_ % steps)) % steps);
    mask_[rotated] = gate ? 1u : 0u;
  }
  if (cursor_ >= mask_.size()) {
    cursor_ %= mask_.size();
  }
}

void EuclidEngine::panic() {
  cursor_ = 0;
  lastGate_ = false;
}
