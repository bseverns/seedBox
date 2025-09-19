# Option B — Granular engine battle plan

This is the manifest for the granular roadmap baked into `engine/Granular.*`.
Nothing renders audio yet; we're locking in the control surface so the DSP graph
can slide in later without rewiring the scheduler.

## Voice budget

- Teensy 4.0 target: **36 comfortable voices**, hard limit of 40 (see
  `GranularEngine::kVoicePoolSize`).
- Native sim: 12 voices to keep unit tests light.
- Voices recycle oldest-on-overflow so behavior stays deterministic even when
  the pool saturates.

## Sources

| Source | Seed encoding | Notes |
|--------|---------------|-------|
| Live input | `GranularEngine::Source::kLiveInput` | Mirrors SGTL5000 I²S RX. Disable in sim builds to keep tests quiet. |
| SD clip bank | `GranularEngine::Source::kSdClip` + `Seed::granular.sdSlot` | Up to 8 slots pre-registered via `registerSdClip`. Slots can carry path/metadata for the eventual streamer. |

Slot 0 is permanently aliased to live input so seeds can reference "microphone"
without juggling table indices.

## Parameters coming from the seed genome

| Seed field | Purpose |
|------------|---------|
| `Seed::pitch` + `Seed::granular.transpose` | Combined into a playback rate (2^n style). |
| `Seed::granular.grainSizeMs` | Envelope length target for each grain. |
| `Seed::granular.sprayMs` | Random offset per grain. RNG stays deterministic via the seed PRNG. |
| `Seed::granular.windowSkew` | Morphs between saw / Hann / exponential windows. Stored for the future DSP nodes. |
| `Seed::granular.stereoSpread` | Mid/side balance per grain. |
| `Seed::granular.source` & `sdSlot` | Pick live input vs SD clip. |

Mutate logic will eventually perturb these fields with bounded random walks
between triggers while still reseeding cleanly.

## Trigger flow (see `EngineRouter` + `PatternScheduler`)

1. Scheduler decides a seed is allowed to fire.
2. `EngineRouter::dispatchThunk` hands the trigger to `GranularEngine::trigger`.
3. `trigger` claims/steals a voice, writes the plan (start sample, playback rate,
   source, etc.).
4. Once the Teensy Audio graph is ready, we drive the buffer offsets from this
   plan and keep the control path untouched.

## CPU sanity checks still todo

- Instrument `activeVoiceCount()` inside a perf HUD to watch density vs. CPU.
- Add optional debug builds that print histogram of grain sizes and spray.
- Stress test with live input muted to make sure SD streaming alone holds 40
  grains without underflowing.
