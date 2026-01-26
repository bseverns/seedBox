# Diagnostics counters

Diagnostics are opt-in. They only increment when `setDiagnosticsEnabledFromHost(true)` is called.

## Scheduler counters

Source: `src/engine/Patterns.h` / `src/engine/Patterns.cpp`

- `immediateQueueOverflows`: `triggerImmediate()` hit the max queue size and dropped an event.
- `quantizedQueueOverflows`: a scheduled trigger could not be queued during `onTick()`.
- `schedulingLag`: the sampled tick interval exceeded 1.5x the expected cadence (sim builds).
- `missedTicks`: the sampled tick interval exceeded 2.5x the expected cadence (sim builds).

## Audio counters

Source: `src/app/AppState.cpp`

- `audioCallbackCount`: how many audio callbacks have run since boot.

## CPU load snapshots

Not currently available in the core firmware. If you add a HAL-specific CPU load
probe, thread it into `AppState::DiagnosticsSnapshot` and document it here.

## Enable/disable

- Enable: `AppState::setDiagnosticsEnabledFromHost(true)`
- Disable: `AppState::setDiagnosticsEnabledFromHost(false)`

No diagnostics are printed by default; consumers are expected to poll the
snapshot when enabled.
