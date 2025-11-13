# Native golden pipeline — receipts for every render

This roadmap is the companion piece to `tests/native_golden`. The goal is to
keep simulator renders honest by capturing both the raw audio artifacts and a
manifest of hashes that tests can diff without golden-ear guesswork.

## Current footprint

- **Render target:** `tests/native_golden/test_main.cpp` now prints four audio
  fixtures and two control logs when `ENABLE_GOLDEN=1`: the original 110 Hz
  mono drone, a sampler chord stack, a resonator tail collage, plus Euclid and
  Burst debug transcripts. All land under `build/fixtures/` so you can audit the
  WAVs or diff the `.txt` files locally.
- **Hash discipline:** `golden::hash_pcm16` still mirrors the Python helper for
  PCM data, and a sibling FNV-1a byte walker fingerprints the log files. Both
  feed the same manifest so hashes stay in lockstep between test harness and
  CLI script.
- **Manifest:** `tests/native_golden/golden.json` stores the digest, artifact
  path, and either audio metadata (sample rate, frame count, channel count) or
  log metadata (line + byte counts). Optional notes keep the liner-book vibe
  intact.

## How to regenerate fixtures

```bash
pio test -e native -D ENABLE_GOLDEN=1
python scripts/compute_golden_hashes.py --write
```

Add `--note name="liner note"` if you want to annotate why a render changed.
The script prints a tidy summary before committing anything to disk, so you can
spot-check hashes before rewriting the manifest.

## Roadmap to richer coverage

1. **Layer more engines.** Sampler grains, resonator tails, and Euclid/Burst
   logs now ship in the harness. Next up: fold granular voices and mixer
   composites into the gallery so the manifest reads like a full track list.
2. **Multichannel adventures.** Once the mixer matrix stabilizes, consider
   adding stereo renders or parallel control streams (e.g. MIDI logs) so the
   manifest proves both the sound and the gestures that created it.
3. **CI artifacts.** Teach the GitHub workflow to upload `build/fixtures/*.wav`
   whenever `ENABLE_GOLDEN` is enabled. Pair the uploads with the manifest so
   reviewers can audition the before/after delta straight from the PR.
4. **Docs to tests loop.** Whenever you add a fixture, update this page and link
   directly to the test or example that produces it. Treat the roadmap like a
   lab book that references both the audio proof and the code sketch.

## Related code & docs

- Rendering + assertions: [`tests/native_golden/test_main.cpp`](../../tests/native_golden/test_main.cpp)
- WAV helpers: [`tests/native_golden/wav_helpers.*`](../../tests/native_golden)
- Hash/manifest script: [`scripts/compute_golden_hashes.py`](../../scripts/compute_golden_hashes.py)
- Manifest source of truth: [`tests/native_golden/golden.json`](../../tests/native_golden/golden.json)
