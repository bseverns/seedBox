# Seed prime modes — proofs, rituals, and where to look

SeedBox mint seeds three ways. Each path rewrites the same `Seed` struct that both
hardware and the simulator share, so every reseed has receipts.

| Prime mode | How to grab it | What changes | Why it matters |
| --- | --- | --- | --- |
| **LFSR** | default state, or reseed from the Seed page | Spins a deterministic xorshift genome for every slot. | Great for "roll the dice" jams that still round-trip through presets and tests. |
| **Tap tempo** | flip the prime mode, then tap a BPM in from the front panel or MIDI | Density/jitter lock to your tapped tempo while the rest of the genome rerolls. | Lets workshops teach groove control without touching code. |
| **Preset** | load a curated bank, then reseed in preset mode | Copies stored genomes (tone, pitch, engine, everything) verbatim. | Classroom-ready starting points that still honor locks and serialization. |

## Where the code lives
- Seeds are plain structs documented in [`include/Seed.h`](../../include/Seed.h).
- All three prime paths flow through `AppState::primeSeeds`, so the scheduler, UI, and tests see the same genomes. The seed prime bypass (`-D SEED_PRIME_BYPASS=1` or Settings Alt-press) leaves non-focused slots empty so you can paint one seed at a time.
- Engines grab only what they need via the shared `Engine` contract; start with [`src/engine/README.md`](../../src/engine/README.md) for the DSP lanes.

## Regression receipts
The tests in [`tests/test_app/test_seed_prime_modes.cpp`](../../tests/test_app/test_seed_prime_modes.cpp) cover each ritual:
- `test_live_input_prime_triggers_live_voice_in_sim` feeds the granular engine the mock "live-in" buffer.
- `test_tap_tempo_prime_updates_lineage` averages recorded taps to keep the lineage glued to the performer’s BPM.
- `test_preset_prime_applies_granular_params` rehydrates curated banks with source/slot data intact even after you mutate unlocked seeds mid-lesson.

Want to watch the preset bank round-trip byte-for-byte? Run the native preset regression:

```bash
pio test -e native --filter test_app --test-name test_preset_round_trip_via_eeprom_store
```

Preset genomes live in [`docs/preset_primes/`](../preset_primes). The firmware hydrates them through `AppState::setSeedPreset` and `AppState::buildPresetSeeds`, so a reseed on the sampler will replay the exact pitches, envelopes, and engine picks captured in those JSON dumps.
