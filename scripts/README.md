# Utility scripts — small helpers, loud intent

This directory is the pit crew for SeedBox builds. Every script should explain
its mission up front and be safe to run on a laptop mid-soundcheck.

## Current roster

- `gen_version.py`
  - Emits `include/BuildInfo.h` with the current git hash, branch, and build
    timestamp.
  - PlatformIO calls this before builds so the firmware can scream its identity
    over serial.
  - If you touch the output format, update the consuming code in `src/` and note
    the change in the README.

## Adding new scripts

1. Keep dependencies standard-library unless you've documented the install
   steps in the top-level README.
2. Make the script idempotent—rerunning it shouldn't trash existing build
   artifacts.
3. Drop a usage example in this file. Future-you will forget the CLI flags.

Scripts are support acts, but they deserve headliner-level docs.
