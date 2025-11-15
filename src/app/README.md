# App orchestration field notes

Welcome to the cockpit where SeedBox decides what actually happens each control
frame. Think of this folder as the jam session where the firmware's state
machine, the human interface, and clock politics negotiate in public. We're
keeping the tone candid, because half the point is to teach you how to riff on
it yourself.

## AppState — mission control

`AppState` is the big switchboard: it owns the seed table, hands steady
snapshots to the UI, and exposes every hook the rest of the rig can poke during
runtime.【F:src/app/AppState.h†L37-L259】 A fresh instance wires clocks and IO,
then lets `tick()` do the heavy lifting each frame: poll the board, refresh the
input pipeline, react to queued gestures, advance the scheduler, and recache the
OLED snapshot.【F:src/app/AppState.cpp†L268-L513】

Most of the real drama lives inside `processInputEvents()`. Incoming gestures
are grouped into mode transitions, seed-prime tweaks, clock negotiations, and
page-specific handlers, so you can follow a single event from button debounce to
engine tweak without diffing half the codebase.【F:src/app/AppState.cpp†L515-L746】
Once you understand this router, the rest of the control flow reads like liner
notes.

## Clock — groove arbitrator

The `ClockProvider` family is a tiny interface that lets the scheduler speak the
same language whether we're self-clocked or following an external MIDI source.【F:include/app/Clock.h†L4-L81】 `AppState` attaches all three providers to the
pattern scheduler at boot and hot-swaps between them based on user gestures and
transport state.【F:src/app/AppState.cpp†L268-L589】 If you want to see the power
struggle in action, `tests/test_app/test_external_midi_priority.cpp` walks
through internal ticks, external clock takeover, and the hand-off back to the
internal groove once the MIDI transport stops.【F:tests/test_app/test_external_midi_priority.cpp†L1-L33】

## InputEvents — intent translator

`InputEvents` takes raw switch/encoder samples from the board, recognizes
press/double-press/hold/chord patterns, and hands `AppState` high-level events
like “Shift + Alt + Tone knob turned two clicks.”【F:src/app/InputEvents.h†L4-L75】
During each `tick()` the input buffer is refreshed, and `processInputEvents()`
uses those synthesized events to decide which page controller should react or
whether a mode change is due.【F:src/app/AppState.cpp†L485-L646】 The quickest
demo of that choreography lives in `tests/test_app/test_app.cpp`, where the
helpers simulate panel gestures and assert that modes, pages, and display
snapshots flip exactly when expected.【F:tests/test_app/test_app.cpp†L28-L229】

## Preset — memory polaroid

The `seedbox::Preset` struct is a documented snapshot of the whole performance
state: seeds, engine picks, focus index, clock preferences, and the active page.【F:include/app/Preset.h†L4-L49】 Its serializer/deserializer keep the JSON payload
self-explanatory, clamping granular/resonator parameters and normalizing live vs
SD clip sources so round-trips are predictable on hardware and in tests.【F:src/app/Preset.cpp†L31-L186】

`AppState` leans on that snapshot when you long-press the storage button: it
mints preset JSON, drops it into whatever `Store` backend is attached, and lets
recalls blend back in over a controlled crossfade.【F:src/app/AppState.cpp†L417-L510】 The exercise is captured in `tests/test_app/test_presets.cpp`, which
saves to an in-memory EEPROM, reseeds, recalls instantly and via crossfade, and
checks that granular params survive the blend.【F:tests/test_app/test_presets.cpp†L12-L44】

## SeedLock — bouncer for the genome table

`SeedLock` is intentionally boring: a per-seed vector and a global flag that say
which genomes are off-limits during reseeds or UI nudges.【F:include/SeedLock.h†L8-L37】 `AppState` exposes lock toggles to the UI and hardware pins, then
consults the guard before mutating seeds or honoring the global freeze switch.【F:src/app/AppState.h†L122-L158】【F:src/app/AppState.cpp†L417-L480】 The tests in
`tests/test_app/test_seed_lock_behaviour.cpp` make sure locked seeds survive
reseed cycles, engine swaps still update the audio backends, and MN-42 quantize
messages keep the sampler aligned.【F:tests/test_app/test_seed_lock_behaviour.cpp†L10-L90】

## Follow the control flow in tests

If you want to trace the whole arc from gesture to audio engine, hop between
these labs:

- `tests/test_app/test_app.cpp` for the end-to-end UI state machine — button and
  encoder macros drive `InputEvents`, trigger `AppState` mode handlers, and
  assert the display cache tells the same story.【F:tests/test_app/test_app.cpp†L28-L229】
- `tests/test_app/test_seed_lock_behaviour.cpp` for how reseeds respect per-seed
  and global locks while keeping engine caches honest.【F:tests/test_app/test_seed_lock_behaviour.cpp†L10-L90】
- `tests/test_app/test_presets.cpp` for preset save/recall flow and the crossfade
  helper that eases between captured genomes.【F:tests/test_app/test_presets.cpp†L12-L44】
- `tests/test_app/test_external_midi_priority.cpp` for the transport duel between
  internal clock ticks and incoming MIDI pulses.【F:tests/test_app/test_external_midi_priority.cpp†L1-L33】

Use this README as a backstage pass: tweak the code, re-run those tests, and
see exactly where the control flow bends. Then write your own riffs and leave
notes for the next punk who drops by.
