# Euclid + Burst pattern lab — rhythm math without the mystery cult

You’ve seen the zine shout-outs to Euclid masks and Burst sprays. This lab turns
that mythology into a step-by-step studio session: we’ll spin deterministic masks,
log clustered bursts, and line the outputs up with the scheduler snapshots that
drive classroom demos.

## TL;DR

- `EuclidEngine::prepare` seeds the mask using the shared master seed, then
  `onParam` rebuilds it whenever steps, fills, or rotation change. The cursor is
  sample-synced so playback never stutters across reseeds.【F:src/engine/EuclidEngine.cpp†L50-L143】
- `BurstEngine::onSeed` generates timestamped trigger clusters, clamping the count
  to 16 voices and flattening negative spacing to zero so flams collapse on beat.【F:src/engine/BurstEngine.cpp†L36-L97】
- `tests/test_engine/test_euclid_burst.cpp` narrates both engines in code: it
  asserts the mask across rotations, round-trips serialization, and proves burst
  spacing obeys clamps.【F:tests/test_engine/test_euclid_burst.cpp†L65-L197】
- Snapshots captured through `captureSnapshotForEngine` confirm the display layer
  tags Euclid as `ECL` and Burst as `BST`, matching the panel readouts.【F:tests/test_engine/test_euclid_burst.cpp†L179-L187】

## 1. Sketch the Euclid mask

The Euclidean engine encodes pulses as a rotating binary mask. Start with the
helper from the test suite and tweak parameters to hear how rotation shifts the
beat emphasis.

```c++
#include "engine/EuclidEngine.h"

void logMask(std::uint8_t steps, std::uint8_t fills, std::uint8_t rotate) {
  Engine::PrepareContext ctx{};
  ctx.masterSeed = 0x12345678u;

  EuclidEngine engine;
  engine.prepare(ctx);
  engine.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kSteps), steps});
  engine.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kFills), fills});
  engine.onParam({0, static_cast<std::uint16_t>(EuclidEngine::Param::kRotate), rotate});

  const auto &mask = engine.mask();
  for (std::size_t i = 0; i < mask.size(); ++i) {
    engine.onTick({static_cast<std::uint32_t>(i)});
    std::printf("step %zu: gate=%u\n", i, mask[i]);
  }
}
```

`rebuildMask()` clamps steps between 1 and 32, caps fills at the current step
count, and wraps rotation so negative values stay musical.【F:src/engine/EuclidEngine.cpp†L68-L143】
Because `onTick` advances an internal cursor, you can call it once per scheduler
frame in your own harness and mirror exactly what the firmware does on stage.【F:src/engine/EuclidEngine.cpp†L57-L66】

## 2. Stress rotation + serialization

Want deterministic receipts? Run the regression straight from the repo root:

```bash
pio test -e native --filter test_engine/test_euclid_burst.cpp
```

The `test_euclid_mask` case spins three rotations (0, 1, 3) and compares every
slot in the mask, then serializes the engine and restores it to prove the state
buffer keeps cursor + generation seed intact.【F:tests/test_engine/test_euclid_burst.cpp†L68-L106】
This is the fastest way to confirm your new rhythmic preset still lands on the
exact same hits after a reseed.

## 3. Burst clusters without mystery maths

Burst complements Euclid by spraying micro-triggers around a seed’s scheduled
start. The lab fixture creates four equally-spaced hits, then cranks the knobs to
show the safety rails.

```c++
#include "engine/BurstEngine.h"

void inspectBurst(std::uint8_t cluster, std::int32_t spacing) {
  BurstEngine engine;
  Engine::PrepareContext ctx{};
  ctx.masterSeed = 0xCAFEBABEu;
  engine.prepare(ctx);
  engine.onParam({0, static_cast<std::uint16_t>(BurstEngine::Param::kClusterCount), cluster});
  engine.onParam({0, static_cast<std::uint16_t>(BurstEngine::Param::kSpacingSamples), spacing});

  Seed seed{};
  seed.id = 7;
  engine.onSeed({seed, 1000});
  for (auto when : engine.pendingTriggers()) {
    std::printf("trigger @ %u\n", when);
  }
}
```

The engine clamps cluster counts above 16, so even absurd inputs fall back to a
sensible ceiling. Negative spacing turns into zero, collapsing the burst into a
flam — exactly what the test asserts when it forces `-42` samples between hits.【F:src/engine/BurstEngine.cpp†L46-L71】【F:tests/test_engine/test_euclid_burst.cpp†L108-L151】

## 4. Router and display receipts

The engine router test keeps Euclid’s `generationSeed` glued to the last
successful reseed unless a seed unlocks, proving the scheduler honours your lock
choices. It also captures display snapshots for both engines, so you can match
firmware logs to the panel UI during a workshop.【F:tests/test_engine/test_euclid_burst.cpp†L154-L187】

```bash
pio test -e native --filter test_engine/test_euclid_burst.cpp::test_router_reseed_and_locks
```

Peek at the snapshots under `out/` when `ENABLE_GOLDEN=1`; they publish the exact
status strings (`ECL`, `BST`) that the OLED will show in class.【F:tests/test_engine/test_euclid_burst.cpp†L179-L187】

## 5. Spin it up in the golden renderer

If you want to go beyond log output, fire the offline golden renderer. It pumps
both engines through the same code path used for snapshot fixtures and drops the
results into `build/fixtures/euclid-mask.wav` plus `build/fixtures/euclid-mask-control.txt`
when `ENABLE_GOLDEN` is set.【F:tools/native_golden_offline.cpp†L1387-L1437】【F:build/fixtures/euclid-mask-control.txt†L1-L53】

```bash
ENABLE_GOLDEN=1 scripts/offline_native_golden.sh --engine Euclid
```

Now you’ve got documentation-grade artefacts plus the code receipts to prove
exactly how the groove is minted. No mysticism, just deterministic rhythm science.
