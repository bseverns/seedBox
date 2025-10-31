#include "app/Clock.h"
#include <algorithm>
#include <cmath>
#include "engine/Patterns.h"

void ClockProvider::dispatchTick() {
  if (scheduler_) {
    scheduler_->onTick();
  }
}

void ClockProvider::onTick() {
  if (!running_) {
    return;
  }
  dispatchTick();
}

double ClockProvider::swingNudgeSamples(uint64_t, double) const { return 0.0; }

void InternalClock::onTick() {
  ClockProvider::onTick();
}

double InternalClock::swingNudgeSamples(uint64_t tickCount, double baseSamplesPerTick) const {
  if (swing_ <= 0.f) {
    return 0.0;
  }
  const double nudge = baseSamplesPerTick * static_cast<double>(std::clamp(swing_, 0.f, 1.f)) / 3.0;
  const uint32_t tickWithinBeat = static_cast<uint32_t>(tickCount % 24ULL);
  const bool isBackbeat = (tickWithinBeat >= 12U);
  return isBackbeat ? -nudge : nudge;
}

void MidiClockIn::onTick() {
  if (!running_) {
    return;
  }
  dispatchTick();
}

void MidiClockOut::onTick() {
  ClockProvider::onTick();
}

