# Utility scripts — the pit crew

`scripts/` holds tiny helpers that keep the build smooth and the version info
honest. Nothing here should feel scary; if a script needs special setup, it must
say so loudly.

## What's in the garage

| Script | Job | Notes |
| --- | --- | --- |
| `gen_version.py` | Generates `include/BuildInfo.h` with git hash + build time. | PlatformIO runs it before builds so the firmware can introduce itself over serial. |

## When you add a script

1. Prefer standard-library dependencies. If you need extra packages, document
   the install steps in the top-level README.
2. Make reruns safe — scripts should be idempotent so they never trash previous
   artifacts.
3. Drop a usage example either in the script header or this README. Future you
   will appreciate the reminder.

Treat these helpers like the band techs: not flashy, but the show can't start
without them.
