# Golden fixture pressings

These WAVs and logs are the receipts from the native golden harness. We keep
them checked in so CI can diff what the synth engines actually spit out without
having to re-render on every review. The placeholders are gone—these files are
fresh captures from `tools/native_golden_offline.cpp` so you can trust the
hashes to actually mean something.

For the public-facing explanation of why these fixtures matter, start with
[`docs/SeedGallery.md`](../../docs/SeedGallery.md).

## What's in the crate?

| Fixture | Story | Format |
| --- | --- | --- |
| `drone-intro.wav` | 110 Hz mono drone capture that proves the DSP plumbing is awake. | 48 kHz, mono |
| `sampler-grains.wav` | Stacked sampler grains (root/fifth/ninth) to catch voicing and detune drift. | 48 kHz, mono |
| `resonator-tail.wav` | Plucked resonator tail that spotlights damping and feedback tweaks. | 48 kHz, mono |
| `granular-haze.wav` | Stereo grain cloud with slow swirl to make sync and width slips obvious. | 48 kHz, stereo |
| `mixer-console.wav` | Stereo console mix of every engine hitting around -3 dBFS for bus sanity. | 48 kHz, stereo |
| `euclid-mask.wav` | Euclid engine rendered as a stereo pan/envelope lab so the mask math is audible. | 48 kHz, stereo |
| `euclid-mask-control.txt` | Gate-by-gate ledger that mirrors the Euclid WAV pan/envelope choices. | text |
| `burst-cluster.wav` | Burst engine trigger cluster rendered as a mono whoosh so you can hear the spacing, not just stare at numbers. | 48 kHz, mono |
| `burst-cluster-control.txt` | Burst engine trigger spacing log for deterministic scheduling. | text |
| `layered-euclid-burst.wav` | Shared Euclid/Burst schedule across multiple engines so timing alignment becomes audible. | 48 kHz, stereo |
| `layered-euclid-burst-control.txt` | Shared-schedule ledger for the layered Euclid/Burst render. | text |
| `modulated-sampler.wav` | Noisy sampler/granular automation stress test. | 48 kHz, stereo |
| `modulated-sampler-control.txt` | Per-lane automation log for the modulated sampler stress case. | text |
| `engine-hybrid-stack.wav` | Multi-engine deterministic stack driven by shared Euclid/Burst scheduling. | 48 kHz, stereo |
| `engine-hybrid-stack-control.txt` | Event ledger + automation dump for the hybrid stack. | text |
| `engine-macro-orbits.wav` | Macro-modulation teaching pass with sweeping orbit/pan/tone lanes. | 48 kHz, stereo |
| `engine-macro-orbits-control.txt` | Detailed macro-lane ledger for the orbit render. | text |
| `engine-multi-ledger.wav` | Per-hit cross-engine study with hit-by-hit macro movement. | 48 kHz, stereo |
| `engine-multi-ledger-control.txt` | Compact step ledger for the multi-engine hit study. | text |
| `quad-bus.wav` | Four-lane bus mix that stress-tests the quad routing math without touching DAW panners. | 48 kHz, quad |
| `quad-bus-control.txt` | Routing ledger for every quad lane so a diff shows which stem moved. | text |
| `surround-bus.wav` | 5.1/6-channel spin on the same stems to keep the surround layout honest. | 48 kHz, 6-channel |
| `surround-bus-control.txt` | Surround bus routing + tilt log so reviewers can trace every hop. | text |
| `stage71-bus.wav` | 7.1 labeled-lane routing stress test. | 48 kHz, 8-channel |
| `stage71-bus-control.txt` | Named-lane routing ledger for the 7.1 render. | text |
| `reseed-A.wav` / `reseed-B.wav` | Baseline reseed passes for event-log and sequencing sanity. | 48 kHz, mono |
| `reseed-A-control.txt` / `reseed-B-control.txt` | Matching reseed ledgers. | text |
| `reseed-C.wav` | Higher-density reseed stress pass for tempo-locked behavior. | 48 kHz, mono |
| `reseed-C-control.txt` | Matching high-density reseed ledger. | text |
| `reseed-poly.wav` | Overlapping multi-lane reseed study. | 48 kHz, mono |
| `reseed-poly-control.txt` | Matching poly reseed ledger. | text |
| `reseed-log.json` | Structured reseed event transcript. | json |
| `long-random-take.wav` | 30-second deterministic long-form regression listen. | 48 kHz, mono |
| `long-random-take-control.txt` | Long-form timing/control ledger for the long take. | text |

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
