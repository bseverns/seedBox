#include "engine/EngineRouter.h"

#include <algorithm>
#include <cstring>

#include "hal/hal_audio.h"

namespace {
// Build a context object that every engine understands.  Engines need to know
// whether we are running on hardware (so they can reach for DMA buffers or
// other Teensy-only tricks), and they need the timing constants so scheduling
// maths stay locked.  The master seed is passed along to keep procedural voices
// deterministic between simulator + hardware builds.
Engine::PrepareContext makePrepareContext(EngineRouter::Mode mode, std::uint32_t masterSeed) {
  Engine::PrepareContext ctx{};
  ctx.hardware = (mode == EngineRouter::Mode::kHardware);
  ctx.sampleRate = hal::audio::sampleRate();
  ctx.framesPerBlock = static_cast<std::uint32_t>(hal::audio::framesPerBlock());
  ctx.masterSeed = masterSeed;
  return ctx;
}

// Some engines (granular, resonator) get cranky if you hot-unplug every seed
// that points at them.  Before letting an engine see a new RNG seed we check if
// at least one of its slots is currently unlocked.  Locked seeds stick with
// their old voice so the classroom demo can highlight how locks freeze parts of
// the groove.
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
  // Short names hit the OLED, so we copy them into a fixed buffer with a
  // trailing null.  It's hand-holdy on purpose: students do not need to juggle
  // std::string lifetimes while learning why the sampler wakes up first.
  const std::size_t copy = std::min<std::size_t>(shortName.size(), entry.shortName.size() - 1);
  std::memcpy(entry.shortName.data(), shortName.data(), copy);
  Engine* raw = engine.get();
  entry.instance = std::move(engine);
  registry_[id] = std::move(entry);

  // Cache raw pointers so AppState can offer ergonomic accessors.  The cast is
  // safe because each engine advertises its type.  This also doubles as a
  // reality check for students experimenting with new Engine subclasses.
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
    case Engine::Type::kToy:
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

  // Class time translation: each registerEngine call plugs a stomp box into the
  // pedal board.  The IDs are curated so presets/tests can reference them
  // without hard-coding pointers.
  registerEngine(kSamplerId, "Sampler", "SMP", std::make_unique<Sampler>());
  registerEngine(kGranularId, "Granular", "GRA", std::make_unique<GranularEngine>());
  registerEngine(kResonatorId, "Resonator", "PING", std::make_unique<ResonatorBank>());
  registerEngine(kEuclidId, "Euclid", "ECL", std::make_unique<EuclidEngine>());
  registerEngine(kBurstId, "Burst", "BST", std::make_unique<BurstEngine>());
  registerEngine(kToyId, "Toy", "TOY", std::make_unique<ToyGenerator>());

  const Engine::PrepareContext ctx = makePrepareContext(mode_, lastMasterSeed_);
  for (auto& [id, entry] : registry_) {
    (void)id;
    if (entry.instance) {
      // Every engine gets a preflight check with the same context.  This is
      // where sample-rate-dependent tables get baked and where hardware builds
      // might allocate codec buffers.  Keep it deterministic so presets never
      // depend on boot order voodoo.
      entry.instance->prepare(ctx);
    }
  }
}

void EngineRouter::setSeedCount(std::size_t count) {
  const std::uint8_t defaultId = sanitizeEngineId(kSamplerId);
  seedAssignments_.resize(count, defaultId);
  seedLocks_.resize(count, false);
  for (auto& assignment : seedAssignments_) {
    // sanitizeEngineId keeps rogue values (from corrupt presets or unit tests)
    // from smuggling in bogus engine IDs.  Students see a defensive pattern:
    // trust the input, but verify it.
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
    // Global lock means "nobody move".  Classroom exercise: flip this on to
    // hear how external clock changes still propagate while engine states stay
    // frozen.
    return;
  }

  const Engine::PrepareContext ctx = makePrepareContext(mode_, masterSeed);
  for (auto& [id, entry] : registry_) {
    if (!entry.instance) continue;
    if (!engineHasUnlockedSeed(id, seedAssignments_, seedLocks_)) {
      // Skip engines whose seeds are entirely padlocked.  Students can observe
      // that only unlocked voices get a fresh coat of random paint.
    continue;
  }
  entry.instance->prepare(ctx);
}
}

void EngineRouter::panic() {
  for (auto& [id, entry] : registry_) {
    (void)id;
    if (entry.instance) {
      entry.instance->panic();
    }
  }
}

void EngineRouter::triggerSeed(const Seed& seed, std::uint32_t whenSamples) {
  const std::uint8_t engineId = sanitizeEngineId(seed.engine);
  Engine* engine = findEngine(engineId);
  if (!engine) {
    // Fallback: when a preset references an engine that no longer exists, we
    // degrade gracefully to the sampler.  The sampler is our safety net because
    // it's always registered first.
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
  // Legacy path: some tests still call onSeed directly before we wired the
  // scheduler thunk.  It mirrors triggerSeed, just without the timestamp.
  switch (seed.engine) {
    case 0:
      if (sampler_) {
        sampler_->onSeed(seed);
      }
      break;
    case 1:
      if (granular_) {
        granular_->onSeed(seed);
      }
      break;
    case 2:
      if (resonator_) {
        resonator_->onSeed(seed);
      }
      break;
    default:
      if (sampler_) {
        sampler_->onSeed(seed);
      }
      break;
  }
}
std::size_t EngineRouter::engineCount() const { return registry_.size(); }

std::uint8_t EngineRouter::sanitizeEngineId(std::uint8_t engineId) const {
  if (registry_.count(engineId)) {
    return engineId;
  }
  // If an ID is unknown we fall back to the sampler.  That makes firmware
  // upgrades forgiving when older presets ship IDs we have not mapped yet.
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
