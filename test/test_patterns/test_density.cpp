/* Each call to PatternScheduler::onTick() stands in for one of the 24 MIDI clock
 * pulses that slice up a beat; these tests are a notebook of how density piles up
 * over those pulses and how the sample-accurate timestamps land.
 *
 * These tests are our density gauntlet: first a sanity check that the density gate even fires,
 * then a fractional-accumulation reality check, and finally a sample-timeline alignment test so
 * future hackers can prove the groove math is honest. If you want the full narrative arc, cruise
 * over to docs/roadmaps/granular.md for the broader design story.
 *
 * The helper structs here are intentionally scrappy mirrors of the PatternScheduler internals, so
 * students can map each assertion back to the scheduler flow in src/engine/Patterns.cpp without
 * needing a decoder ring.
 */
#ifndef ENABLE_GOLDEN
#define ENABLE_GOLDEN 1
#endif

#include <unity.h>
#include <cmath>
#include <vector>
#include "engine/Patterns.h"
#include "app/Clock.h"
#include "Seed.h"
#include "util/Units.h"

namespace {
struct Counter {
  int calls{0};
};

void triggerCounter(void* ctx, const Seed&, uint32_t) {
  if (!ctx) return;
  reinterpret_cast<Counter*>(ctx)->calls++;
}

struct TriggerCapture {
  int calls{0};
  uint32_t lastStart{0};
};

void captureStartSample(void* ctx, const Seed&, uint32_t startSample) {
  if (!ctx) return;
  auto* capture = reinterpret_cast<TriggerCapture*>(ctx);
  capture->calls++;
  capture->lastStart = startSample;
}

struct TimelineCapture {
  std::vector<uint32_t> starts;
};

void recordStartSamples(void* ctx, const Seed&, uint32_t startSample) {
  if (!ctx) return;
  auto* capture = reinterpret_cast<TimelineCapture*>(ctx);
  capture->starts.push_back(startSample);
}
}

void test_density_gate_runs() {
  PatternScheduler ps; ps.setBpm(120.f);
  Counter counter;
  ps.setTriggerCallback(&counter, triggerCounter);
  Seed s{}; s.density = 2.0f; s.probability = 1.0f; s.jitterMs = 0.f;
  ps.addSeed(s);
  // 24 ticks per beat means ~5.33 beats for 128 ticks; we're just smashing
  // through enough clock edges to make sure nothing explodes.
  for (int i=0;i<128;++i) ps.onTick();
  TEST_ASSERT_TRUE(ps.ticks() == 128);
  TEST_ASSERT_TRUE(counter.calls > 0);
}

void test_density_fractional_counts() {
  PatternScheduler ps;
  Counter counter;
  ps.setTriggerCallback(&counter, triggerCounter);

  Seed s{};
  s.density = 1.5f;
  s.probability = 1.0f;
  s.jitterMs = 0.f;
  ps.addSeed(s);

  const int ticksToSimulate = 24 * 16; // 24 ticks/beat * 16 beats = 384 onTick() calls
  for (int i = 0; i < ticksToSimulate; ++i) {
    ps.onTick();
  }

  const int expectedHits = static_cast<int>(16 * s.density);
  TEST_ASSERT_EQUAL_INT(expectedHits, counter.calls);
}

void test_scheduler_counts_silent_ticks() {
  PatternScheduler ps;
  TriggerCapture capture;
  ps.setTriggerCallback(&capture, captureStartSample);

  Seed s{};
  s.density = 1.f;
  s.probability = 1.f;
  s.jitterMs = 0.f;
  ps.addSeed(s);

  const int ticksPerBeat = 24;
  for (int i = 0; i < ticksPerBeat - 1; ++i) {
    ps.onTick();
  }
  TEST_ASSERT_EQUAL_INT(0, capture.calls);

  ps.onTick();
  TEST_ASSERT_EQUAL_INT(1, capture.calls);

  const float bpm = 120.f;
  const double samplesPerBeat = (60.0 / bpm) * static_cast<double>(Units::kSampleRate);
  const uint32_t expectedSamples = static_cast<uint32_t>(std::llround(samplesPerBeat));
  // Units::kSimTickSamples advances the virtual transport every tick, even
  // when no seed fires, so the first hit sits exactly one beat of samples in.
  TEST_ASSERT_EQUAL_UINT32(expectedSamples, capture.lastStart);
}

void test_scheduler_bpm_modulates_when_samples() {
  auto runScenario = [](float bpm) {
    InternalClock clock;
    PatternScheduler ps(&clock);
    ps.setBpm(bpm);
    TimelineCapture capture;
    ps.setTriggerCallback(&capture, recordStartSamples);

    Seed s{};
    s.density = 1.f;
    s.probability = 0.5f;
    s.jitterMs = 0.f;
    s.prng = 0xCAFEBABEu;
    ps.addSeed(s);

    const int ticksToSimulate = 24 * 32; // 32 beats keeps the RNG story stable.
    for (int i = 0; i < ticksToSimulate; ++i) {
      clock.onTick();
    }
    return capture;
  };

  const float slowBpm = 90.f;
  const float fastBpm = 180.f;
  const TimelineCapture slow = runScenario(slowBpm);
  const TimelineCapture fast = runScenario(fastBpm);

  TEST_ASSERT_EQUAL(slow.starts.size(), fast.starts.size());
  TEST_ASSERT_TRUE(slow.starts.size() > 1);

  const auto slowDelta = slow.starts[1] - slow.starts[0];
  const auto fastDelta = fast.starts[1] - fast.starts[0];
  TEST_ASSERT_NOT_EQUAL(slowDelta, fastDelta);

  const double slowBeatSamples = (60.0 / slowBpm) * static_cast<double>(Units::kSampleRate);
  const double fastBeatSamples = (60.0 / fastBpm) * static_cast<double>(Units::kSampleRate);
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(std::llround(slowBeatSamples)), slowDelta);
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(std::llround(fastBeatSamples)), fastDelta);
}

void test_clock_tick_log_tracks_swing() {
  {
    InternalClock clock;
    PatternScheduler straight(&clock);
    straight.clearTickLog();
    const int ticks = 4;
    for (int i = 0; i < ticks; ++i) {
      clock.onTick();
    }
    const auto& log = straight.tickLog();
    TEST_ASSERT_EQUAL_UINT32(ticks, log.size());
    TEST_ASSERT_EQUAL_UINT32(1000u, log[0]);
    for (std::size_t i = 1; i < log.size(); ++i) {
      TEST_ASSERT_EQUAL_UINT32(1000u, log[i] - log[i - 1]);
    }
  }

  {
    InternalClock clock;
    clock.setSwing(0.3f);
    PatternScheduler swung(&clock);
    swung.clearTickLog();
    const int ticks = 24;
    for (int i = 0; i < ticks; ++i) {
      clock.onTick();
    }
    const auto& log = swung.tickLog();
    TEST_ASSERT_EQUAL_UINT32(ticks, log.size());
    TEST_ASSERT_EQUAL_UINT32(1100u, log[0]);
    for (std::size_t i = 1; i < 12 && i < log.size(); ++i) {
      TEST_ASSERT_EQUAL_UINT32(1100u, log[i] - log[i - 1]);
    }
    for (std::size_t i = 12; i < log.size(); ++i) {
      TEST_ASSERT_EQUAL_UINT32(900u, log[i] - log[i - 1]);
    }
  }
}
