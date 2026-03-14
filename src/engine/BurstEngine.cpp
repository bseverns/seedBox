#include "engine/BurstEngine.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "Seed.h"

Engine::Type BurstEngine::type() const noexcept { return Engine::Type::kBurst; }

namespace {
constexpr std::uint8_t clampCluster(std::int32_t value) {
  if (value < 1) return 1;
  if (value > 16) return 16;
  return static_cast<std::uint8_t>(value);
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

std::size_t wrapDelayTap(std::size_t writePos, std::size_t bufferSize, std::size_t delaySamples) {
  if (bufferSize == 0) {
    return 0;
  }
  const std::size_t clampedDelay = std::min(delaySamples, bufferSize - 1);
  return (writePos + bufferSize - clampedDelay) % bufferSize;
}
}  // namespace

void BurstEngine::prepare(const Engine::PrepareContext& ctx) {
  generationSeed_ = ctx.masterSeed;
  lastSeedId_ = 0;
  hostSampleRate_ = (ctx.sampleRate > 0) ? static_cast<float>(ctx.sampleRate) : 48000.0f;
  const std::size_t echoFrames = std::max<std::size_t>(static_cast<std::size_t>(hostSampleRate_), ctx.framesPerBlock + 1u);
  hostEchoLeft_.assign(echoFrames, 0.0f);
  hostEchoRight_.assign(echoFrames, 0.0f);
  hostWritePos_ = 0;
  hostCursor_ = 0;
  // Pending triggers stay resident between seeds; clearing here keeps the
  // scheduler honest when a test fixture swaps from noisy to quiet seeds.
  pending_.clear();
}

void BurstEngine::onTick(const Engine::TickContext& ctx) {
  (void)ctx;
}

void BurstEngine::onParam(const Engine::ParamChange& change) {
  const auto param = static_cast<Param>(change.id);
  switch (param) {
    case Param::kClusterCount:
      clusterCount_ = clampCluster(change.value);
      break;
    case Param::kSpacingSamples:
      if (change.value < 0) {
        spacingSamples_ = 0;
      } else {
        spacingSamples_ = static_cast<std::uint32_t>(change.value);
      }
      break;
    default:
      break;
  }
}

void BurstEngine::onSeed(const Engine::SeedContext& ctx) {
  pending_.clear();
  pending_.reserve(clusterCount_);
  std::uint32_t when = ctx.whenSamples;
  for (std::uint8_t i = 0; i < clusterCount_; ++i) {
    pending_.push_back(when);
    when += spacingSamples_;
  }
  // `lastSeedId_` mirrors the Euclid engine so the router and UI have a stable
  // label for the last trigger cloud emitted by this engine.
  lastSeedId_ = ctx.seed.id;
}

void BurstEngine::processInputAudio(const Seed& seed, const Engine::RenderContext& ctx) {
  if (!ctx.inputLeft || !ctx.left || !ctx.right || ctx.frames == 0) {
    return;
  }
  if (hostEchoLeft_.empty() || hostEchoRight_.empty()) {
    const std::size_t echoFrames =
        std::max<std::size_t>(static_cast<std::size_t>(std::max(48000.0f, hostSampleRate_)), ctx.frames + 1u);
    hostEchoLeft_.assign(echoFrames, 0.0f);
    hostEchoRight_.assign(echoFrames, 0.0f);
  }

  const float density = std::clamp(seed.density, 0.25f, 4.5f);
  const std::size_t cluster = static_cast<std::size_t>(
      std::clamp<int>(static_cast<int>(std::lround(1.0f + (density * 1.6f) + (seed.probability * 3.0f))), 1, 8));
  const std::size_t spacingSamples = std::clamp<std::size_t>(
      static_cast<std::size_t>(hostSampleRate_ * (0.015f + (seed.jitterMs * 0.0008f))), 48u,
      std::max<std::size_t>(96u, hostEchoLeft_.size() / 6u));
  const std::size_t pulseWidth = std::clamp<std::size_t>(
      static_cast<std::size_t>(hostSampleRate_ * (0.006f + (seed.tone * 0.018f))), 24u, spacingSamples);
  const std::size_t cycleSamples = std::max<std::size_t>(
      cluster * spacingSamples,
      static_cast<std::size_t>(hostSampleRate_ * (0.16f + ((1.0f - std::clamp(seed.spread, 0.0f, 1.0f)) * 0.18f))));
  const float feedback = 0.2f + (0.38f * std::clamp(seed.mutateAmt, 0.0f, 1.0f));
  const float wet = 0.7f + (0.2f * std::clamp(seed.probability, 0.0f, 1.0f));
  const std::size_t delaySize = hostEchoLeft_.size();

  for (std::size_t i = 0; i < ctx.frames; ++i) {
    const float inL = ctx.inputLeft[i];
    const float inR = ctx.inputRight ? ctx.inputRight[i] : inL;
    const std::size_t cyclePos = static_cast<std::size_t>(hostCursor_ % cycleSamples);
    const std::size_t pulseIndex = std::min<std::size_t>(cluster - 1u, cyclePos / std::max<std::size_t>(1u, spacingSamples));
    const std::size_t pulseOffset = cyclePos % std::max<std::size_t>(1u, spacingSamples);
    const bool pulseOpen = (pulseIndex < cluster) && (pulseOffset < pulseWidth);

    const std::size_t delayA = spacingSamples * std::max<std::size_t>(1u, pulseIndex + 1u);
    const std::size_t delayB = delayA + std::max<std::size_t>(24u, spacingSamples / 2u);
    const float echoL = (hostEchoLeft_[wrapDelayTap(hostWritePos_, delaySize, delayA)] * 0.75f) +
                        (hostEchoLeft_[wrapDelayTap(hostWritePos_, delaySize, delayB)] * 0.35f);
    const float echoR = (hostEchoRight_[wrapDelayTap(hostWritePos_, delaySize, delayA)] * 0.75f) +
                        (hostEchoRight_[wrapDelayTap(hostWritePos_, delaySize, delayB)] * 0.35f);

    const float gate = pulseOpen ? 1.0f : (0.08f + (0.12f * std::clamp(seed.spread, 0.0f, 1.0f)));
    const float outL = std::tanh((inL * gate * (1.0f - wet * 0.25f)) + (echoL * wet * 1.25f));
    const float outR = std::tanh((inR * gate * (1.0f - wet * 0.25f)) + (echoR * wet * 1.25f));

    ctx.left[i] += outL;
    ctx.right[i] += outR;

    hostEchoLeft_[hostWritePos_] = inL + (echoL * feedback);
    hostEchoRight_[hostWritePos_] = inR + (echoR * feedback);
    hostWritePos_ = (hostWritePos_ + 1u) % delaySize;
    ++hostCursor_;
  }
}

void BurstEngine::renderAudio(const Engine::RenderContext& ctx) {
  (void)ctx;
}

Engine::StateBuffer BurstEngine::serializeState() const {
  Engine::StateBuffer buffer;
  buffer.reserve(10);
  buffer.push_back(clusterCount_);
  appendU32(buffer, spacingSamples_);
  appendU32(buffer, generationSeed_);
  appendU32(buffer, lastSeedId_);
  return buffer;
}

void BurstEngine::deserializeState(const Engine::StateBuffer& state) {
  if (state.empty()) {
    return;
  }
  clusterCount_ = clampCluster(state[0]);
  spacingSamples_ = readU32(state, 1);
  generationSeed_ = readU32(state, 5);
  lastSeedId_ = readU32(state, 9);
}

void BurstEngine::panic() {
  pending_.clear();
  std::fill(hostEchoLeft_.begin(), hostEchoLeft_.end(), 0.0f);
  std::fill(hostEchoRight_.begin(), hostEchoRight_.end(), 0.0f);
  hostWritePos_ = 0;
  hostCursor_ = 0;
}
