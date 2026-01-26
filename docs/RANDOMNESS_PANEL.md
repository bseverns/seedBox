# Randomness Panel

The `RandomnessPanel` lives on the control-rate side of `AppState` so lessons can describe how wide or narrow the procedural engine should roam before the audio layer ever sees a sample. Every parameter is clamped into a safe range (0…1) and consulted when seeds are primed, when `applyRepeatBias` mutates new seeds, and when the scheduler blends between generations.

## Parameters

- `entropy` controls how much `buildLfsrSeeds` leans on `masterSeed`’s deterministic hash vs. purely random values. `0.0` yields the same base set of pitches/density/tone/etc. every time (`mixEntropy` returns the hashed “base” value), while `1.0` lets RNG-set random values win. Intermediate settings gently nudge each field toward the random value.
- `mutationRate` is the floor applied to the parts of each seed that can drift (pitch, density, gate probability, etc.). It works alongside `entropy` so a high mutation rate with low entropy still moves those knobs slowly, while a low mutation rate keeps the voice more static.
- `repeatBias` keeps new seeds from diverging too far from the last generation. When non-zero, `applyRepeatBias` either copies prior seeds back in (hard reset behavior) or blends them based on `entropy` (soft/drift). A `repeatBias` of `0.0` lets new seeds replace the old ones instantly.
- `resetBehavior` selects how the scheduler treats the previous generation:
  * `Hard` wipes the queue and the generated seed list whenever a reseed happens, so the patch jumps immediately to the new genome with no carry-over.
  * `Soft` still biases toward prior seeds, but each regenerated parameter respects the current `entropy` so the system can gently morph.
  * `Drift` keeps the previous seed and drifts it toward the freshly generated values, giving students a hands-on way to explain interpolation and “laggy randomness.”

All four knobs are read at control pace, so knurled controls or hosted automation can step them without touching the audio ISR.

## Teacher Recipes

| Entropy | Mutation | Repeat Bias | Reset | Musical Result |
|---------|----------|-------------|-------|----------------|
| `0.0`   | `0.0`    | `0.0`       | `Hard` | Locked-in, deterministic rigs where `tests/test_app/test_randomness_panel.cpp` proves that rerunning the sim yields the same event trace every time. This is great for classroom “same seed, same output” demos. |
| `0.4`   | `0.2`    | `0.3`       | `Soft` | Gentle breathing: the next generation retains its identity but makes subtle detuning/density shifts. Perfect for stepping through how `entropy` mixes the hashed base and RNG values. |
| `0.9`   | `0.5`    | `0.8`       | `Drift` | Exploratory chaos that never leaves physical bounds. Drift blends locked seeds toward the new mix so the timbre swells in a gradually shifting cloud rather than snapping. |

Pair recipes with MIDI controls so students can see how the master seed and repeat bias interact; entropy is only one part of the story, and the other knobs explain where the guarded randomness lives.
