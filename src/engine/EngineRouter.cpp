#include "engine/EngineRouter.h"

#include <algorithm>
#include <cstring>

#include "hal/hal_audio.h"

namespace {
Engine::PrepareContext makePrepareContext(EngineRouter::Mode mode, std::uint32_t masterSeed) {
  Engine::PrepareContext ctx{};
  ctx.hardware = (mode == EngineRouter::Mode::kHardware);
  ctx.sampleRate = hal::audio::sampleRate();
  ctx.framesPerBlock = static_cast<std::uint32_t>(hal::audio::framesPerBlock());
  ctx.masterSeed = masterSeed;
  return ctx;
}

bool engineHasUnlockedSeed(std::uint8_t engineId, const std::vector<std::uint8_t>& assignments,
                           const std::vector<bool>& locks) {
  for (std::size_t i = 0; i < assignments.size(); ++i) {
    if (assignments[i] == engineId) {
      if (i >= locks.size() || !locks[i]) {
        return true;
      }
    }
  }
  return false;
}
}  // namespace

void EngineRouter::registerEngine(std::uint8_t id, std::string_view name, std::string_view shortName,
                                  std::unique_ptr<Engine> engine) {
  if (!engine) {
    return;
  }
  RegisteredEngine entry;
  entry.id = id;
  entry.name = std::string(name);
  entry.shortName.fill('\0');
  const std::size_t copy = std::min<std::size_t>(shortName.size(), entry.shortName.size() - 1);
  std::memcpy(entry.shortName.data(), shortName.data(), copy);
  Engine* raw = engine.get();
  entry.instance = std::move(engine);
  registry_[id] = std::move(entry);

  switch (raw->type()) {
    case Engine::Type::kSampler:
      sampler_ = static_cast<Sampler*>(raw);
      break;
    case Engine::Type::kGranular:
      granular_ = static_cast<GranularEngine*>(raw);
      break;
    case Engine::Type::kResonator:
      resonator_ = static_cast<ResonatorBank*>(raw);
      break;
    case Engine::Type::kEuclid:
      euclid_ = static_cast<EuclidEngine*>(raw);
      break;
    case Engine::Type::kBurst:
      burst_ = static_cast<BurstEngine*>(raw);
      break;
    case Engine::Type::kUnknown:
    default:
      break;
  }
}

Engine* EngineRouter::findEngine(std::uint8_t engineId) {
  auto it = registry_.find(engineId);
  if (it == registry_.end()) {
    return nullptr;
  }
  return it->second.instance.get();
}

const Engine* EngineRouter::findEngine(std::uint8_t engineId) const {
  auto it = registry_.find(engineId);
  if (it == registry_.end()) {
    return nullptr;
  }
  return it->second.instance.get();
}

void EngineRouter::init(Mode mode) {
  mode_ = mode;
  registry_.clear();
  sampler_ = nullptr;
  granular_ = nullptr;
  resonator_ = nullptr;
  euclid_ = nullptr;
  burst_ = nullptr;

  registerEngine(kSamplerId, "Sampler", "SMP", std::make_unique<Sampler>());
  registerEngine(kGranularId, "Granular", "GRA", std::make_unique<GranularEngine>());
  registerEngine(kResonatorId, "Resonator", "PING", std::make_unique<ResonatorBank>());
  registerEngine(kEuclidId, "Euclid", "ECL", std::make_unique<EuclidEngine>());
  registerEngine(kBurstId, "Burst", "BST", std::make_unique<BurstEngine>());

  const Engine::PrepareContext ctx = makePrepareContext(mode_, lastMasterSeed_);
  for (auto& [id, entry] : registry_) {
    (void)id;
    if (entry.instance) {
      entry.instance->prepare(ctx);
    }
  }
}

void EngineRouter::setSeedCount(std::size_t count) {
  const std::uint8_t defaultId = sanitizeEngineId(kSamplerId);
  seedAssignments_.resize(count, defaultId);
  seedLocks_.resize(count, false);
  for (auto& assignment : seedAssignments_) {
    assignment = sanitizeEngineId(assignment);
  }
}

void EngineRouter::assignSeed(std::size_t index, std::uint8_t engineId) {
  if (index >= seedAssignments_.size()) {
    setSeedCount(index + 1);
  }
  seedAssignments_[index] = sanitizeEngineId(engineId);
}

void EngineRouter::setSeedLock(std::size_t index, bool locked) {
  if (index >= seedLocks_.size()) {
    seedLocks_.resize(index + 1, false);
  }
  seedLocks_[index] = locked;
}

void EngineRouter::setGlobalLock(bool locked) { globalLock_ = locked; }

void EngineRouter::reseed(std::uint32_t masterSeed) {
  lastMasterSeed_ = masterSeed;
  if (globalLock_) {
    return;
  }

  const Engine::PrepareContext ctx = makePrepareContext(mode_, masterSeed);
  for (auto& [id, entry] : registry_) {
    if (!entry.instance) continue;
    if (!engineHasUnlockedSeed(id, seedAssignments_, seedLocks_)) {
      continue;
    }
    entry.instance->prepare(ctx);
  }
}

void EngineRouter::triggerSeed(const Seed& seed, std::uint32_t whenSamples) {
  const std::uint8_t engineId = sanitizeEngineId(seed.engine);
  Engine* engine = findEngine(engineId);
  if (!engine) {
    engine = sampler_;
  }
  if (!engine) {
    return;
  }
  Engine::SeedContext ctx{seed, whenSamples};
  engine->onSeed(ctx);
}

void EngineRouter::dispatchThunk(void* ctx, const Seed& seed, std::uint32_t whenSamples) {
  if (!ctx) return;
  static_cast<EngineRouter*>(ctx)->triggerSeed(seed, whenSamples);
}

void EngineRouter::onSeed(const Seed& seed) {
  switch (seed.engine) {
    case 0:
      sampler_.onSeed(seed);
      break;
    case 1:
      granular_.onSeed(seed);
      break;
    case 2:
      resonator_.onSeed(seed);
      break;
    default:
      sampler_.onSeed(seed);
      break;
  }
}
std::size_t EngineRouter::engineCount() const { return registry_.size(); }

std::uint8_t EngineRouter::sanitizeEngineId(std::uint8_t engineId) const {
  if (registry_.count(engineId)) {
    return engineId;
  }
  return kSamplerId;
}

std::string_view EngineRouter::engineShortName(std::uint8_t engineId) const {
  auto it = registry_.find(engineId);
  if (it == registry_.end()) {
    static constexpr char kUnknown[] = "UNK";
    return std::string_view{kUnknown};
  }
  const auto& entry = it->second;
  const std::size_t len = std::strlen(entry.shortName.data());
  return std::string_view(entry.shortName.data(), len);
}

const std::string& EngineRouter::engineName(std::uint8_t engineId) const {
  auto it = registry_.find(engineId);
  if (it == registry_.end()) {
    static const std::string kUnknown{"Unknown"};
    return kUnknown;
  }
  return it->second.name;
}

