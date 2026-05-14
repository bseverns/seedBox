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
"control-rate." In the JUCE lane, `tick()` is currently still called from the
audio callback, so control-rate work is not automatically real-time-safe.

## Direct Audio-Thread Path

### `SeedboxAudioProcessor::processBlock`

Primary path:

1. stage host input into `inputScratch_`
2. call `app_.setDryInputFromHost(...)`
3. resize/clear `renderScratch_`
4. sync host transport
5. apply controller MIDI to `AppState`
6. render through `hal::audio::renderHostBuffer(...)`
7. poll app MIDI
8. decide between rendered output and dry passthrough
9. call `app_.tick()`

Source:
- [SeedboxAudioProcessor.cpp:135](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L135)

### `JuceHost::audioDeviceIOCallbackWithContext`

Primary path:

1. clear host outputs
2. resize scratch vectors as needed
3. copy input into scratch vectors
4. call `app_.setDryInputFromHost(...)`
5. render through `hal::audio::renderHostBuffer(...)`
6. poll app MIDI
7. optional dry passthrough copy
8. call `app_.tick()`

Source:
- [JuceHost.cpp:161](/Users/bseverns/Documents/GitHub/seedbox/src/juce/JuceHost.cpp#L161)

## Call Risk Table

| Site | Evidence | Alloc / resize | Lock | Filesystem / storage | Current status | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| `inputScratch_.setSize(...)` | [SeedboxAudioProcessor.cpp:145](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L145) | yes | no repo lock | no | unsafe now | `AudioBuffer::setSize` can allocate or reallocate on the audio thread |
| `inputScratch_.setSize(0, 0, ...)` | [SeedboxAudioProcessor.cpp:154](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L154) | yes | no repo lock | no | unsafe now | still mutates owned storage on the audio thread |
| `renderScratch_.setSize(...)` | [SeedboxAudioProcessor.cpp:158](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L158) | yes | no repo lock | no | unsafe now | should be fully preallocated in `prepareToPlay` |
| `scratch(...)` helper in standalone host | [JuceHost.cpp:171](/Users/bseverns/Documents/GitHub/seedbox/src/juce/JuceHost.cpp#L171) | yes | no repo lock | no | unsafe now | helper uses `assign` when frame count changes |
| `inputScratchRight_.clear()` and similar | [JuceHost.cpp:186](/Users/bseverns/Documents/GitHub/seedbox/src/juce/JuceHost.cpp#L186) | may mutate capacity bookkeeping | no repo lock | no | avoid on audio thread | not as bad as reallocate, but still part of mutable vector ownership on callback |
| `app_.setDryInputFromHost(...)` | [AppState.cpp:1025](/Users/bseverns/Documents/GitHub/seedbox/src/app/AppState.cpp#L1025) | delegated | no repo lock | no | unsafe now | flows into vector-backed dry-input staging |
| `InputGateMonitor::setDryInput(...)` | [InputGateMonitor.cpp:44](/Users/bseverns/Documents/GitHub/seedbox/src/app/InputGateMonitor.cpp#L44) | yes | no repo lock | no | unsafe now | uses `assign` and `clear` on owned `std::vector<float>` buffers |
| `hostControl_.syncHostTransport(...)` | [HostControlBridge.cpp:35](/Users/bseverns/Documents/GitHub/seedbox/src/juce/HostControlBridge.cpp#L35) | no repo allocation visible | no repo lock | no | likely safe-ish, not proven | host `getPosition()` is outside repo control; treat as host-boundary risk until documented |
| controller MIDI -> `app_.onExternalControlChange(...)` | [SeedboxAudioProcessor.cpp:173](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L173) | no obvious allocation in current router path | no repo lock | no | needs deeper callgraph audit | current MN-42 path looks scalar and scheduler-oriented, but not formally RT-certified |
| `hal::audio::renderHostBuffer(...)` -> `AppState::handleAudio(...)` | [AppState.cpp:517](/Users/bseverns/Documents/GitHub/seedbox/src/app/AppState.cpp#L517) | none obvious in render path once dry buffers exist | no repo lock | no | closer to safe | render path itself mostly stays in fixed buffers + engine state |
| `app_.midi.poll()` in standalone host | [JuceHost.cpp:197](/Users/bseverns/Documents/GitHub/seedbox/src/juce/JuceHost.cpp#L197) | possible vector move in backend implementations | `JuceHost` MIDI backend uses mutex elsewhere | no | needs separate audit | `JuceHost`'s MIDI backend uses `std::mutex`; keep this path under explicit review |
| `app_.tick()` | [SeedboxAudioProcessor.cpp:227](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L227) | not allocation-free by contract | may reach non-RT control logic | can reach preset/storage flows if flags are armed | unsafe until split | this is the biggest architectural mismatch in the JUCE lane |

## Main Findings

### 1. Scratch-buffer ownership is not real-time-safe yet

Both JUCE entry points resize or mutate owned scratch storage on the audio
thread.

Direct evidence:

- [SeedboxAudioProcessor.cpp:145](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L145)
- [SeedboxAudioProcessor.cpp:158](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L158)
- [JuceHost.cpp:171](/Users/bseverns/Documents/GitHub/seedbox/src/juce/JuceHost.cpp#L171)

### 2. `InputGateMonitor::setDryInput` is the first concrete fix target

This path currently copies dry input into owned vectors and re-probes gate state
from those vectors:

- [HostControlService.cpp:122](/Users/bseverns/Documents/GitHub/seedbox/src/app/HostControlService.cpp#L122)
- [InputGateMonitor.cpp:44](/Users/bseverns/Documents/GitHub/seedbox/src/app/InputGateMonitor.cpp#L44)

It is a good first target because:

- the unsafe behavior is obvious
- the API boundary is already narrow
- fixing it helps both plugin and standalone JUCE paths

### 3. `tick()` is still running on the audio thread

`tick()` does much more than clock stepping:

- polls IO
- updates input grammar
- handles reseed and preset-commit policy
- advances crossfades
- rebuilds display snapshots

Source:
- [AppState.cpp:685](/Users/bseverns/Documents/GitHub/seedbox/src/app/AppState.cpp#L685)

That is acceptable as a transition state, but not as the end state for a
"real-time safe eventually" JUCE lane.

### 4. Parameter/UI thread mutations are a separate boundary

`parameterChanged(...)` is not on the audio callback, but it still mutates
runtime and editor state:

- [SeedboxAudioProcessor.cpp:326](/Users/bseverns/Documents/GitHub/seedbox/src/juce/SeedboxAudioProcessor.cpp#L326)

It writes into:

- `parameterState_` (`std::unordered_map`)
- APVTS `ValueTree` helpers
- seed and preset-facing `AppState` entry points

That is not the same class of bug as the audio-thread issues above, but it
means the JUCE boundary should be documented as two-thread, not one-thread.

## Real-Time Safe Eventually Checklist

### Phase 1: dry-input path

- [ ] replace `InputGateMonitor` vector ownership with preallocated staging or
      caller-owned spans
- [ ] guarantee dry-input staging does not allocate, resize, or clear on the
      audio thread
- [ ] preallocate plugin scratch buffers in `prepareToPlay`
- [ ] preallocate standalone scratch buffers in `audioDeviceAboutToStart`

### Phase 2: split audio-safe work from general control work

- [ ] carve `tick()` into an audio-safe subset and a non-audio-thread control
      subset
- [ ] ensure preset save/recall/storage paths cannot run from the host audio
      callback
- [ ] isolate display snapshot refresh from the audio callback

### Phase 3: thread-boundary hygiene

- [ ] audit `parameterChanged(...)` separately from `processBlock`
- [ ] document which `AppState` host entry points are audio-thread-safe,
      message-thread-safe, or neither
- [ ] remove mutex/heap surprises from JUCE MIDI boundary code or push them off
      the callback

## Recommended First Code Change

Start with `InputGateMonitor::setDryInput`.

Why:

- it is definitely on the audio callback path today
- it owns vectors and performs copies directly
- its contract is small enough to replace without rewriting the whole JUCE lane

That makes it the best first move for turning "desktop build works" into
"desktop audio boundary is becoming disciplined."
