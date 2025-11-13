#ifndef ENABLE_GOLDEN
#define ENABLE_GOLDEN 0
#endif

#include <unity.h>
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
    rotated[(i + len - (amount % len)) % len] = mask[i];
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

  // The engine seeds pulses by comparing quantised divisions of the fill range,
  // so the unrotated mask starts on the third step rather than index zero.
  const std::vector<std::uint8_t> baseMask{0, 0, 1, 0, 0, 1, 0, 1};
  const auto runScenario = [&](std::uint8_t rotate, const std::vector<std::uint8_t>& expected) {
    EuclidEngine engine;
    engine.prepare(ctx);
    engine.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kSteps), 8});
    engine.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kFills), 3});
    engine.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kRotate), rotate});

    const auto& mask = engine.mask();
    TEST_ASSERT_EQUAL_UINT32(expected.size(), mask.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
      TEST_ASSERT_EQUAL_UINT8_MESSAGE(expected[i], mask[i], "Euclid mask mismatch");
    }
    for (std::size_t i = 0; i < expected.size(); ++i) {
      engine.onTick({i});
      TEST_ASSERT_EQUAL_INT_MESSAGE(expected[i] != 0, engine.lastGate(), "Euclid gate mismatch");
    }

    const auto state = engine.serializeState();
    EuclidEngine restored;
    restored.prepare(ctx);
    restored.deserializeState(state);
    const auto& restoredMask = restored.mask();
    TEST_ASSERT_EQUAL_UINT32(expected.size(), restoredMask.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
      TEST_ASSERT_EQUAL_UINT8_MESSAGE(expected[i], restoredMask[i], "Euclid restore mismatch");
    }
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
  TEST_ASSERT_EQUAL_UINT32(4, triggers.size());
  const std::vector<std::uint32_t> expected{1000, 1120, 1240, 1360};
  for (std::size_t i = 0; i < expected.size(); ++i) {
    TEST_ASSERT_EQUAL_UINT32(expected[i], triggers[i]);
  }

  const auto state = engine.serializeState();
  BurstEngine copy;
  copy.prepare(ctx);
  copy.deserializeState(state);
  copy.onSeed({seed, 200});
  const auto& second = copy.pendingTriggers();
  TEST_ASSERT_EQUAL_UINT32(4, second.size());
  TEST_ASSERT_EQUAL_UINT32(200, second.front());
  TEST_ASSERT_EQUAL_UINT32(200 + 3 * 120, second.back());

  // Clamp checks: absurd cluster counts fall back to the 16 note ceiling and negative spacing
  // resolves to zero so bursts collapse into a flam.
  engine.onParam({0, static_cast<std::uint16_t>(BurstEngine::Param::kClusterCount), 32});
  engine.onSeed({seed, 400});
  const auto& clamped = engine.pendingTriggers();
  TEST_ASSERT_EQUAL_UINT32(16, clamped.size());
  TEST_ASSERT_EQUAL_UINT32(400 + 15 * 120, clamped.back());

  engine.onParam({0, static_cast<std::uint16_t>(BurstEngine::Param::kSpacingSamples), -42});
  engine.onSeed({seed, 512});
  const auto& collapsed = engine.pendingTriggers();
  TEST_ASSERT_EQUAL_UINT32(16, collapsed.size());
  for (auto value : collapsed) {
    TEST_ASSERT_EQUAL_UINT32(512, value);
  }
}

void run_router_reseed_and_locks() {
  EngineRouter router;
  router.init(EngineRouter::Mode::kSim);
  router.setSeedCount(2);
  router.assignSeed(0, EngineRouter::kEuclidId);
  router.assignSeed(1, EngineRouter::kEuclidId);

  router.reseed(0x00001234u);
  TEST_ASSERT_EQUAL_UINT32(0x00001234u, router.euclid().generationSeed());

  router.setSeedLock(0, true);
  router.setSeedLock(1, true);
  router.reseed(0x00005678u);
  TEST_ASSERT_EQUAL_UINT32(0x00001234u, router.euclid().generationSeed());

  router.setGlobalLock(true);
  router.reseed(0x00009ABCu);
  TEST_ASSERT_EQUAL_UINT32(0x00001234u, router.euclid().generationSeed());

  router.setGlobalLock(false);
  router.setSeedLock(1, false);
  router.reseed(0x00ABCDEFu);
  TEST_ASSERT_EQUAL_UINT32(0x00ABCDEFu, router.euclid().generationSeed());
}

void run_engine_display_snapshots() {
  const auto euclidSnap = captureSnapshotForEngine(EngineRouter::kEuclidId);
  TEST_ASSERT_NOT_NULL_MESSAGE(std::strstr(euclidSnap.status, "ECL"), "Euclid snapshot missing ECL tag");
  maybeWriteSnapshot("Euclid", euclidSnap);

  const auto burstSnap = captureSnapshotForEngine(EngineRouter::kBurstId);
  TEST_ASSERT_NOT_NULL_MESSAGE(std::strstr(burstSnap.status, "BST"), "Burst snapshot missing BST tag");
  maybeWriteSnapshot("Burst", burstSnap);
}
}  // namespace

void test_euclid_mask() { run_euclid_mask(); }

void test_burst_spacing() { run_burst_spacing(); }

void test_router_reseed_and_locks() { run_router_reseed_and_locks(); }

void test_engine_display_snapshots() { run_engine_display_snapshots(); }

