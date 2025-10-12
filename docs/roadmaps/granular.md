# Option B — Granular engine battle plan

This is the manifest for the granular roadmap baked into `engine/Granular.*`.
The control scaffolding now spins up a real DSP graph (or tidy stubs for the
simulator), so we can reason about routing, voice stealing, and graph patching
before the audio assets exist.

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

> ⚙️ **Teensy Audio quirk** — the stock `AudioEffectGranular` that ships with the
> framework only exposes `setSpeed()` and the pitch/freeze entry points.
> SeedBox still records window skew + grain size as first-class seed data, then
> maps the requests onto whatever knobs the active core hands us (falling back
> to `beginPitchShift()` when `setGrainLength()` is missing). When we eventually
> ship a custom DSP node the same orchestration code will drive its richer API
> without mutating the planner.

## Trigger flow (see `EngineRouter` + `PatternScheduler`)

1. Scheduler decides a seed is allowed to fire.
2. `EngineRouter::dispatchThunk` hands the trigger to `GranularEngine::trigger`.
3. `trigger` claims/steals a voice, writes the plan (start sample, playback rate,
   source, etc.), and immediately maps those parameters onto the Teensy
   `AudioEffectGranular` nodes (hardware) or stub handles (sim).
4. Source routing honors `Seed::granular.source` by flipping the voice input
   mixer between the live I²S bus (slot 0) and a registered SD clip stream.
5. Spray/jitter, window skew, and stereo spread all get baked into the
   `GrainVoice` record so notebooks/tests can narrate exactly what would hit the
   DAC.

## DSP graph sketch

- Each grain voice owns a Teensy `AudioEffectGranular` with a tiny source mixer
  that crossfades between live input and an SD clip trigger.
- Voice outputs land in 10 submix mixers (4 voices per group) that collapse into
  a final L/R mixer pair before feeding I²S. Native builds keep a stub vector of
  "connections" so we can assert routing without dragging `Audio.h` into the
  host toolchain.
- `GrainVoice::dspHandle` tracks which DSP slot a plan inhabits. Tests can dump
  the struct and see which mixer slot the scheduling logic targeted.

## CPU sanity checks still todo

- Instrument `activeVoiceCount()` inside a perf HUD to watch density vs. CPU.
- Add optional debug builds that print histogram of grain sizes and spray.
- Stress test with live input muted to make sure SD streaming alone holds 40
  grains without underflowing.
- Profile the submix cascade once real assets land; the current fan-out (4
  voices per mixer, 3-layer sum) is conservative but we should validate block
  usage once envelopes and jittered buffers start chewing cycles.
