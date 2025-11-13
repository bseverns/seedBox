#ifndef ENABLE_GOLDEN
#define ENABLE_GOLDEN 1
#endif

#include <unity.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "app/Clock.h"
#include "engine/Patterns.h"
#include "util/Units.h"

namespace {

struct Scenario {
  float bpm;
  float swing;
  const char* tag;
};

std::vector<std::uint32_t> computeExpectedTickLog(float bpm, float swing, std::size_t ticks) {
  const float safeBpm = bpm > 0.f ? bpm : 1.f;
  const double beatsPerSecond = static_cast<double>(safeBpm) / 60.0;
  const double ticksPerSecond = beatsPerSecond * 24.0;
  const double baseSamplesPerTick = static_cast<double>(Units::kSampleRate) / ticksPerSecond;
  std::vector<std::uint32_t> out;
  out.reserve(ticks);
  double cursor = 0.0;
  for (std::size_t i = 0; i < ticks; ++i) {
    double interval = baseSamplesPerTick;
    if (swing > 0.f) {
      const double nudge = baseSamplesPerTick * static_cast<double>(swing) / 3.0;
      const bool backbeat = (i % 24U) >= 12U;
      interval += backbeat ? -nudge : nudge;
    }
    if (interval < 1.0) {
      interval = 1.0;
    }
    cursor += interval;
    out.push_back(static_cast<std::uint32_t>(std::llround(cursor)));
  }
  return out;
}

void maybeWriteGolden(const Scenario& scenario, const std::vector<std::uint32_t>& log) {
#if ENABLE_GOLDEN
  try {
    std::filesystem::create_directories("artifacts");
    std::ostringstream name;
    const int swingPercent = static_cast<int>(std::round(scenario.swing * 100.0f));
    name << "artifacts/pattern_ticks_bpm" << static_cast<int>(scenario.bpm)
         << "_swing" << swingPercent << ".txt";
    std::ofstream out(name.str());
    out << "# Tick log for " << scenario.tag << "\n";
    for (const auto value : log) {
      out << value << "\n";
    }
  } catch (...) {
    // Golden capture is a best-effort accessory; failures shouldn't sink the test run.
  }
#endif
}

}  // namespace

void test_clock_tick_log_golden() {
  const Scenario scenarios[] = {
      {60.f, 0.f, "60 BPM, straight"},
      {60.f, 0.54f, "60 BPM, swing 54%"},
      {60.f, 0.62f, "60 BPM, swing 62%"},
      {90.f, 0.f, "90 BPM, straight"},
      {90.f, 0.54f, "90 BPM, swing 54%"},
      {90.f, 0.62f, "90 BPM, swing 62%"},
      {120.f, 0.f, "120 BPM, straight"},
      {120.f, 0.54f, "120 BPM, swing 54%"},
      {120.f, 0.62f, "120 BPM, swing 62%"},
  };

  for (const auto& scenario : scenarios) {
    InternalClock clock;
    PatternScheduler scheduler(&clock);
    clock.setBpm(scenario.bpm);
    clock.setSwing(scenario.swing);
    scheduler.setBpm(scenario.bpm);
    scheduler.clearTickLog();

    const std::size_t tickCount = 24U;
    std::vector<std::uint32_t> captured;
    captured.reserve(tickCount);
    for (std::size_t i = 0; i < tickCount; ++i) {
      clock.onTick();
      captured.push_back(scheduler.nowSamples());
    }

    TEST_ASSERT_EQUAL_UINT32(tickCount, captured.size());
    const auto expected = computeExpectedTickLog(scenario.bpm, scenario.swing, tickCount);
    for (std::size_t i = 0; i < tickCount; ++i) {
      TEST_ASSERT_EQUAL_UINT32(expected[i], captured[i]);
    }
    maybeWriteGolden(scenario, captured);
  }
}
