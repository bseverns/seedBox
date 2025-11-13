#ifndef ENABLE_GOLDEN
#define ENABLE_GOLDEN 0
#endif

#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Seed.h"
#include "app/AppState.h"
#include "engine/BurstEngine.h"
#include "engine/EngineRouter.h"
#include "engine/EuclidEngine.h"
#include "hal/Board.h"

namespace {
std::vector<std::uint8_t> rotateMask(const std::vector<std::uint8_t>& mask, std::uint8_t amount) {
  if (mask.empty()) {
    return mask;
  }
  std::vector<std::uint8_t> rotated(mask.size());
  const std::size_t len = mask.size();
  for (std::size_t i = 0; i < len; ++i) {
    rotated[(i + amount) % len] = mask[i];
  }
  return rotated;
}

void maybeWriteSnapshot(const char* tag, const AppState::DisplaySnapshot& snap) {
#if ENABLE_GOLDEN
  try {
    std::filesystem::create_directories("artifacts");
    std::ofstream out("artifacts/engine_snapshots.txt", std::ios::app);
    out << "[" << tag << "]\n";
    out << snap.title << "\n";
    out << snap.status << "\n";
    out << snap.metrics << "\n";
    out << snap.nuance << "\n\n";
  } catch (...) {
    // Snapshot capture is soft-fail; teaching artifacts should never break CI.
  }
#else
  (void)tag;
  (void)snap;
#endif
}

AppState::DisplaySnapshot captureSnapshotForEngine(uint8_t engineId) {
  hal::nativeBoardReset();
  AppState app;
  app.initSim();
  app.setSeedEngine(0, engineId);
  for (int i = 0; i < 32; ++i) {
    app.tick();
  }
  AppState::DisplaySnapshot snap{};
  app.captureDisplaySnapshot(snap);
  return snap;
}

// These helpers double as lab notes: each test narrates a deterministic engine
// behaviour so students can follow the maths without needing the full firmware
// running.
void run_euclid_mask() {
  Engine::PrepareContext ctx{};
  ctx.masterSeed = 0x12345678u;

  const std::vector<std::uint8_t> baseMask{1, 0, 0, 1, 0, 1, 0, 0};
  const auto runScenario = [&](std::uint8_t rotate, const std::vector<std::uint8_t>& expected) {
    EuclidEngine engine;
    engine.prepare(ctx);
    engine.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kSteps), 8});
    engine.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kFills), 3});
    engine.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kRotate), rotate});

    const auto& mask = engine.mask();
    assert(mask == expected);
    for (std::size_t i = 0; i < expected.size(); ++i) {
      engine.onTick({i});
      assert(engine.lastGate() == (expected[i] != 0));
    }

    const auto state = engine.serializeState();
    EuclidEngine restored;
    restored.prepare(ctx);
    restored.deserializeState(state);
    assert(restored.mask() == expected);
  };

  runScenario(0, baseMask);
  runScenario(1, rotateMask(baseMask, 1));
  runScenario(3, rotateMask(baseMask, 3));
}

void run_burst_spacing() {
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

  // Clamp checks: absurd cluster counts fall back to the 16 note ceiling and negative spacing
  // resolves to zero so bursts collapse into a flam.
  engine.onParam({0, static_cast<std::uint16_t>(BurstEngine::Param::kClusterCount), 32});
  engine.onSeed({seed, 400});
  const auto& clamped = engine.pendingTriggers();
  assert(clamped.size() == 16);
  assert(clamped.back() == 400 + 15 * 120);

  engine.onParam({0, static_cast<std::uint16_t>(BurstEngine::Param::kSpacingSamples), -42});
  engine.onSeed({seed, 512});
  const auto& collapsed = engine.pendingTriggers();
  assert(collapsed.size() == 16);
  for (auto value : collapsed) {
    assert(value == 512);
  }
}

void run_router_reseed_and_locks() {
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

void run_engine_display_snapshots() {
  const auto euclidSnap = captureSnapshotForEngine(EngineRouter::kEuclidId);
  assert(std::strstr(euclidSnap.status, "ECL") != nullptr);
  maybeWriteSnapshot("Euclid", euclidSnap);

  const auto burstSnap = captureSnapshotForEngine(EngineRouter::kBurstId);
  assert(std::strstr(burstSnap.status, "BST") != nullptr);
  maybeWriteSnapshot("Burst", burstSnap);
}
}  // namespace

void test_euclid_mask() { run_euclid_mask(); }

void test_burst_spacing() { run_burst_spacing(); }

void test_router_reseed_and_locks() { run_router_reseed_and_locks(); }

void test_engine_display_snapshots() { run_engine_display_snapshots(); }

