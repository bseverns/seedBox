# Diagnostics counters

Core scheduler diagnostics are opt-in. JUCE host-boundary safety counters are
always collected, then mirrored into `AppState::DiagnosticsSnapshot` on the
non-audio maintenance path so plugin, standalone, and future debug surfaces can
read one shared snapshot.

## Scheduler counters

Source: `src/engine/Patterns.h` / `src/engine/Patterns.cpp`

- `immediateQueueOverflows`: `triggerImmediate()` hit the max queue size and dropped an event.
- `quantizedQueueOverflows`: a scheduled trigger could not be queued during `onTick()`.
- `schedulingLag`: the sampled tick interval exceeded 1.5x the expected cadence (sim builds).
- `missedTicks`: the sampled tick interval exceeded 2.5x the expected cadence (sim builds).

## Audio counters

Source: `src/app/AppState.cpp`

- `audioCallbackCount`: how many audio callbacks have run since boot.

## Host boundary counters

Source:
- `src/juce/SeedboxAudioProcessor.cpp`
- `src/juce/JuceHost.cpp`
- mirrored into `AppState::DiagnosticsSnapshot::host`

- `midiDroppedCount`: MIDI messages dropped because the bounded JUCE ingress
  queue filled before the callback could drain it.
- `oversizeBlockDropCount`: audio blocks dropped because the host delivered
  more frames than the preallocated scratch space could safely handle on the
  callback thread.
- `lastOversizeBlockFrames`: frame count of the most recent fail-closed
  oversize audio block.
- `preparedScratchFrames`: currently prepared JUCE scratch-buffer capacity.

## CPU load snapshots

Not currently available in the core firmware. If you add a HAL-specific CPU load
probe, thread it into `AppState::DiagnosticsSnapshot` and document it here.

## Enable/disable

- Enable: `AppState::setDiagnosticsEnabledFromHost(true)`
- Disable: `AppState::setDiagnosticsEnabledFromHost(false)`

Scheduler diagnostics only advance while enabled. Host boundary counters are
always available because they represent fail-closed callback hazards rather than
normal musical telemetry. No diagnostics are printed by default; consumers are
expected to poll the snapshot when needed.
