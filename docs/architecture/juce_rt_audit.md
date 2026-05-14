# JUCE Real-Time Audit

This is the first-pass audit of the JUCE audio boundary. It is intentionally
pragmatic: identify what is currently reached from the host audio callback,
mark the obvious real-time hazards, and define the next cleanup steps.

## Scope

Audio-thread entry points inspected here:

- `SeedboxAudioProcessor::processBlock`
- `JuceHost::audioDeviceIOCallbackWithContext`

Non-audio-thread path tracked separately:

- `SeedboxAudioProcessor::parameterChanged`
- `HostControlBridge::handleParameterChange`

That distinction matters. In the firmware mental model, `tick()` is
"control-rate." In the JUCE lane, the runtime is now split between
`tickHostAudio()` on the callback and `serviceHostMaintenance()` off the audio
thread, so control-rate work is no longer one undifferentiated bucket.

## Direct Audio-Thread Path

### `SeedboxAudioProcessor::processBlock`

Primary path:

1. stage host input into `inputScratch_`
2. call `app_.setDryInputFromHost(...)`
3. clear/reuse preallocated `renderScratch_`
4. sync host transport
5. apply controller MIDI to `AppState`
6. render through `hal::audio::renderHostBuffer(...)`
7. poll app MIDI
8. decide between rendered output and dry passthrough
9. call `app_.tickHostAudio()`

Source:
- [SeedboxAudioProcessor.cpp:135](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L135)
- [HostAudioRealtimeService.cpp:5](/Users/bseverns/Documents/GitHub/seedbox/src/app/HostAudioRealtimeService.cpp#L5)

### `JuceHost::audioDeviceIOCallbackWithContext`

Primary path:

1. clear host outputs
2. reuse preallocated scratch vectors
3. copy input into scratch vectors
4. call `app_.setDryInputFromHost(...)`
5. render through `hal::audio::renderHostBuffer(...)`
6. poll app MIDI
7. optional dry passthrough copy
8. call `app_.tickHostAudio()`

Source:
- [JuceHost.cpp:161](/Users/bseverns/Documents/GitHub/seedbox/src/juce/JuceHost.cpp#L161)

### `SeedboxAudioProcessor::timerCallback`

Current plugin maintenance path:

1. JUCE timer fires on the host message thread
2. call `app_.serviceHostMaintenance()`
3. let the editor read the refreshed app snapshot on its own UI timer
4. rebuild OLED/debug text only when the cached snapshot is marked dirty

Source:
- [SeedboxAudioProcessor.cpp:245](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L245)
- [SeedboxAudioProcessorEditor.cpp:856](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessorEditor.cpp#L856)

### `JuceHost::MaintenanceTimer`

Current standalone maintenance path:

1. JUCE timer fires off the audio callback
2. call `app_.serviceHostMaintenance()`
3. keep standalone panel/preset/display upkeep off the device callback

Source:
- [JuceHost.cpp:132](/Users/bseverns/Documents/GitHub/seedbox/src/juce/JuceHost.cpp#L132)

## Non-Audio Thread Path

### `SeedboxAudioProcessor::parameterChanged` -> `HostControlBridge::handleParameterChange`

Current parameter-thread path:

1. APVTS notifies `SeedboxAudioProcessor::parameterChanged(...)`
2. the processor builds a narrow `ParameterContext`
3. `HostControlBridge` applies the host-thread mutation policy through `HostControlThreadAccess`
4. the processor shell stays out of the long parameter switch

Sources:
- [SeedboxAudioProcessor.cpp:353](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L353)
- [HostControlBridge.cpp:26](/Users/bseverns/Documents/GitHub/seedbox/src/juce/HostControlBridge.cpp#L26)

## Call Risk Table

| Site | Evidence | Alloc / resize | Lock | Filesystem / storage | Current status | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| scratch buffer preallocation in `prepareToPlay(...)` | [SeedboxAudioProcessor.cpp:104](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L104) | yes, but off audio thread | no repo lock | no | hardened | plugin scratch buffers are now preallocated before callback use |
| scratch buffer preallocation in `audioDeviceAboutToStart(...)` / `configureForTests(...)` | [JuceHost.cpp:123](/Users/bseverns/Documents/GitHub/seedbox/src/juce/JuceHost.cpp#L123) | yes, but off audio thread | no repo lock | no | hardened | standalone scratch vectors are now sized before callback entry |
| callback overflow guard (`numSamples > preparedScratchFrames_`) | [SeedboxAudioProcessor.cpp:143](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L143) | no | no repo lock | no | fail-closed | avoids allocation on unexpected oversize blocks at the cost of dropping that block |
| standalone overflow guard (`frames > preparedScratchFrames_`) | [JuceHost.cpp:171](/Users/bseverns/Documents/GitHub/seedbox/src/juce/JuceHost.cpp#L171) | no | no repo lock | no | fail-closed | same policy for the standalone callback |
| `app_.setDryInputFromHost(...)` | [AppState.cpp:1025](/Users/bseverns/Documents/GitHub/seedbox/src/app/AppState.cpp#L1025) | delegated | no repo lock | no | improved, still on audio thread | no longer owns heap storage, but still part of the callback path |
| `InputGateMonitor::setDryInput(...)` | [InputGateMonitor.cpp:44](/Users/bseverns/Documents/GitHub/seedbox/src/app/InputGateMonitor.cpp#L44) | no | no repo lock | no | hardened | now borrows the current block instead of copying into owned vectors |
| `hostControl_.syncHostTransport(...)` | [HostControlBridge.cpp:35](/Users/bseverns/Documents/GitHub/seedbox/src/juce/HostControlBridge.cpp#L35) | no repo allocation visible | no repo lock | no | likely safe-ish, not proven | host `getPosition()` is outside repo control; treat as host-boundary risk until documented |
| plugin MIDI staging -> `ProcessorMidiBackend` | [SeedboxAudioProcessor.cpp:185](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L185) | no heap growth in current queue | no repo lock | no | improved | plugin MIDI ingress now goes through bounded preallocated staging before `midi.poll()` |
| `hal::audio::renderHostBuffer(...)` -> `AppState::handleAudio(...)` | [AppState.cpp:517](/Users/bseverns/Documents/GitHub/seedbox/src/app/AppState.cpp#L517) | none obvious in render path once dry buffers exist | no repo lock | no | closer to safe | render path itself mostly stays in fixed buffers + engine state |
| `app_.midi.poll()` in standalone host | [JuceHost.cpp:211](/Users/bseverns/Documents/GitHub/seedbox/src/juce/JuceHost.cpp#L211) | no repo allocation in current backend | no mutex in current backend | no | improved | `JuceHost` now uses a bounded preallocated queue and drops overflow instead of locking |
| plugin `ProcessorMidiBackend` queueing | [SeedboxAudioProcessor.cpp:659](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L659) | bounded assignment only | no repo lock | no | hardened | plugin MIDI ingress now uses fixed-capacity storage and drops overflow instead of allocating |
| `app_.tickHostAudio()` | [SeedboxAudioProcessor.cpp:238](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L238) | not allocation-free by formal contract | no repo lock visible | no direct storage/display work | reduced, still under audit | now a thin wrapper over `HostAudioRealtimeService`, which owns the callback-safe heartbeat |
| `app_.serviceHostMaintenance()` in plugin timer | [SeedboxAudioProcessor.cpp:245](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L245) | can allocate indirectly through normal UI/control flows | not on audio thread | may reach preset/display flows | intentionally off callback | this is now where deferred preset commits, reseed requests, and display snapshots are serviced in the plugin lane |
| `app_.serviceHostMaintenance()` in standalone timer | [JuceHost.cpp:132](/Users/bseverns/Documents/GitHub/seedbox/src/juce/JuceHost.cpp#L132) | can allocate indirectly through normal UI/control flows | not on audio thread | may reach preset/display flows | intentionally off callback | standalone now has the same basic maintenance split as the plugin lane |
| JUCE cached display text refresh | [SeedboxAudioProcessorEditor.cpp:872](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessorEditor.cpp#L872) | no snapshot rebuild on clean frames | no repo lock | no | improved | editor/panel now consume `displayCache_` and only rebuild OLED/debug text when `displayDirty_` flips; live output meters still poll `LearnFrame` |
| `parameterChanged(...)` -> `HostControlBridge::handleParameterChange(...)` | [SeedboxAudioProcessor.cpp:353](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L353) | map updates only in current shell | no repo lock | may touch preset/seed/UI state by design | audited, still message-thread-sensitive | host-thread parameter mutations are now centralized, but still need an explicit contract table |

## Main Findings

### 1. Scratch-buffer ownership is preallocated, not fully finished

The plugin and standalone JUCE lanes now preallocate their scratch storage
before callback entry and fail closed if a host hands them a larger-than-
prepared block.

Direct evidence:

- [SeedboxAudioProcessor.cpp:104](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L104)
- [SeedboxAudioProcessor.cpp:143](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L143)
- [JuceHost.cpp:123](/Users/bseverns/Documents/GitHub/seedbox/src/juce/JuceHost.cpp#L123)
- [JuceHost.cpp:171](/Users/bseverns/Documents/GitHub/seedbox/src/juce/JuceHost.cpp#L171)

### 2. `InputGateMonitor::setDryInput` is the first concrete fix target

This path was the first concrete fix target, and it is now materially safer.
It no longer copies the host block into owned vectors; it borrows the current
dry-input span and re-probes gate state from that borrowed block:

- [HostControlService.cpp:122](/Users/bseverns/Documents/GitHub/seedbox/src/app/HostControlService.cpp#L122)
- [InputGateMonitor.cpp:44](/Users/bseverns/Documents/GitHub/seedbox/src/app/InputGateMonitor.cpp#L44)

It was a good first target because:

- the unsafe behavior is obvious
- the API boundary is already narrow
- fixing it helps both plugin and standalone JUCE paths

### 3. The callback now runs `tickHostAudio()`, not the whole maintenance pass

The JUCE callback no longer drives the full `tick()` body.

What remains on the audio thread:

- tempo smoothing
- external-clock watchdog updates
- internal or external clock edge advancement
- input-gate reseed checks
- preset crossfade stepping
- frame / granular-stat counters

What moved off the plugin audio callback:

- simulated panel polling
- input-grammar routing
- manual reseed requests
- deferred preset commits
- display snapshot refresh

Sources:
- [AppState.cpp:736](/Users/bseverns/Documents/GitHub/seedbox/src/app/AppState.cpp#L736)
- [AppState.cpp:759](/Users/bseverns/Documents/GitHub/seedbox/src/app/AppState.cpp#L759)
- [HostAudioRealtimeService.cpp:13](/Users/bseverns/Documents/GitHub/seedbox/src/app/HostAudioRealtimeService.cpp#L13)

That is the right direction, but not the end state. `tickHostAudio()` still
needs a tighter "RT-safe by contract" surface even though both JUCE lanes now
have an explicit non-audio maintenance pump.

### 3a. UI text refresh now uses the dirty snapshot cache

The JUCE editor and panel no longer call `captureDisplaySnapshot(...)` every UI
timer tick. They now:

- read `displayCache_` through `HostReadThreadAccess`
- rebuild OLED/debug text only when `displayDirty_` is set
- keep `LearnFrame` polling for live output-meter updates

That does not eliminate all UI polling, but it removes needless display-frame
rebuilds from clean timer ticks and makes the host lane honor the cached
snapshot that `serviceHostMaintenance()` already owns.

### 4. Both MIDI ingress paths are now bounded, but callback work still needs trimming

`JuceHost` no longer uses the old mutex-protected incoming MIDI queue. It now
uses a bounded preallocated ring and drops overflow when the queue is full:

- [JuceHost.cpp:39](/Users/bseverns/Documents/GitHub/seedbox/src/juce/JuceHost.cpp#L39)
- [SeedboxAudioProcessor.cpp:659](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L659)

That removes the obvious heap/mutex surprise from both JUCE MIDI ingress
paths. The remaining work is now more architectural than mechanical:

- keep shrinking `tickHostAudio()`
- audit parameter-thread mutations separately
- decide how overflow should be surfaced or observed in development builds

### 5. Parameter/UI thread mutations are a separate boundary

`parameterChanged(...)` is not on the audio callback, but it still mutates
runtime and editor state:

- [SeedboxAudioProcessor.cpp:353](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L353)
- [HostControlBridge.cpp:26](/Users/bseverns/Documents/GitHub/seedbox/src/juce/HostControlBridge.cpp#L26)

It writes into:

- `parameterState_` (`std::unordered_map`)
- APVTS `ValueTree` helpers
- seed and preset-facing `AppState` entry points

This path is now explicitly audited and centralized inside `HostControlBridge`,
which is better than leaving the processor shell to own an unbounded switch.
It is still not a trivial path, though:

- master-seed changes reseed the runtime immediately
- focused-seed engine changes update both runtime and persisted seed metadata
- quantize changes maintain a small cross-parameter state machine
- transport and clock toggles mutate runtime policy directly

That is not the same class of bug as the audio-thread issues above, but it
means the JUCE boundary should be documented as two-thread, not one-thread.

## Current Host Entry-Point Contract

This is the practical contract after the current refactor. It is now partly
enforced in the JUCE layer by `HostAudioThreadAccess`,
`HostReadThreadAccess`, and `HostControlThreadAccess`, even though `AppState`
itself still exposes the broader public surface underneath.

| AppState entry point | Current caller class | Intended thread contract | Notes |
| --- | --- | --- | --- |
| `setDryInputFromHost(...)` | `HostAudioThreadAccess` | audio thread only | borrowed span only; no owned heap staging |
| `tickHostAudio()` | `HostAudioThreadAccess` | audio thread only | still needs further trimming |
| `displayCache()`, `displayDirty()`, `uiStateCache()` | `HostReadThreadAccess` | JUCE read thread only | cached UI/snapshot reads; no direct mutation |
| `serviceHostMaintenance()` | `HostControlThreadAccess` | non-audio host thread | allowed to touch UI/storage-facing upkeep |
| `clearDisplayDirtyFlag()` | `HostControlThreadAccess` | non-audio host thread | acknowledges a consumed cached snapshot after UI refresh |
| `syncInternalBpmFromHostTransport(...)` | `HostAudioThreadAccess` | audio thread only | sanitized host transport BPM sync; does not dirty display state |
| `setInternalBpmFromHost(...)` | `HostControlThreadAccess` | host/control thread | control-surface BPM change; still allowed to dirty display state |
| `setTransportLatchFromHost(...)` | parameter path and editor/UI actions | host/control thread | mutates runtime transport policy |
| `setClockSourceExternalFromHost(...)` | parameter path and panel/editor UI | host/control thread | policy toggle, not callback-safe by contract |
| `setFollowExternalClockFromHost(...)` | parameter path and panel/editor UI | host/control thread | policy toggle, not callback-safe by contract |
| `setSwingPercentFromHost(...)` | parameter path | host/control thread | scheduler-facing control mutation |
| `applyQuantizeControlFromHost(...)` | parameter path | host/control thread | coupled scale/root host control |
| `setDebugMetersEnabledFromHost(...)` | parameter path | host/control thread | UI/telemetry-facing runtime toggle |
| `setInputGateDivisionFromHost(...)` | parameter path | host/control thread | reseed-grid policy mutation |
| `setInputGateFloorFromHost(...)` | parameter path | host/control thread | dry-input gate threshold mutation |
| `setFocusSeed(...)`, `setSeedEngine(...)`, `applySeedEditFromHost(...)` | parameter path and editor UI | host/control thread | seed/runtime mutation surface |
| `applyPresetFromHost(...)` | state restore, panel quick preset | host/control thread | immediate apply in host-audio mode when crossfade is off |

## Real-Time Safe Eventually Checklist

### Phase 1: dry-input path

- [x] replace `InputGateMonitor` vector ownership with preallocated staging or
      caller-owned spans
- [x] guarantee dry-input staging does not allocate, resize, or clear on the
      audio thread
- [x] preallocate plugin scratch buffers in `prepareToPlay`
- [x] preallocate standalone scratch buffers in `audioDeviceAboutToStart`
- [ ] decide whether the fail-closed oversize-block guard is sufficient or
      whether host block-size renegotiation needs a stronger non-RT handoff

### Phase 2: split audio-safe work from general control work

- [x] carve `tick()` into an audio-safe subset and a non-audio-thread control
      subset
- [x] isolate display snapshot refresh from the plugin audio callback
- [x] give standalone JUCE a non-audio maintenance pump equivalent to the
      plugin timer
- [x] split host transport BPM sync from the control-thread BPM hook
- [ ] ensure every preset save/recall/storage path stays off every JUCE audio
      callback, not just the main plugin `processBlock` path

### Phase 3: thread-boundary hygiene

- [x] audit `parameterChanged(...)` separately from `processBlock`
- [ ] document which `AppState` host entry points are audio-thread-safe,
      message-thread-safe, or neither
- [x] remove mutex locking from the standalone JUCE MIDI callback path
- [x] remove heap traffic from the plugin JUCE MIDI callback path

## Recommended First Code Change

The next move is no longer queue hardening. That part is in place. The next
move is to tighten the remaining callback contract:

- continue trimming `tickHostAudio()` until its contract is explicit and small
- document `AppState` host entry points by thread contract
- decide whether callback queue overflow should emit a debug-visible signal
