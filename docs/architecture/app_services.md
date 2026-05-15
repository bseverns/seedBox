# App Services

This note is the field guide for the post-`AppState` split. It does not claim
the current seams are final. It explains which extractions are already stable,
which ones are still transitional, and where host/JUCE code is allowed to touch
the runtime.

## Purpose

- keep the refactor teachable
- give contributors a shared vocabulary for the extracted classes
- separate "stable ownership" from "useful transitional seam"
- make the JUCE and hardware boundaries easier to reason about

## Service Categories

### Pure-ish logic units

These classes mostly transform inputs into outputs, labels, or bounded state.
They should stay easy to test in isolation and should avoid reaching deep into
the rest of the machine.

| Class | Role | Owns long-lived runtime state? | Notes |
| --- | --- | --- | --- |
| `SeedPrimeController` | builds the prime-mode seed stories (`LFSR`, tap lineage, preset, live input) | no | the cleanest "logic-only" slice in the app lane |
| `TapTempoTracker` | tap-interval history, BPM inference, and pending-tap bookkeeping | yes, local only | stateful, but still narrow and deterministic |
| `StatusSnapshotBuilder` | machine-readable status payload + JSON serialization | no | pure-ish formatter/builder |
| `DisplaySnapshotBuilder` | OLED/debug display frame assembly | no | pure-ish presenter with lookup helpers |

### State owners

These classes own mutable runtime state that persists across frames or audio
blocks. They are not mere helpers; they are the place where that state lives.

| Class | Role | Why it is a state owner |
| --- | --- | --- |
| `AudioRuntimeState` | host-audio flags, limiter state, test-tone phase, callback count | state must survive every audio block |
| `ClockTransportController` | transport latch, clock-provider ownership, external-clock watchdog state | time ownership has to persist between ticks |
| `InputGateMonitor` | dry-input view, RMS/peak probe, gate-edge state | owns the gate envelope state, but now only borrows the host dry-input span for the current block |
| `PresetController` | active preset slot, queueing, crossfade bookkeeping | owns the preset-transition envelope |
| `SeedLock` | per-seed and global lock truth | lock policy must outlive individual gestures |

### Transitional friend services

These classes exist to take pressure off `AppState` without pretending the
runtime is fully data-oriented yet. They usually operate as "friend services":
small orchestration slices that still reach into `AppState` internals.

| Class | Role | Why it is transitional |
| --- | --- | --- |
| `ModeEventRouter` | page/mode grammar for front-panel gestures | still expressed directly in terms of `AppState` fields |
| `Mn42ControlRouter` | external CC routing for the MN-42 vocabulary | policy is coherent, but not yet independent of `AppState` storage |
| `HostControlService` | host/editor commands into the runtime | useful seam now; later could narrow around explicit host-facing state |
| `SeedMutationService` | focused-seed edits, engine swaps, granular source cycling | centralizes mutation policy but still reaches raw tables |
| `GateQuantizeService` | input-gate reseed timing + quantize mutations | grouped because both affect scheduler grid state |
| `SeedPrimeRuntimeService` | reseed requests, preset/live lineage, seed rebuild orchestration | still deeply coupled to the scheduler + seed table |
| `AppUiClockService` | clock/provider toggles and UI-facing clock state | extracted mostly to reduce `AppState.cpp` weather |
| `SeedLockService` | lock toggles and lock queries | thin policy seam over `SeedLock` |
| `PresetStorageService` | preset save/recall/page storage flow | still coupled to `AppState` page/preset shell |
| `DisplayTelemetryService` | display snapshot and learn-frame assembly | orchestration layer over cached runtime state |
| `PresetTransitionRunner` | queued preset apply + crossfade stepping | important seam, but still lives as a friend-driven runtime slice |
| `HostAudioRealtimeService` | callback-safe JUCE heartbeat | extracted to keep host audio timing work out of the larger `AppState.cpp` weather system |

### Host boundary helpers

These classes are the DAW/desktop boundary. They should translate host concepts
into the same runtime hooks the panel and simulator would use, and they should
be the first place we audit for real-time safety.

| Class | Role | Boundary concern |
| --- | --- | --- |
| `HostAudioThreadAccess` | JUCE-side typed view for callback-safe app hooks | keeps audio callbacks on a narrower surface than raw `AppState` |
| `HostReadThreadAccess` | JUCE-side typed view for cached snapshot/state reads | keeps editor/panel polling on an explicit read surface and lets UI consume `displayCache_` without reaching through broad `AppState` |
| `HostControlThreadAccess` | JUCE-side typed view for timers, editor actions, and host parameter changes | keeps non-audio host mutations on an explicit surface |
| `HostControlBridge` | JUCE parameter and transport bridge into `AppState` | host-thread mutation policy, transport sync, and separation from audio-thread work |
| `SeedboxAudioProcessor` | plugin `processBlock`, state serialization, scratch buffers | audio-thread boundary and host persistence |
| `JuceHost` | standalone device-manager callback path | audio-device callback safety and host MIDI handoff |

## Core Runtime Spine

The current runtime spine is:

1. `AppState` owns the persistent instrument state and still acts as the main
   public facade.
2. state owners (`ClockTransportController`, `AudioRuntimeState`,
   `InputGateMonitor`, `PresetController`, `SeedLock`) keep the long-lived
   mutable truth.
3. transitional friend services pull one policy seam at a time out of
   `AppState.cpp`.
4. `EngineRouter` remains the engine registry/dispatch point beneath the app
   layer.
5. JUCE helpers (`HostAudioThreadAccess`, `HostReadThreadAccess`, `HostControlThreadAccess`,
   `HostControlBridge`, `SeedboxAudioProcessor`, `JuceHost`) are boundary
   adapters, not alternate runtime models.

The generated service graph in
[`app_services.mmd`](/Users/bseverns/Documents/GitHub/seedbox/docs/architecture/app_services.mmd)
shows the current dependency picture.

## Host / JUCE Boundary

The most important architectural caveat right now is that "control-rate" in the
repo still spans two different JUCE execution sites:

- `SeedboxAudioProcessor::processBlock` now reaches `AppState` through
  `HostAudioThreadAccess`.
- `JuceHost::audioDeviceIOCallbackWithContext` does the same in standalone.
- JUCE editor/panel reads now go through `HostReadThreadAccess`, including the
  cached display snapshot and dirty-flag surface.
- `HostAudioThreadAccess` now has a dedicated transport-sync BPM hook, so host
  position updates do not reuse the UI/control BPM path.
- a processor-owned JUCE timer now reaches maintenance work through
  `HostControlThreadAccess` in the plugin lane, even when no editor is open.
- `JuceHost` now owns the same kind of non-audio maintenance timer in the
  standalone shim.
- `HostControlBridge` now routes a small fixed-size deferred set through that
  maintenance path so master-seed reseeds, focused-engine swaps, quantize,
  live-input gate settings, and granular-source stepping do not need to finish
  inline with every host parameter callback.
- `InputGateMonitor::setDryInput` now borrows the current host block instead of
  owning copied audio, but it still executes on the audio callback path.
- JUCE OLED/debug text now consumes `displayCache_` and only rebuilds those
  text frames when `displayDirty_` is set; live learn-frame meters still update
  on the UI timer.

That split is materially better than the old "everything rides through
`tick()`" model, but it is not the final state:

- plugin audio callbacks now avoid the heaviest UI / storage / panel work
- standalone callbacks no longer lock in the JUCE MIDI backend
- plugin MIDI ingress no longer uses per-block heap growth
- `tickHostAudio()` is still broader than a strict RT-minimal contract

The first-pass audit lives in
[`juce_rt_audit.md`](/Users/bseverns/Documents/GitHub/seedbox/docs/architecture/juce_rt_audit.md).

## Refactor Status

What is already strong:

- the extracted seams are real and test-backed
- `AppState.cpp` is materially smaller than before the split
- the teaching comments survived the breakup

What is not final yet:

- friend-service seams are still intentionally pragmatic
- JUCE audio-thread boundaries are not yet hardened
- service categories are descriptive, not prescriptive law

## Known Transitional Seams

These are the areas most likely to keep moving:

- `PresetTransitionRunner`
- `SeedPrimeRuntimeService`
- `HostControlService`
- `DisplayTelemetryService`
- the JUCE split between `tickHostAudio()` and `serviceHostMaintenance()`
- the remaining thread-contract cleanup in `SeedboxAudioProcessor`

The rule of thumb is simple: prefer a useful seam that teaches the system over
an abstract seam that hides it.
