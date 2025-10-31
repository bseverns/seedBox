#include <cassert>
#include <cstdint>
#include <vector>

#include "Seed.h"
#include "engine/BurstEngine.h"
#include "engine/EngineRouter.h"
#include "engine/EuclidEngine.h"

namespace {
// These helpers double as lab notes: each test narrates a deterministic engine
// behaviour so students can follow the maths without needing the full firmware
// running.
void test_euclid_mask() {
  EuclidEngine engine;
  Engine::PrepareContext ctx{};
  ctx.masterSeed = 0x12345678u;
  engine.prepare(ctx);

  engine.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kSteps), 8});
  engine.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kFills), 3});
  engine.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kRotate), 1});

  const std::vector<std::uint8_t> expected{0, 1, 0, 0, 1, 0, 1, 0};
  for (std::size_t i = 0; i < expected.size(); ++i) {
    engine.onTick({i});
    assert(engine.lastGate() == (expected[i] != 0));
  }

  const auto state = engine.serializeState();
  EuclidEngine restored;
  restored.prepare(ctx);
  restored.deserializeState(state);
  for (std::size_t i = 0; i < expected.size(); ++i) {
    restored.onTick({i});
    assert(restored.lastGate() == (expected[i] != 0));
  }
}

void test_burst_spacing() {
  BurstEngine engine;
  Engine::PrepareContext ctx{};
  ctx.masterSeed = 0xCAFEBABEu;
  engine.prepare(ctx);
  engine.onParam({0, static_cast<std::uint16_t>(BurstEngine::Param::kClusterCount), 4});
  engine.onParam({0, static_cast<std::uint16_t>(BurstEngine::Param::kSpacingSamples), 120});

  Seed seed{};
  seed.id = 7;
  engine.onSeed({seed, 1000});

  const auto& triggers = engine.pendingTriggers();
  assert(triggers.size() == 4);
  const std::vector<std::uint32_t> expected{1000, 1120, 1240, 1360};
  assert(triggers == expected);

  const auto state = engine.serializeState();
  BurstEngine copy;
  copy.prepare(ctx);
  copy.deserializeState(state);
  copy.onSeed({seed, 200});
  const auto& second = copy.pendingTriggers();
  assert(second.size() == 4);
  assert(second.front() == 200);
  assert(second.back() == 200 + 3 * 120);
}

void test_router_reseed_and_locks() {
  EngineRouter router;
  router.init(EngineRouter::Mode::kSim);
  router.setSeedCount(2);
  router.assignSeed(0, EngineRouter::kEuclidId);
  router.assignSeed(1, EngineRouter::kEuclidId);

  router.reseed(0x00001234u);
  assert(router.euclid().generationSeed() == 0x00001234u);

  router.setSeedLock(0, true);
  router.setSeedLock(1, true);
  router.reseed(0x00005678u);
  assert(router.euclid().generationSeed() == 0x00001234u);

  router.setGlobalLock(true);
  router.reseed(0x00009ABCu);
  assert(router.euclid().generationSeed() == 0x00001234u);

  router.setGlobalLock(false);
  router.setSeedLock(1, false);
  router.reseed(0x00ABCDEFu);
  assert(router.euclid().generationSeed() == 0x00ABCDEFu);
}
}  // namespace

int main() {
  test_euclid_mask();
  test_burst_spacing();
  test_router_reseed_and_locks();
  return 0;
}

