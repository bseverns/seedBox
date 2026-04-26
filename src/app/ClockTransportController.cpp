#include "app/ClockTransportController.h"

#include <algorithm>

#include "engine/Patterns.h"

namespace {
void alignProviderRunning(ClockProvider* clock, InternalClock& internal, MidiClockIn& midiIn,
                          MidiClockOut& midiOut, bool externalRunning) {
  if (!clock) {
    internal.stopTransport();
    midiIn.stopTransport();
    midiOut.stopTransport();
    return;
  }
  if (clock == &internal) {
    internal.startTransport();
    midiIn.stopTransport();
  } else {
    internal.stopTransport();
    if (externalRunning) {
      midiIn.startTransport();
    } else {
      midiIn.stopTransport();
    }
  }
  if (clock == &midiOut) {
    midiOut.startTransport();
  } else if (clock == &internal || externalRunning) {
    midiOut.startTransport();
  } else {
    midiOut.stopTransport();
  }
}
}  // namespace

ClockTransportController::ClockTransportController(InternalClock& internalClock, MidiClockIn& midiClockIn,
                                                   MidiClockOut& midiClockOut, PatternScheduler& scheduler)
    : internalClock_(internalClock), midiClockIn_(midiClockIn), midiClockOut_(midiClockOut), scheduler_(scheduler) {
  selectClockProvider(&internalClock_);
}

void ClockTransportController::updateClockDominance() {
  externalClockDominant_ = followExternalClockEnabled_ || externalTransportRunning_;
}

void ClockTransportController::applySwing(float swing) {
  swing_ = std::clamp(swing, 0.0f, 0.99f);
  internalClock_.setSwing(swing_);
  midiClockOut_.setSwing(swing_);
  if (clock_) {
    clock_->setSwing(swing_);
  }
}

void ClockTransportController::alignProviderRunning() {
  ::alignProviderRunning(clock_, internalClock_, midiClockIn_, midiClockOut_, externalTransportRunning_);
}

void ClockTransportController::selectClockProvider(ClockProvider* provider) {
  ClockProvider* target = provider ? provider : &internalClock_;
  ClockProvider* previous = clock_;

  if (previous && previous != target) {
    previous->stopTransport();
  }

  clock_ = target;

  internalClock_.attachScheduler(&scheduler_);
  midiClockIn_.attachScheduler(&scheduler_);
  midiClockOut_.attachScheduler(&scheduler_);
  scheduler_.setClockProvider(clock_);

  if (clock_) {
    clock_->setBpm(scheduler_.bpm());
  }

  alignProviderRunning();
  applySwing(swing_);
}

void ClockTransportController::toggleClockProvider() {
  const bool useExternal = (clock_ != &midiClockIn_);
  if (useExternal) {
    selectClockProvider(&midiClockIn_);
    followExternalClockEnabled_ = true;
  } else {
    selectClockProvider(&internalClock_);
    followExternalClockEnabled_ = false;
  }
  updateClockDominance();
}

void ClockTransportController::resetTransportState() {
  externalTransportRunning_ = false;
  transportLatchedRunning_ = false;
  transportGateHeld_ = false;
  updateClockDominance();
}

void ClockTransportController::clearTransportGateHold() { transportGateHeld_ = false; }

void ClockTransportController::onExternalClockTick(uint32_t nowMs) {
  lastExternalClockMs_ = nowMs;
  waitingForExternalClock_ = false;
  externalClockWaitStartMs_ = 0u;
  externalTransportRunning_ = true;
  alignProviderRunning();
  updateClockDominance();
}

void ClockTransportController::onExternalTransportStart() {
  externalTransportRunning_ = true;
  selectClockProvider(&midiClockIn_);
  updateClockDominance();
  if (transportLatchEnabled_) {
    transportLatchedRunning_ = true;
  }
}

void ClockTransportController::onExternalTransportStop() {
  externalTransportRunning_ = false;
  if (!followExternalClockEnabled_) {
    selectClockProvider(&internalClock_);
  } else {
    alignProviderRunning();
  }
  updateClockDominance();
  if (transportLatchEnabled_) {
    transportLatchedRunning_ = false;
  }
}

void ClockTransportController::setTransportLatchEnabled(bool enabled) {
  if (transportLatchEnabled_ == enabled) {
    return;
  }
  transportLatchEnabled_ = enabled;
  if (transportLatchEnabled_) {
    transportLatchedRunning_ = externalTransportRunning_;
  } else {
    transportLatchedRunning_ = false;
    transportGateHeld_ = false;
  }
}

void ClockTransportController::refreshTransportLatchState() {
  if (transportLatchEnabled_) {
    transportLatchedRunning_ = externalTransportRunning_;
  }
}

void ClockTransportController::toggleTransportLatchedRunning() {
  transportLatchedRunning_ = !transportLatchedRunning_;
}

void ClockTransportController::setFollowExternalClockEnabled(bool enabled, uint32_t nowMs) {
  if (followExternalClockEnabled_ == enabled) {
    return;
  }
  followExternalClockEnabled_ = enabled;
  selectClockProvider(enabled ? static_cast<ClockProvider*>(&midiClockIn_)
                              : static_cast<ClockProvider*>(&internalClock_));
  updateClockDominance();
  externalClockWaitStartMs_ = enabled ? nowMs : 0u;
  waitingForExternalClock_ = false;
  lastExternalClockMs_ = 0u;
}

void ClockTransportController::setClockSourceExternal(bool external, uint32_t nowMs) {
  selectClockProvider(external ? static_cast<ClockProvider*>(&midiClockIn_)
                              : static_cast<ClockProvider*>(&internalClock_));
  followExternalClockEnabled_ = external;
  updateClockDominance();
  externalClockWaitStartMs_ = external ? nowMs : 0u;
  waitingForExternalClock_ = false;
  lastExternalClockMs_ = 0u;
}

void ClockTransportController::applyPresetClockState(bool followExternalClockEnabled, bool transportLatchEnabled,
                                                     bool externalTransportRunning) {
  followExternalClockEnabled_ = followExternalClockEnabled;
  const bool previousLatch = transportLatchEnabled_;
  transportLatchEnabled_ = transportLatchEnabled;
  if (!transportLatchEnabled_) {
    transportLatchedRunning_ = false;
    transportGateHeld_ = false;
  } else if (!previousLatch) {
    transportLatchedRunning_ = externalTransportRunning;
  }
  externalTransportRunning_ = externalTransportRunning;
  updateClockDominance();
}

void ClockTransportController::updateExternalClockWatchdog(uint32_t nowMs, uint32_t timeoutMs) {
  if (!followExternalClockEnabled_) {
    waitingForExternalClock_ = false;
    externalClockWaitStartMs_ = 0u;
    return;
  }

  if (lastExternalClockMs_ == 0u) {
    if (externalClockWaitStartMs_ == 0u) {
      externalClockWaitStartMs_ = nowMs;
    }
    waitingForExternalClock_ = (nowMs - externalClockWaitStartMs_) >= timeoutMs;
    if (waitingForExternalClock_) {
      followExternalClockEnabled_ = false;
      selectClockProvider(&internalClock_);
      updateClockDominance();
      waitingForExternalClock_ = false;
      externalClockWaitStartMs_ = 0u;
      lastExternalClockMs_ = 0u;
    }
    return;
  }

  waitingForExternalClock_ = (nowMs - lastExternalClockMs_) >= timeoutMs;
  if (waitingForExternalClock_) {
    followExternalClockEnabled_ = false;
    selectClockProvider(&internalClock_);
    updateClockDominance();
    waitingForExternalClock_ = false;
    externalClockWaitStartMs_ = 0u;
    lastExternalClockMs_ = 0u;
  }
}

void ClockTransportController::handleTransportGate(uint8_t value) {
  const bool gateHigh = value > 0;
  if (transportLatchEnabled_) {
    if (gateHigh && !transportGateHeld_) {
      transportGateHeld_ = true;
      if (transportLatchedRunning_) {
        onExternalTransportStop();
      } else {
        onExternalTransportStart();
      }
    } else if (!gateHigh) {
      transportGateHeld_ = false;
    }
    return;
  }

  if (gateHigh) {
    onExternalTransportStart();
  } else {
    onExternalTransportStop();
  }
}
