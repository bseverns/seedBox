# SeedBox source tree — how the noise is carved

This folder is the beating heart of SeedBox. The layout mirrors the MOARkNOBS
ethos: obvious boundaries, verbose intent, and comments that read like a
late-night pair-programming session. Here's how to navigate it without stepping
on any landmines.

## Directory tour

- `app/`
  - Owns global state (`AppState`) and the teaching-friendly façade around it.
  - Anything that coordinates UI snapshots, reseed rituals, or seed genomes
    lives here.
- `engine/`
  - Audio engines with deterministic trigger plumbing. Today it's mostly
    skeletons (`Sampler`, `Granular`, `Resonator`), but the contracts are real
    and shared across hardware + native builds.
- `io/`
  - Hardware shims: MIDI routers, display drivers, codec setup. Wraps the
    Teensy-specific bits behind `#ifdef SEEDBOX_HW` so the native sim stays
    portable.
- `profiles/`
  - Seed generators and macro maps. This is where we teach the box what a
    "granular seed" even means.
- `util/`
  - Shared helpers (timers, deterministic RNG wrappers, logging). Keep them
    tiny and well-tested; anything spooky belongs here before it touches audio.
- `main.cpp`
  - The thin bootstrapper. Picks hardware vs native init paths and hands control
    to the app layer.

## Development expectations

- **Document your intent inline.** If a function name doesn't scream its
  purpose, leave a block comment above it that does.
- **Keep native + hardware parity.** Build both environments after structural
  changes so determinism stays intact.
- **Respect the seed doctrine.** New engines must flow through the scheduler →
  router → engine trigger pipeline. Drop a README update (here or in docs/) if
  you tweak that choreography.

## Testing from this folder

Most units have dedicated tests under `test/`. If you add a helper here, wire a
matching test case and explain the edge cases in the test comments. The idea is
to leave future contributors a map, not a mystery.
