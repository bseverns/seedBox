#include "engine/BurstEngine.h"

#include <algorithm>
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
}  // namespace

void BurstEngine::prepare(const Engine::PrepareContext& ctx) {
  generationSeed_ = ctx.masterSeed;
  lastSeedId_ = 0;
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
  lastSeedId_ = ctx.seed.id;
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

