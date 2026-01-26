# Teaching preset bank

Eight focused presets for curriculum walkthroughs. Each file is a full
`seedbox::Preset::serialize()` snapshot so it can be loaded in sim or stored on
hardware.

## Preset list

1. `01_clock_subdivision.json` — Clock + subdivision
2. `02_euclidean_rhythm.json` — Euclidean rhythm
3. `03_probability_gates.json` — Probability gates
4. `04_lfsr_determinism.json` — LFSR determinism
5. `05_phase_drift_polyrhythm.json` — Phase drift / polyrhythm
6. `06_motif_mutation.json` — Motif + mutation
7. `07_density_dynamics.json` — Density to dynamics mapping
8. `08_constraint_improv.json` — Constraint improvisation

## Sim boot behavior

`initSim()` now tries to load `presets/teaching/01_clock_subdivision.json` as the
boot preset. If the file is missing or invalid, the sim falls back to the
standard LFSR prime.

## Hardware loading

These JSON files can be copied to SD and loaded through the storage helpers or
external tooling that calls `Storage::loadSeedBank()` / `Store` APIs. When
loaded, use `AppState::setSeedPreset()` and reseed with `SeedPrimeMode::kPreset`
to make the bank active.
