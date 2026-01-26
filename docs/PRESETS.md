# Presets

## Quantized switching
- Preset recalls (storage button, host command, or quick panel) no longer mutate the seed table immediately: `AppState` captures the requested `seedbox::Preset` inside a `pendingPresetRequest_`, marks a boundary (`Step` today, `Bar` when the concept lands), and waits for `maybeCommitPendingPreset()` to run after the next control tick. That means no code path rewrites the seeds while a step is mid-flight, so timbral jumps only happen when the scheduler has already finished a tick.
- The scheduler keeps `kPresetBoundaryTicksPerBar = 24 * 4` ticks per bar so a future `PresetBoundary::Bar` request will automatically land at the next measure boundary; every recall still has at least the step-level safety net.

## Crossfade rules
- The smoothing path uses `AppState::kPresetCrossfadeTicks` (24 ticks → ~500 ms at 120 BPM) to blend the old and new seeds after the boundary commit; you still need to pass `crossfade=true` when calling `recallPreset` to flip the `presetCrossfade_` state.
- Cross fades only happen when the incoming preset has the same seed count as the one being replaced; otherwise the engine simply swaps instantaneously at the safe boundary and lets the next tick pick up the new table.
