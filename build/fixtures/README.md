# Golden fixture pressings

These WAVs and logs are the receipts from the native golden harness. We keep
them checked in so CI can diff what the synth engines actually spit out without
having to re-render on every review. The placeholders are gone—these files are
fresh captures from `tools/native_golden_offline.cpp` so you can trust the
hashes to actually mean something.

## What's in the crate?

| Fixture | Story | Format |
| --- | --- | --- |
| `drone-intro.wav` | 110 Hz mono drone capture that proves the DSP plumbing is awake. | 48 kHz, mono | 
| `sampler-grains.wav` | Stacked sampler grains (root/fifth/ninth) to catch voicing and detune drift. | 48 kHz, mono |
| `resonator-tail.wav` | Plucked resonator tail that spotlights damping and feedback tweaks. | 48 kHz, mono |
| `granular-haze.wav` | Stereo grain cloud with slow swirl to make sync and width slips obvious. | 48 kHz, stereo |
| `mixer-console.wav` | Stereo console mix of every engine hitting around -3 dBFS for bus sanity. | 48 kHz, stereo |
| `euclid-mask.txt` | Euclidean trigger mask parameters + bitmask dump. | text |
| `burst-cluster.wav` | Burst engine trigger cluster rendered as a mono whoosh so you can hear the spacing, not just stare at numbers. | 48 kHz, mono |
| `burst-cluster-control.txt` | Burst engine trigger spacing log for deterministic scheduling. | text |

> Burst got an actual WAV because rhythm should be heard, not inferred. The
> control log sticks around as the forensic ledger when hashes drift.

## Regenerating the stash

- Run `./scripts/offline_native_golden.sh` whenever you change an engine or the
  native golden harness. It compiles `tools/native_golden_offline.cpp`, renders
  everything into `build/fixtures/`, and refreshes the manifest.
- The helper now prints `[delta] fixture-name -> path expected OLD got NEW`
  instead of bailing out when a DSP tweak changes the hash. That's your cue to
  inspect the diff, then re-run `python3 scripts/compute_golden_hashes.py --write`
  so the manifest and header learn about the new audio/log payload.
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
