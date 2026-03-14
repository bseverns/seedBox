#include "engine/EuclidEngine.h"

#include <algorithm>
#include <cmath>
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

std::vector<std::uint8_t> buildHostMask(std::uint8_t steps, std::uint8_t fills, std::uint8_t rotate) {
  std::vector<std::uint8_t> mask(steps, 0u);
  if (fills == 0 || steps == 0) {
    return mask;
  }
  for (std::uint8_t i = 0; i < steps; ++i) {
    const std::uint32_t start = static_cast<std::uint32_t>(i) * fills;
    const std::uint32_t end = static_cast<std::uint32_t>(i + 1) * fills;
    const bool gate = (end / steps) > (start / steps);
    const std::uint8_t rotated = static_cast<std::uint8_t>((i + steps - (rotate % steps)) % steps);
    mask[rotated] = gate ? 1u : 0u;
  }
  return mask;
}
}  // namespace

void EuclidEngine::prepare(const Engine::PrepareContext& ctx) {
  generationSeed_ = ctx.masterSeed;
  cursor_ = 0;
  lastSeedId_ = 0;
  hostSampleRate_ = (ctx.sampleRate > 0) ? static_cast<float>(ctx.sampleRate) : 48000.0f;
  hostCursor_ = 0;
  hostGateLevel_ = 0.0f;
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

void EuclidEngine::processInputAudio(const Seed& seed, const Engine::RenderContext& ctx) {
  if (!ctx.inputLeft || !ctx.left || !ctx.right || ctx.frames == 0) {
    return;
  }

  const float densityNorm = std::clamp(seed.density / 3.0f, 0.0f, 1.0f);
  const std::uint8_t steps = std::clamp<std::uint8_t>(
      static_cast<std::uint8_t>(8u + static_cast<std::uint8_t>(std::lround(densityNorm * 8.0f))), 4u, 16u);
  const std::uint8_t fills = std::clamp<std::uint8_t>(
      static_cast<std::uint8_t>(std::lround(std::clamp(seed.probability, 0.0f, 1.0f) * steps)), 1u, steps);
  const std::uint8_t rotate = wrapRotate(steps, static_cast<int>(std::lround(seed.spread * steps)));
  const std::vector<std::uint8_t> hostMask = buildHostMask(steps, fills, rotate);
  const std::size_t stepSamples = std::clamp<std::size_t>(
      static_cast<std::size_t>(hostSampleRate_ * (0.04f + ((1.0f - densityNorm) * 0.08f))), 128u,
      static_cast<std::size_t>(hostSampleRate_ * 0.25f));
  const float gateFloor = 0.12f + (0.18f * std::clamp(seed.tone, 0.0f, 1.0f));
  const float wet = 0.6f + (0.25f * densityNorm);
  const float cross = 0.08f + (0.22f * std::clamp(seed.spread, 0.0f, 1.0f));

  for (std::size_t i = 0; i < ctx.frames; ++i) {
    const float inL = ctx.inputLeft[i];
    const float inR = ctx.inputRight ? ctx.inputRight[i] : inL;
    const std::size_t stepIndex = static_cast<std::size_t>((hostCursor_ / stepSamples) % hostMask.size());
    const float gateTarget = hostMask[stepIndex] ? 1.0f : gateFloor;
    const float glide = (gateTarget > hostGateLevel_) ? 0.16f : 0.045f;
    hostGateLevel_ += (gateTarget - hostGateLevel_) * glide;

    const float rhythmicL = (inL * hostGateLevel_) + (inR * cross * (1.0f - hostGateLevel_));
    const float rhythmicR = (inR * hostGateLevel_) + (inL * cross * (1.0f - hostGateLevel_));
    ctx.left[i] += std::tanh((inL * (1.0f - wet * 0.35f)) + (rhythmicL * wet * 1.15f));
    ctx.right[i] += std::tanh((inR * (1.0f - wet * 0.35f)) + (rhythmicR * wet * 1.15f));
    ++hostCursor_;
  }
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
  hostCursor_ = 0;
  hostGateLevel_ = 0.0f;
}
