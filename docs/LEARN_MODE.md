# Learn mode telemetry

Learn mode is the control-rate snapshot we use when a teacher wants to answer two questions at once: “what is the generator doing?” and “what does the audio feel like?” `AppState::captureLearnFrame` now exposes both halves of the story in a single struct, so whatever UI, log, or serial dump you prefer can show them side by side without duplicating the scheduler or audio math.

## Audio insight

The `LearnFrame::AudioMetrics` block is refreshed at control rate from `handleAudio` right after the engines mix their voices. It measures:

- `leftRms` / `rightRms` – the channel-specific RMS energy, useful for spotting imbalances.
- `combinedRms` – the aggregate RMS that should match what a meter on a real mixer would read.
- `leftPeak` / `rightPeak` / `combinedPeak` – the highest absolute sample seen since `handleAudio` last ran. Great for watching envelope braking points.
- `clip` – `true` when any channel samples hit ±1.0 (the DIY limiter is being hit). That lets lessons call out when a tight resonator or burst hits the rail.
- `limiter` – currently a mirror of `clip`, reserved for future DSP limiting hooks.

You can fly the same `LearnFrame` into a console log or the JUCE panel; playback won’t change, but the RMS/peak values prove what the speakers “feel.”

## Generator insight

`LearnFrame::GeneratorMetrics` narrates the procedural side of the instrument. The fields include:

- `bpm` – what `PatternScheduler` thinks the tempo is right now.
- `clock` – `UiState::ClockSource::kInternal` vs. `kExternal`, mirroring the OLED banner’s `I`/`E`.
- `tick` / `step` / `bar` – the raw PPQN tick counter, the current 24-PPQN step, and how many full bars have elapsed (4 beats/bar, 24 ticks/beat).
- `events` – the number of seed triggers generated during the most recent tick (internal pulses count even when no MIDI faces the class).
- `focusSeedId` – the ID of the highlighted seed, which is the same value the UI scrolls through.
- `mutationCount` – how many seeds currently have `mutateAmt` > 0, so you can prove that the randomness panel is wired in.
- `focusMutateAmt` – the focused seed’s mutation depth, helpful when showing “this voice is drifting.”
- `density` / `probability` – the controls the scheduler checks before cutting an event; they come straight from the focused seed.
- `primeMode` – the current `SeedPrimeMode` (LFSR, tap-tempo, preset, live input), which corresponds to the tap/button logic the class is poking.
- `tapTempoBpm` / `lastTapIntervalMs` – metrics from the tap-tempo history so you can talk about the interval the students just tapped.
- `mutationRate` – the value from `RandomnessPanel::mutationRate`, showing how aggressively reseeds are allowed to wander.

The new regression test [`tests/test_app/test_learn_frame.cpp`](../tests/test_app/test_learn_frame.cpp) verifies that the generator half of the frame matches the focused seed and tap-tempo helpers, so every new telemetry field is covered before it hits a classroom demo.
