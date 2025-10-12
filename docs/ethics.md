# Ethics, privacy, and data rituals

SeedBox is a teaching instrument first and a performance rig second. That means
we treat every byte that touches the system as part of a workshop — something to
handle gently, log clearly, and erase when the jam is over. Here's the pact.

## Privacy stance

- **No silent logging.** The firmware does not phone home, collect analytics, or
  secretly persist seed tweaks. If a feature needs telemetry, it must be opt-in
  and documented in the README before it lands.
- **Local-first experiments.** Test fixtures and native builds only touch files
  you explicitly point them at. Quiet mode keeps persistence disabled, so your
  laptop's Documents folder stays drama-free.
- **Respect collaborators.** When sharing patches or test captures, scrub any
  student names, MIDI device serials, or personal notes unless the owner consents.

## Data handling promises

- **Explicit saves.** `Storage::saveScene` and its friends are inert until you
  compile with `QUIET_MODE=0`. Even then, the code writes only to paths you pass
  in — no hidden directories, no surprise uploads.
- **Disposable seed banks.** Loading a seed bank is a deliberate action that
  stays in-memory. Rebooting or reseeding wipes the slate so classrooms can reset
  between sessions.
- **Hardware boundaries.** USB MIDI, DIN MIDI, and SD card access are off when
  quiet mode is on. That protects shared rigs in a lab from waking up speakers or
  scribbling to SD while someone is teaching.

## How to stay accountable

1. **Document data flows.** When you add a feature that touches disk, network,
   or sensors, update this file and the relevant README with what gets read or
   written.
2. **Ask before logging.** If you need to capture debug traces that might
   include performance data, warn the performer and explain why.
3. **Default to deletion.** After workshops, clear any generated scenes or logs
   unless everyone involved wants to keep them. Quiet tools should leave quiet
   footprints.

Hold us to this. If you see a commit that sneaks around these ethics, open an
issue, drop a patch, or ping the crew. The synth only earns trust if we guard it
like the community space it is.
