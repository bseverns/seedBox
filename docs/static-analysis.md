# Static Analysis Cheatsheet

We run `pio check -e teensy40` as our baseline lint pass. PlatformIO wraps `cppcheck`, so the analyzer can dig into firmware-only quirks *if* we keep it pointed at the right sources. This page is the running log of how to read the output without losing your mind.

## Scope control

`cppcheck` happily descends into `.pio/libdeps` and even cached framework headers unless we explicitly fence it in. The `check_src_filter` stanza we added to `platformio.ini` keeps the pass focused on `src/` and `include/`. If you notice the tool complaining about vendored libraries again, double-check that filter or pass `--project=.` manually.

## Known noisy diagnostics

A handful of warnings stick around even after the scope fix:

- `unusedFunction` on things like `AppState::initHardware`, `EngineRouter::dispatchThunk`, or the HAL shims. These functions are either entry points for the Arduino runtime (`setup`/`loop`) or public hooks used through function pointers. They *are* exercised at runtime even if the analyzer cannot see the call sites.
- `unusedFunction` for hardware-specific helpers (`hal_audio`, `hal_io`, etc.) when running the simulator build. They get compiled on the board, so leave them be unless you intend to rip out the feature entirely.

Treat those as “double-check manually” rather than “delete the function”. When in doubt, search for the symbol in `src/` and peek at the documentation in `docs/roadmaps/`—we keep the story there up to date.

## Real issues worth fixing

Anything higher than `style` severity deserves a look. For example, we previously saw a `knownConditionTrueFalse` because a debug meter flag was hard-wired to `false` in the simulator. That was a legit dead branch that is now guarded with the right `#ifdef`.

If `cppcheck` ever spits out a preprocessor error from `.pio/libdeps`, it means the scope filter went missing. Restore it before trying to patch upstream headers.

Stay loud, stay curious, and annotate your findings like you’re narrating a zine. Future contributors (including future you) will thank you.
