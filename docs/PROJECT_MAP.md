# PROJECT_MAP

## Core module map

### Clock & scheduler layer
`include/app/Clock.h` defines `ClockProvider` plus `InternalClock`, `MidiClockIn`, and `MidiClockOut`, and `src/app/AppState.cpp` wires whichever provider owns the groove into the scheduler, hands clock events to `PatternScheduler` (`src/engine/Patterns.{h,cpp}`), and mirrors external transport via `src/io/MidiRouter.*`.

### Generator(s)
`src/engine/EngineRouter.{h,cpp}` is the registry/dispatcher that routes scheduler ticks to the five engine families (`Sampler`, `Granular`, `Resonator`, `Euclid`, `Burst`) whose voice/effect plans live under `src/engine/*` (see `src/engine/README.md` for parameter focus and testing notes).

### Output layer
The engine files (`src/engine/Sampler.cpp`, `Granular.cpp`, `Resonator.cpp`, etc.) build the sample/timer plans, `EngineRouter` sends the rendered data into `hal::audio` callbacks, and the host audio path (`src/juce/SeedboxAudioProcessor.cpp` → `hal/hal_audio.cpp` → platform-specific backends) actually paints sound to the speakers.

### UI & MIDI
The JUCE bridge (`src/juce/SeedboxAudioProcessor.{h,cpp}`, `SeedboxAudioProcessorEditor.{h,cpp}`, plus `src/juce/ui/SeedboxPanelView.cpp`) exposes knobs, clock labels, and buttons, `src/ui/TextFrame.cpp` renders the ASCII OLED snapshot that the device UI poller consumes, and `src/io/MidiRouter.*` wires USB/TRS/MIDI transport + clock routing back into `AppState`.

### Storage
`include/io/Store.h` declares `Store`/`StoreEeprom`/`StoreSd`, and `src/io/Store.cpp` plus `src/io/Storage.{h,cpp}` parse URIs and serialize `seedbox::Preset` snapshots so `AppState` can dump and recall clock/engine state without touching hardware glue.

### Simulation harness
The native simulator board (`src/hal/board_native.{h,cpp}`) feeds scripted button/encoder events into `AppState`, the mock `hal::audio` stack (`src/hal/hal_audio.cpp`) captures sample clocks for tests, and the golden harness/settings under `tests/` exercise the same lanes so sim and hardware stay aligned.

## Audio-rate vs control-rate boundaries
- **Audio-rate path:** `SeedboxAudioProcessor::processBlock` (`src/juce/SeedboxAudioProcessor.cpp:126-220`) buffers host I/O, pumps it through `hal::audio::renderHostBuffer`/`Callback`, and lets `EngineRouter` + the engine voices (e.g., `src/engine/Sampler.cpp`) synthesize every sample block.
- **Control-rate path:** The JUCE lane is now split. `processBlock` calls `app_.tickHostAudio()` (`src/app/AppState.cpp`) for the scheduler heartbeat, clock edges, crossfades, and stats, while non-audio JUCE timers in the plugin/standalone host call `app_.serviceHostMaintenance()` for panel polling, deferred preset commits, and display refresh. That keeps the heaviest UI/storage work off the audio callback without inventing a second runtime.

## Top 10 files likely to touch next
1. `src/app/AppState.cpp` – central gesture/clock/resets logic that coordinates the scheduler, presets, and UI flows.
2. `include/app/Clock.h` – where the `ClockProvider` hierarchy lives; any clock boundary tweaks touch these abstractions.
3. `src/engine/Patterns.cpp` – the `PatternScheduler` implementation that dispatches ticks to seeds, so tempo or swing changes go here.
4. `src/engine/EngineRouter.cpp` – seed dispatch across engines, useful when adjusting outputs or engine assignments.
5. `src/engine/Sampler.cpp` – the default audio engine and voice allocator that ultimately shapes what the sequencer hears.
6. `src/io/MidiRouter.cpp` – MIDI transport/clock routing and handler plumbing that drives both hardware and the JUCE host.
7. `src/io/Store.cpp` – preset persistence and clock metadata serialization that `AppState` uses when the storage button is pressed.
8. `src/juce/SeedboxAudioProcessor.cpp` – host audio callback plus `tickHostAudio()` scheduling; it bridges control and audio-rate worlds.
9. `src/ui/TextFrame.cpp` – assembles the OLED snapshot (clock glyph, bpm, engine hints) the UI pages push to the screen.
10. `src/hal/board_native.cpp` – simulation script runner that keeps native tests and replay hooks aligned with hardware assumptions.
