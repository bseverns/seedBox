# SeedBox source tree — story mode

Welcome to the part of the project where ideas turn into actual code. This
folder is organized so you can read it like a workshop logbook: short riffs,
plenty of breadcrumbs, and room to sketch your own experiments.

## Map of the land

| Folder | What you'll find | Why it matters |
| --- | --- | --- |
| `app/` | High-level conductors like `AppState` and UI snapshot helpers. See [field notes](app/README.md). | Keeps the instrument's mood steady and understandable. |
| `engine/` | Audio engines (sampler stubs today, more wild stuff tomorrow). | Where seeds become sound textures. |
| `io/` | MIDI, display, codec glue — all the hardware conversations. | Lets the same code behave on laptop + Teensy. |
| `profiles/` | Seed recipes and macro maps. | Defines what a "granular seed" or other persona means. |
| `util/` | Tiny helpers, timers, deterministic RNG, logging. | The toolbox for everyone else. |
| `main.cpp` | A thin doorway into the whole system. | Chooses native vs hardware boot and hands off to `app/`. |

Each subfolder tries to self-document. If you add something new, drop a short
comment or mini README nearby so the next curious hacker can follow along.

## Working here without stress

- **Trace the flow**: Start at `main.cpp`, hop into `app/`, then explore engines
  and IO as you need. It's okay to treat it like a choose-your-own-adventure.
- **Mirror changes in tests**: Most logic has a friend in `tests/`. When you tweak
  behavior, update the matching test so the intent stays obvious.
- **Keep hardware flags gentle**: Wrap Teensy-only code in `#if SEEDBOX_HW`
  so the `native` build stays a truthful simulator.
- **Check quiet mode**: `SeedBoxConfig::kQuietMode` keeps seeds + IO muted until
  you're ready. Disable it per-env in `platformio.ini` when you want hardware to
  sing.

## When you extend the story

1. Sketch the idea in words first (README, doc comment, or a quick diagram).
2. Build the behavior in code.
3. Capture the lesson in a test or example so future readers see it in action.

Need a compass? Check the [test engine cases](../tests/test_engine) to watch a
SeedBox idea move from header to implementation to golden snapshot.

Think of `src/` as the main stage and everything else as the backstage crew. The
show goes best when the set list is clear and the amps aren't humming.
