# Golden fixture pressings

These WAVs and logs are the receipts from the native golden harness. We keep
them checked in so CI can diff what the synth engines actually spit out without
having to re-render on every review. The placeholders are gone—these files are
fresh captures from `tools/native_golden_offline.cpp` so you can trust the
hashes to actually mean something.

## What's in the crate?

| Fixture | Story | Format |
| --- | --- | --- |
| `drone-intro.wav` | Mono 110 Hz drone for "is the DSP pipeline even awake?" sanity checks. | 48 kHz, mono | 
| `sampler-grains.wav` | Staggered sampler voices stacking root/fifth/ninth grains so voicing bugs scream. | 48 kHz, mono |
| `resonator-tail.wav` | Plucked impulse sloshing through the resonator decay tail; damping tweaks show up instantly. | 48 kHz, mono |
| `granular-haze.wav` | Stereo grain cloud with slow drift to smoke out sync/width regressions. | 48 kHz, stereo |
| `mixer-console.wav` | Console bus capture after routing the other fixtures together, peaks around -3 dBFS. | 48 kHz, stereo |
| `euclid-mask.txt` | Euclidean trigger mask parameters + bitmask dump. | text |
| `burst-cluster.txt` | Burst engine trigger spacing log for deterministic scheduling. | text |

## Regenerating the stash

- Run `./scripts/offline_native_golden.sh` whenever you change an engine or the
  native golden harness. It compiles `tools/native_golden_offline.cpp`, renders
  everything into `build/fixtures/`, and refreshes the manifest.
- Alternatively run `pio test -e native_golden` with `ENABLE_GOLDEN=1`, then
  rehash with `python3 scripts/compute_golden_hashes.py --write` (the script
  insists on Python 3.7+, so calling it through `python3` saves you from dusty
  aliases).
- Never hand-edit the fixtures; the hashes in
  [`tests/native_golden/golden.json`](../../tests/native_golden/golden.json) must
  stay in lock-step with these files. Update the manifest immediately after
  re-rendering so CI can diff the exact payloads.

Yeah it's a little noisy to stash binaries in git, but failing CI because the
`build/fixtures` directory evaporated is way noisier.
